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

  // UI Colors
  init_pair(LINE_NUMBERS, COLOR_YELLOW, -1);            // Yellow line numbers
  init_pair(LINE_NUMBERS_DIM, COLOR_BLACK, -1);         // Dim line numbers
  init_pair(STATUS_BAR, COLOR_WHITE, COLOR_BLUE);       // Status bar background
  init_pair(STATUS_BAR_TEXT, COLOR_BLACK, COLOR_WHITE); // Status bar text
  init_pair(STATUS_BAR_CYAN, COLOR_CYAN, COLOR_BLACK);
  init_pair(STATUS_BAR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
  init_pair(STATUS_BAR_GREEN, COLOR_GREEN, COLOR_BLACK);
  init_pair(STATUS_BAR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
  init_pair(STATUS_BAR_DIM, COLOR_BLACK, COLOR_WHITE);

  // Syntax Highlighting Colors
  init_pair(KEYWORD, COLOR_BLUE, -1);         // Keywords (if, for, class, etc.)
  init_pair(STRING_LITERAL, COLOR_GREEN, -1); // String literals
  init_pair(NUMBER, COLOR_CYAN, -1);          // Numbers
  init_pair(COMMENT, COLOR_BLACK, -1);        // Comments (dim/gray)

  // ENHANCED: Python specific colors with better contrast
  init_pair(PYTHON_KEYWORD, COLOR_RED,
            -1);                    // Changed from COLOR_MAGENTA to COLOR_RED
  init_pair(PYTHON_COMMENT, 8, -1); // Python comments (dark gray)
  init_pair(PYTHON_BUILTIN, COLOR_CYAN, -1); // Python built-in functions
  init_pair(PYTHON_DECORATOR, COLOR_YELLOW,
            -1); // Python decorators (@decorator)
  init_pair(PYTHON_FUNCTION_DEF, COLOR_BLUE,
            -1); // Python function definitions (def function_name)
  init_pair(PYTHON_CLASS_DEF, COLOR_YELLOW,
            -1); // Python class definitions (class ClassName)

  // C++ specific colors
  init_pair(CPP_TYPE, COLOR_YELLOW, -1);   // C++ standard library types
  init_pair(PREPROCESSOR, COLOR_CYAN, -1); // Generic preprocessor directives
  init_pair(PREPROCESSOR_INCLUDE, COLOR_GREEN,
            -1); // #include statements (bright green)
  init_pair(PREPROCESSOR_DEFINE, COLOR_MAGENTA, -1); // #define statements

  // Operators and identifiers
  init_pair(OPERATOR, COLOR_RED, -1);    // Operators (+, -, *, etc.)
  init_pair(FUNCTION, COLOR_YELLOW, -1); // Function names
  init_pair(VARIABLE, COLOR_WHITE, -1);  // Variable names
  init_pair(CLASS_NAME, COLOR_CYAN, -1); // Class names

  // Markdown specific colors - THESE WERE MISSING!
  init_pair(MARKDOWN_HEADING, COLOR_CYAN, -1);  // Headings (# ## ###)
  init_pair(MARKDOWN_BOLD, COLOR_WHITE, -1);    // Bold text
  init_pair(MARKDOWN_ITALIC, COLOR_YELLOW, -1); // Italic text
  init_pair(MARKDOWN_CODE, COLOR_GREEN,
            COLOR_BLACK); // Inline code with background
  init_pair(MARKDOWN_CODE_BLOCK, COLOR_GREEN, -1);  // Code blocks
  init_pair(MARKDOWN_LINK, COLOR_BLUE, -1);         // Link text
  init_pair(MARKDOWN_URL, COLOR_MAGENTA, -1);       // URLs
  init_pair(MARKDOWN_BLOCKQUOTE, COLOR_YELLOW, -1); // Blockquotes
  init_pair(MARKDOWN_LIST, COLOR_CYAN, -1);         // List markers
  init_pair(MARKDOWN_TABLE, COLOR_WHITE, -1);       // Table borders
  init_pair(MARKDOWN_STRIKETHROUGH, 8, -1);         // Strikethrough (dim)

  // Active line background - subtle gray background like VS Code
  init_pair(ACTIVE_LINE_BG, -1,
            COLOR_BLACK); // Default text on very dark background
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