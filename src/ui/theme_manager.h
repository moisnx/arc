#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

// Platform-specific ncurses includes
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

// UNIFIED ColorPairs enum - this replaces the one in colors.h
enum ColorPairs
{
  LINE_NUMBERS = 1,
  LINE_NUMBERS_ACTIVE = 2,
  LINE_NUMBERS_DIM = 3,
  STATUS_BAR = 4,
  STATUS_BAR_TEXT = 5,
  STATUS_BAR_ACTIVE = 6,
  STATUS_BAR_CYAN = 7,
  STATUS_BAR_YELLOW = 8,
  STATUS_BAR_GREEN = 9,
  STATUS_BAR_MAGENTA = 10,
  STATUS_BAR_DIM = 11,
  CURSOR = 12,
  SELECTION = 13,
  LINE_HIGHLIGHT = 14,
  KEYWORD = 20,
  STRING_LITERAL = 21,
  NUMBER = 22,
  COMMENT = 23,
  FUNCTION = 24,
  VARIABLE = 25,
  TYPE = 26,
  OPERATOR = 27,
  PREPROCESSOR = 28,
  PYTHON_KEYWORD = 30,
  PYTHON_COMMENT = 31,
  PYTHON_BUILTIN = 32,
  PYTHON_DECORATOR = 33,
  PYTHON_FUNCTION_DEF = 34,
  PYTHON_CLASS_DEF = 35,
  CPP_TYPE = 40,
  CPP_NAMESPACE = 41,
  PREPROCESSOR_INCLUDE = 42,
  PREPROCESSOR_DEFINE = 43,
  CLASS_NAME = 44,
  MARKDOWN_HEADING = 50,
  MARKDOWN_BOLD = 51,
  MARKDOWN_ITALIC = 52,
  MARKDOWN_CODE = 53,
  MARKDOWN_CODE_BLOCK = 54,
  MARKDOWN_LINK = 55,
  MARKDOWN_URL = 56,
  MARKDOWN_BLOCKQUOTE = 57,
  MARKDOWN_LIST = 58,
  MARKDOWN_TABLE = 59,
  MARKDOWN_STRIKETHROUGH = 60,
  MARKDOWN_QUOTE = 61,
  ACTIVE_LINE_BG = 70
};

// ThemeColor enum - semantic color names
enum class ThemeColor
{
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

struct NamedTheme
{
  std::string name;
  ThemeColor background;
  ThemeColor foreground;
  ThemeColor cursor;
  ThemeColor selection;
  ThemeColor line_highlight;
  ThemeColor line_numbers;
  ThemeColor line_numbers_active;
  ThemeColor status_bar_bg;
  ThemeColor status_bar_fg;
  ThemeColor status_bar_active;
  ThemeColor keyword;
  ThemeColor string_literal;
  ThemeColor number;
  ThemeColor comment;
  ThemeColor function_name;
  ThemeColor variable;
  ThemeColor type;
  ThemeColor operator_color;
  ThemeColor preprocessor;
  ThemeColor python_decorator;
  ThemeColor python_builtin;
  ThemeColor cpp_namespace;
  ThemeColor markdown_heading;
  ThemeColor markdown_bold;
  ThemeColor markdown_italic;
  ThemeColor markdown_code;
  ThemeColor markdown_link;
  ThemeColor markdown_quote;
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

class ThemeManager
{
public:
  ThemeManager();

  // Core theme system
  void initialize();
  bool load_theme_from_file(const std::string &file_path);
  bool load_theme_from_yaml(const std::string &yaml_content);
  const NamedTheme &get_current_theme() const { return current_theme; }
  std::string get_theme_name() const { return current_theme.name; }
  bool is_initialized() const { return initialized; }

  // Helper methods
  int theme_color_to_ncurses_color(ThemeColor color) const;
  int theme_color_to_ncurses_attr(ThemeColor color) const;

  // Terminal capability detection
  bool supports_256_colors() const;
  bool supports_true_color() const;

  // Legacy compatibility functions (for old colors.h API)
  Theme get_legacy_theme() const;
  void apply_legacy_theme(const Theme &theme);

private:
  bool initialized;
  NamedTheme current_theme;

  void load_default_theme();
  void apply_theme();
  ThemeColor string_to_theme_color(const std::string &color_name);
  bool is_light_theme() const;
  std::string trim(const std::string &str);
  std::string remove_quotes(const std::string &str);
  std::map<std::string, std::string>
  parse_yaml(const std::string &yaml_content);
};

// Global instance
extern ThemeManager g_theme_manager;

// Legacy API functions for backward compatibility
void init_colors();
void load_default_theme();
void apply_theme(const Theme &theme);
Theme get_current_theme();