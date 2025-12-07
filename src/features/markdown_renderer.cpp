// src/features/markdown_renderer.cpp
#include "markdown_renderer.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <iostream>
#include <regex>

#ifdef TREE_SITTER_ENABLED
#include <tree_sitter/api.h>
#endif

MarkdownRenderer::MarkdownRenderer() = default;
MarkdownRenderer::~MarkdownRenderer() = default;

// ============================================================================
// PUBLIC API
// ============================================================================

void MarkdownRenderer::updateState(const GapBuffer &buffer, const TSTree *tree,
                                   int viewport_start, int viewport_end)
{
  if (!enabled_)
    return;

  // Expand viewport with buffer zone
  int buffer_lines = buffer.getLineCount();
  if (viewport_end < 0)
    viewport_end = buffer_lines - 1;

  int parse_start = std::max(0, viewport_start - VIEWPORT_BUFFER);
  int parse_end = std::min(buffer_lines - 1, viewport_end + VIEWPORT_BUFFER);

  size_t new_hash = std::hash<std::string>{}(buffer.getText());
  bool full_reparse = (new_hash != last_buffer_hash_);
  last_buffer_hash_ = new_hash;

  if (full_reparse)
  {
    // Full buffer changed - clear everything
    clearCache();
    parseCodeBlocksIncremental(buffer, tree, 0, buffer_lines - 1);
    parseHeadingsIncremental(buffer, parse_start, parse_end);
    parseListsIncremental(buffer, parse_start, parse_end);
  }
  else if (!dirty_lines_.empty())
  {
    // Incremental update - only process dirty lines + viewport
    int min_line = *std::min_element(dirty_lines_.begin(), dirty_lines_.end());
    int max_line = *std::max_element(dirty_lines_.begin(), dirty_lines_.end());

    // Expand to cover dirty region
    parse_start = std::min(parse_start, min_line);
    parse_end = std::max(parse_end, max_line);

    // Clear cache for affected lines
    for (int line : dirty_lines_)
    {
      render_cache_.erase(line);
    }

    // Re-parse only affected region
    parseCodeBlocksIncremental(buffer, tree, parse_start, parse_end);
    parseHeadingsIncremental(buffer, parse_start, parse_end);
    parseListsIncremental(buffer, parse_start, parse_end);

    dirty_lines_.clear();
  }
}

void MarkdownRenderer::invalidateLines(int start_line, int end_line)
{
  for (int i = start_line; i <= end_line; ++i)
  {
    dirty_lines_.insert(i);
    render_cache_.erase(i);
  }
}

void MarkdownRenderer::clearCache()
{
  code_blocks_.clear();
  heading_levels_.clear();
  list_indents_.clear();
  render_cache_.clear();
  dirty_lines_.clear();
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderLine(const std::string &line_text, int line_num,
                             int cursor_line, const GapBuffer &buffer,
                             const TSTree *tree, int viewport_start,
                             int viewport_end)
{
  if (!enabled_)
  {
    RenderInfo info;
    info.display_text = line_text;
    return info;
  }

  bool is_cursor_line = (line_num == cursor_line);

  // CRITICAL: Cursor line ALWAYS shows raw syntax (no concealing)
  // This prevents the "cursor jumping" issue mentioned in the research
  bool use_decoration_mode = !preview_mode_ || is_cursor_line;

  // Check cache (never cache cursor line)
  if (!is_cursor_line && isCacheValid(line_num, line_text))
  {
    return render_cache_[line_num];
  }

  RenderInfo info;

  // === CODE BLOCKS ===
  auto block_it = code_blocks_.find(line_num);
  if (block_it != code_blocks_.end())
  {
    const BlockInfo &block = block_it->second;

    if (line_num == block.start_line || line_num == block.end_line)
    {
      bool is_opening = (line_num == block.start_line);
      if (use_decoration_mode)
      {
        info = decorateCodeBlockDelimiter(line_text, block.language, is_opening,
                                          is_cursor_line);
      }
      else
      {
        info = renderCodeBlockDelimiterPreview(line_text, block.language,
                                               is_opening);
      }
    }
    else
    {
      info = decorateCodeBlockContent(line_text, block.language);
    }

    info.content_hash = hashLine(line_text);
    if (!is_cursor_line)
      render_cache_[line_num] = info;
    return info;
  }

  // === HEADINGS ===
  auto heading_it = heading_levels_.find(line_num);
  if (heading_it != heading_levels_.end())
  {
    if (use_decoration_mode)
    {
      info = decorateHeading(line_text, heading_it->second, is_cursor_line);
    }
    else
    {
      info = renderHeadingPreview(line_text, heading_it->second);
    }

    info.content_hash = hashLine(line_text);
    if (!is_cursor_line)
      render_cache_[line_num] = info;
    return info;
  }

  // === LISTS ===
  auto list_it = list_indents_.find(line_num);
  if (list_it != list_indents_.end())
  {
    if (use_decoration_mode)
    {
      info = decorateListItem(line_text, list_it->second);
    }
    else
    {
      info = renderListItemPreview(line_text, list_it->second);
    }

    info.content_hash = hashLine(line_text);
    if (!is_cursor_line)
      render_cache_[line_num] = info;
    return info;
  }

  // === NORMAL LINE ===
  info = decorateNormalLine(line_text);
  info.content_hash = hashLine(line_text);
  if (!is_cursor_line)
    render_cache_[line_num] = info;
  return info;
}

// ============================================================================
// INCREMENTAL PARSING (Viewport-aware)
// ============================================================================

void MarkdownRenderer::parseCodeBlocksIncremental(const GapBuffer &buffer,
                                                  const TSTree *tree,
                                                  int start_line, int end_line)
{
  // Clear blocks in affected range
  for (auto it = code_blocks_.begin(); it != code_blocks_.end();)
  {
    if (it->first >= start_line && it->first <= end_line)
    {
      it = code_blocks_.erase(it);
    }
    else
    {
      ++it;
    }
  }

  int line_count = buffer.getLineCount();
  int in_block = -1;
  std::string block_language;

  // Scan backward to find if we're already in a block
  for (int i = start_line - 1; i >= 0; i--)
  {
    std::string line = buffer.getLine(i);
    if (line.find("```") == 0)
    {
      in_block = i;
      block_language = line.substr(3);
      block_language.erase(0, block_language.find_first_not_of(" \t"));
      block_language.erase(block_language.find_last_not_of(" \t\n\r") + 1);
      break;
    }
  }

  // Parse forward
  for (int i = start_line; i <= end_line && i < line_count; i++)
  {
    std::string line = buffer.getLine(i);

    if (line.find("```") == 0)
    {
      if (in_block == -1)
      {
        // Opening fence
        in_block = i;
        block_language = line.substr(3);
        block_language.erase(0, block_language.find_first_not_of(" \t"));
        block_language.erase(block_language.find_last_not_of(" \t\n\r") + 1);
      }
      else
      {
        // Closing fence
        BlockInfo block;
        block.start_line = in_block;
        block.end_line = i;
        block.language = block_language;
        block.is_fenced = true;

        for (int j = in_block; j <= i; j++)
        {
          code_blocks_[j] = block;
        }

        in_block = -1;
        block_language.clear();
      }
    }
  }
}

void MarkdownRenderer::parseHeadingsIncremental(const GapBuffer &buffer,
                                                int start_line, int end_line)
{
  for (int i = start_line; i <= end_line && i < buffer.getLineCount(); i++)
  {
    std::string line = buffer.getLine(i);
    int level = getHeadingLevel(line);

    if (level > 0)
    {
      heading_levels_[i] = level;
    }
    else
    {
      heading_levels_.erase(i);
    }
  }
}

void MarkdownRenderer::parseListsIncremental(const GapBuffer &buffer,
                                             int start_line, int end_line)
{
  for (int i = start_line; i <= end_line && i < buffer.getLineCount(); i++)
  {
    std::string line = buffer.getLine(i);
    int indent = 0;
    std::string bullet = getListBullet(line, indent);

    if (!bullet.empty())
    {
      list_indents_[i] = indent;
    }
    else
    {
      list_indents_.erase(i);
    }
  }
}

// ============================================================================
// DECORATION MODE (Edit-friendly - no text modification)
// ============================================================================

MarkdownRenderer::RenderInfo
MarkdownRenderer::decorateHeading(const std::string &line, int level,
                                  bool is_cursor_line)
{
  RenderInfo info;
  info.display_text = line; // Keep text exactly as-is

  // Apply styling to entire line
  int color = MARKUP_HEADING;
  int attr = A_BOLD;

  if (config_.color_headings_by_level)
  {
    switch (level)
    {
    case 1:
      color = ColorPairs::UI_PRIMARY;
      if (config_.underline_h1_h2)
        attr |= A_UNDERLINE;
      break;
    case 2:
      color = ColorPairs::UI_ACCENT;
      if (config_.underline_h1_h2)
        attr |= A_UNDERLINE;
      break;
    case 3:
      color = ColorPairs::MARKUP_HEADING;
      break;
    default:
      color = ColorPairs::UI_SECONDARY;
      break;
    }
  }
  else if (level <= 2 && config_.underline_h1_h2)
  {
    attr |= A_UNDERLINE;
  }

  // Style the whole line
  info.spans.push_back({0, (int)line.length(), color, attr, 100});

  // Optional: Dim the # symbols specifically
  if (config_.fade_markers)
  {
    int hash_end = 0;
    for (char c : line)
    {
      if (c == '#')
        hash_end++;
      else
        break;
    }
    if (hash_end > 0)
    {
      info.spans.push_back(
          {0, hash_end, color, static_cast<int>(attr | A_DIM), 101});
    }
  }

  return info;
}

MarkdownRenderer::RenderInfo MarkdownRenderer::decorateCodeBlockDelimiter(
    const std::string &line, const std::string &language, bool is_opening,
    bool is_cursor_line)
{
  RenderInfo info;
  info.is_code_block_delimiter = true;
  info.display_text = line; // Keep fence visible

  // Dim the entire fence line
  info.spans.push_back({0, (int)line.length(), MARKUP_CODE, A_DIM, 90});

  // Add virtual text badge for language (doesn't modify buffer)
  if (is_opening && !language.empty() && config_.show_language_badge)
  {
    std::string display_lang = prettifyLanguageName(language);
    info.virtual_text_suffix = " [" + display_lang + "]";
    info.virtual_suffix_color = ColorPairs::UI_INFO;
  }

  return info;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::decorateCodeBlockContent(const std::string &line,
                                           const std::string &language)
{
  RenderInfo info;
  info.is_code_block = true;
  info.code_language = language;
  info.display_text = line; // Keep text exactly as-is

  // Apply minimal styling - let Tree-sitter injection handle syntax
  // Just add a subtle left border/padding based on config
  switch (config_.block_indent)
  {
  case CodeBlockIndent::NONE:
    info.left_padding = 0;
    break;
  case CodeBlockIndent::MINIMAL:
    info.left_padding = 2;
    break;
  case CodeBlockIndent::GUTTER:
    info.left_padding = 0;
    info.virtual_text_prefix = "│ ";
    info.virtual_prefix_color = MARKUP_CODE;
    break;
  case CodeBlockIndent::STANDARD:
    info.left_padding = 4;
    break;
  }

  return info;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::decorateListItem(const std::string &line, int indent)
{
  RenderInfo info;
  info.display_text = line; // Keep text as-is
  info.visual_indent = indent;

  static const std::regex list_regex(R"(^(\s*)([-*+]|\d+\.)\s+)",
                                     std::regex::optimize);
  std::smatch match;

  if (std::regex_search(line, match, list_regex))
  {
    int marker_start = match[1].length();
    int marker_end = match[0].length();

    // Color the bullet/number
    int color =
        config_.add_list_bullets_color ? ColorPairs::UI_ACCENT : MARKUP_LIST;
    info.spans.push_back({marker_start, marker_end, color, A_BOLD, 90});
  }

  return info;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::decorateNormalLine(const std::string &line)
{
  RenderInfo info;
  info.display_text = line; // Never modify text in decoration mode

  // Apply inline decorations (without modifying text)
  decorateInlineCode(line, info.spans);
  decorateBoldItalic(line, info.spans);
  decorateLinks(line, info.spans);

  return info;
}

// ============================================================================
// PREVIEW MODE (Read-only rendering with concealing)
// ============================================================================

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderHeadingPreview(const std::string &line, int level)
{
  RenderInfo info;

  // Hide the # symbols
  size_t content_start = line.find_first_not_of('#');
  if (content_start != std::string::npos && content_start < line.length())
  {
    if (line[content_start] == ' ')
      content_start++;

    info.display_text = line.substr(content_start);

    // Build character map for cursor positioning
    info.char_map.resize(info.display_text.length());
    for (size_t i = 0; i < info.display_text.length(); i++)
    {
      info.char_map[i] = content_start + i;
    }

    // Apply styling
    int color = config_.color_headings_by_level
                    ? (level == 1   ? ColorPairs::UI_PRIMARY
                       : level == 2 ? ColorPairs::UI_ACCENT
                                    : MARKUP_HEADING)
                    : MARKUP_HEADING;
    int attr = config_.bold_headings ? A_BOLD : 0;
    if (level <= 2 && config_.underline_h1_h2)
      attr |= A_UNDERLINE;

    info.spans.push_back(
        {0, (int)info.display_text.length(), color, attr, 100});
  }
  else
  {
    info.display_text = line;
  }

  return info;
}

MarkdownRenderer::RenderInfo MarkdownRenderer::renderCodeBlockDelimiterPreview(
    const std::string &line, const std::string &language, bool is_opening)
{
  RenderInfo info;
  info.is_code_block_delimiter = true;

  // In preview mode, we can hide or minimize fences
  switch (config_.block_style)
  {
  case CodeBlockStyle::HIDDEN:
    info.hide_line = true;
    break;

  case CodeBlockStyle::CLEAN:
    if (is_opening && !language.empty())
    {
      info.display_text = "  " + prettifyLanguageName(language);
      info.spans.push_back({2, (int)info.display_text.length(),
                            ColorPairs::UI_SECONDARY, A_BOLD, 85});
    }
    else
    {
      info.hide_line = true;
    }
    break;

  default:
    info.display_text = line;
    info.spans.push_back({0, (int)line.length(), MARKUP_CODE, A_DIM, 75});
    break;
  }

  return info;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderListItemPreview(const std::string &line, int indent)
{
  RenderInfo info;

  static const std::regex list_regex(R"(^(\s*)([-*+]|\d+\.)\s+)",
                                     std::regex::optimize);
  std::smatch match;

  if (std::regex_search(line, match, list_regex))
  {
    std::string prefix = match[1].str();
    std::string marker = match[2].str();
    std::string content = line.substr(match[0].length());

    // Replace bullet
    std::string bullet;
    if (marker.find('.') != std::string::npos)
    {
      bullet = marker + " "; // Keep numbered
    }
    else if (config_.replace_list_markers)
    {
      bullet = "• "; // Nice bullet
    }
    else
    {
      bullet = marker + " ";
    }

    info.display_text = prefix + bullet + content;
    info.visual_indent = indent;

    // Build char map
    info.char_map.resize(info.display_text.length());
    size_t original_marker_len = match[2].length();
    for (size_t i = 0; i < info.display_text.length(); i++)
    {
      if (i < prefix.length())
      {
        info.char_map[i] = i;
      }
      else if (i < prefix.length() + bullet.length())
      {
        info.char_map[i] = prefix.length();
      }
      else
      {
        info.char_map[i] =
            match[0].length() + (i - prefix.length() - bullet.length());
      }
    }

    // Color bullet
    int bullet_end = prefix.length() + bullet.length();
    int color =
        config_.add_list_bullets_color ? ColorPairs::UI_ACCENT : MARKUP_LIST;
    info.spans.push_back({(int)prefix.length(), bullet_end, color, A_BOLD, 90});
  }
  else
  {
    info.display_text = line;
  }

  return info;
}

// ============================================================================
// INLINE DECORATIONS (Applied in decoration mode)
// ============================================================================

void MarkdownRenderer::decorateInlineCode(const std::string &text,
                                          std::vector<ColorSpan> &spans)
{
  if (!config_.style_inline_code)
    return;

  static const std::regex code_regex(R"(`([^`]+)`)", std::regex::optimize);
  std::smatch match;
  std::string::const_iterator search_start(text.cbegin());

  while (std::regex_search(search_start, text.cend(), match, code_regex))
  {
    int start = std::distance(text.cbegin(), match[0].first);
    int end = std::distance(text.cbegin(), match[0].second);

    // Style entire match (including backticks)
    int color = ColorPairs::UI_SECONDARY;
    int attr = 0;

    if (config_.fade_markers)
    {
      // Dim the backticks, highlight the content
      spans.push_back({start, start + 1, color, A_DIM, 94});
      spans.push_back({start + 1, end - 1, color, attr, 95});
      spans.push_back({end - 1, end, color, A_DIM, 94});
    }
    else
    {
      spans.push_back({start, end, color, attr, 95});
    }

    search_start = match[0].second;
  }
}

void MarkdownRenderer::decorateBoldItalic(const std::string &text,
                                          std::vector<ColorSpan> &spans)
{
  // Bold
  static const std::regex bold_regex(R"(\*\*([^*]+)\*\*|__([^_]+)__)",
                                     std::regex::optimize);
  std::smatch match;
  std::string::const_iterator search_start(text.cbegin());

  while (std::regex_search(search_start, text.cend(), match, bold_regex))
  {
    int start = std::distance(text.cbegin(), match[0].first);
    int end = std::distance(text.cbegin(), match[0].second);

    if (config_.fade_markers)
    {
      spans.push_back({start, start + 2, MARKUP_BOLD, A_DIM, 93});
      spans.push_back({start + 2, end - 2, MARKUP_BOLD, A_BOLD, 95});
      spans.push_back({end - 2, end, MARKUP_BOLD, A_DIM, 93});
    }
    else
    {
      spans.push_back({start, end, MARKUP_BOLD, A_BOLD, 95});
    }

    search_start = match[0].second;
  }

  // Italic
  static const std::regex italic_regex(R"(\*([^*\n]+)\*|_([^_\n]+)_)",
                                       std::regex::optimize);
  search_start = text.cbegin();

  while (std::regex_search(search_start, text.cend(), match, italic_regex))
  {
    size_t pos = std::distance(text.cbegin(), match[0].first);

    // Skip if part of bold
    bool is_bold = false;
    char delim = match[0].str()[0];
    if (pos > 0 && text[pos - 1] == delim)
    {
      is_bold = true;
    }
    else if (pos + match[0].length() < text.length() &&
             text[pos + match[0].length()] == delim)
    {
      is_bold = true;
    }

    if (!is_bold)
    {
      int start = std::distance(text.cbegin(), match[0].first);
      int end = std::distance(text.cbegin(), match[0].second);

      if (config_.fade_markers)
      {
        spans.push_back({start, start + 1, MARKUP_ITALIC, A_DIM, 93});
        spans.push_back({start + 1, end - 1, MARKUP_ITALIC, A_UNDERLINE, 94});
        spans.push_back({end - 1, end, MARKUP_ITALIC, A_DIM, 93});
      }
      else
      {
        spans.push_back({start, end, MARKUP_ITALIC, A_UNDERLINE, 94});
      }
    }

    search_start = match[0].second;
  }
}

void MarkdownRenderer::decorateLinks(const std::string &text,
                                     std::vector<ColorSpan> &spans)
{
  static const std::regex link_regex(R"(\[([^\]]+)\]\(([^)]+)\))",
                                     std::regex::optimize);
  std::smatch match;
  std::string::const_iterator search_start(text.cbegin());

  while (std::regex_search(search_start, text.cend(), match, link_regex))
  {
    // Link text
    int text_start = std::distance(text.cbegin(), match[1].first);
    int text_end = std::distance(text.cbegin(), match[1].second);
    int attr = config_.underline_links ? A_UNDERLINE : 0;
    spans.push_back({text_start, text_end, MARKUP_LINK, attr, 96});

    // URL - dim it
    int url_start = std::distance(text.cbegin(), match[2].first);
    int url_end = std::distance(text.cbegin(), match[2].second);
    int url_attr = config_.dim_link_urls ? A_DIM : 0;
    spans.push_back({url_start, url_end, MARKUP_URL, url_attr, 85});

    search_start = match[0].second;
  }
}

// ============================================================================
// UTILITIES
// ============================================================================

int MarkdownRenderer::getHeadingLevel(const std::string &line)
{
  if (line.empty() || line[0] != '#')
    return 0;

  int level = 0;
  for (char c : line)
  {
    if (c == '#')
      level++;
    else if (c == ' ')
      break;
    else
      return 0;
  }

  return (level >= 1 && level <= 6) ? level : 0;
}

std::string MarkdownRenderer::getListBullet(const std::string &line,
                                            int &indent)
{
  static const std::regex list_regex(R"(^(\s*)([-*+]|\d+\.)\s+)",
                                     std::regex::optimize);
  std::smatch match;

  if (std::regex_search(line, match, list_regex))
  {
    indent = match[1].length();
    return match[2].str();
  }

  return "";
}

std::string MarkdownRenderer::prettifyLanguageName(const std::string &lang)
{
  if (lang.empty())
    return "";

  static const std::map<std::string, std::string> lang_map = {
      {"cpp", "C++"},
      {"c++", "C++"},
      {"cxx", "C++"},
      {"js", "JavaScript"},
      {"javascript", "JavaScript"},
      {"ts", "TypeScript"},
      {"typescript", "TypeScript"},
      {"py", "Python"},
      {"python", "Python"},
      {"rs", "Rust"},
      {"rust", "Rust"},
      {"go", "Go"},
      {"golang", "Go"},
      {"rb", "Ruby"},
      {"ruby", "Ruby"},
      {"java", "Java"},
      {"sh", "Shell"},
      {"bash", "Bash"},
      {"html", "HTML"},
      {"css", "CSS"},
      {"json", "JSON"},
      {"yaml", "YAML"},
      {"yml", "YAML"},
      {"xml", "XML"},
      {"sql", "SQL"},
      {"md", "Markdown"}};

  auto it = lang_map.find(lang);
  if (it != lang_map.end())
    return it->second;

  std::string result = lang;
  if (!result.empty())
    result[0] = std::toupper(result[0]);
  return result;
}

size_t MarkdownRenderer::hashLine(const std::string &line) const
{
  return std::hash<std::string>{}(line);
}

bool MarkdownRenderer::isCacheValid(int line_num,
                                    const std::string &line_text) const
{
  auto it = render_cache_.find(line_num);
  if (it == render_cache_.end())
    return false;
  return it->second.content_hash == hashLine(line_text);
}

bool MarkdownRenderer::isInViewport(int line_num, int viewport_start,
                                    int viewport_end) const
{
  if (viewport_start < 0 || viewport_end < 0)
    return true;
  return line_num >= viewport_start - VIEWPORT_BUFFER &&
         line_num <= viewport_end + VIEWPORT_BUFFER;
}

bool MarkdownRenderer::isInCodeBlock(int line_num) const
{
  return code_blocks_.find(line_num) != code_blocks_.end();
}

std::string MarkdownRenderer::getCodeBlockLanguage(int line_num) const
{
  auto it = code_blocks_.find(line_num);
  if (it != code_blocks_.end())
    return it->second.language;
  return "";
}

void MarkdownRenderer::cycleCodeBlockStyle()
{
  switch (config_.block_style)
  {
  case CodeBlockStyle::CLEAN:
    config_.block_style = CodeBlockStyle::INLINE_TAG;
    break;
  case CodeBlockStyle::INLINE_TAG:
    config_.block_style = CodeBlockStyle::CLASSIC;
    break;
  case CodeBlockStyle::CLASSIC:
    config_.block_style = CodeBlockStyle::MINIMAL;
    break;
  case CodeBlockStyle::MINIMAL:
    config_.block_style = CodeBlockStyle::HIDDEN;
    break;
  case CodeBlockStyle::HIDDEN:
    config_.block_style = CodeBlockStyle::CLEAN;
    break;
  }
  clearCache();
}