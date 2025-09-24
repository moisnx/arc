#pragma once
#include <map>
#include <string>
#include <vector>


struct RGB
{
  short r, g, b;
  RGB(short red = 0, short green = 0, short blue = 0)
      : r(red), g(green), b(blue)
  {
  }
};

struct EditorTheme
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
  std::string keyword;
  std::string string_literal;
  std::string number;
  std::string comment;
  std::string function_name;
  std::string variable;
  std::string type;
  std::string operator_color;
  std::string preprocessor;
  std::string python_decorator;
  std::string python_builtin;
  std::string cpp_namespace;
  std::string markdown_heading;
  std::string markdown_bold;
  std::string markdown_italic;
  std::string markdown_code;
  std::string markdown_link;
  std::string markdown_quote;
};

struct ProcessedTheme
{
  std::string name;
  int background_idx;
  int foreground_idx;
  int cursor_idx;
  int selection_idx;
  int line_highlight_idx;
  int line_numbers_idx;
  int line_numbers_active_idx;
  int status_bar_bg_idx;
  int status_bar_fg_idx;
  int status_bar_active_idx;
  int keyword_idx;
  int string_literal_idx;
  int number_idx;
  int comment_idx;
  int function_name_idx;
  int variable_idx;
  int type_idx;
  int operator_color_idx;
  int preprocessor_idx;
  int python_decorator_idx;
  int python_builtin_idx;
  int cpp_namespace_idx;
  int markdown_heading_idx;
  int markdown_bold_idx;
  int markdown_italic_idx;
  int markdown_code_idx;
  int markdown_link_idx;
  int markdown_quote_idx;
};

enum ColorPair
{
  LINE_NUMBERS_ACTIVE = 1,
  LINE_NUMBERS = 2,
  STATUS_BAR = 3,
  STATUS_BAR_TEXT = 4,
  STATUS_BAR_ACTIVE = 5,
  CURSOR = 6,
  SELECTION = 7,
  LINE_HIGHLIGHT = 8,
  KEYWORD = 9,
  STRING_LITERAL = 10,
  NUMBER = 11,
  COMMENT = 12,
  FUNCTION = 13,
  VARIABLE = 14,
  TYPE = 15,
  OPERATOR = 16,
  PREPROCESSOR = 17,
  PYTHON_DECORATOR = 18,
  PYTHON_BUILTIN = 19,
  CPP_NAMESPACE = 20,
  MARKDOWN_HEADING = 21,
  MARKDOWN_BOLD = 22,
  MARKDOWN_ITALIC = 23,
  MARKDOWN_CODE = 24,
  MARKDOWN_LINK = 25,
  MARKDOWN_QUOTE = 26,
  LINE_NUMBERS_DIM = 27,
  STATUS_BAR_CYAN = 28,
  STATUS_BAR_YELLOW = 29,
  STATUS_BAR_GREEN = 30,
  STATUS_BAR_MAGENTA = 31,
  STATUS_BAR_DIM = 32,
  ACTIVE_LINE_BG = 33,
  CPP_TYPE = 34,
  PREPROCESSOR_INCLUDE = 35,
  PREPROCESSOR_DEFINE = 36,
  CLASS_NAME = 37,
  PYTHON_KEYWORD = 38,
  PYTHON_FUNCTION_DEF = 39,
  PYTHON_CLASS_DEF = 40,
  MARKDOWN_CODE_BLOCK = 41,
  MARKDOWN_BLOCKQUOTE = 42,
  MARKDOWN_LIST = 43,
  MARKDOWN_TABLE = 44,
  MARKDOWN_STRIKETHROUGH = 45,
  MARKDOWN_URL = 46
};

class ThemeManager
{
public:
  ThemeManager();
  void initialize();
  void apply_theme();
  bool load_theme_from_file(const std::string &file_path);
  bool load_theme_from_yaml(const std::string &yaml_content);
  void load_default_theme();
  ProcessedTheme process_raw_theme(const EditorTheme &raw_theme);
  RGB hex_to_rgb(const std::string &hex);
  int find_closest_color(const RGB &target);
  double color_distance(const RGB &c1, const RGB &c2);
  void init_terminal_palette();

  // Public accessor for terminal_palette
  const std::vector<RGB> &get_terminal_palette() const
  {
    return terminal_palette;
  }

private:
  bool initialized = false;
  ProcessedTheme current_theme;
  std::vector<RGB> terminal_palette;

  // Manual YAML parsing helper functions
  std::string trim(const std::string &str);
  std::string remove_quotes(const std::string &str);
  std::map<std::string, std::string>
  parse_yaml(const std::string &yaml_content);
};

extern ThemeManager g_theme_manager;