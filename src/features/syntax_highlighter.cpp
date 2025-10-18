#include "syntax_highlighter.h"
#include "src/core/config_manager.h"
#include "src/features/syntax_config_loader.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

#ifdef TREE_SITTER_ENABLED
#include "language_registry.h"
#include "query_manager.h"
#include "tree_sitter/api.h"
#endif

// ============================================================================
// Constructor and Destructor
// ============================================================================

SyntaxHighlighter::SyntaxHighlighter()
    : config_loader_(std::make_unique<SyntaxConfigLoader>()),
      current_language_config_(nullptr), currentLanguage("text")
#ifdef TREE_SITTER_ENABLED
      ,
      markdown_renderer_(std::make_unique<MarkdownRenderer>()),
      parser_(nullptr), tree_(nullptr), current_ts_language_(nullptr),
      current_ts_query_(nullptr),
      injection_manager_(std::make_unique<InjectionManager>())
#endif
{
#ifdef TREE_SITTER_ENABLED
  initializeTreeSitter();
  injection_manager_->initialize(this);
#endif
}

SyntaxHighlighter::~SyntaxHighlighter()
{
#ifdef TREE_SITTER_ENABLED
  cleanupTreeSitter();
#endif
}

// ============================================================================
// Initialization
// ============================================================================

bool SyntaxHighlighter::initialize(const std::string &config_directory)
{
  if (!config_loader_->loadAllLanguageConfigs(config_directory))
  {
    loadBasicRules();
    return false;
  }

  ConfigManager::registerReloadCallback(
      [this, config_directory]()
      {
        config_loader_->language_configs_.clear();
        config_loader_->extension_to_language_.clear();
        config_loader_->loadAllLanguageConfigs(
            ConfigManager::getSyntaxRulesDir());
        setLanguage(this->currentLanguage);
      });

  return true;
}

#ifdef TREE_SITTER_ENABLED
bool SyntaxHighlighter::initializeTreeSitter()
{
  parser_ = ts_parser_new();
  if (!parser_)
    return false;

  registerAllLanguages(language_registry_);
  return true;
}

void SyntaxHighlighter::cleanupTreeSitter()
{
  while (is_parsing_)
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

  std::lock_guard<std::mutex> lock(tree_mutex_);

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
#endif

// ============================================================================
// Language and Configuration Management
// ============================================================================

void SyntaxHighlighter::setLanguage(const std::string &language_name)
{
  const LanguageConfig *config =
      config_loader_->getLanguageConfig(language_name);

  if (!config)
  {
    loadBasicRules();
    currentLanguage = "text";
    current_language_config_ = nullptr;
#ifdef TREE_SITTER_ENABLED
    if (injection_manager_)
      injection_manager_->clearInjections();
    injections_need_reparse_ = false;
#endif
    return;
  }

  current_language_config_ = config;
  currentLanguage = language_name;

#ifdef TREE_SITTER_ENABLED
  if (injection_manager_)
    injection_manager_->clearInjections();
  injections_need_reparse_ = false;

  if (config->parser_name.empty() || !parser_)
  {
    loadBasicRules();
    return;
  }

  const TSLanguage *ts_language = getLanguageFunction(config->parser_name);
  if (!ts_language)
  {
    loadBasicRules();
    return;
  }

  if (!ts_parser_set_language(parser_, ts_language))
  {
    loadBasicRules();
    return;
  }

  current_ts_language_ = ts_language;

  if (current_ts_query_)
  {
    ts_query_delete(current_ts_query_);
    current_ts_query_ = nullptr;
  }
  queries_loaded_.store(false);

  if (config->queries.empty())
  {
    queries_loaded_.store(true);
    return;
  }

  const TSLanguage *lang_ptr = current_ts_language_;
  QueryManager::loadQueriesFromPathAsync(
      config->queries,
      [this, lang_ptr](const std::string &merged_query)
      {
        if (merged_query.empty())
        {
          queries_loaded_.store(true);
          return;
        }

        uint32_t error_offset;
        TSQueryError error_type;
        TSQuery *new_query =
            ts_query_new(lang_ptr, merged_query.c_str(), merged_query.length(),
                         &error_offset, &error_type);
        if (!new_query)
        {
          queries_loaded_.store(true);
          return;
        }

        {
          std::lock_guard<std::mutex> lock(tree_mutex_);
          if (current_ts_query_)
            ts_query_delete(current_ts_query_);
          current_ts_query_ = new_query;
        }

        queries_loaded_.store(true);
        clearLineCache();
        needs_redraw_.store(true, std::memory_order_release);
      });
#else
  loadBasicRules();
#endif
}

const LanguageConfig *
SyntaxHighlighter::getLanguageConfig(const std::string &lang_name) const
{
  if (!config_loader_)
    return nullptr;
  return config_loader_->getLanguageConfig(lang_name);
}

// ============================================================================
// Highlighting Query and Span Retrieval
// ============================================================================

std::vector<ColorSpan>
SyntaxHighlighter::getHighlightSpans(const std::string &line, int lineIndex,
                                     const GapBuffer &buffer) const
{
  auto cache_it = line_cache_.find(lineIndex);
  if (cache_it != line_cache_.end())
    return cache_it->second;

  if (!queries_loaded_.load(std::memory_order_acquire))
    return {};

  std::vector<ColorSpan> result;

#ifdef TREE_SITTER_ENABLED
  bool check_injections = languageSupportsInjections() && injection_manager_ &&
                          !injections_need_reparse_;

  if (check_injections && injection_manager_->isLineInjected(lineIndex))
  {
    result = injection_manager_->getHighlightSpansForLine(lineIndex, line);
    if (!result.empty())
    {
      line_cache_[lineIndex] = result;
      return result;
    }
  }

  if (current_ts_query_ && tree_)
  {
    try
    {
      result = executeTreeSitterQuery(line, lineIndex);
    }
    catch (const std::exception &)
    {
      result = getBasicHighlightSpans(line);
    }
  }
#endif

  if (result.empty())
    result = getBasicHighlightSpans(line);

  line_cache_[lineIndex] = result;
  return result;
}

#ifdef TREE_SITTER_ENABLED
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

std::vector<ColorSpan> SyntaxHighlighter::executeTreeSitterQueryForRegion(
    int startLine, int endLine, const GapBuffer &buffer) const
{
  // Multi-line query execution for complex constructs
  std::vector<ColorSpan> result;
  for (int line = startLine; line <= endLine; ++line)
  {
    std::vector<ColorSpan> line_spans =
        executeTreeSitterQuery(buffer.getLine(line), line);
    result.insert(result.end(), line_spans.begin(), line_spans.end());
  }
  return result;
}
#endif

// ============================================================================
// Buffer Management and Editing
// ============================================================================

void SyntaxHighlighter::bufferChanged(const GapBuffer &buffer)
{
#ifdef TREE_SITTER_ENABLED
  if (!parser_ || !current_ts_language_)
    return;

  if (!tree_ || current_buffer_content_.empty())
    updateTree(buffer);
#endif
}

void SyntaxHighlighter::notifyEdit(size_t byte_pos, size_t old_byte_len,
                                   size_t new_byte_len, uint32_t start_row,
                                   uint32_t start_col, uint32_t old_end_row,
                                   uint32_t old_end_col, uint32_t new_end_row,
                                   uint32_t new_end_col)
{
#ifdef TREE_SITTER_ENABLED
  if (!tree_)
    return;

  TSInputEdit edit = {.start_byte = (uint32_t)byte_pos,
                      .old_end_byte = (uint32_t)(byte_pos + old_byte_len),
                      .new_end_byte = (uint32_t)(byte_pos + new_byte_len),
                      .start_point = {start_row, start_col},
                      .old_end_point = {old_end_row, old_end_col},
                      .new_end_point = {new_end_row, new_end_col}};

  ts_tree_edit(tree_, &edit);
#endif
}

#ifdef TREE_SITTER_ENABLED
void SyntaxHighlighter::updateTree(const GapBuffer &buffer)
{
  if (!parser_ || !current_ts_language_)
    return;

  std::lock_guard<std::mutex> lock(tree_mutex_);

  std::string content;
  int lineCount = buffer.getLineCount();

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
    return;

  current_buffer_content_ = content;

  TSTree *old_tree = tree_;
  tree_ =
      ts_parser_parse_string(parser_, old_tree, current_buffer_content_.c_str(),
                             current_buffer_content_.length());

  if (tree_)
  {
    if (old_tree)
      ts_tree_delete(old_tree);

    if (languageSupportsInjections() && injection_manager_)
    {
      injection_manager_->parseInjections(tree_, currentLanguage,
                                          current_buffer_content_);
      injections_need_reparse_ = false;
      last_injection_parse_ = std::chrono::steady_clock::now();
    }
  }
  else
  {
    tree_ = old_tree;
  }
}

void SyntaxHighlighter::updateTreeAfterEdit(
    const GapBuffer &buffer, size_t byte_pos, size_t old_byte_len,
    size_t new_byte_len, uint32_t start_row, uint32_t start_col,
    uint32_t old_end_row, uint32_t old_end_col, uint32_t new_end_row,
    uint32_t new_end_col)
{
  if (!tree_ || !parser_ || !current_ts_language_)
    return;

  std::lock_guard<std::mutex> lock(tree_mutex_);

  TSInputEdit edit = {.start_byte = (uint32_t)byte_pos,
                      .old_end_byte = (uint32_t)(byte_pos + old_byte_len),
                      .new_end_byte = (uint32_t)(byte_pos + new_byte_len),
                      .start_point = {start_row, start_col},
                      .old_end_point = {old_end_row, old_end_col},
                      .new_end_point = {new_end_row, new_end_col}};

  ts_tree_edit(tree_, &edit);
  tree_version_++;

  bool needs_content_rebuild = current_buffer_content_.empty() ||
                               old_end_row != new_end_row ||
                               (new_byte_len > 100 || old_byte_len > 100);

  if (needs_content_rebuild)
  {
    current_buffer_content_ = buffer.getText();
  }
  else
  {
    if (old_byte_len > 0)
      current_buffer_content_.erase(byte_pos, old_byte_len);
    if (new_byte_len > 0)
    {
      if (new_byte_len == 1 && old_end_row != new_end_row)
      {
        current_buffer_content_.insert(byte_pos, "\n");
      }
      else
      {
        std::string new_text = buffer.getLine(start_row);
        size_t insert_offset = std::min(start_col, (uint32_t)new_text.length());
        size_t insert_len =
            std::min((size_t)new_byte_len, new_text.length() - insert_offset);
        current_buffer_content_.insert(byte_pos, new_text, insert_offset,
                                       insert_len);
      }
    }
  }

  TSTree *old_tree = tree_;
  tree_ =
      ts_parser_parse_string(parser_, old_tree, current_buffer_content_.c_str(),
                             current_buffer_content_.length());

  if (tree_)
  {
    if (old_tree)
      ts_tree_delete(old_tree);

    if (languageSupportsInjections())
    {
      injections_need_reparse_ = true;
      last_edit_time_ = std::chrono::steady_clock::now();
    }
  }
  else
  {
    tree_ = old_tree;
  }

  int invalidate_start = std::min((int)start_row, (int)old_end_row);
  int invalidate_end = std::max((int)new_end_row, invalidate_start + 10);

  for (int line = invalidate_start;
       line <= invalidate_end && line < buffer.getLineCount(); ++line)
  {
    line_cache_.erase(line);
  }
}
#endif

// ============================================================================
// Cache Management
// ============================================================================

void SyntaxHighlighter::invalidateLineCache(int lineNum)
{
  line_cache_.erase(lineNum);
}

void SyntaxHighlighter::invalidateLineRange(int startLine, int endLine)
{
  int change_size = endLine - startLine + 1;

  if (change_size <= 3)
  {
    for (int i = startLine; i <= endLine; ++i)
    {
      line_cache_.erase(i);
      line_states_.erase(i);
    }
    return;
  }

  if (change_size <= 50)
  {
    for (int i = startLine; i <= std::min(endLine + 10, endLine); ++i)
    {
      line_cache_.erase(i);
      line_states_.erase(i);
    }
    return;
  }

  auto cache_it = line_cache_.lower_bound(startLine);
  if (cache_it != line_cache_.end())
    line_cache_.erase(cache_it, line_cache_.end());

  auto state_it = line_states_.lower_bound(startLine);
  if (state_it != line_states_.end())
    line_states_.erase(state_it, line_states_.end());
}

void SyntaxHighlighter::invalidateFromLine(int startLine)
{
  auto it = line_cache_.lower_bound(startLine);
  if (it != line_cache_.end())
    line_cache_.erase(it, line_cache_.end());
}

void SyntaxHighlighter::clearAllCache()
{
  line_cache_.clear();
  line_states_.clear();
  priority_lines_.clear();
  current_buffer_content_.clear();
  is_full_parse_ = false;

#ifdef TREE_SITTER_ENABLED
  if (injection_manager_)
    injection_manager_->clearInjections();
  injections_need_reparse_ = false;
#endif
}

bool SyntaxHighlighter::isLineHighlighted(int lineIndex) const
{
  return line_cache_.find(lineIndex) != line_cache_.end();
}

void SyntaxHighlighter::markViewportLines(int startLine, int endLine) const
{
  priority_lines_.clear();
  for (int i = startLine; i <= endLine; ++i)
    priority_lines_.insert(i);
}

void SyntaxHighlighter::shiftLineCacheAfterEdit(int startLine, int lineDelta)
{
  if (lineDelta == 0)
    return;

  std::map<int, std::vector<ColorSpan>> new_cache;
  for (auto &[line, spans] : line_cache_)
  {
    if (line >= startLine)
      new_cache[line + lineDelta] = spans;
    else
      new_cache[line] = spans;
  }
  line_cache_ = new_cache;
}

void SyntaxHighlighter::invalidateAffectedLinesOnly(uint32_t start_row,
                                                    uint32_t old_end_row,
                                                    uint32_t new_end_row)
{
  int invalidate_start = std::min((int)start_row, (int)old_end_row);
  int invalidate_end = std::max((int)new_end_row, invalidate_start + 5);

  for (int line = invalidate_start; line <= invalidate_end; ++line)
    line_cache_.erase(line);
}

void SyntaxHighlighter::reparseDirtyRegions(const GapBuffer &buffer)
{
#ifdef TREE_SITTER_ENABLED
  if (!tree_ || !parser_)
    return;

  for (int line : priority_lines_)
  {
    if (line < buffer.getLineCount())
      updateLineHighlighting(buffer, line);
  }
#endif
}

// ============================================================================
// Parsing Operations
// ============================================================================

#ifdef TREE_SITTER_ENABLED
const TSLanguage *
SyntaxHighlighter::getLanguageFunction(const std::string &parser_name)
{
  auto it = language_registry_.find(parser_name);
  if (it != language_registry_.end())
    return it->second();

  return nullptr;
}

TSQuery *
SyntaxHighlighter::loadQueryFromFile(const std::string &query_file_path)
{
  std::ifstream file(query_file_path);
  if (!file.is_open())
    return nullptr;

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string query_source = buffer.str();

  if (query_source.empty())
    return nullptr;

  uint32_t error_offset;
  TSQueryError error_type;
  TSQuery *query =
      ts_query_new(current_ts_language_, query_source.c_str(),
                   query_source.length(), &error_offset, &error_type);

  if (!query)
  {
    if (error_offset < query_source.length())
    {
      int context_start = std::max(0, (int)error_offset - 50);
      int context_end =
          std::min((int)query_source.length(), (int)error_offset + 50);
    }
    return nullptr;
  }

  return query;
}
#endif

void SyntaxHighlighter::updateLineHighlighting(const GapBuffer &buffer,
                                               int lineIndex)
{
#ifdef TREE_SITTER_ENABLED
  if (!current_ts_query_ || !tree_)
    return;

  std::string line = buffer.getLine(lineIndex);

  try
  {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    std::vector<ColorSpan> spans = executeTreeSitterQuery(line, lineIndex);
    line_cache_[lineIndex] = spans;
  }
  catch (const std::exception &)
  {
    // Fall back to basic highlighting on error
  }
#endif
}

void SyntaxHighlighter::scheduleIncrementalParse(const GapBuffer &buffer,
                                                 int editLine)
{
#ifdef TREE_SITTER_ENABLED
  if (!parser_ || !tree_)
    return;

  last_edit_time_ = std::chrono::steady_clock::now();

  if (parse_scheduled_.exchange(true))
    return;

  if (languageSupportsInjections())
    injections_need_reparse_ = true;

  std::thread(
      [this, buffer, editLine]()
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        auto elapsed = std::chrono::steady_clock::now() - last_edit_time_;
        if (elapsed < std::chrono::milliseconds(30))
        {
          parse_scheduled_.store(false);
          return;
        }

        try
        {
          std::lock_guard<std::mutex> lock(tree_mutex_);

          if (!tree_ || !parser_)
          {
            parse_scheduled_.store(false);
            return;
          }

          std::string full_text = buffer.getText();

          TSTree *new_tree = ts_parser_parse_string(
              parser_, tree_, full_text.c_str(), full_text.length());

          if (new_tree)
          {
            ts_tree_delete(tree_);
            tree_ = new_tree;
            current_buffer_content_ = full_text;

            TSRange *ranges;
            uint32_t range_count;
            ranges = ts_tree_get_changed_ranges(tree_, tree_, &range_count);

            for (uint32_t i = 0; i < range_count; i++)
            {
              uint32_t start_line = ranges[i].start_point.row;
              uint32_t end_line = ranges[i].end_point.row;

              for (uint32_t line = start_line; line <= end_line + 2; line++)
                line_cache_.erase(line);
            }

            free(ranges);
            needs_redraw_.store(true);
          }
        }
        catch (const std::exception &)
        {
          // Parse error handled silently
        }

        parse_scheduled_.store(false);
      })
      .detach();
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

  uint64_t expected_version = tree_version_.load();

  TSParser *temp_parser = ts_parser_new();
  if (!ts_parser_set_language(temp_parser, current_ts_language_))
  {
    ts_parser_delete(temp_parser);
    is_parsing_ = false;
    return;
  }

  std::string lang = currentLanguage;
  bool supports_injections = languageSupportsInjections();
  InjectionManager *inj_mgr = injection_manager_.get();

  parse_thread_ = std::thread(
      [this, content, temp_parser, expected_version, lang, supports_injections,
       inj_mgr]() mutable
      {
        TSTree *new_tree = ts_parser_parse_string(
            temp_parser, nullptr, content.c_str(), content.length());

        if (new_tree)
        {
          std::lock_guard<std::mutex> lock(tree_mutex_);

          if (tree_version_.load() == expected_version)
          {
            TSTree *old_tree = tree_;
            tree_ = new_tree;
            current_buffer_content_ = std::move(content);
            is_full_parse_ = true;

            if (old_tree)
              ts_tree_delete(old_tree);

            if (supports_injections && inj_mgr)
            {
              inj_mgr->parseInjections(tree_, lang, current_buffer_content_);
              injections_need_reparse_ = false;
              last_injection_parse_ = std::chrono::steady_clock::now();
            }
          }
          else
          {
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

  std::string content;
  int lineCount = buffer.getLineCount();
  content.reserve(lineCount * 50);

  for (int i = 0; i < lineCount; i++)
  {
    if (i > 0)
      content += "\n";
    content += buffer.getLine(i);
  }

  if (content.empty())
    return;

  TSTree *old_tree = tree_;
  tree_ = ts_parser_parse_string(parser_, old_tree, content.c_str(),
                                 content.length());

  if (tree_)
  {
    current_buffer_content_ = std::move(content);
    is_full_parse_ = true;

    if (old_tree)
      ts_tree_delete(old_tree);

    if (languageSupportsInjections() && injection_manager_)
    {
      injection_manager_->parseInjections(tree_, currentLanguage,
                                          current_buffer_content_);
      injections_need_reparse_ = false;
      last_injection_parse_ = std::chrono::steady_clock::now();
    }
  }
  else
  {
    tree_ = old_tree;
    return;
  }
#endif

  line_cache_.clear();
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
    std::lock_guard<std::mutex> lock(tree_mutex_);
    if (tree_)
      ts_tree_delete(tree_);
    tree_ = new_tree;
    current_buffer_content_ = buffer.getText();
    viewport_start_line_ = startLine;
    is_full_parse_ = false;
  }
#endif
}

void SyntaxHighlighter::reparseInjectionsIfNeeded()
{
#ifdef TREE_SITTER_ENABLED
  if (!injections_need_reparse_ || !languageSupportsInjections())
    return;

  auto now = std::chrono::steady_clock::now();

  auto since_last_edit = std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - last_edit_time_)
                             .count();

  if (since_last_edit < 800)
    return;

  auto since_last_reparse =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now - last_injection_parse_)
          .count();

  if (since_last_reparse < 1000)
    return;

  if (injection_manager_ && tree_)
  {
    std::lock_guard<std::mutex> lock(tree_mutex_);

    std::set<int> affected = injection_manager_->getAffectedLines();
    for (int line : affected)
      line_cache_.erase(line);

    injection_manager_->parseInjections(tree_, currentLanguage,
                                        current_buffer_content_);
    injections_need_reparse_ = false;
    last_injection_parse_ = now;

    needs_redraw_.store(true);
  }
#endif
}

// ============================================================================
// Color and Highlighting Utilities
// ============================================================================

int SyntaxHighlighter::getColorPairForCapture(
    const std::string &capture_name) const
{
  static const std::unordered_map<std::string, std::string> capture_to_color = {
      {"keyword", "KEYWORD"},
      {"keyword.control", "KEYWORD"},
      {"keyword.function", "KEYWORD"},
      {"keyword.operator", "KEYWORD"},
      {"keyword.return", "KEYWORD"},
      {"keyword.conditional", "KEYWORD"},
      {"keyword.repeat", "KEYWORD"},
      {"keyword.import", "KEYWORD"},
      {"keyword.exception", "KEYWORD"},
      {"keyword.bracket", "KEYWORD"},
      {"type", "TYPE"},
      {"type.builtin", "TYPE"},
      {"type.definition", "TYPE"},
      {"class", "TYPE"},
      {"interface", "TYPE"},
      {"function", "FUNCTION"},
      {"function.call", "FUNCTION"},
      {"function.builtin", "FUNCTION"},
      {"function.method", "FUNCTION"},
      {"method", "FUNCTION"},
      {"variable", "VARIABLE"},
      {"variable.parameter", "VARIABLE"},
      {"variable.builtin", "CONSTANT"},
      {"variable.member", "VARIABLE"},
      {"constant", "CONSTANT"},
      {"constant.builtin", "CONSTANT"},
      {"parameter", "VARIABLE"},
      {"string", "STRING_LITERAL"},
      {"string_literal", "STRING_LITERAL"},
      {"number", "NUMBER"},
      {"integer", "NUMBER"},
      {"float", "NUMBER"},
      {"boolean", "CONSTANT"},
      {"comment", "COMMENT"},
      {"operator", "OPERATOR"},
      {"punctuation", "PUNCTUATION"},
      {"punctuation.bracket", "PUNCTUATION"},
      {"punctuation.delimiter", "PUNCTUATION"},
      {"namespace", "NAMESPACE"},
      {"property", "PROPERTY"},
      {"field", "PROPERTY"},
      {"attribute", "DECORATOR"},
      {"decorator", "DECORATOR"},
      {"label", "LABEL"},
      {"tag", "LABEL"},
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
      {"code_fence_content", "MARKUP_CODE_BLOCK"},
      {"code_span", "MARKUP_CODE"},
  };

  auto it = capture_to_color.find(capture_name);
  if (it != capture_to_color.end())
    return getColorPairValue(it->second);

  // Hierarchical fallback matching
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

  return 0;
}

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

  // Comment detection
  size_t comment_pos = line.find('#');
  if (comment_pos == std::string::npos)
    comment_pos = line.find("//");

  if (comment_pos != std::string::npos)
  {
    spans.push_back({static_cast<int>(comment_pos),
                     static_cast<int>(line.length()),
                     getColorPairValue("COMMENT"), 0, 100});
  }

  // String detection
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
  // Fallback mode when Tree-sitter is unavailable
}

// ============================================================================
// Markdown Support
// ============================================================================

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
        currentState = MarkdownState::IN_FENCED_CODE_BLOCK;
      else if (line.rfind(">", 0) == 0)
        line_states_[i] = MarkdownState::IN_BLOCKQUOTE;
    }
    else if (currentState == MarkdownState::IN_FENCED_CODE_BLOCK)
    {
      if (line.rfind("```", 0) == 0)
        currentState = MarkdownState::DEFAULT;
      line_states_[i] = MarkdownState::IN_FENCED_CODE_BLOCK;
    }
  }
}

// ============================================================================
// Language and Extension Support
// ============================================================================

std::vector<std::string> SyntaxHighlighter::getSupportedExtensions() const
{
  return {"cpp", "h", "hpp", "c", "py", "md", "txt"};
}

#ifdef TREE_SITTER_ENABLED
bool SyntaxHighlighter::languageSupportsInjections() const
{
  static const std::unordered_set<std::string> injection_languages = {
      "markdown", "html", "vue", "svelte", "erb"};
  return injection_languages.count(currentLanguage) > 0;
}
#else
bool SyntaxHighlighter::languageSupportsInjections() const { return false; }
#endif