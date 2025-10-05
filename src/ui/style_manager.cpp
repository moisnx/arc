#include "style_manager.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif
#include <sstream>

StyleManager::StyleManager()
    : initialized(false), supports_256_colors_cache(false),
      next_custom_color_id(16)
{
}

void StyleManager::initialize()
{
  if (initialized)
  {
    std::cerr << "StyleManager already initialized" << std::endl;
    return;
  }

  // Critical: Check if ncurses has been initialized first
  if (!stdscr)
  {
    std::cerr << "ERROR: ncurses not initialized. Call initscr() first!"
              << std::endl;
    return;
  }

  if (!has_colors())
  {
    std::cerr << "Terminal does not support colors" << std::endl;
    return;
  }

  // Initialize color support
  if (start_color() == ERR)
  {
    std::cerr << "Failed to start color support" << std::endl;
    return;
  }

  // Use default terminal colors - CRITICAL for transparent support
  if (use_default_colors() == ERR)
  {
    std::cerr << "Warning: use_default_colors() failed, using fallback"
              << std::endl;
    // Fallback: assume black background, white foreground
    assume_default_colors(COLOR_WHITE, COLOR_BLACK);
  }

  // Initialize color capability cache and custom color tracking
  supports_256_colors_cache = supports_256_colors();
  next_custom_color_id = 16; // Start custom colors at ID 16
  color_cache.clear();

  std::cerr << "=== UNIFIED THEME SYSTEM WITH HEX SUPPORT ===" << std::endl;
  std::cerr << "COLORS: " << COLORS << std::endl;
  std::cerr << "COLOR_PAIRS: " << COLOR_PAIRS << std::endl;
  std::cerr << "256-color support: "
            << (supports_256_colors_cache ? "YES" : "NO") << std::endl;
  std::cerr << "TERM: " << (getenv("TERM") ? getenv("TERM") : "not set")
            << std::endl;

  // Check for WSL-specific issues
  const char *wsl_distro = getenv("WSL_DISTRO_NAME");
  if (wsl_distro)
  {
    std::cerr << "WSL detected: " << wsl_distro << std::endl;
    // WSL sometimes has color issues, force TERM if needed
    if (!getenv("TERM") || strcmp(getenv("TERM"), "dumb") == 0)
    {
      std::cerr << "Setting TERM=xterm-256color for WSL" << std::endl;
      setenv("TERM", "xterm-256color", 1);
    }
  }

  load_default_theme();
  apply_theme();

  initialized = true;
  std::cerr << "Unified theme system initialized successfully" << std::endl;
}

short StyleManager::resolve_theme_color(const std::string &config_value)
{
  // Handle transparent/default colors
  if (config_value.empty() || config_value == "transparent" ||
      config_value == "default")
  {
    return -1; // Use terminal default
  }

  // Check if it's a hex color
  if (config_value.length() == 7 && config_value[0] == '#')
  {
    // Check cache first
    auto cache_it = color_cache.find(config_value);
    if (cache_it != color_cache.end())
    {
      return cache_it->second;
    }

    // Parse the hex color
    RGB rgb = parse_hex_color(config_value);

    if (supports_256_colors_cache && next_custom_color_id < COLORS)
    {
      // 256-color mode: use init_color for true color mapping
      // Scale R, G, B from 0-255 to ncurses' 0-1000 range
      short r_1000 = (rgb.r * 1000) / 255;
      short g_1000 = (rgb.g * 1000) / 255;
      short b_1000 = (rgb.b * 1000) / 255;

      short color_id = next_custom_color_id;
      if (init_color(color_id, r_1000, g_1000, b_1000) == OK)
      {
        // Cache the mapping and increment for next time
        auto cache_it = color_cache.find(config_value);
        if (cache_it != color_cache.end())
        {
          return cache_it->second; // OK: using iterator
        }

        color_cache[config_value] = color_id;
        const_cast<StyleManager *>(this)->next_custom_color_id++;

        std::cerr << "Created custom color " << color_id << " for "
                  << config_value << " (RGB: " << rgb.r << "," << rgb.g << ","
                  << rgb.b << ")" << std::endl;
        return color_id;
      }
      else
      {
        std::cerr << "Failed to create custom color for " << config_value
                  << ", falling back to closest 8-color" << std::endl;
      }
    }

    // Fallback to legacy 8-color mode
    return find_closest_8color(rgb);
  }

  // Legacy named color support (for backward compatibility)
  // This handles old theme files that might still use color names
  ThemeColor legacy_color = string_to_theme_color(config_value);
  return theme_color_to_ncurses_color(legacy_color);
}

// NEW: Hex color parsing utility
RGB StyleManager::parse_hex_color(const std::string &hex_str) const
{
  RGB rgb;

  if (hex_str.length() != 7 || hex_str[0] != '#')
  {
    std::cerr << "Invalid hex color format: " << hex_str << ", using black"
              << std::endl;
    return RGB(0, 0, 0);
  }

  try
  {
    // Parse each component (skip the '#')
    std::string r_str = hex_str.substr(1, 2);
    std::string g_str = hex_str.substr(3, 2);
    std::string b_str = hex_str.substr(5, 2);

    rgb.r = std::stoi(r_str, nullptr, 16);
    rgb.g = std::stoi(g_str, nullptr, 16);
    rgb.b = std::stoi(b_str, nullptr, 16);

    // Clamp to valid range
    rgb.r = std::max(0, std::min(255, rgb.r));
    rgb.g = std::max(0, std::min(255, rgb.g));
    rgb.b = std::max(0, std::min(255, rgb.b));
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error parsing hex color " << hex_str << ": " << e.what()
              << std::endl;
    return RGB(0, 0, 0);
  }

  return rgb;
}

// NEW: Find closest 8-color match for fallback
short StyleManager::find_closest_8color(const RGB &rgb) const
{
  // Define the standard 8 colors in RGB
  struct ColorMapping
  {
    short ncurses_color;
    RGB rgb;
    const char *name;
  };

  static const ColorMapping basic_colors[] = {
      {COLOR_BLACK, RGB(0, 0, 0), "black"},
      {COLOR_RED, RGB(128, 0, 0), "red"},
      {COLOR_GREEN, RGB(0, 128, 0), "green"},
      {COLOR_YELLOW, RGB(128, 128, 0), "yellow"},
      {COLOR_BLUE, RGB(0, 0, 128), "blue"},
      {COLOR_MAGENTA, RGB(128, 0, 128), "magenta"},
      {COLOR_CYAN, RGB(0, 128, 128), "cyan"},
      {COLOR_WHITE, RGB(192, 192, 192), "white"}};

  // Find the closest color using simple Euclidean distance
  double min_distance = 1000000;
  short closest_color = COLOR_WHITE;

  for (const auto &color : basic_colors)
  {
    double dr = rgb.r - color.rgb.r;
    double dg = rgb.g - color.rgb.g;
    double db = rgb.b - color.rgb.b;
    double distance = dr * dr + dg * dg + db * db;

    if (distance < min_distance)
    {
      min_distance = distance;
      closest_color = color.ncurses_color;
    }
  }

  return closest_color;
}

// Legacy function - kept only for load_default_theme compatibility
ThemeColor
StyleManager::string_to_theme_color(const std::string &color_name) const
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

// Legacy function - kept for backward compatibility
int StyleManager::theme_color_to_ncurses_color(ThemeColor color) const
{
  switch (color)
  {
  case ThemeColor::BLACK:
    return COLOR_BLACK;
  case ThemeColor::DARK_GRAY:
    return COLOR_BLACK; // Will use A_BOLD attribute for brighter black
  case ThemeColor::GRAY:
    return COLOR_WHITE; // Will use A_DIM attribute for dimmed white
  case ThemeColor::LIGHT_GRAY:
    return COLOR_WHITE; // Will use A_DIM + A_BOLD for medium brightness
  case ThemeColor::WHITE:
    return COLOR_WHITE;
  case ThemeColor::RED:
    return COLOR_RED;
  case ThemeColor::GREEN:
    return COLOR_GREEN;
  case ThemeColor::BLUE:
    return COLOR_BLUE;
  case ThemeColor::YELLOW:
    return COLOR_YELLOW;
  case ThemeColor::MAGENTA:
    return COLOR_MAGENTA;
  case ThemeColor::CYAN:
    return COLOR_CYAN;
  case ThemeColor::BRIGHT_RED:
    return COLOR_RED; // Will use A_BOLD attribute
  case ThemeColor::BRIGHT_GREEN:
    return COLOR_GREEN; // Will use A_BOLD attribute
  case ThemeColor::BRIGHT_BLUE:
    return COLOR_BLUE; // Will use A_BOLD attribute
  case ThemeColor::BRIGHT_YELLOW:
    return COLOR_YELLOW; // Will use A_BOLD attribute
  case ThemeColor::BRIGHT_MAGENTA:
    return COLOR_MAGENTA; // Will use A_BOLD attribute
  case ThemeColor::BRIGHT_CYAN:
    return COLOR_CYAN; // Will use A_BOLD attribute
  case ThemeColor::TERMINAL:
    return COLOR_RED;
  default:
    return COLOR_WHITE;
  }
}

// Legacy function - simplified since 256-color provides enough fidelity
int StyleManager::theme_color_to_ncurses_attr(ThemeColor color) const
{
  // With 256-color support, we can rely more on actual colors than attributes
  // Keep only essential attributes
  switch (color)
  {
  case ThemeColor::BRIGHT_RED:
  case ThemeColor::BRIGHT_GREEN:
  case ThemeColor::BRIGHT_BLUE:
  case ThemeColor::BRIGHT_YELLOW:
  case ThemeColor::BRIGHT_MAGENTA:
  case ThemeColor::BRIGHT_CYAN:
    return A_BOLD; // Keep bold for legacy compatibility
  default:
    return A_NORMAL;
  }
}

bool StyleManager::is_light_theme() const
{
  // Check if background is a light hex color or legacy light theme
  if (current_theme.background[0] == '#')
  {
    RGB bg_rgb = parse_hex_color(current_theme.background);
    // Consider it light if the average RGB value is > 128
    return (bg_rgb.r + bg_rgb.g + bg_rgb.b) / 3 > 128;
  }

  // Legacy check for named colors
  ThemeColor legacy_bg = string_to_theme_color(current_theme.background);
  return (legacy_bg == ThemeColor::WHITE ||
          legacy_bg == ThemeColor::LIGHT_GRAY);
}

void StyleManager::load_default_theme()
{
  current_theme = {
      "Default Dark (Hex)",
      "#000000", // background
      "#FFFFFF", // foreground
      "#FFFFFF", // cursor
      "#0000FF", // selection
      "#333333", // line_highlight
      "#FFFF00", // line_numbers
      "#FFFF99", // line_numbers_active
      "#000080", // status_bar_bg
      "#FFFFFF", // status_bar_fg
      "#00FFFF", // status_bar_active

      // Semantic categories
      "#569CD6", // keyword
      "#CE9178", // string_literal
      "#B5CEA8", // number
      "#6A9955", // comment
      "#DCDCAA", // function_name
      "#9CDCFE", // variable
      "#4EC9B0", // type
      "#D4D4D4", // operator
      "#D4D4D4", // punctuation
      "#4FC1FF", // constant
      "#4EC9B0", // namespace
      "#9CDCFE", // property
      "#DCDCAA", // decorator
      "#C586C0", // macro
      "#569CD6", // label

      // Markup
      "#569CD6", // markup_heading
      "#D4D4D4", // markup_bold
      "#CE9178", // markup_italic
      "#CE9178", // markup_code
      "#CE9178", // markup_code_block
      "#3794FF", // markup_link
      "#3794FF", // markup_url
      "#6A9955", // markup_list
      "#6A9955", // markup_blockquote
      "#FF6B6B", // markup_strikethrough
      "#6A9955"  // markup_quote
  };
}

void StyleManager::apply_theme()
{
  if (!initialized)
  {
    std::cerr << "StyleManager not initialized, cannot apply theme"
              << std::endl;
    return;
  }

  std::cerr << "Applying theme: " << current_theme.name << std::endl;

  short terminal_bg = resolve_theme_color(current_theme.background);
  short terminal_fg = resolve_theme_color(current_theme.foreground);

  init_pair(0, terminal_fg, terminal_bg);
  const int BACKGROUND_PAIR_ID = 100;
  init_pair(BACKGROUND_PAIR_ID, terminal_fg, terminal_bg);

  auto init_pair_enhanced = [&](int pair_id, const std::string &fg_color_str,
                                const std::string &bg_color_str) -> bool
  {
    if (pair_id >= COLOR_PAIRS)
      return false;
    short fg = resolve_theme_color(fg_color_str);
    short bg = resolve_theme_color(bg_color_str);
    int result = init_pair(pair_id, fg, bg);
    return (result == OK);
  };

  // UI Elements
  init_pair_enhanced(2, current_theme.line_numbers, current_theme.background);
  init_pair_enhanced(3, current_theme.line_numbers_active,
                     current_theme.background);
  init_pair_enhanced(4, "#808080", current_theme.background);

  // Status bar
  init_pair_enhanced(5, current_theme.status_bar_fg,
                     current_theme.status_bar_bg);
  init_pair_enhanced(6, current_theme.status_bar_fg,
                     current_theme.status_bar_bg);
  init_pair_enhanced(7, current_theme.status_bar_active,
                     current_theme.status_bar_bg);
  init_pair_enhanced(8, "#00FFFF", current_theme.status_bar_bg);
  init_pair_enhanced(9, "#FFFF00", current_theme.status_bar_bg);
  init_pair_enhanced(10, "#00FF00", current_theme.status_bar_bg);
  init_pair_enhanced(11, "#FF00FF", current_theme.status_bar_bg);
  init_pair_enhanced(12, "#808080", current_theme.status_bar_bg);

  // Selection and cursor
  init_pair_enhanced(13, current_theme.cursor, current_theme.background);
  init_pair_enhanced(14, current_theme.foreground, current_theme.selection);
  init_pair_enhanced(15, current_theme.foreground,
                     current_theme.line_highlight);

  // Semantic categories (20-39)
  init_pair_enhanced(20, current_theme.keyword, current_theme.background);
  init_pair_enhanced(21, current_theme.string_literal,
                     current_theme.background);
  init_pair_enhanced(22, current_theme.number, current_theme.background);
  init_pair_enhanced(23, current_theme.comment, current_theme.background);
  init_pair_enhanced(24, current_theme.function_name, current_theme.background);
  init_pair_enhanced(25, current_theme.variable, current_theme.background);
  init_pair_enhanced(26, current_theme.type, current_theme.background);
  init_pair_enhanced(27, current_theme.operator_color,
                     current_theme.background);
  init_pair_enhanced(28, current_theme.punctuation, current_theme.background);
  init_pair_enhanced(29, current_theme.constant, current_theme.background);
  init_pair_enhanced(30, current_theme.namespace_color,
                     current_theme.background);
  init_pair_enhanced(31, current_theme.property, current_theme.background);
  init_pair_enhanced(32, current_theme.decorator, current_theme.background);
  init_pair_enhanced(33, current_theme.macro, current_theme.background);
  init_pair_enhanced(34, current_theme.label, current_theme.background);

  // Markup (50-61)
  init_pair_enhanced(50, current_theme.markup_heading,
                     current_theme.background);
  init_pair_enhanced(51, current_theme.markup_bold, current_theme.background);
  init_pair_enhanced(52, current_theme.markup_italic, current_theme.background);
  init_pair_enhanced(53, current_theme.markup_code, current_theme.background);
  init_pair_enhanced(54, current_theme.markup_code_block,
                     current_theme.background);
  init_pair_enhanced(55, current_theme.markup_link, current_theme.background);
  init_pair_enhanced(56, current_theme.markup_url, current_theme.background);
  init_pair_enhanced(57, current_theme.markup_blockquote,
                     current_theme.background);
  init_pair_enhanced(58, current_theme.markup_list, current_theme.background);
  init_pair_enhanced(59, current_theme.operator_color,
                     current_theme.background);
  init_pair_enhanced(60, current_theme.markup_strikethrough,
                     current_theme.background);
  init_pair_enhanced(61, current_theme.markup_quote, current_theme.background);

  // Special pairs
  init_pair_enhanced(70, current_theme.foreground,
                     current_theme.line_highlight);

  bkgdset(' ' | COLOR_PAIR(BACKGROUND_PAIR_ID));
  clear();
  refresh();
}

// Also add this helper function to detect and optimize for WSL
bool StyleManager::is_wsl_environment() const
{
  return getenv("WSL_DISTRO_NAME") != nullptr;
}

// Enhanced color initialization for WSL
void StyleManager::optimize_for_wsl()
{
  if (!is_wsl_environment())
    return;

  std::cerr << "WSL environment detected - applying optimizations" << std::endl;

  // Check Windows Terminal version and capabilities
  const char *wt_session = getenv("WT_SESSION");
  if (wt_session)
  {
    std::cerr << "Windows Terminal detected" << std::endl;

    // Windows Terminal supports true color
    if (supports_true_color())
    {
      std::cerr << "True color support detected" << std::endl;
    }

    // Force TERM to get better colors
    if (!getenv("TERM") || strcmp(getenv("TERM"), "xterm-256color") != 0)
    {
      std::cerr << "Setting TERM=xterm-256color for better WSL colors"
                << std::endl;
      setenv("TERM", "xterm-256color", 1);
    }
  }
}

// Helper function to apply color with attributes (simplified for 256-color)
void StyleManager::apply_color_pair(int pair_id, ThemeColor theme_color) const
{
  int attrs = COLOR_PAIR(pair_id) | theme_color_to_ncurses_attr(theme_color);
  attrset(attrs);
}

// Legacy compatibility functions

// Terminal capability detection
bool StyleManager::supports_256_colors() const { return COLORS >= 256; }

bool StyleManager::supports_true_color() const
{
  const char *colorterm = getenv("COLORTERM");
  return (colorterm && (std::strcmp(colorterm, "truecolor") == 0 ||
                        std::strcmp(colorterm, "24bit") == 0));
}

void StyleManager::apply_legacy_theme(const Theme &theme)
{
  if (initialized)
  {
    apply_theme();
  }
}

// YAML parsing utilities remain the same
std::string StyleManager::trim(const std::string &str)
{
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

std::string StyleManager::remove_quotes(const std::string &str)
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
StyleManager::parse_yaml(const std::string &yaml_content)
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

bool StyleManager::load_theme_from_yaml(const std::string &yaml_content)
{
  try
  {
    auto config = parse_yaml(yaml_content);
    NamedTheme theme;

    auto get_color = [&](const std::string &key,
                         const std::string &default_color) -> std::string
    {
      auto it = config.find(key);
      return (it != config.end()) ? it->second : default_color;
    };

    theme.name = config.count("name") ? config["name"] : "Custom Theme";
    theme.background = get_color("background", "#000000");
    theme.foreground = get_color("foreground", "#FFFFFF");
    theme.cursor = get_color("cursor", "#FFFFFF");
    theme.selection = get_color("selection", "#0000FF");
    theme.line_highlight = get_color("line_highlight", "#333333");
    theme.line_numbers = get_color("line_numbers", "#808080");
    theme.line_numbers_active = get_color("line_numbers_active", "#FFFFFF");
    theme.status_bar_bg = get_color("status_bar_bg", "#000080");
    theme.status_bar_fg = get_color("status_bar_fg", "#FFFFFF");
    theme.status_bar_active = get_color("status_bar_active", "#00FFFF");

    // Semantic categories
    theme.keyword = get_color("keyword", "#569CD6");
    theme.string_literal = get_color("string_literal", "#CE9178");
    theme.number = get_color("number", "#B5CEA8");
    theme.comment = get_color("comment", "#6A9955");
    theme.function_name = get_color("function_name", "#DCDCAA");
    theme.variable = get_color("variable", "#9CDCFE");
    theme.type = get_color("type", "#4EC9B0");
    theme.operator_color = get_color("operator", "#D4D4D4");
    theme.punctuation = get_color("punctuation", "#D4D4D4");
    theme.constant = get_color("constant", "#4FC1FF");
    theme.namespace_color = get_color("namespace", "#4EC9B0");
    theme.property = get_color("property", "#9CDCFE");
    theme.decorator = get_color("decorator", "#DCDCAA");
    theme.macro = get_color("macro", "#C586C0");
    theme.label = get_color("label", "#569CD6");

    // Markup
    theme.markup_heading = get_color("markup_heading", "#569CD6");
    theme.markup_bold = get_color("markup_bold", "#D4D4D4");
    theme.markup_italic = get_color("markup_italic", "#CE9178");
    theme.markup_code = get_color("markup_code", "#CE9178");
    theme.markup_code_block = get_color("markup_code_block", "#CE9178");
    theme.markup_link = get_color("markup_link", "#3794FF");
    theme.markup_url = get_color("markup_url", "#3794FF");
    theme.markup_list = get_color("markup_list", "#6A9955");
    theme.markup_blockquote = get_color("markup_blockquote", "#6A9955");
    theme.markup_strikethrough = get_color("markup_strikethrough", "#FF6B6B");
    theme.markup_quote = get_color("markup_quote", "#6A9955");

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

bool StyleManager::load_theme_from_file(const std::string &file_path)
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
StyleManager g_style_manager;

// Legacy API functions for backward compatibility
void init_colors() { g_style_manager.initialize(); }

void load_default_theme()
{
  if (!g_style_manager.is_initialized())
  {
    g_style_manager.initialize();
  }
}

void apply_theme(const Theme &theme)
{
  g_style_manager.apply_legacy_theme(theme);
}
