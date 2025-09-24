#ifndef COLORS_H
#define COLORS_H

#include <string>
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

// UNIFIED enum - replace your existing ColorPair enum with this
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

// Keep your Theme struct for compatibility
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

void init_colors();
void load_default_theme();
Theme get_current_theme();
void apply_theme(const Theme &theme);

#endif