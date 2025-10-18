// src/features/markdown_renderer.cpp
#include "markdown_renderer.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <sstream>

#ifdef TREE_SITTER_ENABLED
#include <tree_sitter/api.h>
#endif

MarkdownRenderer::MarkdownRenderer() = default;
MarkdownRenderer::~MarkdownRenderer() = default;

void MarkdownRenderer::updateState(const GapBuffer &buffer, const TSTree *tree)
{
  if (!enabled_)
    return;

  size_t new_hash = std::hash<std::string>{}(buffer.getText());
  bool full_reparse = (new_hash != last_buffer_hash_);
  last_buffer_hash_ = new_hash;

  if (full_reparse)
  {
    clearCache();
    parseCodeBlocks(buffer, tree);
    parseHeadingsIncremental(buffer, 0, buffer.getLineCount() - 1);
    parseListsIncremental(buffer, 0, buffer.getLineCount() - 1);
  }
  else if (!dirty_lines_.empty())
  {
    int min_line = *dirty_lines_.begin();
    int max_line = *dirty_lines_.cbegin();

    for (int line : dirty_lines_)
    {
      render_cache_.erase(line);
    }

    parseCodeBlocks(buffer, tree);
    parseHeadingsIncremental(buffer, min_line, max_line);
    parseListsIncremental(buffer, min_line, max_line);

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

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderLine(const std::string &line_text, int line_num,
                             int cursor_line, const GapBuffer &buffer,
                             const TSTree *tree)
{
  if (!enabled_)
  {
    RenderInfo info;
    info.display_text = line_text;
    return info;
  }

  bool is_cursor_line = (line_num == cursor_line);

  // Check cache
  if (!is_cursor_line && isCacheValid(line_num, line_text))
  {
    return render_cache_[line_num];
  }

  // Check if in code block
  auto block_it = code_blocks_.find(line_num);
  if (block_it != code_blocks_.end())
  {
    const BlockInfo &block = block_it->second;
    RenderInfo info;

    if (line_num == block.start_line)
    {
      info = renderCodeBlockDelimiter(line_text, block.language, true,
                                      is_cursor_line);
    }
    else if (line_num == block.end_line)
    {
      info = renderCodeBlockDelimiter(line_text, block.language, false,
                                      is_cursor_line);
    }
    else
    {
      info = renderCodeBlockContent(line_text, block.language);
    }

    info.content_hash = hashLine(line_text);
    if (!is_cursor_line)
    {
      render_cache_[line_num] = info;
    }
    return info;
  }

  // Check if heading
  auto heading_it = heading_levels_.find(line_num);
  if (heading_it != heading_levels_.end())
  {
    auto info = renderHeading(line_text, heading_it->second, is_cursor_line);
    info.content_hash = hashLine(line_text);
    if (!is_cursor_line)
    {
      render_cache_[line_num] = info;
    }
    return info;
  }

  // Check if list item
  auto list_it = list_indents_.find(line_num);
  if (list_it != list_indents_.end())
  {
    auto info = renderListItem(line_text, list_it->second);
    info.content_hash = hashLine(line_text);
    if (!is_cursor_line)
    {
      render_cache_[line_num] = info;
    }
    return info;
  }

  // Normal line
  auto info = renderNormalLine(line_text);
  info.content_hash = hashLine(line_text);
  if (!is_cursor_line)
  {
    render_cache_[line_num] = info;
  }
  return info;
}

void MarkdownRenderer::parseCodeBlocks(const GapBuffer &buffer,
                                       const TSTree *tree)
{
  code_blocks_.clear();

  int line_count = buffer.getLineCount();
  int in_block = -1;
  std::string block_language;

  for (int i = 0; i < line_count; i++)
  {
    std::string line = buffer.getLine(i);

    if (line.find("```") == 0)
    {
      if (in_block == -1)
      {
        in_block = i;
        block_language = line.substr(3);
        block_language.erase(0, block_language.find_first_not_of(" \t"));
        block_language.erase(block_language.find_last_not_of(" \t\n\r") + 1);
      }
      else
      {
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

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderHeading(const std::string &line, int level,
                                bool is_cursor_line)
{
  RenderInfo info;

  if (is_cursor_line || config_.heading_style == HeadingStyle::CLASSIC)
  {
    // Show raw syntax when editing or in classic mode
    info.display_text = line;

    // Style based on level
    int color = MARKUP_HEADING;
    int attr = A_BOLD;

    if (config_.color_headings_by_level)
    {
      // Use different colors for different levels
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

    info.spans.push_back({0, (int)line.length(), color, attr, 100});
  }
  else
  {
    // Hide the # symbols
    size_t content_start = line.find_first_not_of('#');
    if (content_start != std::string::npos && content_start < line.length())
    {
      if (line[content_start] == ' ')
        content_start++;

      info.display_text = line.substr(content_start);

      // Build char map
      info.char_map.resize(info.display_text.length());
      for (size_t i = 0; i < info.display_text.length(); i++)
      {
        info.char_map[i] = content_start + i;
      }

      // Style based on level
      int color = MARKUP_HEADING;
      int attr = config_.bold_headings ? A_BOLD : 0;

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

      info.spans.push_back(
          {0, (int)info.display_text.length(), color, attr, 100});
    }
    else
    {
      info.display_text = line;
    }
  }

  return info;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderCodeBlockDelimiter(const std::string &line,
                                           const std::string &language,
                                           bool is_opening, bool is_cursor_line)
{
  RenderInfo info;
  info.is_code_block_delimiter = true;

  if (is_cursor_line)
  {
    // Always show raw syntax on cursor line
    info.display_text = line;
    info.spans.push_back({0, (int)line.length(), MARKUP_CODE, A_DIM, 90});
    return info;
  }

  std::string display_lang = prettifyLanguageName(language);

  switch (config_.block_style)
  {
  case CodeBlockStyle::HIDDEN:
    info.hide_line = true;
    break;

  case CodeBlockStyle::CLEAN:
    // Clean, minimal style (NEW DEFAULT)
    if (is_opening && !display_lang.empty() && config_.show_language_badge)
    {
      // Top: subtle line with language badge
      info.display_text = "  " + display_lang;
      info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 70});
      info.spans.push_back({2, (int)info.display_text.length(),
                            ColorPairs::UI_SECONDARY, A_BOLD, 85});
    }
    else if (is_opening)
    {
      // Just a subtle separator
      info.display_text = "  ";
      info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 70});
    }
    else
    {
      // Bottom: minimal or hidden
      if (config_.add_block_padding)
      {
        info.display_text = "  ";
        info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 70});
      }
      else
      {
        info.hide_line = true;
      }
    }
    break;

  case CodeBlockStyle::INLINE_TAG:
    // Language inline with border
    if (is_opening)
    {
      if (!display_lang.empty() && config_.show_language_badge)
      {
        info.display_text = "╭─ " + display_lang + " ";
        info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 75});
        info.spans.push_back({3, (int)info.display_text.length(),
                              ColorPairs::UI_INFO, A_BOLD, 85});
      }
      else
      {
        info.display_text = "╭─";
        info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 75});
      }
    }
    else
    {
      info.display_text = "╰─";
      info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 75});
    }
    break;

  case CodeBlockStyle::CLASSIC:
    // Traditional separator style
    if (!display_lang.empty() && config_.show_language_badge)
    {
      info.display_text = "─── " + display_lang + " ───";
      info.spans.push_back({0, 4, MARKUP_CODE, A_DIM, 75});
      info.spans.push_back(
          {4, 4 + (int)display_lang.length(), ColorPairs::UI_INFO, A_BOLD, 85});
      info.spans.push_back({4 + (int)display_lang.length(),
                            (int)info.display_text.length(), MARKUP_CODE, A_DIM,
                            75});
    }
    else
    {
      info.display_text = "────────────";
      info.spans.push_back(
          {0, (int)info.display_text.length(), MARKUP_CODE, A_DIM, 75});
    }
    break;

  case CodeBlockStyle::MINIMAL:
    info.display_text = "  ···";
    info.spans.push_back(
        {0, (int)info.display_text.length(), MARKUP_CODE, A_DIM, 70});
    break;
  }

  return info;
}

std::string MarkdownRenderer::prettifyLanguageName(const std::string &lang)
{
  if (lang.empty())
    return "";

  // Common languages with proper capitalization
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
      {"zsh", "Zsh"},
      {"html", "HTML"},
      {"css", "CSS"},
      {"scss", "SCSS"},
      {"json", "JSON"},
      {"yaml", "YAML"},
      {"yml", "YAML"},
      {"toml", "TOML"},
      {"xml", "XML"},
      {"sql", "SQL"},
      {"graphql", "GraphQL"},
      {"md", "Markdown"},
      {"markdown", "Markdown"},
      {"php", "PHP"},
      {"swift", "Swift"},
      {"kt", "Kotlin"},
      {"kotlin", "Kotlin"},
      {"scala", "Scala"},
      {"hs", "Haskell"},
      {"haskell", "Haskell"},
      {"lua", "Lua"},
      {"r", "R"},
      {"dart", "Dart"},
      {"perl", "Perl"},
      {"vim", "Vim"},
      {"dockerfile", "Docker"}};

  auto it = lang_map.find(lang);
  if (it != lang_map.end())
  {
    return it->second;
  }

  // Default: capitalize first letter
  std::string result = lang;
  if (!result.empty())
  {
    result[0] = std::toupper(result[0]);
  }
  return result;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderCodeBlockContent(const std::string &line,
                                         const std::string &language)
{
  RenderInfo info;
  info.is_code_block = true;
  info.code_language = language;
  info.display_text = line;

  // Apply indent style
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
    info.display_text = "│ " + line;
    info.spans.push_back({0, 2, MARKUP_CODE, A_DIM, 5});
    break;

  case CodeBlockIndent::STANDARD:
    info.left_padding = 4;
    break;
  }

  // Let injection system handle syntax highlighting

  return info;
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

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderListItem(const std::string &line, int indent)
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

    // Determine bullet character
    std::string bullet;
    if (marker.find('.') != std::string::npos)
    {
      // Numbered list - keep number
      bullet = marker + " ";
    }
    else if (config_.replace_list_markers)
    {
      // Use nice bullet
      bullet = "• ";
    }
    else
    {
      bullet = marker + " ";
    }

    info.display_text = prefix + bullet + content;
    info.visual_indent = indent;

    // Build char map
    info.char_map.resize(info.display_text.length());
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

    // Color the bullet
    int bullet_end = prefix.length() + bullet.length();
    int bullet_color =
        config_.add_list_bullets_color ? ColorPairs::UI_ACCENT : MARKUP_LIST;
    info.spans.push_back(
        {(int)prefix.length(), bullet_end, bullet_color, 0, 90});
  }
  else
  {
    info.display_text = line;
  }

  return info;
}

MarkdownRenderer::RenderInfo
MarkdownRenderer::renderNormalLine(const std::string &line)
{
  RenderInfo info;
  info.display_text = line;

  // Process inline elements
  processInlineCode(info.display_text, info.spans);
  processBoldItalic(info.display_text, info.spans);
  processLinks(info.display_text, info.spans);

  return info;
}

void MarkdownRenderer::processInlineCode(std::string &text,
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

    // Style the whole thing including backticks (or hide them if configured)
    int color =
        config_.hide_code_backticks ? MARKUP_CODE : ColorPairs::UI_SECONDARY;
    spans.push_back({start, end, color, 0, 95});

    search_start = match[0].second;
  }
}

void MarkdownRenderer::processBoldItalic(std::string &text,
                                         std::vector<ColorSpan> &spans)
{
  // Process bold (**text** or __text__)
  static const std::regex bold_regex(R"(\*\*([^*]+)\*\*|__([^_]+)__)",
                                     std::regex::optimize);
  std::smatch match;
  std::string::const_iterator search_start(text.cbegin());

  while (std::regex_search(search_start, text.cend(), match, bold_regex))
  {
    int start = std::distance(text.cbegin(), match[0].first);
    int end = std::distance(text.cbegin(), match[0].second);

    int attr = A_BOLD;
    if (config_.fade_markers && !config_.hide_emphasis_markers)
    {
      // Dim the markers
      spans.push_back({start, start + 2, MARKUP_BOLD, A_DIM, 93});
      spans.push_back({start + 2, end - 2, MARKUP_BOLD, attr, 95});
      spans.push_back({end - 2, end, MARKUP_BOLD, A_DIM, 93});
    }
    else
    {
      spans.push_back({start, end, MARKUP_BOLD, attr, 95});
    }

    search_start = match[0].second;
  }

  // Process italic (*text* or _text_)
  // Use simpler regex without lookbehind/lookahead (not supported in all C++
  // regex)
  static const std::regex italic_regex(R"(\*([^*\n]+)\*|_([^_\n]+)_)",
                                       std::regex::optimize);
  search_start = text.cbegin();

  while (std::regex_search(search_start, text.cend(), match, italic_regex))
  {
    size_t pos = std::distance(text.cbegin(), match[0].first);

    // Manual check: skip if part of bold (**, __)
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

      int attr = A_UNDERLINE; // Use underline for italic in terminal
      if (config_.fade_markers && !config_.hide_emphasis_markers)
      {
        spans.push_back({start, start + 1, MARKUP_ITALIC, A_DIM, 93});
        spans.push_back({start + 1, end - 1, MARKUP_ITALIC, attr, 94});
        spans.push_back({end - 1, end, MARKUP_ITALIC, A_DIM, 93});
      }
      else
      {
        spans.push_back({start, end, MARKUP_ITALIC, attr, 94});
      }
    }

    search_start = match[0].second;
  }
}

void MarkdownRenderer::processLinks(std::string &text,
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

    // URL - dim it if configured
    int url_start = std::distance(text.cbegin(), match[2].first);
    int url_end = std::distance(text.cbegin(), match[2].second);
    if (config_.dim_link_urls)
    {
      spans.push_back({url_start, url_end, MARKUP_URL, A_DIM, 85});
    }
    else
    {
      spans.push_back({url_start, url_end, MARKUP_URL, 0, 85});
    }

    search_start = match[0].second;
  }
}

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

bool MarkdownRenderer::isInCodeBlock(int line_num) const
{
  return code_blocks_.find(line_num) != code_blocks_.end();
}

std::string MarkdownRenderer::getCodeBlockLanguage(int line_num) const
{
  auto it = code_blocks_.find(line_num);
  if (it != code_blocks_.end())
  {
    return it->second.language;
  }
  return "";
}