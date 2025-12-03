// src/features/markdown_config.h
#pragma once

#include <string>

/**
 * Configuration for Markdown rendering styles
 */
enum class CodeBlockStyle
{
  CLEAN,      // Clean minimal style with subtle language badge (NEW DEFAULT)
  INLINE_TAG, // Language tag inline with top border
  CLASSIC,    // Traditional separator style
  MINIMAL,    // Just subtle dots
  HIDDEN      // Completely hide delimiter lines
};

enum class CodeBlockIndent
{
  NONE,    // No indent
  MINIMAL, // 2 spaces
  GUTTER,  // Vertical bar on left with padding
  STANDARD // 4 spaces
};

enum class HeadingStyle
{
  MODERN,    // Clean bold with subtle underlines (NEW DEFAULT)
  CLASSIC,   // Traditional # markers visible
  MINIMAL,   // Just bold, no markers or underlines
  UNDERLINED // Heavy underlines for all headings
};

struct MarkdownRenderConfig
{
  // Code block styling
  CodeBlockStyle block_style = CodeBlockStyle::INLINE_TAG;
  CodeBlockIndent block_indent = CodeBlockIndent::MINIMAL;
  bool show_language_badge = true; // Show language name badge
  bool dim_code_delimiters = true; // Dim the fence markers
  bool add_block_padding = true;   // Add visual padding around blocks

  // Heading styling
  HeadingStyle heading_style = HeadingStyle::MODERN;
  bool bold_headings = true;           // Make headings bold
  bool underline_h1_h2 = true;         // Underline only H1 and H2
  bool color_headings_by_level = true; // Different colors per level

  // List styling
  bool replace_list_markers = true; // Replace - * + with •
  bool indent_nested_lists = true;  // Visual indent for nested lists
  bool align_list_content = true;   // Align multi-line list content

  // Inline styling
  bool hide_emphasis_markers = true; // Hide ** __ * _
  bool hide_code_backticks = true;   // Hide `backticks`
  bool style_inline_code = true;     // Add subtle background to inline code
  bool underline_links = true;       // Underline link text
  bool dim_link_urls = true;         // Dim URLs in [text](url)

  // Visual enhancements
  bool add_list_bullets_color = true; // Color bullets differently
  bool highlight_active_line = false; // Different style when cursor on line
  bool fade_markers = true; // Fade emphasis/link markers instead of hiding

  // Performance
  bool enable_caching = true;     // Cache rendered lines
  bool incremental_update = true; // Only reparse changed lines

  // Load from config file
  static MarkdownRenderConfig load();

  // Save to config file
  void save() const;

  // Get display name for styles
  static std::string getStyleName(CodeBlockStyle style);
  static std::string getStyleName(HeadingStyle style);
};

// Implementation
inline std::string MarkdownRenderConfig::getStyleName(CodeBlockStyle style)
{
  switch (style)
  {
  case CodeBlockStyle::CLEAN:
    return "clean";
  case CodeBlockStyle::INLINE_TAG:
    return "inline-tag";
  case CodeBlockStyle::CLASSIC:
    return "classic";
  case CodeBlockStyle::MINIMAL:
    return "minimal";
  case CodeBlockStyle::HIDDEN:
    return "hidden";
  default:
    return "clean";
  }
}

inline std::string MarkdownRenderConfig::getStyleName(HeadingStyle style)
{
  switch (style)
  {
  case HeadingStyle::MODERN:
    return "modern";
  case HeadingStyle::CLASSIC:
    return "classic";
  case HeadingStyle::MINIMAL:
    return "minimal";
  case HeadingStyle::UNDERLINED:
    return "underlined";
  default:
    return "modern";
  }
}

inline MarkdownRenderConfig MarkdownRenderConfig::load()
{
  // TODO: Load from ~/.config/arc/markdown.yaml
  // For now, return defaults
  return MarkdownRenderConfig();
}

inline void MarkdownRenderConfig::save() const
{
  // TODO: Save to ~/.config/arc/markdown.yaml
}