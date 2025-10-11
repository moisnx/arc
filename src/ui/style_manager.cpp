#include "style_manager.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <curses.h>
inline int setenv(const char *name, const char *value, int overwrite)
{
  if (!overwrite)
  {
    size_t envsize = 0;
    getenv_s(&envsize, nullptr, 0, name);
    if (envsize != 0)
      return 0;
  }
  return _putenv_s(name, value);
}
#else
#include <ncursesw/ncurses.h>
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
    return;
  }

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

  if (start_color() == ERR)
  {
    std::cerr << "Failed to start color support" << std::endl;
    return;
  }

  if (use_default_colors() == ERR)
  {
    std::cerr << "Warning: use_default_colors() failed, using fallback"
              << std::endl;
    assume_default_colors(COLOR_WHITE, COLOR_BLACK);
  }

  supports_256_colors_cache = supports_256_colors();
  next_custom_color_id = 16;
  color_cache.clear();

  load_default_theme();
  apply_theme();

  initialized = true;
}

short StyleManager::resolve_theme_color(const std::string &config_value)
{
  if (config_value.empty() || config_value == "transparent" ||
      config_value == "default")
  {
    return -1;
  }

  if (config_value.length() == 7 && config_value[0] == '#')
  {
    auto cache_it = color_cache.find(config_value);
    if (cache_it != color_cache.end())
    {
      return cache_it->second;
    }

    RGB rgb = parse_hex_color(config_value);

    if (supports_256_colors_cache && next_custom_color_id < COLORS)
    {
      short r_1000 = (rgb.r * 1000) / 255;
      short g_1000 = (rgb.g * 1000) / 255;
      short b_1000 = (rgb.b * 1000) / 255;

      short color_id = next_custom_color_id;
      if (init_color(color_id, r_1000, g_1000, b_1000) == OK)
      {
        color_cache[config_value] = color_id;
        next_custom_color_id++;
        return color_id;
      }
      else
      {
        std::cerr << "Failed to create custom color for " << config_value
                  << ", falling back to closest 8-color" << std::endl;
      }
    }

    return find_closest_8color(rgb);
  }

  // Fallback for any legacy color names
  return COLOR_WHITE;
}

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
    std::string r_str = hex_str.substr(1, 2);
    std::string g_str = hex_str.substr(3, 2);
    std::string b_str = hex_str.substr(5, 2);

    rgb.r = std::stoi(r_str, nullptr, 16);
    rgb.g = std::stoi(g_str, nullptr, 16);
    rgb.b = std::stoi(b_str, nullptr, 16);

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

short StyleManager::find_closest_8color(const RGB &rgb) const
{
  struct ColorMapping
  {
    short ncurses_color;
    RGB rgb;
  };

  static const ColorMapping basic_colors[] = {
      {COLOR_BLACK, RGB(0, 0, 0)},    {COLOR_RED, RGB(128, 0, 0)},
      {COLOR_GREEN, RGB(0, 128, 0)},  {COLOR_YELLOW, RGB(128, 128, 0)},
      {COLOR_BLUE, RGB(0, 0, 128)},   {COLOR_MAGENTA, RGB(128, 0, 128)},
      {COLOR_CYAN, RGB(0, 128, 128)}, {COLOR_WHITE, RGB(192, 192, 192)}};

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

bool StyleManager::is_light_theme() const
{
  if (current_theme.background[0] == '#')
  {
    RGB bg_rgb = parse_hex_color(current_theme.background);
    return (bg_rgb.r + bg_rgb.g + bg_rgb.b) / 3 > 128;
  }
  return false;
}

void StyleManager::load_default_theme()
{
  current_theme = {
      "Default Dark (Semantic)",

      // Core
      "transparent", // background
      "#C9D1D9",     // foreground

      // State colors
      "#58A6FF", // state_active
      "#264F78", // state_selected
      "#161B22", // state_hover
      "#6E7681", // state_disabled

      // Semantic UI
      "#58A6FF", // ui_primary
      "#8B949E", // ui_secondary
      "#D2A8FF", // ui_accent
      "#7EE787", // ui_success
      "#E3B341", // ui_warning
      "#FF7B72", // ui_error
      "#79C0FF", // ui_info
      "#30363D", // ui_border

      // Editor-specific
      "#FFFFFF", // cursor
      "#6E7681", // line_numbers
      "#C9D1D9", // line_numbers_active
      "#161B22", // line_highlight

      // Status bar
      "#21262D", // status_bar_bg
      "#C9D1D9", // status_bar_fg
      "#58A6FF", // status_bar_active

      // Syntax
      "#FF7B72", // keyword
      "#7EE787", // string_literal
      "#D2A8FF", // number
      "#8B949E", // comment
      "#D2A8FF", // function_name
      "#C9D1D9", // variable
      "#79C0FF", // type
      "#FF7B72", // operator
      "#C9D1D9", // punctuation
      "#79C0FF", // constant
      "#79C0FF", // namespace
      "#C9D1D9", // property
      "#D2A8FF", // decorator
      "#FF7B72", // macro
      "#FF7B72", // label

      // Markup
      "#FF7B72", // markup_heading
      "#C9D1D9", // markup_bold
      "#7EE787", // markup_italic
      "#D2A8FF", // markup_code
      "#D2A8FF", // markup_code_block
      "#58A6FF", // markup_link
      "#58A6FF", // markup_url
      "#7EE787", // markup_list
      "#8B949E", // markup_blockquote
      "#FF6B6B", // markup_strikethrough
      "#8B949E"  // markup_quote
  };
}

void StyleManager::apply_theme()
{
  if (!initialized)
  {
    return;
  }

  short terminal_bg = resolve_theme_color(current_theme.background);
  short terminal_fg = resolve_theme_color(current_theme.foreground);

  // Helper lambda for initializing color pairs
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

  // Core pairs (0-9)
  init_pair(0, terminal_fg, terminal_bg);
  init_pair_enhanced(1, current_theme.foreground, current_theme.background);

  init_pair_enhanced(2, current_theme.foreground, current_theme.background);
  // State colors (10-19) - THE NEW SEMANTIC PAIRS
  init_pair_enhanced(ColorPairs::STATE_ACTIVE, current_theme.state_active,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::STATE_SELECTED, current_theme.state_selected,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::STATE_HOVER, current_theme.foreground,
                     current_theme.state_hover);
  init_pair_enhanced(ColorPairs::STATE_DISABLED, current_theme.state_disabled,
                     current_theme.background);

  // Semantic UI (20-29) - THE NEW UI PAIRS
  init_pair_enhanced(ColorPairs::UI_PRIMARY, current_theme.ui_primary,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_SECONDARY, current_theme.ui_secondary,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_ACCENT, current_theme.ui_accent,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_SUCCESS, current_theme.ui_success,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_WARNING, current_theme.ui_warning,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_ERROR, current_theme.ui_error,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_INFO, current_theme.ui_info,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::UI_BORDER, current_theme.ui_border,
                     current_theme.background);

  // Editor-specific (30-39)
  init_pair_enhanced(ColorPairs::CURSOR, current_theme.cursor,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::LINE_NUMBERS, current_theme.line_numbers,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::LINE_NUMBERS_ACTIVE,
                     current_theme.line_numbers_active,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::LINE_HIGHLIGHT, current_theme.foreground,
                     current_theme.line_highlight);

  // Status bar (40-49)
  init_pair_enhanced(ColorPairs::STATUS_BAR, current_theme.status_bar_fg,
                     current_theme.status_bar_bg);
  init_pair_enhanced(ColorPairs::STATUS_BAR_TEXT, current_theme.status_bar_fg,
                     current_theme.status_bar_bg);
  init_pair_enhanced(ColorPairs::STATUS_BAR_ACTIVE,
                     current_theme.status_bar_active,
                     current_theme.status_bar_bg);
  init_pair_enhanced(ColorPairs::STATUS_BAR_CYAN, "#00FFFF",
                     current_theme.status_bar_bg);
  init_pair_enhanced(ColorPairs::STATUS_BAR_YELLOW, "#FFFF00",
                     current_theme.status_bar_bg);
  init_pair_enhanced(ColorPairs::STATUS_BAR_GREEN, "#00FF00",
                     current_theme.status_bar_bg);
  init_pair_enhanced(ColorPairs::STATUS_BAR_MAGENTA, "#FF00FF",
                     current_theme.status_bar_bg);

  // Syntax highlighting (50-69)
  init_pair_enhanced(ColorPairs::SYNTAX_KEYWORD, current_theme.keyword,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_STRING, current_theme.string_literal,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_NUMBER, current_theme.number,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_COMMENT, current_theme.comment,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_FUNCTION, current_theme.function_name,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_VARIABLE, current_theme.variable,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_TYPE, current_theme.type,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_OPERATOR, current_theme.operator_color,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_PUNCTUATION, current_theme.punctuation,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_CONSTANT, current_theme.constant,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_NAMESPACE,
                     current_theme.namespace_color, current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_PROPERTY, current_theme.property,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_DECORATOR, current_theme.decorator,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_MACRO, current_theme.macro,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::SYNTAX_LABEL, current_theme.label,
                     current_theme.background);

  // Markup (70-79)
  init_pair_enhanced(ColorPairs::MARKUP_HEADING, current_theme.markup_heading,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_BOLD, current_theme.markup_bold,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_ITALIC, current_theme.markup_italic,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_CODE, current_theme.markup_code,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_CODE_BLOCK,
                     current_theme.markup_code_block, current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_LINK, current_theme.markup_link,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_URL, current_theme.markup_url,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_BLOCKQUOTE,
                     current_theme.markup_blockquote, current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_LIST, current_theme.markup_list,
                     current_theme.background);
  init_pair_enhanced(ColorPairs::MARKUP_STRIKETHROUGH,
                     current_theme.markup_strikethrough,
                     current_theme.background);

  bkgdset(' ' | COLOR_PAIR(1));
  clear();
}

bool StyleManager::supports_256_colors() const { return COLORS >= 256; }

bool StyleManager::supports_true_color() const
{
  const char *colorterm = getenv("COLORTERM");
  return (colorterm && (std::strcmp(colorterm, "truecolor") == 0 ||
                        std::strcmp(colorterm, "24bit") == 0));
}

// YAML parsing utilities
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
    SemanticTheme theme;

    auto get_color = [&](const std::string &key,
                         const std::string &default_color) -> std::string
    {
      auto it = config.find(key);
      return (it != config.end()) ? it->second : default_color;
    };

    theme.name = config.count("name") ? config["name"] : "Custom Theme";

    // Core
    theme.background = get_color("background", "transparent");
    theme.foreground = get_color("foreground", "#FFFFFF");

    // State colors - with backward compatibility fallbacks
    theme.state_active =
        get_color("state_active", get_color("status_bar_active", "#58A6FF"));
    theme.state_selected =
        get_color("state_selected", get_color("selection", "#264F78"));
    theme.state_hover =
        get_color("state_hover", get_color("line_highlight", "#161B22"));
    theme.state_disabled =
        get_color("state_disabled", get_color("line_numbers", "#6E7681"));

    // Semantic UI - with backward compatibility
    theme.ui_primary = get_color("ui_primary", "#58A6FF");
    theme.ui_secondary =
        get_color("ui_secondary", get_color("comment", "#8B949E"));
    theme.ui_accent = get_color("ui_accent", get_color("decorator", "#D2A8FF"));
    theme.ui_success =
        get_color("ui_success", get_color("string_literal", "#7EE787"));
    theme.ui_warning = get_color("ui_warning", "#E3B341");
    theme.ui_error = get_color("ui_error", get_color("keyword", "#FF7B72"));
    theme.ui_info = get_color("ui_info", get_color("type", "#79C0FF"));
    theme.ui_border = get_color("ui_border", "#30363D");

    // Editor-specific
    theme.cursor = get_color("cursor", "#FFFFFF");
    theme.line_numbers = get_color("line_numbers", "#808080");
    theme.line_numbers_active = get_color("line_numbers_active", "#FFFFFF");
    theme.line_highlight = get_color("line_highlight", "#333333");

    // Status bar
    theme.status_bar_bg = get_color("status_bar_bg", "#000080");
    theme.status_bar_fg = get_color("status_bar_fg", "#FFFFFF");
    theme.status_bar_active = get_color("status_bar_active", "#00FFFF");

    // Syntax (keep all existing ones)
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

// Legacy API functions
void init_colors() { g_style_manager.initialize(); }

void load_default_theme()
{
  if (!g_style_manager.is_initialized())
  {
    g_style_manager.initialize();
  }
}