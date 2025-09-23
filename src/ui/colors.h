#ifndef COLORS_H
#define COLORS_H

#ifdef _WIN32
#include <curses.h> // pdcurses on Windows
#else
#include <ncurses.h> // ncurses on Unix-like systems
#endif
#include <string>

// Color pair enums - much cleaner than magic numbers
enum ColorPair
{
  // UI Colors
  LINE_NUMBERS = 1,
  LINE_NUMBERS_DIM = 2,
  STATUS_BAR = 10,
  STATUS_BAR_TEXT = 11,
  STATUS_BAR_CYAN = 12,
  STATUS_BAR_YELLOW = 13,
  STATUS_BAR_GREEN = 14,
  STATUS_BAR_MAGENTA = 15,
  STATUS_BAR_DIM = 16,

  // Syntax Highlighting Colors
  KEYWORD = 20,
  STRING_LITERAL = 21,
  NUMBER = 22,
  COMMENT = 23,
  PYTHON_KEYWORD = 24,
  PYTHON_COMMENT = 25,
  CPP_TYPE = 26,             // C++ standard library types
  PREPROCESSOR = 27,         // Generic preprocessor directives
  PREPROCESSOR_INCLUDE = 28, // #include statements
  PREPROCESSOR_DEFINE = 29,  // #define statements

  // Operators and identifiers
  OPERATOR = 30,
  FUNCTION = 31,
  VARIABLE = 32,
  CLASS_NAME = 33,
  PYTHON_BUILTIN = 34,
  PYTHON_DECORATOR = 35,
  PYTHON_FUNCTION_DEF = 36,
  PYTHON_CLASS_DEF = 37,

  // Markdown specific colors
  MARKDOWN_HEADING = 50,       // # ## ### etc.
  MARKDOWN_BOLD = 51,          // **bold** __bold__
  MARKDOWN_ITALIC = 52,        // *italic* _italic_
  MARKDOWN_CODE = 53,          // `inline code`
  MARKDOWN_CODE_BLOCK = 54,    // ```code blocks```
  MARKDOWN_LINK = 55,          // [link text](url)
  MARKDOWN_URL = 56,           // URLs in links
  MARKDOWN_BLOCKQUOTE = 57,    // > blockquotes
  MARKDOWN_LIST = 58,          // - * + list items
  MARKDOWN_TABLE = 59,         // | table | cells |
  MARKDOWN_STRIKETHROUGH = 60, // ~~strikethrough~~

  // Future colors (reserve space)
  ACTIVE_LINE_BG = 70
};

// Theme structure for future theming support
struct Theme
{
  std::string name;

  // UI colors
  int line_numbers_fg, line_numbers_bg;
  int status_bar_fg, status_bar_bg;

  // Syntax colors
  int keyword_fg, keyword_bg;
  int string_fg, string_bg;
  int comment_fg, comment_bg;
  int number_fg, number_bg;
  int preprocessor_fg, preprocessor_bg;
  int function_fg, function_bg;
  int operator_fg, operator_bg;

  // Markdown colors
  int markdown_heading_fg, markdown_heading_bg;
  int markdown_bold_fg, markdown_bold_bg;
  int markdown_italic_fg, markdown_italic_bg;
  int markdown_code_fg, markdown_code_bg;
  int markdown_link_fg, markdown_link_bg;
};

// Function declarations
void init_colors();
void load_default_theme();
Theme get_current_theme();
void apply_theme(const Theme &theme);

#endif // COLORS_H