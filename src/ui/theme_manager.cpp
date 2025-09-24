#include "theme_manager.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

ThemeManager::ThemeManager() : initialized(false) {}

void ThemeManager::initialize()
{
  if (initialized)
  {
    std::cerr << "ThemeManager already initialized" << std::endl;
    return;
  }

  if (!has_colors())
  {
    std::cerr << "Terminal does not support colors" << std::endl;
    return;
  }

  start_color();
  use_default_colors();

  std::cerr << "=== UNIFIED THEME SYSTEM ===" << std::endl;
  std::cerr << "COLORS: " << COLORS << std::endl;
  std::cerr << "COLOR_PAIRS: " << COLOR_PAIRS << std::endl;
  std::cerr << "TERM: " << (getenv("TERM") ? getenv("TERM") : "not set")
            << std::endl;

  load_default_theme();
  apply_theme();

  initialized = true;
  std::cerr << "Unified theme system initialized successfully" << std::endl;
}

ThemeColor ThemeManager::string_to_theme_color(const std::string &color_name)
{
  std::string lower_name = color_name;
  std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
                 ::tolower);

  if (lower_name == "black")
    return ThemeColor::BLACK;
  if (lower_name == "dark_gray" || lower_name == "dark_grey")
    return ThemeColor::DARK_GRAY;
  if (lower_name == "gray" || lower_name == "grey")
    return ThemeColor::GRAY;
  if (lower_name == "light_gray" || lower_name == "light_grey")
    return ThemeColor::LIGHT_GRAY;
  if (lower_name == "white")
    return ThemeColor::WHITE;
  if (lower_name == "red")
    return ThemeColor::RED;
  if (lower_name == "green")
    return ThemeColor::GREEN;
  if (lower_name == "blue")
    return ThemeColor::BLUE;
  if (lower_name == "yellow")
    return ThemeColor::YELLOW;
  if (lower_name == "magenta")
    return ThemeColor::MAGENTA;
  if (lower_name == "cyan")
    return ThemeColor::CYAN;
  if (lower_name == "bright_red")
    return ThemeColor::BRIGHT_RED;
  if (lower_name == "bright_green")
    return ThemeColor::BRIGHT_GREEN;
  if (lower_name == "bright_blue")
    return ThemeColor::BRIGHT_BLUE;
  if (lower_name == "bright_yellow")
    return ThemeColor::BRIGHT_YELLOW;
  if (lower_name == "bright_magenta")
    return ThemeColor::BRIGHT_MAGENTA;
  if (lower_name == "bright_cyan")
    return ThemeColor::BRIGHT_CYAN;

  std::cerr << "Unknown color name: " << color_name << ", using white"
            << std::endl;
  return ThemeColor::WHITE;
}

int ThemeManager::theme_color_to_ncurses_color(ThemeColor color) const
{
  switch (color)
  {
  case ThemeColor::BLACK:
    return COLOR_BLACK;
  case ThemeColor::DARK_GRAY:
    return COLOR_BLACK;
  case ThemeColor::GRAY:
    return COLOR_WHITE;
  case ThemeColor::LIGHT_GRAY:
    return COLOR_WHITE;
  case ThemeColor::WHITE:
    return COLOR_WHITE;
  case ThemeColor::RED:
  case ThemeColor::BRIGHT_RED:
    return COLOR_RED;
  case ThemeColor::GREEN:
  case ThemeColor::BRIGHT_GREEN:
    return COLOR_GREEN;
  case ThemeColor::BLUE:
  case ThemeColor::BRIGHT_BLUE:
    return COLOR_BLUE;
  case ThemeColor::YELLOW:
  case ThemeColor::BRIGHT_YELLOW:
    return COLOR_YELLOW;
  case ThemeColor::MAGENTA:
  case ThemeColor::BRIGHT_MAGENTA:
    return COLOR_MAGENTA;
  case ThemeColor::CYAN:
  case ThemeColor::BRIGHT_CYAN:
    return COLOR_CYAN;
  default:
    return COLOR_WHITE;
  }
}

int ThemeManager::theme_color_to_ncurses_attr(ThemeColor color) const
{
  switch (color)
  {
  case ThemeColor::DARK_GRAY:
    return A_BOLD;
  case ThemeColor::GRAY:
    return A_DIM;
  case ThemeColor::WHITE:
    return A_BOLD;
  case ThemeColor::BRIGHT_RED:
  case ThemeColor::BRIGHT_GREEN:
  case ThemeColor::BRIGHT_BLUE:
  case ThemeColor::BRIGHT_YELLOW:
  case ThemeColor::BRIGHT_MAGENTA:
  case ThemeColor::BRIGHT_CYAN:
    return A_BOLD;
  default:
    return 0;
  }
}

bool ThemeManager::is_light_theme() const
{
  return (current_theme.background == ThemeColor::WHITE ||
          current_theme.background == ThemeColor::LIGHT_GRAY);
}

void ThemeManager::load_default_theme()
{
  current_theme = {
      "Default Dark",
      ThemeColor::BLACK,         // background
      ThemeColor::WHITE,         // foreground
      ThemeColor::WHITE,         // cursor
      ThemeColor::BLUE,          // selection
      ThemeColor::DARK_GRAY,     // line_highlight
      ThemeColor::YELLOW,        // line_numbers
      ThemeColor::BRIGHT_YELLOW, // line_numbers_active
      ThemeColor::BLUE,          // status_bar_bg
      ThemeColor::WHITE,         // status_bar_fg
      ThemeColor::CYAN,          // status_bar_active
      ThemeColor::BLUE,          // keyword
      ThemeColor::GREEN,         // string_literal
      ThemeColor::CYAN,          // number
      ThemeColor::GRAY,          // comment
      ThemeColor::YELLOW,        // function_name
      ThemeColor::WHITE,         // variable
      ThemeColor::YELLOW,        // type
      ThemeColor::RED,           // operator
      ThemeColor::CYAN,          // preprocessor
      ThemeColor::YELLOW,        // python_decorator
      ThemeColor::CYAN,          // python_builtin
      ThemeColor::CYAN,          // cpp_namespace
      ThemeColor::CYAN,          // markdown_heading
      ThemeColor::WHITE,         // markdown_bold
      ThemeColor::YELLOW,        // markdown_italic
      ThemeColor::GREEN,         // markdown_code
      ThemeColor::BLUE,          // markdown_link
      ThemeColor::YELLOW         // markdown_quote
  };
}

void ThemeManager::apply_theme()
{
  if (!initialized)
  {
    std::cerr << "ThemeManager not initialized, cannot apply theme"
              << std::endl;
    return;
  }

  // Set default terminal colors based on theme background
  int default_bg = theme_color_to_ncurses_color(current_theme.background);
  int default_fg = theme_color_to_ncurses_color(current_theme.foreground);

  // For ncurses, we need to handle this carefully
  if (is_light_theme())
  {
    assume_default_colors(COLOR_BLACK, COLOR_WHITE);
  }
  else
  {
    assume_default_colors(COLOR_WHITE, COLOR_BLACK);
  }

  // Helper to initialize color pairs safely
  auto init_pair_safe = [&](int pair_id, ThemeColor fg_theme_color,
                            ThemeColor bg_theme_color = ThemeColor::BLACK)
  {
    int fg_color = theme_color_to_ncurses_color(fg_theme_color);
    int bg_color;

    // Handle background colors properly for light/dark themes
    if (bg_theme_color == ThemeColor::BLACK)
    {
      // Use default background (-1) for most elements
      bg_color = -1;
    }
    else
    {
      // Specific background color requested (like selection, status bar, etc.)
      bg_color = theme_color_to_ncurses_color(bg_theme_color);
    }

    int result = init_pair(pair_id, fg_color, bg_color);
    if (result == OK)
    {
      std::cerr << "✓ Pair " << pair_id << ": fg=" << fg_color
                << " bg=" << bg_color << std::endl;
    }
    else
    {
      std::cerr << "✗ Failed pair " << pair_id << std::endl;
    }
    return result == OK;
  };

  // Apply ALL color pairs used throughout your editor

  // UI Elements - use proper background colors
  init_pair_safe(LINE_NUMBERS, current_theme.line_numbers);
  init_pair_safe(LINE_NUMBERS_ACTIVE, current_theme.line_numbers_active);
  init_pair_safe(LINE_NUMBERS_DIM, ThemeColor::GRAY);

  init_pair_safe(STATUS_BAR, current_theme.status_bar_fg,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_TEXT, current_theme.status_bar_fg,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_ACTIVE, current_theme.status_bar_active,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_CYAN, ThemeColor::CYAN,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_YELLOW, ThemeColor::YELLOW,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_GREEN, ThemeColor::GREEN,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_MAGENTA, ThemeColor::MAGENTA,
                 current_theme.status_bar_bg);
  init_pair_safe(STATUS_BAR_DIM, ThemeColor::GRAY, current_theme.status_bar_bg);

  init_pair_safe(CURSOR, current_theme.cursor);
  init_pair_safe(SELECTION, current_theme.foreground, current_theme.selection);
  init_pair_safe(LINE_HIGHLIGHT, current_theme.foreground,
                 current_theme.line_highlight);

  // Basic Syntax Highlighting - these use default background
  init_pair_safe(KEYWORD, current_theme.keyword);
  init_pair_safe(STRING_LITERAL, current_theme.string_literal);
  init_pair_safe(NUMBER, current_theme.number);
  init_pair_safe(COMMENT, current_theme.comment);
  init_pair_safe(FUNCTION, current_theme.function_name);
  init_pair_safe(VARIABLE, current_theme.variable);
  init_pair_safe(TYPE, current_theme.type);
  init_pair_safe(OPERATOR, current_theme.operator_color);
  init_pair_safe(PREPROCESSOR, current_theme.preprocessor);

  // Python-specific
  init_pair_safe(PYTHON_KEYWORD, current_theme.keyword);
  init_pair_safe(PYTHON_COMMENT, current_theme.comment);
  init_pair_safe(PYTHON_BUILTIN, current_theme.python_builtin);
  init_pair_safe(PYTHON_DECORATOR, current_theme.python_decorator);
  init_pair_safe(PYTHON_FUNCTION_DEF, current_theme.function_name);
  init_pair_safe(PYTHON_CLASS_DEF, current_theme.type);

  // C++ specific
  init_pair_safe(CPP_TYPE, current_theme.type);
  init_pair_safe(CPP_NAMESPACE, current_theme.cpp_namespace);
  init_pair_safe(PREPROCESSOR_INCLUDE, current_theme.preprocessor);
  init_pair_safe(PREPROCESSOR_DEFINE, current_theme.preprocessor);
  init_pair_safe(CLASS_NAME, current_theme.type);

  // Markdown
  init_pair_safe(MARKDOWN_HEADING, current_theme.markdown_heading);
  init_pair_safe(MARKDOWN_BOLD, current_theme.markdown_bold);
  init_pair_safe(MARKDOWN_ITALIC, current_theme.markdown_italic);
  init_pair_safe(MARKDOWN_CODE, current_theme.markdown_code);
  init_pair_safe(MARKDOWN_CODE_BLOCK, current_theme.markdown_code);
  init_pair_safe(MARKDOWN_LINK, current_theme.markdown_link);
  init_pair_safe(MARKDOWN_URL, current_theme.markdown_link);
  init_pair_safe(MARKDOWN_BLOCKQUOTE, current_theme.markdown_quote);
  init_pair_safe(MARKDOWN_LIST, current_theme.keyword);
  init_pair_safe(MARKDOWN_TABLE, current_theme.operator_color);
  init_pair_safe(MARKDOWN_STRIKETHROUGH, current_theme.comment);
  init_pair_safe(MARKDOWN_QUOTE, current_theme.markdown_quote);

  // Special
  init_pair_safe(ACTIVE_LINE_BG, current_theme.foreground,
                 current_theme.line_highlight);

  std::cerr << "Theme '" << current_theme.name << "' applied successfully!"
            << std::endl;
}

// Legacy compatibility functions
Theme ThemeManager::get_legacy_theme() const
{
  Theme legacy;
  legacy.name = current_theme.name;
  legacy.line_numbers_fg =
      theme_color_to_ncurses_color(current_theme.line_numbers);
  legacy.line_numbers_bg = COLOR_BLACK;
  legacy.status_bar_fg =
      theme_color_to_ncurses_color(current_theme.status_bar_fg);
  legacy.status_bar_bg =
      theme_color_to_ncurses_color(current_theme.status_bar_bg);
  legacy.keyword_fg = theme_color_to_ncurses_color(current_theme.keyword);
  legacy.keyword_bg = COLOR_BLACK;
  legacy.string_fg = theme_color_to_ncurses_color(current_theme.string_literal);
  legacy.string_bg = COLOR_BLACK;
  legacy.comment_fg = theme_color_to_ncurses_color(current_theme.comment);
  legacy.comment_bg = COLOR_BLACK;
  legacy.number_fg = theme_color_to_ncurses_color(current_theme.number);
  legacy.number_bg = COLOR_BLACK;
  legacy.preprocessor_fg =
      theme_color_to_ncurses_color(current_theme.preprocessor);
  legacy.preprocessor_bg = COLOR_BLACK;
  legacy.function_fg =
      theme_color_to_ncurses_color(current_theme.function_name);
  legacy.function_bg = COLOR_BLACK;
  legacy.operator_fg =
      theme_color_to_ncurses_color(current_theme.operator_color);
  legacy.operator_bg = COLOR_BLACK;
  legacy.markdown_heading_fg =
      theme_color_to_ncurses_color(current_theme.markdown_heading);
  legacy.markdown_heading_bg = COLOR_BLACK;
  legacy.markdown_bold_fg =
      theme_color_to_ncurses_color(current_theme.markdown_bold);
  legacy.markdown_bold_bg = COLOR_BLACK;
  legacy.markdown_italic_fg =
      theme_color_to_ncurses_color(current_theme.markdown_italic);
  legacy.markdown_italic_bg = COLOR_BLACK;
  legacy.markdown_code_fg =
      theme_color_to_ncurses_color(current_theme.markdown_code);
  legacy.markdown_code_bg = COLOR_BLACK;
  legacy.markdown_link_fg =
      theme_color_to_ncurses_color(current_theme.markdown_link);
  legacy.markdown_link_bg = COLOR_BLACK;
  return legacy;
}

void ThemeManager::apply_legacy_theme(const Theme &theme)
{
  // Convert legacy theme to modern theme and apply
  // This is for backward compatibility only
  if (initialized)
  {
    apply_theme();
  }
}

// YAML parsing utilities
std::string ThemeManager::trim(const std::string &str)
{
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

std::string ThemeManager::remove_quotes(const std::string &str)
{
  std::string trimmed = trim(str);
  if (trimmed.length() >= 2 &&
      ((trimmed.front() == '"' && trimmed.back() == '"') ||
       (trimmed.front() == '\'' && trimmed.back() == '\'')))
  {
    return trimmed.substr(1, trimmed.length() - 2);
  }
  return trimmed;
}

std::map<std::string, std::string>
ThemeManager::parse_yaml(const std::string &yaml_content)
{
  std::map<std::string, std::string> result;
  std::istringstream stream(yaml_content);
  std::string line;

  while (std::getline(stream, line))
  {
    std::string trimmed_line = trim(line);
    if (trimmed_line.empty() || trimmed_line[0] == '#')
      continue;

    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos)
      continue;

    std::string key = trim(line.substr(0, colon_pos));
    std::string value = trim(line.substr(colon_pos + 1));
    value = remove_quotes(value);

    if (!key.empty() && !value.empty())
    {
      result[key] = value;
    }
  }
  return result;
}

bool ThemeManager::load_theme_from_yaml(const std::string &yaml_content)
{
  try
  {
    auto config = parse_yaml(yaml_content);
    NamedTheme theme;

    auto get_color = [&](const std::string &key,
                         ThemeColor default_color) -> ThemeColor
    {
      auto it = config.find(key);
      return (it != config.end()) ? string_to_theme_color(it->second)
                                  : default_color;
    };

    theme.name = config.count("name") ? config["name"] : "Custom Theme";
    theme.background = get_color("background", ThemeColor::BLACK);
    theme.foreground = get_color("foreground", ThemeColor::WHITE);
    theme.cursor = get_color("cursor", ThemeColor::WHITE);
    theme.selection = get_color("selection", ThemeColor::BLUE);
    theme.line_highlight = get_color("line_highlight", ThemeColor::DARK_GRAY);
    theme.line_numbers = get_color("line_numbers", ThemeColor::GRAY);
    theme.line_numbers_active =
        get_color("line_numbers_active", ThemeColor::WHITE);
    theme.status_bar_bg = get_color("status_bar_bg", ThemeColor::BLUE);
    theme.status_bar_fg = get_color("status_bar_fg", ThemeColor::WHITE);
    theme.status_bar_active = get_color("status_bar_active", ThemeColor::CYAN);
    theme.keyword = get_color("keyword", ThemeColor::BLUE);
    theme.string_literal = get_color("string_literal", ThemeColor::GREEN);
    theme.number = get_color("number", ThemeColor::CYAN);
    theme.comment = get_color("comment", ThemeColor::GRAY);
    theme.function_name = get_color("function_name", ThemeColor::YELLOW);
    theme.variable = get_color("variable", ThemeColor::WHITE);
    theme.type = get_color("type", ThemeColor::YELLOW);
    theme.operator_color = get_color("operator", ThemeColor::RED);
    theme.preprocessor = get_color("preprocessor", ThemeColor::CYAN);
    theme.python_decorator = get_color("python_decorator", ThemeColor::YELLOW);
    theme.python_builtin = get_color("python_builtin", ThemeColor::CYAN);
    theme.cpp_namespace = get_color("cpp_namespace", ThemeColor::CYAN);
    theme.markdown_heading = get_color("markdown_heading", ThemeColor::CYAN);
    theme.markdown_bold = get_color("markdown_bold", ThemeColor::WHITE);
    theme.markdown_italic = get_color("markdown_italic", ThemeColor::YELLOW);
    theme.markdown_code = get_color("markdown_code", ThemeColor::GREEN);
    theme.markdown_link = get_color("markdown_link", ThemeColor::BLUE);
    theme.markdown_quote = get_color("markdown_quote", ThemeColor::YELLOW);

    current_theme = theme;
    if (initialized)
    {
      apply_theme();
    }
    return true;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error parsing theme: " << e.what() << std::endl;
    load_default_theme();
    return false;
  }
}

bool ThemeManager::load_theme_from_file(const std::string &file_path)
{
  try
  {
    std::ifstream file(file_path);
    if (!file.is_open())
    {
      std::cerr << "Failed to open theme file: " << file_path << std::endl;
      load_default_theme();
      return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return load_theme_from_yaml(buffer.str());
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error reading theme file " << file_path << ": " << e.what()
              << std::endl;
    load_default_theme();
    return false;
  }
}

// Global instance
ThemeManager g_theme_manager;

// Legacy API functions for backward compatibility
void init_colors() { g_theme_manager.initialize(); }

void load_default_theme()
{
  if (!g_theme_manager.is_initialized())
  {
    g_theme_manager.initialize();
  }
}

void apply_theme(const Theme &theme)
{
  g_theme_manager.apply_legacy_theme(theme);
}

Theme get_current_theme() { return g_theme_manager.get_legacy_theme(); }