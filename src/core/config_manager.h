// ============================================================================
// config_manager.h - Enhanced with Editor Settings
// ============================================================================
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace efsw
{
class FileWatcher;
class FileWatchListener;
} // namespace efsw
using ConfigReloadCallback = std::function<void()>;

// Syntax highlighting modes
enum class SyntaxMode
{
  NONE,     // Completely disabled
  VIEWPORT, // Only highlight visible lines (default, fastest)
  FULL      // Highlight entire file (slower for large files)
};

// Editor configuration structure
struct EditorConfig
{
  int tab_size = 4;
  bool line_numbers = true;
  std::string cursor_style = "auto"; // auto, block, bar, underline
};

// Syntax configuration structure
struct SyntaxConfig
{
  SyntaxMode highlighting = SyntaxMode::VIEWPORT;
};

class ConfigManager
{
public:
  // Existing methods...
  static bool ensureConfigStructure();
  static std::string getConfigDir();
  static std::string getThemesDir();
  static std::string getSyntaxRulesDir();
  static std::string getConfigFile();
  static std::string getThemeFile(const std::string &theme_name);
  static std::string getSyntaxFile(const std::string &language);
  static bool loadConfig();
  static bool saveConfig();
  static std::string getActiveTheme();
  static bool setActiveTheme(const std::string &theme_name);
  static bool copyProjectFilesToConfig();
  static bool startWatchingConfig();
  static void registerReloadCallback(ConfigReloadCallback callback);
  static void handleFileChange();
  static bool isReloadPending();

  // NEW: Configuration getters
  static EditorConfig getEditorConfig() { return editor_config_; }
  static SyntaxConfig getSyntaxConfig() { return syntax_config_; }
  static int getTabSize() { return editor_config_.tab_size; }
  static bool getLineNumbers() { return editor_config_.line_numbers; }
  static std::string getCursorStyle() { return editor_config_.cursor_style; }
  static SyntaxMode getSyntaxMode() { return syntax_config_.highlighting; }

  // NEW: Configuration setters (also saves to file)
  static void setTabSize(int size);
  static void setLineNumbers(bool enabled);
  static void setCursorStyle(const std::string &style);
  static void setSyntaxMode(SyntaxMode mode);

  // NEW: Helper to convert string to SyntaxMode
  static SyntaxMode parseSyntaxMode(const std::string &mode_str);
  static std::string syntaxModeToString(SyntaxMode mode);

private:
  static std::string config_dir_cache_;
  static std::string active_theme_;
  static std::unique_ptr<efsw::FileWatchListener> watcher_listener_;
  static std::unique_ptr<efsw::FileWatcher> watcher_instance_;
  static std::vector<ConfigReloadCallback> reload_callbacks_;
  static std::atomic<bool> reload_pending_;
  static bool createDefaultConfig(const std::string &config_file);

  // NEW: Cached configuration
  static EditorConfig editor_config_;
  static SyntaxConfig syntax_config_;
};
