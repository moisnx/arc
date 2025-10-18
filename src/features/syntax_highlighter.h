#pragma once

#include "color_span.h"
#include "src/core/buffer.h"
#include "src/core/config_manager.h"
#include "src/features/markdown_renderer.h"
#include "src/features/markdown_state.h"
#include "src/ui/style_manager.h"
#include "syntax_config_loader.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef TREE_SITTER_ENABLED
#include "indent_manager.h"
#include "injection_manager.h"
#include <tree_sitter/api.h>
#endif

class SyntaxHighlighter
{
public:
  SyntaxHighlighter();
  ~SyntaxHighlighter();

  bool initialize(const std::string &config_directory = "treesitter/");

  // Language and configuration management
  void setLanguage(const std::string &language_name);
  std::string getCurrentLanguage() const { return currentLanguage; }
  const LanguageConfig *getLanguageConfig(const std::string &lang_name) const;
  std::vector<std::string> getSupportedExtensions() const;

  // Highlighting queries
  std::vector<ColorSpan> getHighlightSpans(const std::string &line, int lineNum,
                                           const GapBuffer &buffer) const;

  int getColorPairValue(const std::string &color_name) const;
  int getColorPairForCapture(const std::string &capture_name) const;

  // Buffer state management
  void bufferChanged(const GapBuffer &buffer);
  void notifyEdit(size_t byte_pos, size_t old_byte_len, size_t new_byte_len,
                  uint32_t start_row, uint32_t start_col, uint32_t old_end_row,
                  uint32_t old_end_col, uint32_t new_end_row,
                  uint32_t new_end_col);

  // Cache management
  void invalidateLineCache(int lineNum);
  void invalidateLineRange(int startLine, int endLine);
  void invalidateFromLine(int startLine);
  void clearAllCache();

  void updateTree(const GapBuffer &buffer);
  void updateTreeAfterEdit(const GapBuffer &buffer, size_t byte_pos,
                           size_t old_byte_len, size_t new_byte_len,
                           uint32_t start_row, uint32_t start_col,
                           uint32_t old_end_row, uint32_t old_end_col,
                           uint32_t new_end_row, uint32_t new_end_col);

  // Parsing operations
  void updateLineHighlighting(const GapBuffer &buffer, int lineIndex);
  void scheduleIncrementalParse(const GapBuffer &buffer, int editLine);
  void scheduleBackgroundParse(const GapBuffer &buffer);
  void forceFullReparse(const GapBuffer &buffer);
  void parseViewportOnly(const GapBuffer &buffer, int targetLine);

  // Viewport tracking and optimization
  void setViewport(int top, int height)
  {
    current_viewport_top_ = top;
    current_viewport_height_ = height;
  }

  bool isLineInViewport(int line) const
  {
    return line >= current_viewport_top_ &&
           line < current_viewport_top_ + current_viewport_height_;
  }

  void markViewportLines(int startLine, int endLine) const;

  // State query methods
  bool hasValidTree() const { return tree_ != nullptr; }
  bool isReady() const
  {
    return queries_loaded_.load(std::memory_order_acquire);
  }
  bool isLineHighlighted(int lineIndex) const;
  bool needsRedraw()
  {
    return needs_redraw_.exchange(false, std::memory_order_acq_rel);
  }

  void setEditing(bool editing) { is_editing_ = editing; }
  bool isEditing() const { return is_editing_; }

  void setSyntaxMode(SyntaxMode mode) { syntax_mode_ = mode; }
  void linkIndentManager(IndentManager *mgr) { indent_mgr_ = mgr; }

  // Markdown-specific
  void updateMarkdownState(const GapBuffer &buffer);
  const MarkdownRenderer *getMarkdownRenderer() const
  {
    return markdown_renderer_.get();
  }

  // Injection management
  void reparseInjectionsIfNeeded();
  bool languageSupportsInjections() const;

#ifdef TREE_SITTER_ENABLED
  TSTree *getTree() const { return tree_; }
  const TSLanguage *getLanguageFunction(const std::string &parser_name);
#endif

private:
  // ========== Configuration and State ==========
  std::unique_ptr<SyntaxConfigLoader> config_loader_;
  const LanguageConfig *current_language_config_ = nullptr;
  std::string currentLanguage = "text";
  SyntaxMode syntax_mode_ = SyntaxMode::VIEWPORT;

  IndentManager *indent_mgr_ = nullptr;

  // ========== Caching and Line State ==========
  mutable std::map<int, std::vector<ColorSpan>> line_cache_;
  std::map<int, MarkdownState> line_states_;
  std::vector<uint32_t> line_byte_offsets_;

  mutable std::string last_buffer_hash_;
  mutable std::unordered_map<int, bool> line_highlight_pending_;
  mutable std::unordered_set<int> priority_lines_;

  // ========== Viewport and Rendering ==========
  int current_viewport_top_ = 0;
  int current_viewport_height_ = 0;
  int viewport_start_line_ = 0;
  bool is_full_parse_ = true;

  // ========== Buffer Content Management ==========
  // void updateTree(const GapBuffer &buffer);
  // void updateTreeAfterEdit(const GapBuffer &buffer, size_t byte_pos,
  //                          size_t old_byte_len, size_t new_byte_len,
  //                          uint32_t start_row, uint32_t start_col,
  //                          uint32_t old_end_row, uint32_t old_end_col,
  //                          uint32_t new_end_row, uint32_t new_end_col);

  // ========== Edit Timing and Debouncing ==========
  std::chrono::steady_clock::time_point last_parse_time_;
  std::chrono::steady_clock::time_point last_edit_time_;
  std::chrono::steady_clock::time_point last_injection_parse_;

  static constexpr int REPARSE_DELAY_MS = 50;
  static constexpr int INJECTION_REPARSE_DELAY_MS = 800;
  static constexpr int MAIN_REPARSE_DELAY_MS = 30;
  static constexpr int INJECTION_MIN_INTERVAL_MS = 1000;

  // ========== Threading and Synchronization ==========
  mutable std::mutex tree_mutex_;
  std::thread parse_thread_;

  std::atomic<bool> queries_loaded_{false};
  std::atomic<bool> is_parsing_{false};
  std::atomic<bool> parse_complete_{false};
  std::atomic<bool> parse_scheduled_{false};
  std::atomic<bool> is_editing_{false};
  std::atomic<bool> needs_redraw_{false};
  std::atomic<uint64_t> tree_version_{0};

  bool parse_pending_ = true;
  mutable bool tree_initialized_ = false;
  bool injections_need_reparse_ = false;
  void clearLineCache()
  {
    std::lock_guard<std::mutex> lock(tree_mutex_);
    line_cache_.clear();
  }

  // ========== Tree-sitter State ==========
#ifdef TREE_SITTER_ENABLED
  TSParser *parser_ = nullptr;
  TSTree *tree_ = nullptr;
  const TSLanguage *current_ts_language_ = nullptr;
  TSQuery *current_ts_query_ = nullptr;
  std::string current_buffer_content_;
  std::unordered_map<std::string, const TSLanguage *(*)()> language_registry_;

  std::unique_ptr<MarkdownRenderer> markdown_renderer_;
  std::unique_ptr<InjectionManager> injection_manager_;
  std::atomic<bool> injections_ready_{false};
#endif

  // ========== Initialization and Cleanup ==========
  bool initializeTreeSitter();
  void cleanupTreeSitter();

  // ========== Buffer Content Management ==========
  // void updateTree(const GapBuffer &buffer);
  // void updateTreeAfterEdit(const GapBuffer &buffer, size_t byte_pos,
  //                          size_t old_byte_len, size_t new_byte_len,
  //                          uint32_t start_row, uint32_t start_col,
  //                          uint32_t old_end_row, uint32_t old_end_col,
  //                          uint32_t new_end_row, uint32_t new_end_col);
  void updateBufferContentIncremental(size_t byte_pos, size_t old_byte_len,
                                      size_t new_byte_len,
                                      const GapBuffer &buffer);
  void rebuildBufferContent(const GapBuffer &buffer);

  // ========== Cache Invalidation ==========
  void invalidateAffectedLinesOnly(uint32_t start_row, uint32_t old_end_row,
                                   uint32_t new_end_row);
  void shiftLineCacheAfterEdit(int startLine, int lineDelta);
  void reparseDirtyRegions(const GapBuffer &buffer);

  // ========== Query Execution ==========
  std::vector<ColorSpan> executeTreeSitterQuery(const std::string &line,
                                                int lineNum) const;
  std::vector<ColorSpan>
  executeTreeSitterQueryForRegion(int startLine, int endLine,
                                  const GapBuffer &buffer) const;

  // ========== Highlighting and Color Mapping ==========
  std::vector<ColorSpan> getBasicHighlightSpans(const std::string &line) const;
  void loadBasicRules();
  // int getColorPairForCapture(const std::string &capture_name) const;
  // int getColorPairValue(const std::string &color_name) const;
  int getAttributeValue(const std::string &attribute_name) const;

  // ========== Background Parsing ==========
  void backgroundParse(const GapBuffer &buffer);

  // ========== File Loading ==========
#ifdef TREE_SITTER_ENABLED
  TSQuery *loadQueryFromFile(const std::string &query_file_path);
#endif

  // ========== Utility Methods ==========
  std::string computeBufferHash(const GapBuffer &buffer) const
  {
    std::string content;
    int lineCount = buffer.getLineCount();
    for (int i = 0; i < lineCount; i++)
    {
      if (i > 0)
        content += "\n";
      content += buffer.getLine(i);
    }
    return std::to_string(std::hash<std::string>{}(content));
  }

#ifdef TREE_SITTER_ENABLED
  void getExtension(const std::string &filename);
#endif
};