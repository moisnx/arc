// src/features/query_manager.h
#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef TREE_SITTER_ENABLED
#include "embedded_queries.h"
#endif

namespace fs = std::filesystem;

/**
 * QueryManager - Handles Tree-sitter query loading with embedded-first approach
 *
 * CRITICAL CHANGE: Embedded queries are now PRIMARY, filesystem is for
 * overrides only
 */
class QueryManager
{
public:
  /**
   * Get query content from a path specified in languages.yaml
   *
   * NEW PRIORITY ORDER:
   * 1. Embedded queries (always available)
   * 2. User config override (~/.config/arc/queries)
   * 3. Development override (./runtime/queries) - only if explicitly enabled
   */
  static std::string getQueryFromPath(const std::string &query_path)
  {
    // Extract language and query type from path
    // e.g., "runtime/queries/c/highlights.scm" -> lang="c", type="highlights"
    fs::path p(query_path);
    std::string query_type = p.stem().string();
    std::string lang = p.parent_path().filename().string();

    if (lang.empty() || query_type.empty())
    {
      std::cerr << "❌ Invalid query path format: " << query_path << std::endl;
      return "";
    }

    // if (verbose_)
    // {
    //   std::cerr << "🔍 Loading query: lang=" << lang << ", type=" <<
    //   query_type
    //             << std::endl;
    // }

    // Use standard resolution (embedded-first)
    return getQuery(lang, query_type);
  }

  static void
  loadQueriesFromPathAsync(const std::vector<std::string> &query_paths,
                           std::function<void(const std::string &)> callback)
  {
    std::thread(
        [query_paths, callback]()
        {
          std::string merged = loadQueriesFromPaths(query_paths);

          if (callback)
          {
            callback(merged);
          }
        })
        .detach();
  }

  static void warmupCache(const std::vector<std::string> &languages)
  {
#ifdef TREE_SITTER_ENABLED
    for (const auto &lang : languages)
    {
      std::vector<std::string> types = {"highlights", "indents", "injections"};
      for (const auto &type : types)
      {
        embedded_queries::hasEmbeddedQuery(lang, type);
      }
    }
#endif
  }

  /**
   * Get a query for a specific language and type
   *
   * NEW PRIORITY ORDER:
   * 1. Embedded (PRIMARY - always works)
   * 2. User config override (for customization)
   * 3. Dev override (only if ARCEDITOR_DEV_MODE=1)
   */
  static std::string getQuery(const std::string &lang,
                              const std::string &query_type)
  {
    // 1. EMBEDDED QUERIES (PRIMARY SOURCE)
#ifdef TREE_SITTER_ENABLED
    const char *embedded = embedded_queries::getEmbeddedQuery(lang, query_type);
    if (embedded)
    {
      std::string embedded_content(embedded);

      // Check for user override
      auto user_path = getUserQueryPath(lang, query_type);
      if (fs::exists(user_path))
      {
        if (verbose_)
        {
          std::cerr << "📁 User override found: " << user_path << std::endl;
        }
        return readFile(user_path);
      }

      // Check for dev override ONLY if explicitly enabled
      if (isDevModeEnabled())
      {
        auto dev_path = getDevQueryPath(lang, query_type);
        if (fs::exists(dev_path))
        {
          if (verbose_)
          {
            std::cerr << "🔧 Dev override found: " << dev_path << std::endl;
          }
          return readFile(dev_path);
        }
      }

      // Use embedded version
      // if (verbose_)
      // {
      //   std::cerr << "📦 Using embedded query: " << lang << "/" << query_type
      //             << " (" << embedded_content.length() << " bytes)"
      //             << std::endl;
      // }
      return embedded_content;
    }
#endif

    // 2. FALLBACK: Try filesystem if embedded not available
    if (verbose_)
    {
      std::cerr << "⚠️  No embedded query for " << lang << "/" << query_type
                << ", trying filesystem..." << std::endl;
    }

    // User config
    auto user_path = getUserQueryPath(lang, query_type);
    if (fs::exists(user_path))
    {
      if (verbose_)
      {
        std::cerr << "📁 Loading from user config: " << user_path << std::endl;
      }
      return readFile(user_path);
    }

    // System install
    auto system_path = getSystemQueryPath(lang, query_type);
    if (!system_path.empty() && fs::exists(system_path))
    {
      if (verbose_)
      {
        std::cerr << "📁 Loading from system: " << system_path << std::endl;
      }
      return readFile(system_path);
    }

    // Dev path (last resort)
    auto dev_path = getDevQueryPath(lang, query_type);
    if (fs::exists(dev_path))
    {
      if (verbose_)
      {
        std::cerr << "📁 Loading from dev: " << dev_path << std::endl;
      }
      return readFile(dev_path);
    }

    std::cerr << "❌ Query not found anywhere: " << lang << "/" << query_type
              << std::endl;
    return "";
  }

  /**
   * Load all queries from a list of paths (handles dependencies like C++ -> C)
   */
  static std::string
  loadQueriesFromPaths(const std::vector<std::string> &query_paths)
  {
    std::string merged;

    for (const auto &path : query_paths)
    {
      std::string content = getQueryFromPath(path);

      if (!content.empty())
      {
        if (!merged.empty())
        {
          merged += "\n\n";
        }
        merged += content;
      }
      else
      {
        std::cerr << "⚠️  Warning: Could not load query from: " << path
                  << std::endl;
      }
    }

    return merged;
  }

  /**
   * Get all available query types for a language
   */
  static std::vector<std::string> getAvailableQueries(const std::string &lang)
  {
    std::set<std::string> query_set;

    // Collect from embedded queries FIRST
#ifdef TREE_SITTER_ENABLED
    std::vector<std::string> common_types = {
        "highlights", "indents",     "injections", "locals",
        "tags",       "textobjects", "rainbows"};

    for (const auto &type : common_types)
    {
      if (embedded_queries::hasEmbeddedQuery(lang, type))
      {
        query_set.insert(type);
      }
    }
#endif

    // Add user overrides
    collectQueriesFromDir(getUserQueryDir(lang), query_set);

    // Add system
    auto sys_dir = getSystemQueryDir(lang);
    if (!sys_dir.empty())
    {
      collectQueriesFromDir(sys_dir, query_set);
    }

    // Add dev (if enabled)
    if (isDevModeEnabled())
    {
      collectQueriesFromDir(getDevQueryDir(lang), query_set);
    }

    return std::vector<std::string>(query_set.begin(), query_set.end());
  }

  /**
   * Check if a query exists (embedded or filesystem)
   */
  static bool hasQuery(const std::string &lang, const std::string &query_type)
  {
#ifdef TREE_SITTER_ENABLED
    if (embedded_queries::hasEmbeddedQuery(lang, query_type))
    {
      return true;
    }
#endif
    return !getQuery(lang, query_type).empty();
  }

  /**
   * Enable/disable verbose logging
   */
  static void setVerbose(bool verbose) { verbose_ = verbose; }

  /**
   * Get the search paths being used (for debugging)
   */
  static std::vector<std::string> getSearchPaths(const std::string &lang)
  {
    std::vector<std::string> paths;

    paths.push_back("<embedded>"); // Now first priority!

    auto user = getUserQueryDir(lang);
    if (!user.empty())
      paths.push_back(user.string() + " (override)");

    if (isDevModeEnabled())
    {
      auto dev = getDevQueryDir(lang);
      if (!dev.empty())
        paths.push_back(dev.string() + " (dev override)");
    }

    auto sys = getSystemQueryDir(lang);
    if (!sys.empty())
      paths.push_back(sys.string() + " (system)");

    return paths;
  }

private:
  static inline bool verbose_ = false;

  /**
   * Check if development mode is enabled
   * Set environment variable: export ARCEDITOR_DEV_MODE=1
   */
  static bool isDevModeEnabled()
  {
    static bool checked = false;
    static bool enabled = false;

    if (!checked)
    {
      const char *env = std::getenv("ARCEDITOR_DEV_MODE");
      enabled = (env != nullptr && std::string(env) == "1");
      checked = true;

      if (enabled && verbose_)
      {
        std::cerr << "🔧 Development mode enabled - filesystem overrides active"
                  << std::endl;
      }
    }

    return enabled;
  }

  // User config paths (~/.config/arc/queries/)
  static fs::path getUserQueryPath(const std::string &lang,
                                   const std::string &type)
  {
#ifdef _WIN32
    if (const char *appdata = std::getenv("APPDATA"))
    {
      return fs::path(appdata) / "arc/queries" / lang / (type + ".scm");
    }
    if (const char *userprofile = std::getenv("USERPROFILE"))
    {
      return fs::path(userprofile) / ".config/arc/queries" / lang /
             (type + ".scm");
    }
#else
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"))
    {
      return fs::path(xdg) / "arc/queries" / lang / (type + ".scm");
    }
    if (const char *home = std::getenv("HOME"))
    {
      return fs::path(home) / ".config/arc/queries" / lang / (type + ".scm");
    }
#endif
    return "";
  }

  static fs::path getUserQueryDir(const std::string &lang)
  {
#ifdef _WIN32
    if (const char *appdata = std::getenv("APPDATA"))
    {
      return fs::path(appdata) / "arc/queries" / lang;
    }
    if (const char *userprofile = std::getenv("USERPROFILE"))
    {
      return fs::path(userprofile) / ".config/arc/queries" / lang;
    }
#else
    if (const char *xdg = std::getenv("XDG_CONFIG_HOME"))
    {
      return fs::path(xdg) / "arc/queries" / lang;
    }
    if (const char *home = std::getenv("HOME"))
    {
      return fs::path(home) / ".config/arc/queries" / lang;
    }
#endif
    return "";
  }

  // System install paths
  static fs::path getSystemQueryPath(const std::string &lang,
                                     const std::string &type)
  {
#ifdef _WIN32
    std::vector<fs::path> candidates = {
        fs::path("C:/Program Files/arc/share/queries") / lang / (type + ".scm"),
        fs::path("C:/ProgramData/arc/queries") / lang / (type + ".scm"),
    };
#else
    std::vector<fs::path> candidates = {
        fs::path("/usr/local/share/arc/queries") / lang / (type + ".scm"),
        fs::path("/usr/share/arc/queries") / lang / (type + ".scm"),
    };
#endif

    for (const auto &path : candidates)
    {
      if (fs::exists(path))
        return path;
    }
    return "";
  }

  static fs::path getSystemQueryDir(const std::string &lang)
  {
#ifdef _WIN32
    std::vector<fs::path> candidates = {
        fs::path("C:/Program Files/arc/share/queries") / lang,
        fs::path("C:/ProgramData/arc/queries") / lang,
    };
#else
    std::vector<fs::path> candidates = {
        fs::path("/usr/local/share/arc/queries") / lang,
        fs::path("/usr/share/arc/queries") / lang,
    };
#endif

    for (const auto &path : candidates)
    {
      if (fs::exists(path))
        return path;
    }
    return "";
  }

  // Development paths (only used when ARCEDITOR_DEV_MODE=1)
  static fs::path getDevQueryPath(const std::string &lang,
                                  const std::string &type)
  {
    return fs::path("runtime/queries") / lang / (type + ".scm");
  }

  static fs::path getDevQueryDir(const std::string &lang)
  {
    return fs::path("runtime/queries") / lang;
  }

  // Helper to collect query files from a directory
  static void collectQueriesFromDir(const fs::path &dir,
                                    std::set<std::string> &query_set)
  {
    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
      return;
    }

    try
    {
      for (const auto &entry : fs::directory_iterator(dir))
      {
        if (entry.is_regular_file() && entry.path().extension() == ".scm")
        {
          query_set.insert(entry.path().stem().string());
        }
      }
    }
    catch (const fs::filesystem_error &)
    {
      // Silently ignore errors
    }
  }

  // Helper to read file contents
  static std::string readFile(const fs::path &path)
  {
    try
    {
      std::ifstream file(path);
      if (!file)
      {
        return "";
      }

      std::stringstream buffer;
      buffer << file.rdbuf();
      return buffer.str();
    }
    catch (const std::exception &e)
    {
      std::cerr << "ERROR: Failed to read " << path << ": " << e.what()
                << std::endl;
      return "";
    }
  }
};