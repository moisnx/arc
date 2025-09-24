#include "theme_manager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>


#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

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
    std::cerr << "Terminal does not support colors. Ensure TERM is set to a "
                 "color-capable terminal (e.g., xterm-256color)."
              << std::endl;
    return;
  }

  // MUST call start_color() before checking COLORS!
  start_color();
  use_default_colors(); // Use terminal's default colors

  std::cerr << "=== TERMINAL COLOR DEBUG ===" << std::endl;
  std::cerr << "COLORS: " << COLORS << std::endl;
  std::cerr << "COLOR_PAIRS: " << COLOR_PAIRS << std::endl;
  std::cerr << "TERM: " << (getenv("TERM") ? getenv("TERM") : "not set")
            << std::endl;
  std::cerr << "can_change_color(): " << (can_change_color() ? "YES" : "NO")
            << std::endl;

  // Windows Terminal/PDCurses sometimes reports COLORS=0 even when colors work
  // Force COLORS to a reasonable value on Windows if it's 0
#ifdef _WIN32
  if (COLORS <= 0)
  {
    std::cerr << "Windows detected - COLORS=" << COLORS
              << ", forcing to 16 colors" << std::endl;
    // We can't actually change the COLORS constant, but we can work with what
    // we have
  }
  else
  {
    std::cerr << "Windows detected - proceeding with " << COLORS << " colors"
              << std::endl;
  }
#else
  if (COLORS < 8)
  {
    std::cerr << "Terminal supports only " << COLORS
              << " colors. At least 8 are required." << std::endl;
    return;
  }
#endif

  init_terminal_palette();
  std::cerr << "Terminal palette size: " << terminal_palette.size()
            << std::endl;

  load_default_theme();
  apply_theme();
  initialized = true;
  std::cerr << "ThemeManager initialized" << std::endl;
}

void ThemeManager::init_terminal_palette()
{
  terminal_palette.clear();
  // Standard NCurses colors as fallback
  terminal_palette = {
      {0, 0, 0},       // Black (COLOR_BLACK)
      {255, 0, 0},     // Red (COLOR_RED)
      {0, 255, 0},     // Green (COLOR_GREEN)
      {255, 255, 0},   // Yellow (COLOR_YELLOW)
      {0, 0, 255},     // Blue (COLOR_BLUE)
      {255, 0, 255},   // Magenta (COLOR_MAGENTA)
      {0, 255, 255},   // Cyan (COLOR_CYAN)
      {255, 255, 255}, // White (COLOR_WHITE)
      {128, 128, 128}  // Dark gray (approximating color 8)
  };

  // Populate extended colors if available
  if (COLORS >= 16)
  {
    for (int i = 0; i < COLORS && i < 256; ++i)
    {
      short r, g, b;
      if (color_content(i, &r, &g, &b) == OK)
      {
        terminal_palette.emplace_back(RGB{static_cast<short>(r * 255 / 1000),
                                          static_cast<short>(g * 255 / 1000),
                                          static_cast<short>(b * 255 / 1000)});
      }
    }
  }
}

RGB ThemeManager::hex_to_rgb(const std::string &hex)
{
  std::string h = (hex[0] == '#' ? hex.substr(1) : hex);
  if (h.length() != 6)
  {
    std::cerr << "Invalid hex color: " << hex << ", using default #000000"
              << std::endl;
    return RGB{0, 0, 0};
  }

  unsigned int r, g, b;
  if (sscanf(h.c_str(), "%2x%2x%2x", &r, &g, &b) != 3)
  {
    std::cerr << "Failed to parse hex color: " << hex
              << ", using default #000000" << std::endl;
    return RGB{0, 0, 0};
  }

  return RGB{static_cast<short>(r), static_cast<short>(g),
             static_cast<short>(b)};
}

double ThemeManager::color_distance(const RGB &c1, const RGB &c2)
{
  return std::sqrt(std::pow(c1.r - c2.r, 2) + std::pow(c1.g - c2.g, 2) +
                   std::pow(c1.b - c2.b, 2));
}

int ThemeManager::find_closest_color(const RGB &target)
{
  if (terminal_palette.empty())
  {
    std::cerr << "Terminal palette is empty, returning default color index 0"
              << std::endl;
    return COLOR_BLACK;
  }

  int closest_idx = 0;
  double min_distance = std::numeric_limits<double>::max();
  RGB scaled_target = {static_cast<short>(target.r * 1000 / 255),
                       static_cast<short>(target.g * 1000 / 255),
                       static_cast<short>(target.b * 1000 / 255)};

  for (size_t i = 0; i < terminal_palette.size(); ++i)
  {
    double dist = color_distance(scaled_target, terminal_palette[i]);
    if (dist < min_distance)
    {
      min_distance = dist;
      closest_idx = i;
    }
  }
  return closest_idx;
}

int hex_to_ncurses_color(const std::string &hex)
{
  std::string h = (hex[0] == '#' ? hex.substr(1) : hex);
  if (h.length() != 6)
  {
    std::cerr << "Invalid hex color: " << hex << ", using COLOR_WHITE"
              << std::endl;
    return COLOR_WHITE;
  }

  // Convert hex to RGB
  unsigned int r, g, b;
  if (sscanf(h.c_str(), "%2x%2x%2x", &r, &g, &b) != 3)
  {
    std::cerr << "Failed to parse hex color: " << hex << ", using COLOR_WHITE"
              << std::endl;
    return COLOR_WHITE;
  }

  std::cerr << "Converting " << hex << " -> RGB(" << r << "," << g << "," << b
            << ")";

  // Map specific GitHub Dark theme colors to best-matching 8 colors
  // Background/UI colors
  if (h == "24292e" || h == "2f363d")
  { // Dark backgrounds
    std::cerr << " -> COLOR_BLACK" << std::endl;
    return COLOR_BLACK;
  }
  if (h == "3e444b" || h == "44475a")
  { // Medium dark colors
    std::cerr << " -> COLOR_BLACK" << std::endl;
    return COLOR_BLACK;
  }
  if (h == "d1d5da" || h == "959da5" || h == "ffffff")
  { // Light text
    std::cerr << " -> COLOR_WHITE" << std::endl;
    return COLOR_WHITE;
  }

  // Comments and subtle colors
  if (h == "6a737d")
  { // Comments should be dim
    std::cerr << " -> COLOR_BLACK" << std::endl;
    return COLOR_BLACK; // Use black for dimmed text - will need A_DIM attribute
  }

  // Syntax colors - map to vibrant colors
  if (h == "f97583")
  { // Pink keywords/operators -> RED
    std::cerr << " -> COLOR_RED" << std::endl;
    return COLOR_RED;
  }
  if (h == "9ecb95")
  { // Green strings -> GREEN
    std::cerr << " -> COLOR_GREEN" << std::endl;
    return COLOR_GREEN;
  }
  if (h == "b392f0")
  { // Purple numbers -> MAGENTA
    std::cerr << " -> COLOR_MAGENTA" << std::endl;
    return COLOR_MAGENTA;
  }
  if (h == "d2a8ff")
  { // Light purple functions/decorators -> MAGENTA
    std::cerr << " -> COLOR_MAGENTA" << std::endl;
    return COLOR_MAGENTA;
  }
  if (h == "79b8ff")
  { // Blue types/links -> BLUE
    std::cerr << " -> COLOR_BLUE" << std::endl;
    return COLOR_BLUE;
  }

  // For limited color terminals, do intelligent mapping based on RGB values
  if (COLORS <= 16)
  {
    // Calculate brightness
    int brightness = (r + g + b) / 3;

    // Very dark colors -> black
    if (brightness < 64)
    {
      std::cerr << " -> COLOR_BLACK" << std::endl;
      return COLOR_BLACK;
    }

    // Very bright colors -> white
    if (brightness > 200)
    {
      std::cerr << " -> COLOR_WHITE" << std::endl;
      return COLOR_WHITE;
    }

    // Medium brightness colors - map by dominant component
    int max_component = std::max({r, g, b});
    int min_component = std::min({r, g, b});

    // High saturation colors (one component dominates)
    if (max_component - min_component > 80)
    {
      if (r == max_component && r > 120)
      {
        std::cerr << " -> COLOR_RED" << std::endl;
        return COLOR_RED;
      }
      if (g == max_component && g > 120)
      {
        std::cerr << " -> COLOR_GREEN" << std::endl;
        return COLOR_GREEN;
      }
      if (b == max_component && b > 120)
      {
        std::cerr << " -> COLOR_BLUE" << std::endl;
        return COLOR_BLUE;
      }
    }

    // Mixed colors
    if (r > 100 && g > 100 && b < 100)
    {
      std::cerr << " -> COLOR_YELLOW" << std::endl;
      return COLOR_YELLOW;
    }
    if (r > 100 && b > 100 && g < 100)
    {
      std::cerr << " -> COLOR_MAGENTA" << std::endl;
      return COLOR_MAGENTA;
    }
    if (g > 100 && b > 100 && r < 100)
    {
      std::cerr << " -> COLOR_CYAN" << std::endl;
      return COLOR_CYAN;
    }

    // Grayish colors - use brightness to decide
    if (brightness < 100)
    {
      std::cerr << " -> COLOR_BLACK" << std::endl;
      return COLOR_BLACK;
    }

    std::cerr << " -> COLOR_WHITE" << std::endl;
    return COLOR_WHITE;
  }

  // For terminals with more colors, try to use the palette
  if (COLORS >= 256 && !g_theme_manager.get_terminal_palette().empty())
  {
    RGB target_rgb{static_cast<short>(r), static_cast<short>(g),
                   static_cast<short>(b)};
    int closest = g_theme_manager.find_closest_color(target_rgb);
    std::cerr << " -> " << closest << std::endl;
    return closest;
  }

  std::cerr << " -> COLOR_WHITE (fallback)" << std::endl;
  return COLOR_WHITE;
}

ProcessedTheme ThemeManager::process_raw_theme(const EditorTheme &raw_theme)
{
  ProcessedTheme processed;
  processed.name = raw_theme.name;

  processed.background_idx = hex_to_ncurses_color(raw_theme.background);
  processed.foreground_idx = hex_to_ncurses_color(raw_theme.foreground);
  processed.cursor_idx = hex_to_ncurses_color(raw_theme.cursor);
  processed.selection_idx = hex_to_ncurses_color(raw_theme.selection);
  processed.line_highlight_idx = hex_to_ncurses_color(raw_theme.line_highlight);
  processed.line_numbers_idx = hex_to_ncurses_color(raw_theme.line_numbers);
  processed.line_numbers_active_idx =
      hex_to_ncurses_color(raw_theme.line_numbers_active);
  processed.status_bar_bg_idx = hex_to_ncurses_color(raw_theme.status_bar_bg);
  processed.status_bar_fg_idx = hex_to_ncurses_color(raw_theme.status_bar_fg);
  processed.status_bar_active_idx =
      hex_to_ncurses_color(raw_theme.status_bar_active);
  processed.keyword_idx = hex_to_ncurses_color(raw_theme.keyword);
  processed.string_literal_idx = hex_to_ncurses_color(raw_theme.string_literal);
  processed.number_idx = hex_to_ncurses_color(raw_theme.number);
  processed.comment_idx = hex_to_ncurses_color(raw_theme.comment);
  processed.function_name_idx = hex_to_ncurses_color(raw_theme.function_name);
  processed.variable_idx = hex_to_ncurses_color(raw_theme.variable);
  processed.type_idx = hex_to_ncurses_color(raw_theme.type);
  processed.operator_color_idx = hex_to_ncurses_color(raw_theme.operator_color);
  processed.preprocessor_idx = hex_to_ncurses_color(raw_theme.preprocessor);
  processed.python_decorator_idx =
      hex_to_ncurses_color(raw_theme.python_decorator);
  processed.python_builtin_idx = hex_to_ncurses_color(raw_theme.python_builtin);
  processed.cpp_namespace_idx = hex_to_ncurses_color(raw_theme.cpp_namespace);
  processed.markdown_heading_idx =
      hex_to_ncurses_color(raw_theme.markdown_heading);
  processed.markdown_bold_idx = hex_to_ncurses_color(raw_theme.markdown_bold);
  processed.markdown_italic_idx =
      hex_to_ncurses_color(raw_theme.markdown_italic);
  processed.markdown_code_idx = hex_to_ncurses_color(raw_theme.markdown_code);
  processed.markdown_link_idx = hex_to_ncurses_color(raw_theme.markdown_link);
  processed.markdown_quote_idx = hex_to_ncurses_color(raw_theme.markdown_quote);

  return processed;
}

void ThemeManager::load_default_theme()
{
  EditorTheme default_theme = {
      "Default Dark",
      "#000000", // background (black)
      "#ffffff", // foreground (white)
      "#808080", // cursor (gray)
      "#0000ff", // selection (blue)
      "#2a2d2e", // line_highlight (dark gray)
      "#ffff00", // line_numbers (yellow)
      "#ffffff", // line_numbers_active (white)
      "#0000ff", // status_bar_bg (blue)
      "#ffffff", // status_bar_fg (white)
      "#1177bb", // status_bar_active
      "#0000ff", // keyword (blue)
      "#00ff00", // string_literal (green)
      "#00ffff", // number (cyan)
      "#808080", // comment (dark gray)
      "#ffff00", // function_name (yellow)
      "#ffffff", // variable (white)
      "#ffff00", // type (yellow)
      "#ff0000", // operator (red)
      "#00ffff", // preprocessor (cyan)
      "#ffff00", // python_decorator (yellow)
      "#00ffff", // python_builtin (cyan)
      "#00ffff", // cpp_namespace (cyan)
      "#00ffff", // markdown_heading (cyan)
      "#ffffff", // markdown_bold (white)
      "#ffff00", // markdown_italic (yellow)
      "#00ff00", // markdown_code (green)
      "#0000ff", // markdown_link (blue)
      "#ffff00"  // markdown_quote (yellow)
  };
  current_theme = process_raw_theme(default_theme);
}

// Manual YAML parsing functions
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
    // Skip empty lines and comments
    std::string trimmed_line = trim(line);
    if (trimmed_line.empty() || trimmed_line[0] == '#')
      continue;

    // Find the colon separator
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string::npos)
      continue;

    // Extract key and value
    std::string key = trim(line.substr(0, colon_pos));
    std::string value = trim(line.substr(colon_pos + 1));

    // Remove quotes from value if present
    value = remove_quotes(value);

    // Store the key-value pair
    if (!key.empty() && !value.empty())
    {
      result[key] = value;
    }
  }

  return result;
}

bool ThemeManager::load_theme_from_file(const std::string &file_path)
{
  try
  {
    std::ifstream file(file_path);
    if (!file.is_open())
    {
      std::cerr << "Failed to open theme file: " << file_path
                << ", using default theme" << std::endl;
      load_default_theme();
      return false;
    }

    // Read entire file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    return load_theme_from_yaml(buffer.str());
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error reading theme file " << file_path << ": " << e.what()
              << ", using default theme" << std::endl;
    load_default_theme();
    return false;
  }
}

bool ThemeManager::load_theme_from_yaml(const std::string &yaml_content)
{
  try
  {
    auto config = parse_yaml(yaml_content);
    EditorTheme raw_theme;

    // Helper function to get value with default
    auto get_value = [&config](const std::string &key,
                               const std::string &default_value) -> std::string
    {
      auto it = config.find(key);
      return (it != config.end()) ? it->second : default_value;
    };

    raw_theme.name = get_value("name", "Custom Theme");
    raw_theme.background = get_value("background", "#000000");
    raw_theme.foreground = get_value("foreground", "#ffffff");
    raw_theme.cursor = get_value("cursor", "#808080");
    raw_theme.selection = get_value("selection", "#0000ff");
    raw_theme.line_highlight = get_value("line_highlight", "#2a2d2e");
    raw_theme.line_numbers = get_value("line_numbers", "#ffff00");
    raw_theme.line_numbers_active = get_value("line_numbers_active", "#ffffff");
    raw_theme.status_bar_bg = get_value("status_bar_bg", "#0000ff");
    raw_theme.status_bar_fg = get_value("status_bar_fg", "#ffffff");
    raw_theme.status_bar_active = get_value("status_bar_active", "#1177bb");
    raw_theme.keyword = get_value("keyword", "#0000ff");
    raw_theme.string_literal = get_value("string_literal", "#00ff00");
    raw_theme.number = get_value("number", "#00ffff");
    raw_theme.comment = get_value("comment", "#808080");
    raw_theme.function_name = get_value("function_name", "#ffff00");
    raw_theme.variable = get_value("variable", "#ffffff");
    raw_theme.type = get_value("type", "#ffff00");
    raw_theme.operator_color = get_value("operator", "#ff0000");
    raw_theme.preprocessor = get_value("preprocessor", "#00ffff");
    raw_theme.python_decorator = get_value("python_decorator", "#ffff00");
    raw_theme.python_builtin = get_value("python_builtin", "#00ffff");
    raw_theme.cpp_namespace = get_value("cpp_namespace", "#00ffff");
    raw_theme.markdown_heading = get_value("markdown_heading", "#00ffff");
    raw_theme.markdown_bold = get_value("markdown_bold", "#ffffff");
    raw_theme.markdown_italic = get_value("markdown_italic", "#ffff00");
    raw_theme.markdown_code = get_value("markdown_code", "#00ff00");
    raw_theme.markdown_link = get_value("markdown_link", "#0000ff");
    raw_theme.markdown_quote = get_value("markdown_quote", "#ffff00");

    current_theme = process_raw_theme(raw_theme);
    if (initialized)
    {
      apply_theme();
    }
    return true;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error parsing YAML content: " << e.what()
              << ", using default theme" << std::endl;
    load_default_theme();
    return false;
  }
}

void ThemeManager::apply_theme()
{
  if (!initialized)
  {
    std::cerr << "ThemeManager not initialized, cannot apply theme"
              << std::endl;
    return;
  }

  auto init_pair_safe = [&](int pair, int fg, int bg)
  {
    if (fg >= 0 && fg < COLORS && (bg == -1 || bg < COLORS) &&
        pair < COLOR_PAIRS)
    {
      if (init_pair(pair, fg, bg) == OK)
      {
        std::cerr << "Initialized pair " << pair << " with fg=" << fg
                  << ", bg=" << bg << std::endl;
        return true;
      }
      else
      {
        std::cerr << "Failed to init_pair(" << pair << ", " << fg << ", " << bg
                  << ")" << std::endl;
        return false;
      }
    }
    else
    {
      std::cerr << "Invalid color indices for pair " << pair << ": fg=" << fg
                << ", bg=" << bg << std::endl;
      return false;
    }
  };

  init_pair_safe(LINE_NUMBERS_ACTIVE, current_theme.line_numbers_active_idx,
                 -1);
  init_pair_safe(LINE_NUMBERS, current_theme.line_numbers_idx, -1);
  init_pair_safe(STATUS_BAR, current_theme.status_bar_fg_idx,
                 current_theme.status_bar_bg_idx);
  init_pair_safe(STATUS_BAR_TEXT, current_theme.status_bar_fg_idx,
                 current_theme.status_bar_bg_idx);
  init_pair_safe(STATUS_BAR_ACTIVE, current_theme.status_bar_fg_idx,
                 current_theme.status_bar_active_idx);
  init_pair_safe(CURSOR, current_theme.cursor_idx, -1);
  init_pair_safe(SELECTION, current_theme.selection_idx, -1);
  init_pair_safe(LINE_HIGHLIGHT, current_theme.line_highlight_idx, -1);
  init_pair_safe(KEYWORD, current_theme.keyword_idx, -1);
  init_pair_safe(STRING_LITERAL, current_theme.string_literal_idx, -1);
  init_pair_safe(NUMBER, current_theme.number_idx, -1);
  init_pair_safe(COMMENT, current_theme.comment_idx, -1);
  init_pair_safe(FUNCTION, current_theme.function_name_idx, -1);
  init_pair_safe(VARIABLE, current_theme.variable_idx, -1);
  init_pair_safe(TYPE, current_theme.type_idx, -1);
  init_pair_safe(OPERATOR, current_theme.operator_color_idx, -1);
  init_pair_safe(PREPROCESSOR, current_theme.preprocessor_idx, -1);
  init_pair_safe(PYTHON_DECORATOR, current_theme.python_decorator_idx, -1);
  init_pair_safe(PYTHON_BUILTIN, current_theme.python_builtin_idx, -1);
  init_pair_safe(CPP_NAMESPACE, current_theme.cpp_namespace_idx, -1);
  init_pair_safe(MARKDOWN_HEADING, current_theme.markdown_heading_idx, -1);
  init_pair_safe(MARKDOWN_BOLD, current_theme.markdown_bold_idx, -1);
  init_pair_safe(MARKDOWN_ITALIC, current_theme.markdown_italic_idx, -1);
  init_pair_safe(MARKDOWN_CODE, current_theme.markdown_code_idx,
                 current_theme.background_idx);
  init_pair_safe(MARKDOWN_LINK, current_theme.markdown_link_idx, -1);
  init_pair_safe(MARKDOWN_QUOTE, current_theme.markdown_quote_idx, -1);
  init_pair_safe(LINE_NUMBERS_DIM, current_theme.line_numbers_idx, -1);
  init_pair_safe(STATUS_BAR_CYAN, COLOR_CYAN, -1);
  init_pair_safe(STATUS_BAR_YELLOW, COLOR_YELLOW, -1);
  init_pair_safe(STATUS_BAR_GREEN, COLOR_GREEN, -1);
  init_pair_safe(STATUS_BAR_MAGENTA, COLOR_MAGENTA, -1);
  init_pair_safe(STATUS_BAR_DIM, 8, -1);
  init_pair_safe(ACTIVE_LINE_BG, -1, current_theme.line_highlight_idx);
  init_pair_safe(CPP_TYPE, current_theme.type_idx, -1);
  init_pair_safe(PREPROCESSOR_INCLUDE, current_theme.preprocessor_idx, -1);
  init_pair_safe(PREPROCESSOR_DEFINE, current_theme.preprocessor_idx, -1);
  init_pair_safe(CLASS_NAME, current_theme.type_idx, -1);
  init_pair_safe(PYTHON_KEYWORD, current_theme.keyword_idx, -1);
  init_pair_safe(PYTHON_FUNCTION_DEF, current_theme.function_name_idx, -1);
  init_pair_safe(PYTHON_CLASS_DEF, current_theme.type_idx, -1);
  init_pair_safe(MARKDOWN_CODE_BLOCK, current_theme.markdown_code_idx, -1);
  init_pair_safe(MARKDOWN_BLOCKQUOTE, current_theme.markdown_quote_idx, -1);
  init_pair_safe(MARKDOWN_LIST, current_theme.keyword_idx, -1);
  init_pair_safe(MARKDOWN_TABLE, current_theme.operator_color_idx, -1);
  init_pair_safe(MARKDOWN_STRIKETHROUGH, current_theme.comment_idx, -1);
  init_pair_safe(MARKDOWN_URL, current_theme.markdown_link_idx, -1);
}

ThemeManager g_theme_manager;