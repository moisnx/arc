#include "colors.h"

static Theme current_theme;

void init_colors()
{
  if (!has_colors())
  {
    return;
  }

  //   start_color();
  use_default_colors(); // Allow terminal's default colors
}

void load_default_theme()
{
  // Define the default dark theme
  current_theme = {
      "default",
      // UI colors
      COLOR_WHITE,
      COLOR_BLACK, // line numbers
      COLOR_WHITE,
      COLOR_BLUE, // status bar
      // Syntax colors
      COLOR_CYAN,
      COLOR_BLACK, // keywords
      COLOR_GREEN,
      COLOR_BLACK, // strings
      8,
      COLOR_BLACK, // comments (dark gray)
      COLOR_MAGENTA,
      COLOR_BLACK, // numbers

  };

  apply_theme(current_theme);
}

void apply_theme(const Theme &theme)
{
  // UI color pairs
  init_pair(LINE_NUMBERS, theme.line_numbers_fg, theme.line_numbers_bg);
  init_pair(LINE_NUMBERS_DIM, 8, COLOR_BLACK); // Keep dim version

  init_pair(STATUS_BAR, theme.status_bar_fg, theme.status_bar_bg);
  init_pair(STATUS_BAR_TEXT, COLOR_WHITE, COLOR_BLACK);
  init_pair(STATUS_BAR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(STATUS_BAR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(STATUS_BAR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(STATUS_BAR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(STATUS_BAR_DIM, 8, COLOR_BLACK);

  // Syntax highlighting color pairs
  init_pair(KEYWORD, theme.keyword_fg, theme.keyword_bg);
  init_pair(STRING_LITERAL, theme.string_fg, theme.string_bg);
  init_pair(NUMBER, theme.number_fg, theme.number_bg);
  init_pair(COMMENT, theme.comment_fg, theme.comment_bg);

  // ENHANCED: Python specific colors with theme support
  init_pair(PYTHON_KEYWORD, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(PYTHON_COMMENT, 8, COLOR_BLACK); // Dark gray
  init_pair(PYTHON_BUILTIN, COLOR_CYAN, COLOR_BLACK);
  init_pair(PYTHON_DECORATOR, COLOR_YELLOW, COLOR_BLACK);
  init_pair(PYTHON_FUNCTION_DEF, COLOR_BLUE, COLOR_BLACK);
  init_pair(PYTHON_CLASS_DEF, COLOR_YELLOW, COLOR_BLACK);

  current_theme = theme;
}

Theme get_current_theme() { return current_theme; }