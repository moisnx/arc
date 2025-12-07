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
 * PHILOSOPHY: "Decorate, Don't Render" (Helix/NeoVim approach)
 * - Keep text exactly as-is in the buffer
 * - Apply styles via ANSI color codes and attributes
 * - No layout recalculation or text hiding during editing
 * - Use virtual text overlays for visual enhancements
 *
 * OPTIMIZATIONS:
 * - Viewport-based rendering (only visible lines)
 * - Incremental Tree-sitter parsing with tree.edit()
 * - Per-line caching with content hashing
 * - Cursor line always shows raw syntax
 */
class MarkdownRenderer
{
public:
  MarkdownRenderer();
  ~MarkdownRenderer();

  struct RenderInfo
  {
    std::string display_text;     // Text to display (usually unchanged)
    std::vector<ColorSpan> spans; // Style spans (color/bold/italic)
    std::vector<int>
        char_map; // Maps display pos -> buffer pos (for concealing)

    // Virtual text (overlays, doesn't modify buffer)
    std::string virtual_text_prefix; // Shown before line (e.g., language badge)
    std::string virtual_text_suffix; // Shown after line
    int virtual_prefix_color = -1;
    int virtual_suffix_color = -1;

    int left_padding = 0;   // Visual padding
    bool hide_line = false; // Hide line completely (preview mode only)
    int visual_indent = 0;  // Visual indentation
    bool is_code_block = false;
    bool is_code_block_delimiter = false;
    std::string code_language;

    size_t content_hash = 0; // For cache validation
  };

  /**
   * Render a line with viewport awareness
   * @param line_text The actual line text
   * @param line_num Line number in buffer
   * @param cursor_line Current cursor line (shows raw syntax)
   * @param buffer Reference to gap buffer
   * @param tree Tree-sitter parse tree (optional)
   * @param viewport_start First visible line
   * @param viewport_end Last visible line
   */
  RenderInfo renderLine(const std::string &line_text, int line_num,
                        int cursor_line, const GapBuffer &buffer,
                        const TSTree *tree = nullptr, int viewport_start = -1,
                        int viewport_end = -1);

  /**
   * Update state incrementally after edits
   * Only processes lines within viewport + buffer zone
   */
  void updateState(const GapBuffer &buffer, const TSTree *tree = nullptr,
                   int viewport_start = 0, int viewport_end = -1);

  /**
   * Invalidate specific lines (called on edit)
   * Marks lines as dirty for incremental reparse
   */
  void invalidateLines(int start_line, int end_line);

  /**
   * Clear all cached state (full reparse)
   */
  void clearCache();

  /**
   * Enable/disable rendering (instant toggle)
   */
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool isEnabled() const { return enabled_; }

  /**
   * Preview mode: full rendering with concealing/hiding
   * Edit mode: decoration only (default)
   */
  void setPreviewMode(bool preview)
  {
    preview_mode_ = preview;
    clearCache();
  }
  bool isPreviewMode() const { return preview_mode_; }

  /**
   * Configuration
   */
  const MarkdownRenderConfig &getConfig() const { return config_; }
  void setConfig(const MarkdownRenderConfig &config)
  {
    config_ = config;
    clearCache();
  }

  /**
   * Quick style cycling
   */
  void cycleCodeBlockStyle();

  /**
   * Query helpers for syntax injection
   */
  bool isInCodeBlock(int line_num) const;
  std::string getCodeBlockLanguage(int line_num) const;

private:
  bool enabled_ = true;
  bool preview_mode_ = false; // Toggle between edit/preview
  MarkdownRenderConfig config_;

  // Block tracking (lightweight)
  struct BlockInfo
  {
    int start_line = -1;
    int end_line = -1;
    std::string language;
    bool is_fenced = false;
  };

  std::map<int, BlockInfo> code_blocks_;
  std::map<int, int> heading_levels_;
  std::map<int, int> list_indents_;

  // Cache with content validation
  std::unordered_map<int, RenderInfo> render_cache_;
  std::unordered_set<int> dirty_lines_;
  size_t last_buffer_hash_ = 0;

  // Viewport buffer zone (parse N lines above/below visible area)
  static constexpr int VIEWPORT_BUFFER = 10;

  // === INCREMENTAL PARSING (Tree-sitter based) ===
  void parseCodeBlocksIncremental(const GapBuffer &buffer, const TSTree *tree,
                                  int start_line, int end_line);
  void parseHeadingsIncremental(const GapBuffer &buffer, int start_line,
                                int end_line);
  void parseListsIncremental(const GapBuffer &buffer, int start_line,
                             int end_line);

  // === DECORATION RENDERING (No text modification) ===
  RenderInfo decorateHeading(const std::string &line, int level,
                             bool is_cursor_line);
  RenderInfo decorateCodeBlockDelimiter(const std::string &line,
                                        const std::string &language,
                                        bool is_opening, bool is_cursor_line);
  RenderInfo decorateCodeBlockContent(const std::string &line,
                                      const std::string &language);
  RenderInfo decorateListItem(const std::string &line, int indent);
  RenderInfo decorateNormalLine(const std::string &line);

  // === PREVIEW RENDERING (With concealing) ===
  RenderInfo renderHeadingPreview(const std::string &line, int level);
  RenderInfo renderCodeBlockDelimiterPreview(const std::string &line,
                                             const std::string &language,
                                             bool is_opening);
  RenderInfo renderListItemPreview(const std::string &line, int indent);

  // === INLINE DECORATION (Applied to normal lines) ===
  void decorateInlineCode(const std::string &text,
                          std::vector<ColorSpan> &spans);
  void decorateBoldItalic(const std::string &text,
                          std::vector<ColorSpan> &spans);
  void decorateLinks(const std::string &text, std::vector<ColorSpan> &spans);

  // === UTILITIES ===
  int getHeadingLevel(const std::string &line);
  std::string getListBullet(const std::string &line, int &indent);
  std::string prettifyLanguageName(const std::string &lang);
  size_t hashLine(const std::string &line) const;
  bool isCacheValid(int line_num, const std::string &line_text) const;
  bool isInViewport(int line_num, int viewport_start, int viewport_end) const;
};