#include "config_manager.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include <yaml-cpp/yaml.h>

// EFSW includes
#include <efsw/efsw.h>
#include <efsw/efsw.hpp>

namespace fs = std::filesystem;

// Static initialization
std::string ConfigManager::config_dir_cache_;
std::string ConfigManager::active_theme_ = "default";
std::vector<ConfigReloadCallback> ConfigManager::reload_callbacks_;
std::unique_ptr<efsw::FileWatchListener> ConfigManager::watcher_listener_ =
    nullptr;
std::unique_ptr<efsw::FileWatcher> ConfigManager::watcher_instance_ = nullptr;
std::atomic<bool> ConfigManager::reload_pending_ = false;
EditorConfig ConfigManager::editor_config_;
SyntaxConfig ConfigManager::syntax_config_;

// --- EFSW Listener Class ---

// We define a custom listener that efsw will use to communicate events.
class ConfigFileListener : public efsw::FileWatchListener
{
public:
  void handleFileAction(efsw::WatchID watchid, const std::string &dir,
                        const std::string &filename, efsw::Action action,
                        std::string oldFilename) override
  {
    // We are interested in Modification and Renaming (e.g., atomic save by
    // editor)
    if (filename == "config.yaml" &&
        (action == efsw::Action::Modified || action == efsw::Action::Moved))
    {
      // Call the static handler in ConfigManager
      ConfigManager::handleFileChange();
    }
  }
};

// -----------------------------------------------------------------
// CONFIG DIRECTORY AND PATH GETTERS
// -----------------------------------------------------------------

std::string ConfigManager::getConfigDir()
{
  // Return cached value if already determined
  if (!config_dir_cache_.empty())
  {
    return config_dir_cache_;
  }

  std::vector<std::string> search_paths;

#ifdef _WIN32
  // Windows: %APPDATA%\arceditor
  const char *appdata = std::getenv("APPDATA");
  if (appdata)
  {
    search_paths.push_back(std::string(appdata) + "\\arceditor");
  }
  // Fallback: %USERPROFILE%\.config\arceditor
  const char *userprofile = std::getenv("USERPROFILE");
  if (userprofile)
  {
    search_paths.push_back(std::string(userprofile) + "\\.config\\arceditor");
  }
#else
  // Linux/macOS: XDG_CONFIG_HOME or ~/.config
  const char *xdg_config = std::getenv("XDG_CONFIG_HOME");
  if (xdg_config)
  {
    search_paths.push_back(std::string(xdg_config) + "/arceditor");
  }

  const char *home = std::getenv("HOME");
  if (home)
  {
    search_paths.push_back(std::string(home) + "/.config/arceditor");
  }
#endif

  // Development fallback to use ./config/arceditor
  std::string cwd = fs::current_path().string();
  std::string dev_config_path = cwd + "/.config/arceditor";

  if (fs::exists(dev_config_path) && fs::is_directory(dev_config_path))
  {
    // Insert the local development path as the highest priority
    search_paths.insert(search_paths.begin(), dev_config_path);
  }

  // Find first existing config directory
  for (const auto &path : search_paths)
  {
    if (fs::exists(path) && fs::is_directory(path))
    {
      config_dir_cache_ = path;
      // std::cerr << "Using config directory: " << config_dir_cache_ <<
      // std::endl;
      return config_dir_cache_;
    }
  }

  // If no config dir exists, create one in the standard location (first in
  // search_paths)
  if (!search_paths.empty())
  {
    std::string target_dir = search_paths[0];
    try
    {
      fs::create_directories(target_dir);
      config_dir_cache_ = target_dir;
      std::cerr << "Created config directory: " << config_dir_cache_
                << std::endl;
      return config_dir_cache_;
    }
    catch (const fs::filesystem_error &e)
    {
      std::cerr << "Failed to create config directory: " << e.what()
                << std::endl;
    }
  }

  // Last resort: use current directory
  config_dir_cache_ = cwd;
  std::cerr << "Warning: Using current directory as config dir" << std::endl;
  return config_dir_cache_;
}

std::string ConfigManager::getThemesDir() { return getConfigDir() + "/themes"; }

std::string ConfigManager::getSyntaxRulesDir()
{
  return getConfigDir() + "/syntax_rules";
}

std::string ConfigManager::getConfigFile()
{
  return getConfigDir() + "/config.yaml";
}

bool ConfigManager::ensureConfigStructure()
{
  try
  {
    std::string config_dir = getConfigDir();

    // Create main config directory (already handled in getConfigDir, but safety
    // check)
    if (!fs::exists(config_dir))
    {
      fs::create_directories(config_dir);
    }

    // Create subdirectories
    std::string themes_dir = config_dir + "/themes";
    std::string syntax_dir = config_dir + "/syntax_rules";

    if (!fs::exists(themes_dir))
    {
      fs::create_directories(themes_dir);
      std::cerr << "Created themes directory: " << themes_dir << std::endl;
    }

    if (!fs::exists(syntax_dir))
    {
      fs::create_directories(syntax_dir);
      std::cerr << "Created syntax_rules directory: " << syntax_dir
                << std::endl;
    }

    // Create default config file if it doesn't exist
    std::string config_file = getConfigFile();
    if (!fs::exists(config_file))
    {
      createDefaultConfig(config_file);
    }

    return true;
  }
  catch (const fs::filesystem_error &e)
  {
    std::cerr << "Error ensuring config structure: " << e.what() << std::endl;
    return false;
  }
}

// -----------------------------------------------------------------
// CONFIGURATION (YAML) MANAGEMENT
// -----------------------------------------------------------------

bool ConfigManager::createDefaultConfig(const std::string &config_file)
{
  try
  {
    YAML::Node config;
    config["appearance"]["theme"] = "default";
    config["editor"]["tab_size"] = 4;
    config["editor"]["line_numbers"] = true;
    config["editor"]["cursor_style"] = "auto";
    config["syntax"]["highlighting"] = "viewport"; // Changed to string

    std::ofstream file(config_file);
    if (!file.is_open())
    {
      std::cerr << "Failed to create default config file" << std::endl;
      return false;
    }
    file << "# arceditor Configuration File\n";
    file << "# This file is automatically generated\n\n";
    file << config;
    file.close();

    std::cerr << "Created default config: " << config_file << std::endl;
    return true;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Failed to create config: " << e.what() << std::endl;
    return false;
  }
}

bool ConfigManager::loadConfig()
{
  std::string config_file = getConfigFile();

  if (!fs::exists(config_file))
  {
    std::cerr << "Config file not found, using defaults" << std::endl;
    return false;
  }

  try
  {
    YAML::Node config = YAML::LoadFile(config_file);

    // Load appearance section
    if (config["appearance"] && config["appearance"]["theme"])
    {
      active_theme_ = config["appearance"]["theme"].as<std::string>();
    }

    // Load editor section
    if (config["editor"])
    {
      if (config["editor"]["tab_size"])
      {
        editor_config_.tab_size = config["editor"]["tab_size"].as<int>();
      }
      if (config["editor"]["line_numbers"])
      {
        editor_config_.line_numbers =
            config["editor"]["line_numbers"].as<bool>();
      }
      if (config["editor"]["cursor_style"])
      {
        editor_config_.cursor_style =
            config["editor"]["cursor_style"].as<std::string>();
      }
    }

    // Load syntax section
    if (config["syntax"] && config["syntax"]["highlighting"])
    {
      std::string mode_str = config["syntax"]["highlighting"].as<std::string>();
      syntax_config_.highlighting = parseSyntaxMode(mode_str);
    }

    return true;
  }
  catch (const YAML::Exception &e)
  {
    std::cerr << "Failed to load config: " << e.what() << std::endl;
    return false;
  }
}

bool ConfigManager::saveConfig()
{
  std::string config_file = getConfigFile();
  YAML::Node config;

  try
  {
    // Try to load existing config to preserve comments/structure
    config = YAML::LoadFile(config_file);
  }
  catch (const YAML::BadFile &)
  {
    // File doesn't exist, create new structure
  }

  // Update all sections
  config["appearance"]["theme"] = active_theme_;
  config["editor"]["tab_size"] = editor_config_.tab_size;
  config["editor"]["line_numbers"] = editor_config_.line_numbers;
  config["editor"]["cursor_style"] = editor_config_.cursor_style;
  config["syntax"]["highlighting"] =
      syntaxModeToString(syntax_config_.highlighting);

  try
  {
    std::ofstream file(config_file);
    if (!file.is_open())
    {
      std::cerr << "Failed to open config file for saving" << std::endl;
      return false;
    }
    file << "# arceditor Configuration File\n\n";
    file << config;
    file.close();
    return true;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Failed to save config: " << e.what() << std::endl;
    return false;
  }
}

// -----------------------------------------------------------------
// LIVE RELOAD IMPLEMENTATION (EFSW)
// -----------------------------------------------------------------

void ConfigManager::registerReloadCallback(ConfigReloadCallback callback)
{
  reload_callbacks_.push_back(callback);
}

void ConfigManager::handleFileChange()
{
  std::cerr << "Config file modified. Attempting hot reload..." << std::endl;

  // 1. Re-read the configuration file
  if (!loadConfig())
  {
    std::cerr << "Failed to hot reload configuration. File may be invalid."
              << std::endl;
    return;
  }

  // 2. Notify all subscribed components
  for (const auto &callback : reload_callbacks_)
  {
    // NOTE: In a multi-threaded app, this should be marshaled to the main
    // thread.
    callback();
  }
  reload_pending_.store(true);
  // std::cerr << "Configuration hot reload complete." << std::endl;
}

bool ConfigManager::isReloadPending()
{
  // Atomically check the flag and reset it to false in one operation
  return reload_pending_.exchange(false);
}

bool ConfigManager::startWatchingConfig()
{
  // 1. Check if watching is already active
  if (watcher_instance_ && watcher_listener_)
  {
    return true; // Already watching
  }

  // 2. Instantiate the FileWatcher and Listener
  try
  {
    // The FileWatcher instance is heavy and should be long-lived
    watcher_instance_ = std::make_unique<efsw::FileWatcher>();

    // The listener is the custom class we defined earlier
    watcher_listener_ = std::make_unique<ConfigFileListener>();

    // 3. Get the directory to watch (the config directory)
    std::string configDir = getConfigDir();

    // 4. Add the watch. The 'true' is for recursive watching,
    // but the listener only cares about 'config.yaml' anyway.
    efsw::WatchID watchID = watcher_instance_->addWatch(
        configDir, watcher_listener_.get(), false // false for non-recursive
    );

    if (watchID < 0)
    {
      std::cerr << "Error: EFSW failed to add watch for config directory: "
                << configDir << std::endl;
      // Cleanup pointers if the watch failed
      watcher_instance_.reset();
      watcher_listener_.reset();
      return false;
    }

    // 5. Start the watcher thread
    watcher_instance_->watch(); // This starts the background thread

    // std::cerr << "Config watching started for: " << configDir << std::endl;
    return true;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Fatal Error starting EFSW watcher: " << e.what() << std::endl;
    watcher_instance_.reset();
    watcher_listener_.reset();
    return false;
  }
}

// -----------------------------------------------------------------
// THEME AND SYNTAX MANAGEMENT
// -----------------------------------------------------------------

std::string ConfigManager::getThemeFile(const std::string &theme_name)
{
  std::string themes_dir = getThemesDir();
  std::string theme_file = themes_dir + "/" + theme_name + ".theme";

  if (fs::exists(theme_file))
  {
    return theme_file;
  }

  // Fallback: Check project's "themes" directory for default files
  std::string dev_theme = "themes/" + theme_name + ".theme";
  if (fs::exists(dev_theme))
  {
    return dev_theme;
  }

  std::cerr << "Theme file not found: " << theme_name << std::endl;
  return "";
}

std::string ConfigManager::getSyntaxFile(const std::string &language)
{
  std::string syntax_dir = getSyntaxRulesDir();
  std::string syntax_file = syntax_dir + "/" + language + ".yaml";

  if (fs::exists(syntax_file))
  {
    return syntax_file;
  }

  // Fallback: Check project's "syntax_rules" directory for default files
  std::string dev_syntax = "treesitter/" + language + ".yaml";
  if (fs::exists(dev_syntax))
  {
    return dev_syntax;
  }

  return ""; // Not found
}

std::string ConfigManager::getActiveTheme() { return active_theme_; }

bool ConfigManager::setActiveTheme(const std::string &theme_name)
{
  // Verify theme exists
  std::string theme_file = getThemeFile(theme_name);
  if (theme_file.empty())
  {
    std::cerr << "Cannot set theme, file not found: " << theme_name
              << std::endl;
    return false;
  }

  active_theme_ = theme_name;
  saveConfig(); // Persist the change
  return true;
}

bool ConfigManager::copyProjectFilesToConfig()
{
  try
  {
    std::string config_dir = getConfigDir();

    // Copy themes
    if (fs::exists("themes") && fs::is_directory("themes"))
    {
      std::string target_themes = config_dir + "/themes";
      fs::create_directories(target_themes);

      for (const auto &entry : fs::directory_iterator("themes"))
      {
        if (entry.is_regular_file() && entry.path().extension() == ".theme")
        {
          std::string filename = entry.path().filename().string();
          std::string target = target_themes + "/" + filename;

          // Only copy if doesn't exist (don't overwrite user themes)
          if (!fs::exists(target))
          {
            fs::copy_file(entry.path(), target);
            std::cerr << "Copied theme: " << filename << std::endl;
          }
        }
      }
    }

    // Copy syntax rules
    if (fs::exists("syntax_rules") && fs::is_directory("syntax_rules"))
    {
      std::string target_syntax = config_dir + "/syntax_rules";
      fs::create_directories(target_syntax);

      for (const auto &entry : fs::directory_iterator("syntax_rules"))
      {
        if (entry.is_regular_file() && entry.path().extension() == ".yaml")
        {
          std::string filename = entry.path().filename().string();
          std::string target = target_syntax + "/" + filename;

          // Only copy if doesn't exist
          if (!fs::exists(target))
          {
            fs::copy_file(entry.path(), target);
            std::cerr << "Copied syntax rule: " << filename << std::endl;
          }
        }
      }
    }

    return true;
  }
  catch (const fs::filesystem_error &e)
  {
    std::cerr << "Error copying project files: " << e.what() << std::endl;
    return false;
  }
}

SyntaxMode ConfigManager::parseSyntaxMode(const std::string &mode_str)
{
  std::string lower = mode_str;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower == "none" || lower == "false" || lower == "off")
    return SyntaxMode::NONE;
  else if (lower == "viewport" || lower == "lazy" || lower == "dynamic")
    return SyntaxMode::VIEWPORT;
  else if (lower == "full" || lower == "immediate" || lower == "true")
    return SyntaxMode::FULL;

  std::cerr << "Unknown syntax mode '" << mode_str << "', using viewport"
            << std::endl;
  return SyntaxMode::VIEWPORT;
}

std::string ConfigManager::syntaxModeToString(SyntaxMode mode)
{
  switch (mode)
  {
  case SyntaxMode::NONE:
    return "none";
  case SyntaxMode::VIEWPORT:
    return "viewport";
  case SyntaxMode::FULL:
    return "full";
  default:
    return "viewport";
  }
}

// NEW: Setters
void ConfigManager::setTabSize(int size)
{
  if (size < 1)
    size = 1;
  if (size > 16)
    size = 16;
  editor_config_.tab_size = size;
  saveConfig();
}

void ConfigManager::setLineNumbers(bool enabled)
{
  editor_config_.line_numbers = enabled;
  saveConfig();
}

void ConfigManager::setCursorStyle(const std::string &style)
{
  editor_config_.cursor_style = style;
  saveConfig();
}

void ConfigManager::setSyntaxMode(SyntaxMode mode)
{
  syntax_config_.highlighting = mode;
  saveConfig();
}