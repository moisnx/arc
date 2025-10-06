#pragma once

#include <map>
#include <string>

// Platform-specific ncurses includes
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

// UNIFIED ColorPairs enum - this replaces the one in colors.h
// UNIFIED ColorPairs enum - updated to match the new apply_theme()
// implementation
enum ColorPairs
{
  // Reserved pairs
  DEFAULT_PAIR = 0,
  BACKGROUND_PAIR = 1,

  // UI Elements
  LINE_NUMBERS = 2,
  LINE_NUMBERS_ACTIVE = 3,
  LINE_NUMBERS_DIM = 4,

  // Status bar
  STATUS_BAR = 5,
  STATUS_BAR_TEXT = 6,
  STATUS_BAR_ACTIVE = 7,
  STATUS_BAR_CYAN = 8,
  STATUS_BAR_YELLOW = 9,
  STATUS_BAR_GREEN = 10,
  STATUS_BAR_MAGENTA = 11,
  STATUS_BAR_DIM = 12,

  // Selection and cursor
  CURSOR = 13,
  SELECTION = 14,
  LINE_HIGHLIGHT = 15,

  // Semantic categories (20-39)
  KEYWORD = 20,
  STRING_LITERAL = 21, // Keep name for backward compat
  NUMBER = 22,
  COMMENT = 23,
  FUNCTION = 24,
  VARIABLE = 25,
  TYPE = 26,
  OPERATOR = 27,
  PUNCTUATION = 28,
  CONSTANT = 29,
  NAMESPACE = 30,
  PROPERTY = 31,
  DECORATOR = 32,
  MACRO = 33,
  LABEL = 34,

  // Markup (50-59) - Keep for Markdown
  MARKUP_HEADING = 50,
  MARKUP_BOLD = 51,
  MARKUP_ITALIC = 52,
  MARKUP_CODE = 53,
  MARKUP_CODE_BLOCK = 54,
  MARKUP_LINK = 55,
  MARKUP_URL = 56,
  MARKUP_BLOCKQUOTE = 57,
  MARKUP_LIST = 58,
  MARKUP_TABLE = 59,
  MARKUP_STRIKETHROUGH = 60,
  MARKUP_QUOTE = 61,

  // Special pairs
  ACTIVE_LINE_BG = 70
};

// ThemeColor enum - semantic color names (kept for backward compatibility with
// load_default_theme)
enum class ThemeColor
{
  TERMINAL,
  BLACK,
  DARK_GRAY,
  GRAY,
  LIGHT_GRAY,
  WHITE,
  RED,
  GREEN,
  BLUE,
  YELLOW,
  MAGENTA,
  CYAN,
  BRIGHT_RED,
  BRIGHT_GREEN,
  BRIGHT_BLUE,
  BRIGHT_YELLOW,
  BRIGHT_MAGENTA,
  BRIGHT_CYAN
};

// RGB color structure for hex parsing
struct RGB
{
  int r, g, b; // 0-255 range
  RGB() : r(0), g(0), b(0) {}
  RGB(int red, int green, int blue) : r(red), g(green), b(blue) {}
};

struct NamedTheme
{
  std::string name;
  std::string background;
  std::string foreground;
  std::string cursor;
  std::string selection;
  std::string line_highlight;
  std::string line_numbers;
  std::string line_numbers_active;
  std::string status_bar_bg;
  std::string status_bar_fg;
  std::string status_bar_active;

  // Semantic categories
  std::string keyword;
  std::string string_literal;
  std::string number;
  std::string comment;
  std::string function_name;
  std::string variable;
  std::string type;
  std::string operator_color;
  std::string punctuation;
  std::string constant;
  std::string namespace_color;
  std::string property;
  std::string decorator;
  std::string macro;
  std::string label;

  // Markup
  std::string markup_heading;
  std::string markup_bold;
  std::string markup_italic;
  std::string markup_code;
  std::string markup_code_block;
  std::string markup_link;
  std::string markup_url;
  std::string markup_list;
  std::string markup_blockquote;
  std::string markup_strikethrough;
  std::string markup_quote;
};

// Legacy Theme struct for compatibility (keep your old colors.h expectations)
struct Theme
{
  std::string name;
  int line_numbers_fg, line_numbers_bg;
  int status_bar_fg, status_bar_bg;
  int keyword_fg, keyword_bg;
  int string_fg, string_bg;
  int comment_fg, comment_bg;
  int number_fg, number_bg;
  int preprocessor_fg, preprocessor_bg;
  int function_fg, function_bg;
  int operator_fg, operator_bg;
  int markdown_heading_fg, markdown_heading_bg;
  int markdown_bold_fg, markdown_bold_bg;
  int markdown_italic_fg, markdown_italic_bg;
  int markdown_code_fg, markdown_code_bg;
  int markdown_link_fg, markdown_link_bg;
};

class StyleManager
{
public:
  StyleManager();

  // Core theme system
  void initialize();
  bool load_theme_from_file(const std::string &file_path);
  bool load_theme_from_yaml(const std::string &yaml_content);
  const NamedTheme &get_current_theme() const { return current_theme; }
  std::string get_theme_name() const { return current_theme.name; }
  bool is_initialized() const { return initialized; }

  // Helper methods (legacy - kept for compatibility)
  int theme_color_to_ncurses_color(ThemeColor color) const;
  int theme_color_to_ncurses_attr(ThemeColor color) const;

  // Terminal capability detection
  bool supports_256_colors() const;
  bool supports_true_color() const;

  // Legacy compatibility functions (for old colors.h API)
  Theme get_legacy_theme();
  void apply_legacy_theme(const Theme &theme);
  void apply_color_pair(int pair_id, ThemeColor theme_color) const;
  bool is_light_theme() const;
  void optimize_for_wsl();
  bool is_wsl_environment() const;

private:
  bool initialized;
  NamedTheme current_theme;

  // New private members for 256-color support
  bool supports_256_colors_cache;
  std::map<std::string, short>
      color_cache;            // Maps Hex code to ncurses ID (>= 16)
  short next_custom_color_id; // Starts at 16

  void load_default_theme();
  void apply_theme();

  ThemeColor string_to_theme_color(const std::string &color_name) const;

  // New core color resolution function
  short resolve_theme_color(const std::string &config_value);

  // Hex parsing utilities
  RGB parse_hex_color(const std::string &hex_str) const;
  short find_closest_8color(const RGB &rgb) const;
  short find_closest_256color(const RGB &rgb) const;

  std::string trim(const std::string &str);
  std::string remove_quotes(const std::string &str);
  std::map<std::string, std::string>
  parse_yaml(const std::string &yaml_content);
};

// Global instance
extern StyleManager g_style_manager;

// Legacy API functions for backward compatibility
void init_colors();
void load_default_theme();
void apply_theme(const Theme &theme);
Theme get_current_theme();