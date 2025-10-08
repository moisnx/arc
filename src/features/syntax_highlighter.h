// src/features/syntax_highlighter.h
#pragma once

#include "src/core/buffer.h"
#include "src/core/config_manager.h"
#include "src/features/markdown_state.h"
#include "src/ui/style_manager.h"
#include "syntax_config_loader.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef TREE_SITTER_ENABLED
#include <tree_sitter/api.h>
#endif

struct ColorSpan
{
  int start;
  int end;
  int colorPair;
  int attribute;
  int priority;
};

class SyntaxHighlighter
{
public:
  SyntaxHighlighter();
  ~SyntaxHighlighter();

  // Initialize with config directory path
  bool initialize(const std::string &config_directory = "treesitter/");
  void setSyntaxMode(SyntaxMode mode) { syntax_mode_ = mode; }
  int viewport_start_line_ = 0;
  bool is_full_parse_ = true;

  // Core functionality
  void setLanguage(const std::string &extension);
  std::vector<ColorSpan> getHighlightSpans(const std::string &line, int lineNum,
                                           const GapBuffer &buffer) const;

  // Buffer update notification for Tree-sitter
  void bufferChanged(const GapBuffer &buffer);

  // Get current language info
  std::string getCurrentLanguage() const { return currentLanguage; }
  void updateMarkdownState(const GapBuffer &buffer);
  std::vector<std::string> getSupportedExtensions() const;
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
  };
  void invalidateLineRange(int startLine, int endLine);
  void notifyEdit(size_t byte_pos, size_t old_byte_len, size_t new_byte_len,
                  uint32_t start_row, uint32_t start_col, uint32_t old_end_row,
                  uint32_t old_end_col, uint32_t new_end_row,
                  uint32_t new_end_col);
  void markViewportLines(int startLine, int endLine) const;
  bool isLineHighlighted(int lineIndex) const;
  void debugTreeSitterState() const;
  void updateTreeAfterEdit(const GapBuffer &buffer, size_t byte_pos,
                           size_t old_byte_len, size_t new_byte_len,
                           uint32_t start_row, uint32_t start_col,
                           uint32_t old_end_row, uint32_t old_end_col,
                           uint32_t new_end_row, uint32_t new_end_col);
  void parseViewportOnly(const GapBuffer &buffer, int targetLine);
  void scheduleBackgroundParse(const GapBuffer &buffer);

  void forceFullReparse(const GapBuffer &buffer);
  void invalidateFromLine(int startLine);
  void clearAllCache();

private:
  // Configuration management
  std::unique_ptr<SyntaxConfigLoader> config_loader_;
  const LanguageConfig *current_language_config_;
  std::string currentLanguage;
  SyntaxMode syntax_mode_ = SyntaxMode::VIEWPORT;
  mutable bool tree_initialized_ = false;
  bool parse_pending_ = true;
  std::thread parse_thread_;
  std::atomic<bool> is_parsing_{false};
  std::atomic<bool> parse_complete_{false};
  mutable std::mutex tree_mutex_;

  void backgroundParse(const GapBuffer &buffer);

  // Markdown state tracking
  std::map<int, MarkdownState> line_states_;
  mutable std::map<int, std::vector<ColorSpan>>
      line_cache_; // Changed from unordered_map

  mutable std::string last_buffer_hash_;
  mutable std::unordered_map<int, bool> line_highlight_pending_;
  mutable std::unordered_set<int> priority_lines_;
  std::vector<uint32_t> line_byte_offsets_; // Cached byte offsets for each line

#ifdef TREE_SITTER_ENABLED
  // Tree-sitter state
  TSParser *parser_;
  TSTree *tree_;
  const TSLanguage *current_ts_language_;
  TSQuery *current_ts_query_;
  std::string current_buffer_content_;

  // NEW: Language function registry (auto-populated from generated header)
  std::unordered_map<std::string, const TSLanguage *(*)()> language_registry_;

  void diagnoseGrammar() const;

  // Tree-sitter management methods
  bool initializeTreeSitter();
  void cleanupTreeSitter();
  const TSLanguage *getLanguageFunction(const std::string &parser_name);
  TSQuery *loadQueryFromFile(const std::string &query_file_path);
  void updateTree(const GapBuffer &buffer);
  void debugParseTree(const std::string &code) const;

  // Query execution
  std::vector<ColorSpan> executeTreeSitterQuery(const std::string &line,
                                                int lineNum) const;
  int getColorPairForCapture(const std::string &capture_name) const;
#endif

  // Color and attribute mapping
  int getColorPairValue(const std::string &color_name) const;
  int getAttributeValue(const std::string &attribute_name) const;

  // Fallback highlighting
  std::vector<ColorSpan> getBasicHighlightSpans(const std::string &line) const;
  void loadBasicRules();
};