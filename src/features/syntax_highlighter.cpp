#include "syntax_highlighter.h"
#include "src/core/config_manager.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif
#include <iostream>

#ifdef TREE_SITTER_ENABLED
#include "language_registry.h"
#include "tree_sitter/api.h"
#endif

#include "query_manager.h"

SyntaxHighlighter::SyntaxHighlighter()
    : config_loader_(std::make_unique<SyntaxConfigLoader>()),
      current_language_config_(nullptr), currentLanguage("text")
#ifdef TREE_SITTER_ENABLED
      ,
      parser_(nullptr), tree_(nullptr), current_ts_language_(nullptr),
      current_ts_query_(nullptr)
#endif
{
#ifdef TREE_SITTER_ENABLED
  initializeTreeSitter();
#endif
}

SyntaxHighlighter::~SyntaxHighlighter()
{
#ifdef TREE_SITTER_ENABLED
  cleanupTreeSitter();
#endif
}

bool SyntaxHighlighter::initialize(const std::string &config_directory)
{
  // std::cerr << "=== SyntaxHighlighter::initialize ===\n";
  // std::cerr << "Config directory: " << config_directory << std::endl;

  if (!config_loader_->loadAllLanguageConfigs(config_directory))
  {
    std::cerr << "Failed to load language configurations from: "
              << config_directory << std::endl;
    // Fall back to basic highlighting rules
    loadBasicRules();
    return false;
  }
  ConfigManager::registerReloadCallback(
      [this, config_directory]()
      {
        std::cerr << "Syntax config reload triggered." << std::endl;
        // Clear old configs and reload them
        config_loader_->language_configs_.clear();
        config_loader_->extension_to_language_.clear();

        // Reload all config files from the directory
        this->config_loader_->loadAllLanguageConfigs(
            ConfigManager::getSyntaxRulesDir());

        // Re-apply the parser for the current file
        setLanguage(this->currentLanguage); // Re-set language to pick up new
                                            // rules/queries
        // NOTE: Force a full buffer re-highlight/re-parse (e.g., set a flag)
      });

  // std::cout << "Successfully loaded language configurations" << std::endl;
  return true;
}

// Replace the setLanguage() method in syntax_highlighter.cpp with this debug
// version:

void SyntaxHighlighter::setLanguage(const std::string &extension)
{
  std::string language_name =
      config_loader_->getLanguageFromExtension(extension);

  const LanguageConfig *config =
      config_loader_->getLanguageConfig(language_name);

  if (config)
  {
    current_language_config_ = config;
    currentLanguage = language_name;

#ifdef TREE_SITTER_ENABLED
    if (!config->parser_name.empty() && parser_)
    {
      const TSLanguage *ts_language = getLanguageFunction(config->parser_name);
      if (ts_language)
      {
        if (!ts_parser_set_language(parser_, ts_language))
        {
          std::cerr << "ERROR: Failed to set language for parser" << std::endl;
          loadBasicRules();
          return;
        }
        current_ts_language_ = ts_language;

        // Clean up old query
        if (current_ts_query_)
        {
          ts_query_delete(current_ts_query_);
          current_ts_query_ = nullptr;
        }

        // DEBUG: Show what we're loading
        // std::cerr << "\n=== QUERY LOADING DEBUG for " << config->parser_name
        //           << " ===" << std::endl;

        // CHANGED: Don't use getAvailableQueries - it might be getting stale
        // data Instead, use the queries list from languages.yaml directly
        std::string merged_query_source;

        if (config->queries.empty())
        {
          std::cerr << "ERROR: No queries defined in languages.yaml for "
                    << config->parser_name << std::endl;
          loadBasicRules();
          return;
        }

        // std::cerr << "Queries defined in languages.yaml: "
        //           << config->queries.size() << std::endl;
        // for (const auto &query_path : config->queries)
        // {
        //   std::cerr << "  - " << query_path << std::endl;
        // }

        // FIXED: Use loadQueriesFromPaths which handles:
        // 1. Multiple query dependencies (C++ -> C + C++)
        // 2. Proper fallback chain (dev -> user -> system -> embedded)
        // std::cerr << "Loading queries from paths defined in
        // languages.yaml..."
        //           << std::endl;

        // Enable verbose mode for debugging
        QueryManager::setVerbose(true);

        merged_query_source =
            QueryManager::loadQueriesFromPaths(config->queries);

        QueryManager::setVerbose(false);

        // std::cerr << "\nTotal merged query size: "
        //           << merged_query_source.length() << " bytes" << std::endl;
        // std::cerr << "=== END QUERY LOADING DEBUG ===" << std::endl;

        // Parse the merged query
        if (!merged_query_source.empty())
        {
          uint32_t error_offset;
          TSQueryError error_type;
          current_ts_query_ = ts_query_new(
              current_ts_language_, merged_query_source.c_str(),
              merged_query_source.length(), &error_offset, &error_type);

          if (!current_ts_query_)
          {
            std::cerr << "\n❌ ERROR: Failed to parse merged query for "
                      << config->parser_name << std::endl;
            std::cerr << "  Error offset: " << error_offset << std::endl;
            std::cerr << "  Error type: " << error_type;

            // Provide detailed error information
            switch (error_type)
            {
            case TSQueryErrorNone:
              std::cerr << " (None)";
              break;
            case TSQueryErrorSyntax:
              std::cerr << " (Syntax Error)";
              break;
            case TSQueryErrorNodeType:
              std::cerr << " (Unknown Node Type)";
              break;
            case TSQueryErrorField:
              std::cerr << " (Unknown Field)";
              break;
            case TSQueryErrorCapture:
              std::cerr << " (Unknown Capture)";
              break;
            case TSQueryErrorStructure:
              std::cerr << " (Invalid Structure)";
              break;
            default:
              std::cerr << " (Unknown Error)";
              break;
            }
            std::cerr << std::endl;

            // Show context around error
            if (error_offset < merged_query_source.length())
            {
              int context_start = std::max(0, (int)error_offset - 100);
              int context_end = std::min((int)merged_query_source.length(),
                                         (int)error_offset + 100);

              std::cerr << "\nContext around error:" << std::endl;
              std::cerr << "..."
                        << merged_query_source.substr(
                               context_start, context_end - context_start)
                        << "..." << std::endl;

              // Point to error location
              std::cerr << std::string(error_offset - context_start + 3, ' ')
                        << "^" << std::endl;
            }

            // Fall back to basic highlighting
            std::cerr << "Falling back to basic highlighting" << std::endl;
            loadBasicRules();
          }
          else
          {
            // Check how many captures we got
            // uint32_t pattern_count =
            // ts_query_pattern_count(current_ts_query_); uint32_t capture_count
            // = ts_query_capture_count(current_ts_query_);

            // std::cerr << "\n✅ Successfully loaded Tree-sitter query for "
            //           << config->parser_name << std::endl;
            // std::cerr << "   Patterns: " << pattern_count << std::endl;
            // std::cerr << "   Captures: " << capture_count << std::endl;

            // List first 10 capture names
            // std::cerr << "   Sample captures: ";
            // for (uint32_t i = 0; i < std::min(10u, capture_count); ++i)
            // {
            //   uint32_t len;
            //   const char *name =
            //       ts_query_capture_name_for_id(current_ts_query_, i, &len);
            //   std::cerr << std::string(name, len) << " ";
            // }
            std::cerr << std::endl;
          }
        }
        else
        {
          std::cerr << "WARNING: No query content loaded for "
                    << config->parser_name << std::endl;
          loadBasicRules();
        }
      }
      else
      {
        std::cerr << "ERROR: No Tree-sitter language function found for: "
                  << config->parser_name << std::endl;
        loadBasicRules();
      }
    }
    else
    {
      std::cerr << "Tree-sitter not available or no parser specified, using "
                   "basic highlighting"
                << std::endl;
      loadBasicRules();
    }
#else
    std::cerr << "Tree-sitter disabled, using basic highlighting" << std::endl;
    loadBasicRules();
#endif
  }
  else
  {
    std::cerr << "ERROR: No config found for language: " << language_name
              << std::endl;
    loadBasicRules();
    currentLanguage = "text";
    current_language_config_ = nullptr;
  }
}

std::vector<ColorSpan>
SyntaxHighlighter::getHighlightSpans(const std::string &line, int lineIndex,
                                     const GapBuffer &buffer) const
{
  // Check cache first
  auto cache_it = line_cache_.find(lineIndex);
  if (cache_it != line_cache_.end())
  {
    return cache_it->second;
  }

  // Handle Markdown special states
  if (currentLanguage == "Markdown" && line_states_.count(lineIndex))
  {
    MarkdownState state = line_states_.at(lineIndex);
    if (state == MarkdownState::IN_FENCED_CODE_BLOCK)
    {
      std::vector<ColorSpan> result = {
          {0, (int)line.length(), getColorPairValue("MARKDOWN_CODE_BLOCK"),
           A_NORMAL, 100}};
      line_cache_[lineIndex] = result;
      return result;
    }
    else if (state == MarkdownState::IN_BLOCKQUOTE)
    {
      std::vector<ColorSpan> result = {
          {0, (int)line.length(), getColorPairValue("MARKDOWN_BLOCKQUOTE"),
           A_NORMAL, 90}};
      line_cache_[lineIndex] = result;
      return result;
    }
  }

  std::vector<ColorSpan> result;

#ifdef TREE_SITTER_ENABLED
  // REMOVED: The lazy reparse check that was causing full reparses
  // Tree-sitter tree is kept up-to-date via incremental edits

  if (current_ts_query_ && tree_)
  {
    try
    {
      result = executeTreeSitterQuery(line, lineIndex);
    }
    catch (const std::exception &e)
    {
      std::cerr << "Tree-sitter query error on line " << lineIndex << ": "
                << e.what() << std::endl;
      result = getBasicHighlightSpans(line);
    }
  }
#endif

  // Fall back to basic highlighting if no Tree-sitter result
  if (result.empty())
  {
    result = getBasicHighlightSpans(line);
  }

  // Cache the result
  line_cache_[lineIndex] = result;
  return result;
}

void SyntaxHighlighter::updateTreeAfterEdit(
    const GapBuffer &buffer, size_t byte_pos, size_t old_byte_len,
    size_t new_byte_len, uint32_t start_row, uint32_t start_col,
    uint32_t old_end_row, uint32_t old_end_col, uint32_t new_end_row,
    uint32_t new_end_col)
{
#ifdef TREE_SITTER_ENABLED
  if (!tree_ || !parser_ || !current_ts_language_)
    return;

  std::lock_guard<std::mutex> lock(tree_mutex_);

  // Step 1: Apply the edit to the tree structure
  TSInputEdit edit = {.start_byte = (uint32_t)byte_pos,
                      .old_end_byte = (uint32_t)(byte_pos + old_byte_len),
                      .new_end_byte = (uint32_t)(byte_pos + new_byte_len),
                      .start_point = {start_row, start_col},
                      .old_end_point = {old_end_row, old_end_col},
                      .new_end_point = {new_end_row, new_end_col}};

  ts_tree_edit(tree_, &edit);
  tree_version_++;

  // Step 2: Update the content string
  bool needs_content_rebuild = false;

  if (current_buffer_content_.empty() || old_end_row != new_end_row ||
      (new_byte_len > 100 || old_byte_len > 100))
  {
    needs_content_rebuild = true;
  }

  if (needs_content_rebuild)
  {
    current_buffer_content_ = buffer.getText();
  }
  else
  {
    if (old_byte_len > 0)
    {
      current_buffer_content_.erase(byte_pos, old_byte_len);
    }
    if (new_byte_len > 0)
    {
      // For newline insertion, just insert the newline directly
      if (new_byte_len == 1 && old_end_row != new_end_row)
      {
        current_buffer_content_.insert(byte_pos, "\n");
      }
      else
      {
        // For regular character insertion
        std::string new_text = buffer.getLine(start_row);
        size_t insert_offset = std::min(start_col, (uint32_t)new_text.length());
        size_t insert_len =
            std::min((size_t)new_byte_len, new_text.length() - insert_offset);
        current_buffer_content_.insert(byte_pos, new_text, insert_offset,
                                       insert_len);
      }
    }
  }

  // Step 3: Incremental reparse
  TSTree *old_tree = tree_;
  tree_ =
      ts_parser_parse_string(parser_, old_tree, current_buffer_content_.c_str(),
                             current_buffer_content_.length());

  if (tree_)
  {
    if (old_tree)
      ts_tree_delete(old_tree);
  }
  else
  {
    tree_ = old_tree;
  }

  // Step 4: Cache invalidation
  int invalidate_start = std::min((int)start_row, (int)old_end_row);
  int invalidate_end = std::max((int)new_end_row, invalidate_start + 50);

  for (int line = invalidate_start;
       line <= invalidate_end && line < buffer.getLineCount(); ++line)
  {
    line_cache_.erase(line);
  }

#endif
}

void SyntaxHighlighter::invalidateLineCache(int lineNum)
{
  line_cache_.erase(lineNum);
}

void SyntaxHighlighter::bufferChanged(const GapBuffer &buffer)
{
#ifdef TREE_SITTER_ENABLED
  if (!parser_ || !current_ts_language_)
    return;

  // Only do full reparse if we have no tree or content is empty
  if (!tree_ || current_buffer_content_.empty())
  {
    updateTree(buffer);
  }
  // Otherwise, incremental edits via updateTreeAfterEdit() are sufficient
#endif

  if (currentLanguage == "Markdown")
  {
    updateMarkdownState(buffer);
  }
}

void SyntaxHighlighter::invalidateFromLine(int startLine)
{
  // This is for structural changes (insert/delete lines)
  // Clear only lines >= startLine, but do it efficiently

  auto it = line_cache_.lower_bound(startLine);
  if (it != line_cache_.end())
  {
    line_cache_.erase(it, line_cache_.end());
  }

  // Don't clear content cache unless change is massive
  // Let incremental edits handle the tree updates
}

#ifdef TREE_SITTER_ENABLED
bool SyntaxHighlighter::initializeTreeSitter()
{
  parser_ = ts_parser_new();
  if (!parser_)
  {
    std::cerr << "ERROR: Failed to create Tree-sitter parser" << std::endl;
    return false;
  }

  // Auto-register all languages from generated header
  registerAllLanguages(language_registry_);

  // std::cerr << "Tree-sitter initialized with " << language_registry_.size()
  //           << " language parser(s)" << std::endl;

  return true;
}

void SyntaxHighlighter::cleanupTreeSitter()
{
  // Wait for background thread
  while (is_parsing_)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::lock_guard<std::mutex> lock(tree_mutex_); // ADD LOCK

  if (current_ts_query_)
  {
    ts_query_delete(current_ts_query_);
    current_ts_query_ = nullptr;
  }

  if (tree_)
  {
    ts_tree_delete(tree_);
    tree_ = nullptr;
  }

  if (parser_)
  {
    ts_parser_delete(parser_);
    parser_ = nullptr;
  }
}

const TSLanguage *
SyntaxHighlighter::getLanguageFunction(const std::string &parser_name)
{
  auto it = language_registry_.find(parser_name);
  if (it != language_registry_.end())
  {
    return it->second(); // Call the function pointer
  }

  // Enhanced error message showing available languages
  std::cerr << "WARNING: No Tree-sitter language found for: '" << parser_name
            << "'" << std::endl;
  std::cerr << "  Available languages: ";
  bool first = true;
  for (const auto &pair : language_registry_)
  {
    if (!first)
      std::cerr << ", ";
    std::cerr << pair.first;
    first = false;
  }
  std::cerr << std::endl;

  return nullptr;
}

TSQuery *
SyntaxHighlighter::loadQueryFromFile(const std::string &query_file_path)
{
  std::ifstream file(query_file_path);
  if (!file.is_open())
  {
    std::cerr << "ERROR: Cannot open query file: " << query_file_path
              << std::endl;
    return nullptr;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string query_source = buffer.str();

  if (query_source.empty())
  {
    std::cerr << "ERROR: Query file is empty: " << query_file_path << std::endl;
    return nullptr;
  }

  // Debug: Print the query source around the error offset
  // std::cerr << "Query source length: " << query_source.length() << "
  // characters"
  //           << std::endl;

  uint32_t error_offset;
  TSQueryError error_type;
  TSQuery *query =
      ts_query_new(current_ts_language_, query_source.c_str(),
                   query_source.length(), &error_offset, &error_type);

  if (!query)
  {
    std::cerr << "ERROR: Failed to parse query file " << query_file_path
              << std::endl;
    std::cerr << "  Error offset: " << error_offset << std::endl;
    std::cerr << "  Error type: " << error_type;

    // Provide more detailed error information
    switch (error_type)
    {
    case TSQueryErrorNone:
      std::cerr << " (None)";
      break;
    case TSQueryErrorSyntax:
      std::cerr << " (Syntax Error)";
      break;
    case TSQueryErrorNodeType:
      std::cerr << " (Unknown Node Type)";
      break;
    case TSQueryErrorField:
      std::cerr << " (Unknown Field)";
      break;
    case TSQueryErrorCapture:
      std::cerr << " (Unknown Capture)";
      break;
    case TSQueryErrorStructure:
      std::cerr << " (Invalid Structure)";
      break;
    default:
      std::cerr << " (Unknown Error)";
      break;
    }
    std::cerr << std::endl;

    // Show context around error
    if (error_offset < query_source.length())
    {
      int context_start = std::max(0, (int)error_offset - 50);
      int context_end =
          std::min((int)query_source.length(), (int)error_offset + 50);

      std::cerr << "Context around error:" << std::endl;
      std::cerr << "..."
                << query_source.substr(context_start,
                                       context_end - context_start)
                << "..." << std::endl;

      // Point to error location
      std::cerr << std::string(error_offset - context_start + 3, ' ') << "^"
                << std::endl;
    }

    return nullptr;
  }

  // std::cerr << "Successfully loaded query from: " << query_file_path
  //           << std::endl;
  return query;
}

void SyntaxHighlighter::notifyEdit(size_t byte_pos, size_t old_byte_len,
                                   size_t new_byte_len, uint32_t start_row,
                                   uint32_t start_col, uint32_t old_end_row,
                                   uint32_t old_end_col, uint32_t new_end_row,
                                   uint32_t new_end_col)
{
#ifdef TREE_SITTER_ENABLED
  if (!tree_)
  {
    return;
  }

  TSInputEdit edit = {.start_byte = (uint32_t)byte_pos,
                      .old_end_byte = (uint32_t)(byte_pos + old_byte_len),
                      .new_end_byte = (uint32_t)(byte_pos + new_byte_len),
                      .start_point = {start_row, start_col},
                      .old_end_point = {old_end_row, old_end_col},
                      .new_end_point = {new_end_row, new_end_col}};

  ts_tree_edit(tree_, &edit);

  // CRITICAL FIX: Mark that we need to reparse on next access
  // This forces updateTree() to be called on next getHighlightSpans()
  // current_buffer_content_.clear();
#endif
}

void SyntaxHighlighter::invalidateLineRange(int startLine, int endLine)
{
  // OPTIMIZATION: Don't clear entire cache for small changes
  int change_size = endLine - startLine + 1;

  if (change_size <= 3)
  {
    // Small change: clear only affected lines
    for (int i = startLine; i <= endLine; ++i)
    {
      line_cache_.erase(i);
      line_states_.erase(i);
    }
    return;
  }

  if (change_size <= 50)
  {
    // Medium change: clear affected range + small buffer
    for (int i = startLine; i <= std::min(endLine + 10, endLine); ++i)
    {
      line_cache_.erase(i);
      line_states_.erase(i);
    }
    return;
  }

  // Large change: clear from startLine onwards
  auto cache_it = line_cache_.lower_bound(startLine);
  if (cache_it != line_cache_.end())
  {
    line_cache_.erase(cache_it, line_cache_.end());
  }

  auto state_it = line_states_.lower_bound(startLine);
  if (state_it != line_states_.end())
  {
    line_states_.erase(state_it, line_states_.end());
  }

  // DON'T clear current_buffer_content_ unless truly necessary
  // Incremental edits keep it up-to-date
}

void SyntaxHighlighter::updateTree(const GapBuffer &buffer)
{
#ifdef TREE_SITTER_ENABLED
  if (!parser_ || !current_ts_language_)
    return;

  std::lock_guard<std::mutex> lock(tree_mutex_);

  // Build content string (optimized)
  std::string content;
  int lineCount = buffer.getLineCount();
  std::cerr << "Built byte offsets: " << line_byte_offsets_.size() << " for "
            << lineCount << " lines\n";
  // Pre-allocate to avoid reallocations
  size_t estimated_size = 0;
  for (int i = 0; i < std::min(100, lineCount); i++)
  {
    estimated_size += buffer.getLineLength(i) + 1;
  }
  estimated_size = (estimated_size / std::min(100, lineCount)) * lineCount;
  content.reserve(estimated_size);

  // Build line offset cache while building content
  line_byte_offsets_.clear();
  line_byte_offsets_.reserve(lineCount + 1);
  line_byte_offsets_.push_back(0);

  for (int i = 0; i < lineCount; i++)
  {
    if (i > 0)
      content += "\n";
    content += buffer.getLine(i);
    line_byte_offsets_.push_back(content.length());
  }

  if (content.empty())
  {
    std::cerr << "WARNING: Attempting to parse empty buffer\n";
    return;
  }

  current_buffer_content_ = buffer.getText(); // One optimized call

  // Parse with old tree for incremental benefits (even in "full" reparse)
  TSTree *old_tree = tree_;
  tree_ = ts_parser_parse_string(parser_,
                                 old_tree, // Use old tree if available
                                 current_buffer_content_.c_str(),
                                 current_buffer_content_.length());

  if (tree_)
  {
    if (old_tree)
      ts_tree_delete(old_tree);
  }
  else
  {
    std::cerr << "ERROR: Failed to parse tree\n";
    tree_ = old_tree; // Keep old tree on failure
  }
#endif
}

void SyntaxHighlighter::markViewportLines(int startLine, int endLine) const
{
  priority_lines_.clear();
  for (int i = startLine; i <= endLine; ++i)
  {
    priority_lines_.insert(i);
  }
}

bool SyntaxHighlighter::isLineHighlighted(int lineIndex) const
{
  return line_cache_.find(lineIndex) != line_cache_.end();
}

std::vector<ColorSpan>
SyntaxHighlighter::executeTreeSitterQuery(const std::string &line,
                                          int lineNum) const
{

  if (!current_ts_query_ || !tree_)
    return {};

  std::lock_guard<std::mutex> lock(tree_mutex_);
  std::vector<ColorSpan> spans;
  TSQueryCursor *cursor = ts_query_cursor_new();
  TSNode root_node = ts_tree_root_node(tree_);

  int adjusted_line =
      is_full_parse_ ? lineNum : (lineNum - viewport_start_line_);
  if (adjusted_line < 0 ||
      adjusted_line >= ts_node_end_point(root_node).row + 1)
  {
    ts_query_cursor_delete(cursor);
    return {};
  }

  // Calculate byte range for current line
  uint32_t line_start_byte = 0;
  uint32_t line_end_byte = 0;

  std::istringstream content_stream(current_buffer_content_);
  std::string content_line;
  int current_line = 0;

  while (std::getline(content_stream, content_line) && current_line <= lineNum)
  {
    if (current_line == lineNum)
    {
      line_end_byte = line_start_byte + content_line.length();
      break;
    }
    line_start_byte += content_line.length() + 1;
    current_line++;
  }

  ts_query_cursor_set_byte_range(cursor, line_start_byte, line_end_byte);
  ts_query_cursor_exec(cursor, current_ts_query_, root_node);

  TSQueryMatch match;
  while (ts_query_cursor_next_match(cursor, &match))
  {
    for (uint32_t i = 0; i < match.capture_count; i++)
    {
      TSQueryCapture capture = match.captures[i];
      TSNode node = capture.node;

      TSPoint start_point = ts_node_start_point(node);
      TSPoint end_point = ts_node_end_point(node);

      // ORIGINAL: Only process captures starting on current line
      // Check if this capture affects the current line
      if (start_point.row <= (uint32_t)lineNum &&
          end_point.row >= (uint32_t)lineNum)
      {
        uint32_t name_length;
        const char *capture_name_ptr = ts_query_capture_name_for_id(
            current_ts_query_, capture.index, &name_length);
        std::string capture_name(capture_name_ptr, name_length);

        int start_col =
            (start_point.row == (uint32_t)lineNum) ? start_point.column : 0;
        int end_col = (end_point.row == (uint32_t)lineNum) ? end_point.column
                                                           : (int)line.length();

        start_col = std::max(0, std::min(start_col, (int)line.length()));
        end_col = std::max(start_col, std::min(end_col, (int)line.length()));

        if (start_col < end_col)
        {
          int color_pair = getColorPairForCapture(capture_name);
          spans.push_back({start_col, end_col, color_pair, 0, 100});
        }
      }
    }
  }

  ts_query_cursor_delete(cursor);
  return spans;
}

int SyntaxHighlighter::getColorPairForCapture(
    const std::string &capture_name) const
{
  static const std::unordered_map<std::string, std::string> capture_to_color = {
      // Keywords
      {"keyword", "KEYWORD"},
      {"keyword.control", "KEYWORD"},
      {"keyword.function", "KEYWORD"},
      {"keyword.operator", "KEYWORD"},
      {"keyword.return", "KEYWORD"},
      {"keyword.conditional", "KEYWORD"},
      {"keyword.repeat", "KEYWORD"},
      {"keyword.import", "KEYWORD"},
      {"keyword.exception", "KEYWORD"},

      // Types
      {"type", "TYPE"},
      {"type.builtin", "TYPE"},
      {"type.definition", "TYPE"},
      {"class", "TYPE"},
      {"interface", "TYPE"},

      // Functions
      {"function", "FUNCTION"},
      {"function.call", "FUNCTION"},
      {"function.builtin", "FUNCTION"},
      {"function.method", "FUNCTION"},
      {"method", "FUNCTION"},

      // Variables & constants

      {"variable", "VARIABLE"},
      {"variable.parameter", "VARIABLE"},
      {"variable.builtin", "CONSTANT"},
      {"variable.member", "VARIABLE"},
      {"constant", "CONSTANT"},
      {"constant.builtin", "CONSTANT"},
      {"parameter", "VARIABLE"},

      // Literals
      {"string", "STRING_LITERAL"},
      {"string_literal", "STRING_LITERAL"},
      {"number", "NUMBER"},
      {"integer", "NUMBER"},
      {"float", "NUMBER"},
      {"boolean", "CONSTANT"},

      // Comments
      {"comment", "COMMENT"},

      // Operators & punctuation
      {"operator", "OPERATOR"},
      {"punctuation", "PUNCTUATION"},
      {"punctuation.bracket", "PUNCTUATION"},
      {"punctuation.delimiter", "PUNCTUATION"},

      // Specialized
      {"namespace", "NAMESPACE"},
      {"property", "PROPERTY"},
      {"field", "PROPERTY"},
      {"attribute", "DECORATOR"},
      {"decorator", "DECORATOR"},
      {"label", "LABEL"},
      {"tag", "LABEL"},

      // Preprocessor/macro
      {"preproc", "MACRO"},
      {"preproc_include", "MACRO"},
      {"preproc_def", "MACRO"},
      {"preproc_call", "MACRO"},
      {"preproc_if", "MACRO"},
      {"preproc_ifdef", "MACRO"},
      {"preproc_ifndef", "MACRO"},
      {"preproc_else", "MACRO"},
      {"preproc_elif", "MACRO"},
      {"preproc_endif", "MACRO"},
      {"macro", "MACRO"},

      // Markup (Markdown, etc.)
      {"markup.heading", "MARKUP_HEADING"},
      {"heading", "MARKUP_HEADING"},
      {"markup.bold", "MARKUP_BOLD"},
      {"markup.italic", "MARKUP_ITALIC"},
      {"emphasis", "MARKUP_ITALIC"},
      {"markup.code", "MARKUP_CODE"},
      {"code", "MARKUP_CODE"},
      {"markup.link", "MARKUP_LINK"},
      {"link_text", "MARKUP_LINK"},
      {"markup.url", "MARKUP_URL"},
      {"link_uri", "MARKUP_URL"},
      {"markup.quote", "MARKUP_BLOCKQUOTE"},
      {"markup.list", "MARKUP_LIST"},
      {"markup.code", "MARKUP_CODE"},
      {"code_fence_content", "MARKUP_CODE_BLOCK"},
      {"code_span", "MARKUP_CODE"},

      // Markdown structure
      {"markup.list", "MARKUP_LIST"},
      {"markup.quote", "MARKUP_BLOCKQUOTE"},
  };

  auto it = capture_to_color.find(capture_name);
  if (it != capture_to_color.end())
  {
    return getColorPairValue(it->second);
  }

  // Fallback: hierarchical matching
  if (capture_name.find("keyword") != std::string::npos)
    return getColorPairValue("KEYWORD");
  if (capture_name.find("type") != std::string::npos)
    return getColorPairValue("TYPE");
  if (capture_name.find("function") != std::string::npos)
    return getColorPairValue("FUNCTION");
  if (capture_name.find("string") != std::string::npos)
    return getColorPairValue("STRING_LITERAL");
  if (capture_name.find("comment") != std::string::npos)
    return getColorPairValue("COMMENT");
  if (capture_name.find("number") != std::string::npos)
    return getColorPairValue("NUMBER");
  if (capture_name.find("constant") != std::string::npos)
    return getColorPairValue("CONSTANT");

  return 0; // Default
}
#endif

int SyntaxHighlighter::getColorPairValue(const std::string &color_name) const
{
  static const std::unordered_map<std::string, int> color_map = {
      {"COMMENT", SYNTAX_COMMENT},
      {"KEYWORD", SYNTAX_KEYWORD},
      {"STRING_LITERAL", SYNTAX_STRING},
      {"NUMBER", SYNTAX_NUMBER},
      {"FUNCTION", SYNTAX_FUNCTION},
      {"VARIABLE", SYNTAX_VARIABLE},
      {"TYPE", SYNTAX_TYPE},
      {"OPERATOR", SYNTAX_OPERATOR},
      {"PUNCTUATION", SYNTAX_PUNCTUATION},
      {"CONSTANT", SYNTAX_CONSTANT},
      {"NAMESPACE", SYNTAX_NAMESPACE},
      {"PROPERTY", SYNTAX_PROPERTY},
      {"DECORATOR", SYNTAX_DECORATOR},
      {"MACRO", SYNTAX_MACRO},
      {"LABEL", SYNTAX_LABEL},
      {"MARKUP_HEADING", MARKUP_HEADING},
      {"MARKUP_BOLD", MARKUP_BOLD},
      {"MARKUP_ITALIC", MARKUP_ITALIC},
      {"MARKUP_CODE", MARKUP_CODE},
      {"MARKUP_CODE_BLOCK", MARKUP_CODE_BLOCK},
      {"MARKUP_LINK", MARKUP_LINK},
      {"MARKUP_URL", MARKUP_URL},
      {"MARKUP_LIST", MARKUP_LIST},
      {"MARKUP_BLOCKQUOTE", MARKUP_BLOCKQUOTE},
      {"MARKUP_STRIKETHROUGH", MARKUP_STRIKETHROUGH},
      {"MARKUP_QUOTE", MARKUP_QUOTE}};

  auto it = color_map.find(color_name);
  return (it != color_map.end()) ? it->second : 0;
}

int SyntaxHighlighter::getAttributeValue(
    const std::string &attribute_name) const
{
  static const std::unordered_map<std::string, int> attribute_map = {
      {"0", 0},
      {"A_BOLD", A_BOLD},
      {"A_DIM", A_DIM},
      {"A_UNDERLINE", A_UNDERLINE},
      {"A_REVERSE", A_REVERSE}};

  auto it = attribute_map.find(attribute_name);
  return (it != attribute_map.end()) ? it->second : 0;
}

std::vector<ColorSpan>
SyntaxHighlighter::getBasicHighlightSpans(const std::string &line) const
{
  std::vector<ColorSpan> spans;

  // Very basic regex-based highlighting as fallback
  // Comments (# and //)
  size_t comment_pos = line.find('#');
  if (comment_pos == std::string::npos)
  {
    comment_pos = line.find("//");
  }
  if (comment_pos != std::string::npos)
  {
    spans.push_back({static_cast<int>(comment_pos),
                     static_cast<int>(line.length()),
                     getColorPairValue("COMMENT"), 0, 100});
  }

  // Simple string detection (basic)
  bool in_string = false;
  char string_char = 0;
  size_t string_start = 0;

  for (size_t i = 0; i < line.length(); i++)
  {
    char c = line[i];
    if (!in_string && (c == '"' || c == '\''))
    {
      in_string = true;
      string_char = c;
      string_start = i;
    }
    else if (in_string && c == string_char && (i == 0 || line[i - 1] != '\\'))
    {
      spans.push_back({static_cast<int>(string_start), static_cast<int>(i + 1),
                       getColorPairValue("STRING_LITERAL"), 0, 90});
      in_string = false;
    }
  }

  return spans;
}

void SyntaxHighlighter::loadBasicRules()
{
  // This is called as a fallback when Tree-sitter is not available
  std::cerr << "Loading basic highlighting rules (fallback mode)" << std::endl;
}

// Markdown state management (unchanged from original)
void SyntaxHighlighter::updateMarkdownState(const GapBuffer &buffer)
{
  if (currentLanguage != "Markdown")
  {
    line_states_.clear();
    return;
  }

  line_states_.clear();
  MarkdownState currentState = MarkdownState::DEFAULT;

  int lineCount = buffer.getLineCount();
  for (int i = 0; i < lineCount; ++i)
  {
    std::string line = buffer.getLine(i);
    line_states_[i] = currentState;

    if (currentState == MarkdownState::DEFAULT)
    {
      if (line.rfind("```", 0) == 0)
      {
        currentState = MarkdownState::IN_FENCED_CODE_BLOCK;
      }
      else if (line.rfind(">", 0) == 0)
      {
        line_states_[i] = MarkdownState::IN_BLOCKQUOTE;
      }
    }
    else if (currentState == MarkdownState::IN_FENCED_CODE_BLOCK)
    {
      if (line.rfind("```", 0) == 0)
      {
        currentState = MarkdownState::DEFAULT;
      }
      line_states_[i] = MarkdownState::IN_FENCED_CODE_BLOCK;
    }
  }
}

std::vector<std::string> SyntaxHighlighter::getSupportedExtensions() const
{
  return {"cpp", "h", "hpp", "c", "py", "md", "txt"};
}

void SyntaxHighlighter::debugTreeSitterState() const
{
#ifdef TREE_SITTER_ENABLED
  std::cerr << "=== Tree-sitter State Debug ===\n";
  std::cerr << "Current language: " << currentLanguage << "\n";
  std::cerr << "Parser: " << (parser_ ? "EXISTS" : "NULL") << "\n";
  std::cerr << "Tree: " << (tree_ ? "EXISTS" : "NULL") << "\n";
  std::cerr << "TS Language: " << (current_ts_language_ ? "EXISTS" : "NULL")
            << "\n";
  std::cerr << "TS Query: " << (current_ts_query_ ? "EXISTS" : "NULL") << "\n";
  std::cerr << "Buffer content length: " << current_buffer_content_.length()
            << "\n";
  std::cerr << "Line cache size: " << line_cache_.size() << "\n";

  if (tree_)
  {
    TSNode root = ts_tree_root_node(tree_);
    char *tree_str = ts_node_string(root);
    std::cerr << "Parse tree (truncated): "
              << std::string(tree_str).substr(0, 200) << "...\n";
    free(tree_str);
  }
  std::cerr << "=== End Debug ===\n";
#else
  std::cerr << "Tree-sitter not enabled\n";
#endif
}

void SyntaxHighlighter::parseViewportOnly(const GapBuffer &buffer,
                                          int targetLine)
{
#ifdef TREE_SITTER_ENABLED
  if (!parser_ || !current_ts_language_)
    return;

  int startLine = std::max(0, targetLine - 50);
  int endLine = std::min(buffer.getLineCount() - 1, targetLine + 50);

  std::string content;
  for (int i = startLine; i <= endLine; i++)
  {
    if (i > startLine)
      content += "\n";
    content += buffer.getLine(i);
  }

  if (content.empty())
    return;

  TSTree *new_tree = ts_parser_parse_string(parser_, nullptr, content.c_str(),
                                            content.length());

  if (new_tree)
  {
    std::lock_guard<std::mutex> lock(tree_mutex_); // LOCK ADDED
    if (tree_)
      ts_tree_delete(tree_);
    tree_ = new_tree;
    current_buffer_content_ = buffer.getText(); // One optimized call
    viewport_start_line_ = startLine;
    is_full_parse_ = false;
  }
#endif
}

void SyntaxHighlighter::scheduleBackgroundParse(const GapBuffer &buffer)
{
#ifdef TREE_SITTER_ENABLED
  if (is_parsing_ || !parser_ || !current_ts_language_)
    return;

  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - last_parse_time_)
                     .count();

  if (elapsed < 500)
    return;

  // Copy content BEFORE starting thread
  std::string content;
  int lineCount = buffer.getLineCount();
  content.reserve(lineCount * 80);

  for (int i = 0; i < lineCount; i++)
  {
    if (i > 0)
      content += "\n";
    content += buffer.getLine(i);
  }

  if (content.empty())
    return;

  is_parsing_ = true;
  last_parse_time_ = now;

  // NEW: Capture current version
  uint64_t expected_version = tree_version_.load();

  // Create a COPY of parser state to avoid races
  TSParser *temp_parser = ts_parser_new();
  if (!ts_parser_set_language(temp_parser, current_ts_language_))
  {
    ts_parser_delete(temp_parser);
    is_parsing_ = false;
    return;
  }

  parse_thread_ = std::thread(
      [this, content, temp_parser, expected_version]() mutable
      {
        TSTree *new_tree = ts_parser_parse_string(
            temp_parser, nullptr, content.c_str(), content.length());

        if (new_tree)
        {
          std::lock_guard<std::mutex> lock(tree_mutex_);

          // NEW: Only update if no newer edits happened
          if (tree_version_.load() == expected_version)
          {
            TSTree *old_tree = tree_;
            tree_ = new_tree;
            current_buffer_content_ = std::move(content);
            is_full_parse_ = true;

            if (old_tree)
              ts_tree_delete(old_tree);
          }
          else
          {
            // Discard stale parse - user has made newer edits
            ts_tree_delete(new_tree);
          }
        }

        ts_parser_delete(temp_parser);
        is_parsing_ = false;
        parse_complete_ = true;
      });

  parse_thread_.detach();
#endif
}

void SyntaxHighlighter::forceFullReparse(const GapBuffer &buffer)
{
#ifdef TREE_SITTER_ENABLED
  if (!parser_ || !current_ts_language_)
    return;

  std::lock_guard<std::mutex> lock(tree_mutex_);

  // Build fresh content
  std::string content;
  int lineCount = buffer.getLineCount();

  // Pre-allocate to avoid reallocations
  size_t estimated_size = lineCount * 50; // Rough estimate
  content.reserve(estimated_size);

  for (int i = 0; i < lineCount; i++)
  {
    if (i > 0)
      content += "\n";
    content += buffer.getLine(i);
  }

  if (content.empty())
  {
    std::cerr << "WARNING: Empty buffer in forceFullReparse\n";
    return;
  }

  // OPTIMIZATION: Use the old tree as a reference for faster re-parsing
  TSTree *old_tree = tree_;
  tree_ = ts_parser_parse_string(parser_, old_tree, content.c_str(),
                                 content.length());

  if (tree_)
  {
    current_buffer_content_ = std::move(content); // Move instead of copy
    is_full_parse_ = true;

    // Delete old tree AFTER successful parse
    if (old_tree)
      ts_tree_delete(old_tree);
  }
  else
  {
    std::cerr << "ERROR: Reparse failed, keeping old tree\n";
    tree_ = old_tree; // Restore old tree
    return;
  }
#endif

  // Clear cache ONLY, don't rebuild markdown state unless necessary
  line_cache_.clear();

  if (currentLanguage == "Markdown")
  {
    updateMarkdownState(buffer);
  }
}

void SyntaxHighlighter::clearAllCache()
{
  // Clear ALL cached line highlighting
  line_cache_.clear();

  // Clear line states (for Markdown)
  line_states_.clear();

  // Clear priority lines
  priority_lines_.clear();

  // CRITICAL: Force tree-sitter content to be marked as stale
  current_buffer_content_.clear();

  // Mark that we need a full reparse
  is_full_parse_ = false;
}