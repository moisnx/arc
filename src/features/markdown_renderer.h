// src/features/markdown_renderer.h
#pragma once

#include "color_span.h"
#include "markdown_config.h"
#include "src/core/buffer.h"
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef TREE_SITTER_ENABLED
#include <tree_sitter/api.h>
#endif

/**
 * MarkdownRenderer - Provides rich visual rendering for Markdown
 *
 * OPTIMIZED VERSION with:
 * - Incremental updates (only reparse changed lines)
 * - Per-line caching with hash validation
 * - Integration with injection system for code blocks
 * - Configurable styling options
 */
class MarkdownRenderer
{
public:
  MarkdownRenderer();
  ~MarkdownRenderer();

  struct RenderInfo
  {
    std::string display_text;     // Transformed text to display
    std::vector<ColorSpan> spans; // Color/style spans
    std::vector<int> char_map;    // Maps display pos -> buffer pos
    int left_padding = 0;         // Extra left padding
    bool hide_line = false;       // Hide entire line (e.g., empty fence)
    int visual_indent = 0;        // Visual indentation level
    bool is_code_block = false;   // Inside fenced code block
    bool is_code_block_delimiter = false; // The ``` line itself
    std::string code_language;            // Language for syntax highlighting

    // Cache validation
    size_t content_hash = 0; // Hash of source line for cache validation
  };

  /**
   * Render a line of Markdown with rich visual enhancements
   */
  RenderInfo renderLine(const std::string &line_text, int line_num,
                        int cursor_line, const GapBuffer &buffer,
                        const TSTree *tree = nullptr);

  /**
   * Check if a line is inside a fenced code block
   */
  bool isInCodeBlock(int line_num) const;

  /**
   * Get the language of the code block at a given line
   */
  std::string getCodeBlockLanguage(int line_num) const;

  /**
   * Update internal state after buffer changes - INCREMENTAL
   */
  void updateState(const GapBuffer &buffer, const TSTree *tree = nullptr);

  /**
   * Invalidate specific lines (called on edit)
   */
  void invalidateLines(int start_line, int end_line);

  /**
   * Clear all cached state
   */
  void clearCache();

  /**
   * Enable/disable rich rendering (toggle feature)
   */
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool isEnabled() const { return enabled_; }

  /**
   * Get/set configuration
   */
  const MarkdownRenderConfig &getConfig() const { return config_; }
  void setConfig(const MarkdownRenderConfig &config)
  {
    config_ = config;
    clearCache(); // Force re-render with new style
  }

  /**
   * Cycle through code block styles (for quick toggle)
   */
  void cycleCodeBlockStyle();

private:
  bool enabled_ = true;
  MarkdownRenderConfig config_;

  // State tracking
  struct BlockInfo
  {
    int start_line = -1;
    int end_line = -1;
    std::string language;
    bool is_fenced = false;
  };

  std::map<int, BlockInfo> code_blocks_; // Line -> block info
  std::map<int, int> heading_levels_;    // Line -> heading level (1-6)
  std::map<int, int> list_indents_;      // Line -> list indent level

  // Per-line render cache with content hash
  std::unordered_map<int, RenderInfo> render_cache_;

  // Track which lines need reparsing
  std::unordered_set<int> dirty_lines_;

  // Last buffer hash to detect full changes
  size_t last_buffer_hash_ = 0;

  // Parse helpers - INCREMENTAL
  void parseCodeBlocks(const GapBuffer &buffer, const TSTree *tree);
  void parseHeadingsIncremental(const GapBuffer &buffer, int start_line,
                                int end_line);
  void parseListsIncremental(const GapBuffer &buffer, int start_line,
                             int end_line);

  // Rendering helpers
  RenderInfo renderHeading(const std::string &line, int level,
                           bool is_cursor_line);
  RenderInfo renderCodeBlockDelimiter(const std::string &line,
                                      bool is_cursor_line);
  RenderInfo renderCodeBlockContent(const std::string &line,
                                    const std::string &language);
  RenderInfo renderListItem(const std::string &line, int indent);
  RenderInfo renderNormalLine(const std::string &line);
  MarkdownRenderer::RenderInfo
  renderCodeBlockDelimiter(const std::string &line, const std::string &language,
                           bool is_opening, bool is_cursor_line);
  // Inline element processing - OPTIMIZED
  void processInlineCode(std::string &text, std::vector<ColorSpan> &spans);
  void processBoldItalic(std::string &text, std::vector<ColorSpan> &spans);
  void processLinks(std::string &text, std::vector<ColorSpan> &spans);

  // Utility
  int getHeadingLevel(const std::string &line);
  std::string getListBullet(const std::string &line, int &indent);
  std::string prettifyLanguageName(const std::string &lang);

  // Cache helpers
  size_t hashLine(const std::string &line) const;
  bool isCacheValid(int line_num, const std::string &line_text) const;
};