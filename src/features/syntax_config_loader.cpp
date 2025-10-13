// src/features/syntax_config_loader.cpp
#include "syntax_config_loader.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <yaml-cpp/yaml.h>

#ifdef TREE_SITTER_ENABLED
#include "embedded_config.h"
#endif

SyntaxConfigLoader::SyntaxConfigLoader() {}

bool SyntaxConfigLoader::loadFromRegistry(const std::string &registry_path)
{
  if (!std::filesystem::exists(registry_path))
  {
    std::cerr << "Registry file does not exist: " << registry_path << std::endl;
    return false;
  }

  return parseRegistryFile(registry_path);
}

bool SyntaxConfigLoader::loadFromString(const std::string &yaml_content)
{
  try
  {
    YAML::Node root = YAML::Load(yaml_content);

    if (!root["languages"])
    {
      std::cerr << "ERROR: No 'languages' section in YAML content" << std::endl;
      return false;
    }

    YAML::Node languages = root["languages"];
    int loaded_count = 0;

    for (auto it = languages.begin(); it != languages.end(); ++it)
    {
      std::string lang_key = it->first.as<std::string>();
      YAML::Node lang_node = it->second;

      auto config = std::make_unique<LanguageConfig>();

      // Parse language configuration
      if (lang_node["name"])
      {
        config->name = lang_node["name"].as<std::string>();
      }
      else
      {
        config->name = lang_key;
      }

      if (config->name.empty())
      {
        config->name = lang_key;
      }

      if (lang_node["builtin"])
        config->builtin = lang_node["builtin"].as<bool>();
      else
        config->builtin = true;

      if (lang_node["parser_name"])
        config->parser_name = lang_node["parser_name"].as<std::string>();

      if (lang_node["query_path"])
        config->query_file_path = lang_node["query_path"].as<std::string>();

      if (lang_node["queries"] && lang_node["queries"].IsSequence())
      {
        for (const auto &query_node : lang_node["queries"])
        {
          std::string query_path = query_node.as<std::string>();
          if (!query_path.empty())
          {
            config->queries.push_back(query_path);
          }
        }
      }

      // Parse extensions
      if (lang_node["extensions"] && lang_node["extensions"].IsSequence())
      {
        for (const auto &ext_node : lang_node["extensions"])
        {
          std::string ext = ext_node.as<std::string>();
          if (!ext.empty())
          {
            config->extensions.push_back(ext);
            extension_to_language_[ext] = config->name;
          }
        }

        if (config->extensions.empty())
        {
          std::cerr << "WARNING: Language '" << lang_key
                    << "' has no valid extensions, skipping" << std::endl;
          continue;
        }
      }
      else
      {
        std::cerr << "WARNING: Language '" << lang_key
                  << "' has no extensions defined, skipping" << std::endl;
        continue;
      }

      // Store configuration
      language_configs_[config->name] = std::move(config);
      loaded_count++;
    }

    // std::cerr << "Successfully loaded " << loaded_count
    //           << " languages from YAML content" << std::endl;
    return loaded_count > 0;
  }
  catch (const YAML::ParserException &e)
  {
    std::cerr << "ERROR: YAML parsing error: " << e.what() << std::endl;
    return false;
  }
  catch (const std::exception &e)
  {
    std::cerr << "ERROR: Exception while parsing YAML: " << e.what()
              << std::endl;
    return false;
  }
}

bool SyntaxConfigLoader::parseRegistryFile(const std::string &filepath)
{
  try
  {
    YAML::Node root = YAML::LoadFile(filepath);

    if (!root["languages"])
    {
      std::cerr << "ERROR: No 'languages' section in registry file"
                << std::endl;
      return false;
    }

    YAML::Node languages = root["languages"];
    int loaded_count = 0;

    for (auto it = languages.begin(); it != languages.end(); ++it)
    {
      std::string lang_key = it->first.as<std::string>();
      YAML::Node lang_node = it->second;

      auto config = std::make_unique<LanguageConfig>();

      if (lang_node["name"])
      {
        config->name = lang_node["name"].as<std::string>();
      }
      else
      {
        config->name = lang_key;
      }

      if (config->name.empty())
      {
        config->name = lang_key;
      }

      if (lang_node["builtin"])
        config->builtin = lang_node["builtin"].as<bool>();
      else
        config->builtin = true;

      if (lang_node["parser_name"])
        config->parser_name = lang_node["parser_name"].as<std::string>();

      if (lang_node["query_path"])
        config->query_file_path = lang_node["query_path"].as<std::string>();

      if (lang_node["queries"] && lang_node["queries"].IsSequence())
      {
        for (const auto &query_node : lang_node["queries"])
        {
          std::string query_path = query_node.as<std::string>();
          if (!query_path.empty())
          {
            config->queries.push_back(query_path);
          }
        }
      }

      if (lang_node["extensions"] && lang_node["extensions"].IsSequence())
      {
        for (const auto &ext_node : lang_node["extensions"])
        {
          std::string ext = ext_node.as<std::string>();
          if (!ext.empty())
          {
            config->extensions.push_back(ext);
            extension_to_language_[ext] = config->name;
          }
        }

        if (config->extensions.empty())
        {
          std::cerr << "WARNING: Language '" << lang_key
                    << "' has no valid extensions, skipping" << std::endl;
          continue;
        }
      }
      else
      {
        std::cerr << "WARNING: Language '" << lang_key
                  << "' has no extensions defined, skipping" << std::endl;
        continue;
      }

      std::string stored_name = config->name;
      language_configs_[config->name] = std::move(config);
      loaded_count++;
    }

    return loaded_count > 0;
  }
  catch (const YAML::BadFile &e)
  {
    std::cerr << "ERROR: Cannot open registry file: " << filepath << " - "
              << e.what() << std::endl;
    return false;
  }
  catch (const YAML::ParserException &e)
  {
    std::cerr << "ERROR: YAML parsing error in registry: " << filepath << ": "
              << e.what() << std::endl;
    return false;
  }
  catch (const std::exception &e)
  {
    std::cerr << "ERROR: Exception while parsing registry " << filepath << ": "
              << e.what() << std::endl;
    return false;
  }
}

bool SyntaxConfigLoader::loadLanguageConfig(const std::string &language_name,
                                            const std::string &config_path)
{
  auto config = std::make_unique<LanguageConfig>();

  if (!parseYamlFile(config_path, *config))
  {
    std::cerr << "ERROR: Failed to parse YAML file: " << config_path
              << std::endl;
    return false;
  }

  std::string actualLanguageName =
      config->name.empty() ? language_name : config->name;

  for (const auto &ext : config->extensions)
  {
    extension_to_language_[ext] = actualLanguageName;
  }

  language_configs_[actualLanguageName] = std::move(config);
  return true;
}

const LanguageConfig *
SyntaxConfigLoader::getLanguageConfig(const std::string &language_name) const
{
  auto it = language_configs_.find(language_name);
  return (it != language_configs_.end()) ? it->second.get() : nullptr;
}

std::string
SyntaxConfigLoader::getLanguageFromExtension(const std::string &extension) const
{
  auto it = extension_to_language_.find(extension);
  return (it != extension_to_language_.end()) ? it->second : "text";
}

void SyntaxConfigLoader::debugCurrentState() const
{
  std::cerr << "\n=== LANGUAGE REGISTRY DEBUG STATE ===" << std::endl;

  for (const auto &pair : language_configs_)
  {
    std::cerr << "  Language: '" << pair.first << "'" << std::endl;
    std::cerr << "    Config name: '" << pair.second->name << "'" << std::endl;
    std::cerr << "    Builtin: " << (pair.second->builtin ? "yes" : "no")
              << std::endl;
    std::cerr << "    Extensions: ";
    for (const auto &ext : pair.second->extensions)
    {
      std::cerr << "'" << ext << "' ";
    }
    std::cerr << std::endl;
    std::cerr << "    Parser name: '" << pair.second->parser_name << "'"
              << std::endl;
    std::cerr << "    Query file: '" << pair.second->query_file_path << "'"
              << std::endl;
  }

  std::cerr << "\nExtension mappings: " << extension_to_language_.size()
            << std::endl;
  for (const auto &pair : extension_to_language_)
  {
    std::cerr << "  '" << pair.first << "' -> '" << pair.second << "'"
              << std::endl;
  }

  std::cerr << "=== END DEBUG STATE ===\n" << std::endl;
}

bool SyntaxConfigLoader::loadAllLanguageConfigs(
    const std::string &config_directory)
{
  // Fallback chain:
  // 1. User config (~/.config/arc/languages.yaml or
  // %APPDATA%/arc/languages.yaml)
  // 2. System config (/usr/local/share/arc/languages.yaml or C:/Program
  // Files/arc/languages.yaml)
  // 3. Development config (./runtime/languages.yaml)
  // 4. Embedded config (compiled into binary)

  std::vector<std::string> registry_paths;

// 1. User config directory
// 1. User config directory
#ifdef _WIN32
  const char *appdata = std::getenv("APPDATA");
  if (appdata)
  {
    registry_paths.push_back(std::string(appdata) + "/arc/languages.yaml");
  }
  const char *userprofile = std::getenv("USERPROFILE");
  if (userprofile)
  {
    registry_paths.push_back(std::string(userprofile) +
                             "/.config/arc/languages.yaml");
  }
#else
  const char *xdg_config = std::getenv("XDG_CONFIG_HOME");
  if (xdg_config)
  {
    registry_paths.push_back(std::string(xdg_config) + "/arc/languages.yaml");
  }
  const char *home = std::getenv("HOME");
  if (home)
  {
    registry_paths.push_back(std::string(home) + "/.config/arc/languages.yaml");
  }
#endif

// 2. System config directory
#ifdef _WIN32
  registry_paths.push_back("C:/Program Files/arc/share/languages.yaml");
  registry_paths.push_back("C:/ProgramData/arc/languages.yaml");
#else
  registry_paths.push_back("/usr/local/share/arc/languages.yaml");
  registry_paths.push_back("/usr/share/arc/languages.yaml");
  registry_paths.push_back("/opt/arc/share/languages.yaml");
#endif

  // 3. Development paths (relative to current directory)
  registry_paths.push_back("runtime/languages.yaml");
  registry_paths.push_back("./runtime/languages.yaml");
  registry_paths.push_back("../runtime/languages.yaml");
  registry_paths.push_back("config/languages.yaml");

  // Try executable-relative paths
  try
  {
    std::filesystem::path exe_path = std::filesystem::current_path();
    registry_paths.push_back(
        (exe_path / "runtime" / "languages.yaml").string());
    registry_paths.push_back(
        (exe_path.parent_path() / "runtime" / "languages.yaml").string());
  }
  catch (...)
  {
  }

  // Try each path in order
  for (const auto &registry_path : registry_paths)
  {
    if (std::filesystem::exists(registry_path))
    {
      // std::cerr << "✓ Loading languages.yaml from: " << registry_path
      //           << std::endl;
      return loadFromRegistry(registry_path);
    }
  }

// 4. Fall back to embedded configuration
#ifdef TREE_SITTER_ENABLED
  if (embedded_config::hasEmbeddedConfig())
  {
    // std::cerr << "✓ Using embedded languages.yaml configuration" <<
    // std::endl;
    return loadFromString(embedded_config::languages_yaml);
  }
#endif

  std::cerr << "ERROR: No languages.yaml found in any location and no embedded "
               "config available"
            << std::endl;
  return false;
}

bool SyntaxConfigLoader::parseYamlFile(const std::string &filepath,
                                       LanguageConfig &config)
{
  try
  {
    YAML::Node root = YAML::LoadFile(filepath);

    if (root["language_info"])
    {
      YAML::Node lang_info = root["language_info"];

      if (lang_info["name"])
        config.name = lang_info["name"].as<std::string>();

      if (lang_info["extensions"])
      {
        for (const auto &ext : lang_info["extensions"])
        {
          config.extensions.push_back(ext.as<std::string>());
        }
      }

      if (lang_info["parser_name"])
        config.parser_name = lang_info["parser_name"].as<std::string>();

      if (lang_info["query_file_path"])
        config.query_file_path = lang_info["query_file_path"].as<std::string>();

      config.builtin = true;
    }
    else
    {
      std::cerr << "ERROR: No language_info section found!" << std::endl;
      return false;
    }

    return true;
  }
  catch (const YAML::BadFile &e)
  {
    std::cerr << "ERROR: Cannot open file: " << filepath << " - " << e.what()
              << std::endl;
    return false;
  }
  catch (const YAML::ParserException &e)
  {
    std::cerr << "ERROR: YAML parsing error in file " << filepath << ": "
              << e.what() << std::endl;
    return false;
  }
  catch (const std::exception &e)
  {
    std::cerr << "ERROR: General exception while parsing " << filepath << ": "
              << e.what() << std::endl;
    return false;
  }
}