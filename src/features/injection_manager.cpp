// src/features/injection_manager.cpp
#include "injection_manager.h"
#include "query_manager.h"
#include "src/features/syntax_config_loader.h"
#include "src/features/syntax_highlighter.h"
#include <algorithm>
#include <iostream>

// NOTE: We do NOT include syntax_highlighter.h here to avoid circular includes
// Methods that need SyntaxHighlighter are in injection_manager_impl.cpp

#ifdef TREE_SITTER_ENABLED

InjectionManager::InjectionManager() {}

InjectionManager::~InjectionManager() { clearInjections(); }

void InjectionManager::initialize(SyntaxHighlighter *parent)
{
  parent_highlighter_ = parent;
}

void InjectionManager::clearInjections()
{
  std::lock_guard<std::mutex> lock(injection_mutex_);
  layers_.clear();
  line_to_layer_.clear();
}

void InjectionManager::parseInjections(const TSTree *parent_tree,
                                       const std::string &parent_language,
                                       const std::string &buffer_content)
{
  if (!parent_tree || !parent_highlighter_)
    return;

  std::lock_guard<std::mutex> lock(injection_mutex_);

  // Track old injection line ranges before clearing
  std::set<int> old_lines;
  for (const auto &[line, _] : line_to_layer_)
  {
    old_lines.insert(line);
  }

  layers_.clear();
  line_to_layer_.clear();

  std::string injection_query_str =
      QueryManager::getQuery(parent_language, "injections");
  if (injection_query_str.empty())
  {
    // Clear affected lines from old injections
    affected_lines_ = old_lines;
    return;
  }

  const TSLanguage *parent_ts_lang = getLanguageForInjection(parent_language);
  if (!parent_ts_lang)
  {
    affected_lines_ = old_lines;
    return;
  }

  uint32_t error_offset;
  TSQueryError error_type;
  TSQuery *injection_query =
      ts_query_new(parent_ts_lang, injection_query_str.c_str(),
                   injection_query_str.length(), &error_offset, &error_type);

  if (!injection_query)
  {
    std::cerr << "Failed to compile injection query for " << parent_language
              << " at offset " << error_offset << "\n";
    affected_lines_ = old_lines;
    return;
  }

  TSQueryCursor *cursor = ts_query_cursor_new();
  TSNode root = ts_tree_root_node(parent_tree);
  ts_query_cursor_exec(cursor, injection_query, root);

  struct InjectionCandidate
  {
    std::string language;
    TSNode content_node;
    uint32_t start_byte;
    uint32_t end_byte;
    uint32_t start_row;
    uint32_t end_row;
  };
  std::vector<InjectionCandidate> candidates;

  TSQueryMatch match;
  while (ts_query_cursor_next_match(cursor, &match))
  {
    std::string injected_lang;
    TSNode content_node;
    bool has_content = false;
    bool has_language = false;

    for (uint32_t i = 0; i < match.capture_count; i++)
    {
      uint32_t name_len;
      const char *name_ptr = ts_query_capture_name_for_id(
          injection_query, match.captures[i].index, &name_len);
      std::string capture_name(name_ptr, name_len);

      if (capture_name == "injection.language")
      {
        TSNode lang_node = match.captures[i].node;
        uint32_t start = ts_node_start_byte(lang_node);
        uint32_t end = ts_node_end_byte(lang_node);

        if (start < buffer_content.length() && end <= buffer_content.length())
        {
          injected_lang = buffer_content.substr(start, end - start);
          injected_lang.erase(
              std::remove_if(
                  injected_lang.begin(), injected_lang.end(), [](char c)
                  { return c == '"' || c == '\'' || c == ' ' || c == '\n'; }),
              injected_lang.end());

          injected_lang = normalizeLanguageName(injected_lang);
          has_language = true;
        }
      }
      else if (capture_name == "injection.content")
      {
        content_node = match.captures[i].node;
        has_content = true;
      }
    }

    if (!has_language && has_content)
    {
      uint32_t pattern_index = match.pattern_index;
      injected_lang =
          extractLanguageFromPattern(injection_query_str, pattern_index);
      if (!injected_lang.empty())
      {
        has_language = true;
      }
    }

    if (has_language && has_content && !injected_lang.empty())
    {
      uint32_t start_byte = ts_node_start_byte(content_node);
      uint32_t end_byte = ts_node_end_byte(content_node);
      TSPoint start_point = ts_node_start_point(content_node);
      TSPoint end_point = ts_node_end_point(content_node);

      bool is_duplicate = false;
      for (const auto &existing : candidates)
      {
        if (!(end_byte <= existing.start_byte ||
              start_byte >= existing.end_byte))
        {
          is_duplicate = true;
          break;
        }
      }

      if (!is_duplicate)
      {
        candidates.push_back({injected_lang, content_node, start_byte, end_byte,
                              start_point.row, end_point.row});
      }
    }
  }

  ts_query_cursor_delete(cursor);
  ts_query_delete(injection_query);

  std::sort(candidates.begin(), candidates.end(),
            [](const InjectionCandidate &a, const InjectionCandidate &b)
            { return a.start_byte < b.start_byte; });

  // Create injection layers
  for (const auto &candidate : candidates)
  {
    createInjectionLayer(candidate.language, candidate.content_node,
                         buffer_content);
  }

  // Track new injection line ranges
  std::set<int> new_lines;
  for (const auto &[line, _] : line_to_layer_)
  {
    new_lines.insert(line);
  }

  // Combine old and new lines as affected
  affected_lines_ = old_lines;
  affected_lines_.insert(new_lines.begin(), new_lines.end());

  if (!layers_.empty())
  {
    // std::cerr << "✓ " << layers_.size() << " injection(s) for "
    //           << parent_language;

    std::unordered_map<std::string, int> lang_counts;
    for (const auto &layer : layers_)
    {
      lang_counts[layer->language]++;
    }
  }
}

void InjectionManager::createInjectionLayer(const std::string &lang_name,
                                            TSNode content_node,
                                            const std::string &buffer_content)
{
  const TSLanguage *ts_lang = getLanguageForInjection(lang_name);
  if (!ts_lang)
  {
    // std::cerr << "⚠️  No parser for injected language: " << lang_name << "\n";
    return;
  }

  TSParser *parser = ts_parser_new();
  if (!ts_parser_set_language(parser, ts_lang))
  {
    ts_parser_delete(parser);
    // std::cerr << "⚠️  Failed to set parser language: " << lang_name << "\n";
    return;
  }

  uint32_t start_byte = ts_node_start_byte(content_node);
  uint32_t end_byte = ts_node_end_byte(content_node);
  TSPoint start_point = ts_node_start_point(content_node);
  TSPoint end_point = ts_node_end_point(content_node);

  if (start_byte >= buffer_content.length() ||
      end_byte > buffer_content.length())
  {
    ts_parser_delete(parser);
    return;
  }

  std::string content =
      buffer_content.substr(start_byte, end_byte - start_byte);

  TSTree *tree = ts_parser_parse_string(parser, nullptr, content.c_str(),
                                        content.length());
  ts_parser_delete(parser);

  if (!tree)
  {
    std::cerr << "⚠️  Failed to parse injected " << lang_name << " content\n";
    return;
  }

  TSQuery *query = getQueryForInjection(lang_name, ts_lang);
  if (!query)
  {
    ts_tree_delete(tree);
    std::cerr << "⚠️  No highlight query for injected language: " << lang_name
              << "\n";
    return;
  }

  auto layer = std::make_unique<InjectionLayer>();
  layer->language = lang_name;
  layer->tree = tree;
  layer->query = query;
  layer->start_row = start_point.row;
  layer->end_row = end_point.row;
  layer->start_byte = start_byte;
  layer->end_byte = end_byte;
  layer->content = std::move(content);

  int layer_index = layers_.size();
  for (uint32_t line = start_point.row; line <= end_point.row; line++)
  {
    line_to_layer_[line] = layer_index;
  }

  layers_.push_back(std::move(layer));
}

std::vector<ColorSpan> InjectionManager::getHighlightSpansForLine(
    int line_num, const std::string &line_content) const
{
  std::lock_guard<std::mutex> lock(injection_mutex_);

  const InjectionLayer *layer = findLayerForLine(line_num);
  if (!layer)
    return {}; // Not in an injection

  return executeInjectionQuery(layer, line_num, line_content);
}

const InjectionLayer *InjectionManager::findLayerForLine(int line_num) const
{
  auto it = line_to_layer_.find(line_num);
  if (it == line_to_layer_.end())
    return nullptr;

  int layer_idx = it->second;
  if (layer_idx >= 0 && layer_idx < (int)layers_.size())
    return layers_[layer_idx].get();

  return nullptr;
}

std::vector<ColorSpan>
InjectionManager::executeInjectionQuery(const InjectionLayer *layer,
                                        int line_num,
                                        const std::string &line_content) const
{
  if (!layer || !layer->tree || !layer->query)
    return {};

  std::vector<ColorSpan> spans;

  // Calculate the line's position within the injection
  int relative_line = line_num - layer->start_row;
  if (relative_line < 0)
    return {};

  // Find byte range for this line within the injection content
  uint32_t line_start_byte = 0;
  uint32_t line_end_byte = layer->content.length();

  // Count newlines to find line start
  int lines_counted = 0;
  for (size_t i = 0;
       i < layer->content.length() && lines_counted < relative_line; i++)
  {
    if (layer->content[i] == '\n')
    {
      lines_counted++;
      if (lines_counted == relative_line)
      {
        line_start_byte = i + 1;
      }
    }
  }

  // Find line end
  for (size_t i = line_start_byte; i < layer->content.length(); i++)
  {
    if (layer->content[i] == '\n')
    {
      line_end_byte = i;
      break;
    }
  }

  // Execute query on this line's byte range
  TSQueryCursor *cursor = ts_query_cursor_new();
  ts_query_cursor_set_byte_range(cursor, line_start_byte, line_end_byte);

  TSNode root = ts_tree_root_node(layer->tree);
  ts_query_cursor_exec(cursor, layer->query, root);

  TSQueryMatch match;
  while (ts_query_cursor_next_match(cursor, &match))
  {
    for (uint32_t i = 0; i < match.capture_count; i++)
    {
      TSQueryCapture capture = match.captures[i];
      TSNode node = capture.node;

      uint32_t start_byte = ts_node_start_byte(node);
      uint32_t end_byte = ts_node_end_byte(node);

      // Check if this capture affects our line
      if (start_byte >= line_start_byte && start_byte < line_end_byte)
      {
        uint32_t name_len;
        const char *name_ptr = ts_query_capture_name_for_id(
            layer->query, capture.index, &name_len);
        std::string capture_name(name_ptr, name_len);

        // Convert to line-relative column positions
        int start_col = start_byte - line_start_byte;
        int end_col = std::min((int)(end_byte - line_start_byte),
                               (int)line_content.length());

        if (start_col < end_col && end_col <= (int)line_content.length())
        {
          int color = getColorPairForCapture(capture_name);
          spans.push_back(
              {start_col, end_col, color, 0, 120}); // Higher priority
        }
      }
    }
  }

  ts_query_cursor_delete(cursor);
  return spans;
}

bool InjectionManager::isLineInjected(int line_num) const
{
  std::lock_guard<std::mutex> lock(injection_mutex_);
  return line_to_layer_.find(line_num) != line_to_layer_.end();
}

// NOTE: getLanguageForInjection and getColorPairForCapture
// are implemented in injection_manager_impl.cpp to avoid circular includes

TSQuery *InjectionManager::getQueryForInjection(const std::string &lang_name,
                                                const TSLanguage *ts_lang) const
{
  if (!ts_lang)
    return nullptr;

  // CRITICAL: Get the language config to read the queries list
  const LanguageConfig *config = getLanguageConfigFromParent(lang_name);

  if (!config || config->queries.empty())
  {
    // Fallback: try single query file
    std::string query_str = QueryManager::getQuery(lang_name, "highlights");

    if (query_str.empty())
      return nullptr;

    uint32_t error_offset;
    TSQueryError error_type;
    return ts_query_new(ts_lang, query_str.c_str(), query_str.length(),
                        &error_offset, &error_type);
  }

  // Load all queries from the config (just like the main highlighter does!)
  std::string merged_query;

  for (const auto &query_path : config->queries)
  {
    std::string partial = QueryManager::getQueryFromPath(query_path);
    if (!partial.empty())
    {
      if (!merged_query.empty())
        merged_query += "\n\n";
      merged_query += partial;
    }
  }

  if (merged_query.empty())
  {
    return nullptr;
  }

  // Compile the merged query
  uint32_t error_offset;
  TSQueryError error_type;
  TSQuery *query =
      ts_query_new(ts_lang, merged_query.c_str(), merged_query.length(),
                   &error_offset, &error_type);

  if (!query)
  {
    std::cerr << "Failed to compile highlight query for " << lang_name
              << " at offset " << error_offset << "\n";

    if (error_offset < merged_query.length())
    {
      int context_start = std::max(0, (int)error_offset - 50);
      int context_end =
          std::min((int)merged_query.length(), (int)error_offset + 50);
      std::cerr << "Context: ..."
                << merged_query.substr(context_start,
                                       context_end - context_start)
                << "...\n";
    }
  }

  return query;
}

const LanguageConfig *InjectionManager::getLanguageConfigFromParent(
    const std::string &lang_name) const
{
  if (!parent_highlighter_)
    return nullptr;

  return parent_highlighter_->getLanguageConfig(lang_name);
}

std::string
InjectionManager::normalizeLanguageName(const std::string &name) const
{
  // Map common aliases to canonical names
  static const std::unordered_map<std::string, std::string> aliases = {
      {"js", "javascript"},
      {"jsx", "javascript"},
      {"ts", "typescript"},
      {"tsx", "typescript"},
      {"py", "python"},
      {"rb", "ruby"},
      {"sh", "bash"},
      {"shell", "bash"},
      {"css", "css"},
      {"html", "html"},
      {"htm", "html"},
      {"md", "markdown"},
      {"markdown", "markdown"},
      {"cpp", "cpp"},
      {"c++", "cpp"},
      {"cxx", "cpp"},
      {"c", "c"},
      {"rs", "rust"},
      {"go", "go"},
      {"java", "java"},
      {"kt", "kotlin"},
      {"swift", "swift"}};

  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  auto it = aliases.find(lower);
  return (it != aliases.end()) ? it->second : lower;
}

std::string
InjectionManager::extractLanguageFromPattern(const std::string &query_str,
                                             uint32_t pattern_index) const
{
  // Parse query string to find #set! injection.language "xxx" directives
  size_t pos = 0;
  int current_pattern = 0;

  while (pos < query_str.length())
  {
    // Find next pattern (starts with '(')
    size_t pattern_start = query_str.find('(', pos);
    if (pattern_start == std::string::npos)
      break;

    // Find the matching closing ')'
    int depth = 1;
    size_t pattern_end = pattern_start + 1;
    while (pattern_end < query_str.length() && depth > 0)
    {
      if (query_str[pattern_end] == '(')
        depth++;
      else if (query_str[pattern_end] == ')')
        depth--;
      pattern_end++;
    }

    // Check if this is the pattern we're looking for
    if (current_pattern == (int)pattern_index)
    {
      std::string pattern =
          query_str.substr(pattern_start, pattern_end - pattern_start);

      // Look for #set! injection.language directive
      size_t set_pos = pattern.find("#set! injection.language");
      if (set_pos != std::string::npos)
      {
        // Find the quoted language name
        size_t quote_start = pattern.find('"', set_pos);
        if (quote_start != std::string::npos)
        {
          size_t quote_end = pattern.find('"', quote_start + 1);
          if (quote_end != std::string::npos)
          {
            return pattern.substr(quote_start + 1, quote_end - quote_start - 1);
          }
        }
      }

      break;
    }

    current_pattern++;
    pos = pattern_end;
  }

  return "";
}

#endif // TREE_SITTER_ENABLED