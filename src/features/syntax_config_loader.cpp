// src/features/syntax_config_loader.cpp
#include "syntax_config_loader.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <yaml-cpp/yaml.h>

SyntaxConfigLoader::SyntaxConfigLoader() {}

bool SyntaxConfigLoader::loadFromRegistry(const std::string &registry_path)
{
  // std::cerr << "Loading language registry from: " << registry_path <<
  // std::endl;

  if (!std::filesystem::exists(registry_path))
  {
    std::cerr << "ERROR: Registry file does not exist: " << registry_path
              << std::endl;
    return false;
  }

  return parseRegistryFile(registry_path);
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

      // Parse language configuration
      if (lang_node["name"])
      {
        config->name = lang_node["name"].as<std::string>();
      }
      else
      {
        config->name = lang_key; // Use key as fallback
      }

      // Ensure name is never empty
      if (config->name.empty())
      {
        std::cerr << "WARNING: Empty name for language '" << lang_key
                  << "', using key as name" << std::endl;
        config->name = lang_key;
      }

      if (lang_node["builtin"])
        config->builtin = lang_node["builtin"].as<bool>();
      else
        config->builtin = true; // Default to builtin

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

      // Parse extensions (CRITICAL: Must have at least one extension)
      if (lang_node["extensions"] && lang_node["extensions"].IsSequence())
      {
        for (const auto &ext_node : lang_node["extensions"])
        {
          std::string ext = ext_node.as<std::string>();
          if (!ext.empty())
          {
            config->extensions.push_back(ext);
            // Map extension to language name
            extension_to_language_[ext] = config->name;
          }
        }

        if (config->extensions.empty())
        {
          std::cerr << "WARNING: Language '" << lang_key
                    << "' has no valid extensions, skipping" << std::endl;
          continue; // Skip this language
        }
      }
      else
      {
        std::cerr << "WARNING: Language '" << lang_key
                  << "' has no extensions defined, skipping" << std::endl;
        continue; // Skip this language
      }

      // Store configuration
      std::string stored_name = config->name; // Save for logging
      language_configs_[config->name] = std::move(config);
      loaded_count++;

      // std::cerr << "✓ Loaded: " << lang_key << " -> \"" << stored_name
      //           << "\" with "
      //           << language_configs_[stored_name]->extensions.size()
      //           << " extensions" << std::endl;
    }

    // std::cerr << "Successfully loaded " << loaded_count
    //           << " languages from registry" << std::endl;
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
  std::string result =
      (it != extension_to_language_.end()) ? it->second : "text";
  return result;
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
  // DEPRECATED: Check if registry.yaml exists first
  std::string registry_path = "runtime/languages.yaml";

  // std::cerr << "Registry path: " << registry_path << std::endl;

  if (std::filesystem::exists(registry_path))
  {
    // std::cerr << "Found registry.yaml, using unified registry loading"
    //           << std::endl;
    return loadFromRegistry(registry_path);
  }

  // Legacy fallback: Load individual YAML files
  std::cerr << "No registry.yaml found, falling back to individual file loading"
            << std::endl;

  try
  {
    if (!std::filesystem::exists(config_directory))
    {
      std::cerr << "ERROR: Config directory does not exist!" << std::endl;
      return false;
    }

    if (!std::filesystem::is_directory(config_directory))
    {
      std::cerr << "ERROR: Path is not a directory!" << std::endl;
      return false;
    }

    int success_count = 0;

    for (const auto &entry :
         std::filesystem::directory_iterator(config_directory))
    {
      if (entry.is_regular_file() && entry.path().extension() == ".yaml")
      {
        std::string language_name = entry.path().stem().string();
        std::string config_path = entry.path().string();

        if (loadLanguageConfig(language_name, config_path))
        {
          success_count++;
        }
        else
        {
          std::cerr << "ERROR: Failed to load config for language: "
                    << language_name << std::endl;
          return false;
        }
      }
    }

    return success_count > 0;
  }
  catch (const std::filesystem::filesystem_error &ex)
  {
    std::cerr << "Filesystem error: " << ex.what() << std::endl;
    return false;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "General error: " << ex.what() << std::endl;
    return false;
  }
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

      // Set builtin to true for legacy configs
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