// style_manager.h - Refactored with semantic color system
#pragma once

#include <map>
#include <string>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

// SEMANTIC ColorPairs enum - organized by purpose, not component
enum ColorPairs
{
  // Core (0-9)
  DEFAULT_PAIR = 0,
  BACKGROUND_PAIR = 1,
  FOREGROUND_PAIR = 2,

  // State Colors (10-19) - Reusable across ALL components
  STATE_ACTIVE = 10,   // Any focused/active element
  STATE_SELECTED = 11, // Any selected item
  STATE_HOVER = 12,    // Hover/highlight state
  STATE_DISABLED = 13, // Disabled/dimmed elements

  // Semantic UI (20-29) - Reusable meaning-based colors
  UI_PRIMARY = 20,   // Primary actions/links
  UI_SECONDARY = 21, // Secondary text/metadata
  UI_ACCENT = 22,    // Special/accent items
  UI_SUCCESS = 23,   // Success/positive/executables
  UI_WARNING = 24,   // Warnings/modified
  UI_ERROR = 25,     // Errors/dangerous
  UI_INFO = 26,      // Info/directories/metadata
  UI_BORDER = 27,    // Borders/separators

  // Text Editor Specific (30-39)
  CURSOR = 30,
  LINE_NUMBERS = 31,
  LINE_NUMBERS_ACTIVE = 32,
  LINE_HIGHLIGHT = 33, // Current line background

  // Status Bar (40-49)
  STATUS_BAR = 40,
  STATUS_BAR_TEXT = 41,
  STATUS_BAR_ACTIVE = 42,
  STATUS_BAR_CYAN = 43,
  STATUS_BAR_YELLOW = 44,
  STATUS_BAR_GREEN = 45,
  STATUS_BAR_MAGENTA = 46,

  // Syntax Highlighting - Semantic (50-69)
  SYNTAX_KEYWORD = 50,
  SYNTAX_STRING = 51,
  SYNTAX_NUMBER = 52,
  SYNTAX_COMMENT = 53,
  SYNTAX_FUNCTION = 54,
  SYNTAX_VARIABLE = 55,
  SYNTAX_TYPE = 56,
  SYNTAX_OPERATOR = 57,
  SYNTAX_PUNCTUATION = 58,
  SYNTAX_CONSTANT = 59,
  SYNTAX_NAMESPACE = 60,
  SYNTAX_PROPERTY = 61,
  SYNTAX_DECORATOR = 62,
  SYNTAX_MACRO = 63,
  SYNTAX_LABEL = 64,

  // Markup/Markdown (70-79)
  MARKUP_HEADING = 70,
  MARKUP_BOLD = 71,
  MARKUP_ITALIC = 72,
  MARKUP_CODE = 73,
  MARKUP_CODE_BLOCK = 74,
  MARKUP_LINK = 75,
  MARKUP_URL = 76,
  MARKUP_BLOCKQUOTE = 77,
  MARKUP_LIST = 78,
  MARKUP_STRIKETHROUGH = 79,
  MARKUP_QUOTE = 80
};

// RGB color structure
struct RGB
{
  int r, g, b;
  RGB() : r(0), g(0), b(0) {}
  RGB(int red, int green, int blue) : r(red), g(green), b(blue) {}
};

// Semantic theme structure - organized by purpose
struct SemanticTheme
{
  std::string name;

  // Core colors
  std::string background;
  std::string foreground;

  // State colors (reusable across all UI)
  std::string state_active;
  std::string state_selected;
  std::string state_hover;
  std::string state_disabled;

  // Semantic UI colors (reusable by meaning)
  std::string ui_primary;
  std::string ui_secondary;
  std::string ui_accent;
  std::string ui_success;
  std::string ui_warning;
  std::string ui_error;
  std::string ui_info;
  std::string ui_border;

  // Editor-specific
  std::string cursor;
  std::string line_numbers;
  std::string line_numbers_active;
  std::string line_highlight;

  // Status bar
  std::string status_bar_bg;
  std::string status_bar_fg;
  std::string status_bar_active;

  // Syntax highlighting (semantic names)
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

class StyleManager
{
public:
  StyleManager();

  // Core API
  void initialize();
  bool load_theme_from_file(const std::string &file_path);
  bool load_theme_from_yaml(const std::string &yaml_content);

  // Theme access
  const SemanticTheme &get_current_theme() const { return current_theme; }
  std::string get_theme_name() const { return current_theme.name; }
  bool is_initialized() const { return initialized; }

  // Semantic color getters - these return COLOR_PAIR() ready values
  int get_state_active() const { return COLOR_PAIR(STATE_ACTIVE); }
  int get_state_selected() const { return COLOR_PAIR(STATE_SELECTED); }
  int get_state_hover() const { return COLOR_PAIR(STATE_HOVER); }
  int get_state_disabled() const { return COLOR_PAIR(STATE_DISABLED); }

  int get_ui_primary() const { return COLOR_PAIR(UI_PRIMARY); }
  int get_ui_secondary() const { return COLOR_PAIR(UI_SECONDARY); }
  int get_ui_accent() const { return COLOR_PAIR(UI_ACCENT); }
  int get_ui_success() const { return COLOR_PAIR(UI_SUCCESS); }
  int get_ui_warning() const { return COLOR_PAIR(UI_WARNING); }
  int get_ui_error() const { return COLOR_PAIR(UI_ERROR); }
  int get_ui_info() const { return COLOR_PAIR(UI_INFO); }
  int get_ui_border() const { return COLOR_PAIR(UI_BORDER); }

  // Terminal capabilities
  bool supports_256_colors() const;
  bool supports_true_color() const;
  bool is_light_theme() const;

private:
  bool initialized;
  SemanticTheme current_theme;
  bool supports_256_colors_cache;
  std::map<std::string, short> color_cache;
  short next_custom_color_id;

  void load_default_theme();
  void apply_theme();
  short resolve_theme_color(const std::string &config_value);
  RGB parse_hex_color(const std::string &hex_str) const;
  short find_closest_8color(const RGB &rgb) const;

  std::string trim(const std::string &str);
  std::string remove_quotes(const std::string &str);
  std::map<std::string, std::string>
  parse_yaml(const std::string &yaml_content);
};

extern StyleManager g_style_manager;