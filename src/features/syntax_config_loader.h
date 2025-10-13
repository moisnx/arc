// src/features/syntax_config_loader.h
#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct LanguageConfig
{
  std::string name;
  std::vector<std::string> extensions;
  std::vector<std::string> keywords;
  std::vector<std::string> types;
  std::vector<std::string> builtin_functions;
  std::vector<std::string> operators;
  std::string comment_start;
  std::string comment_end;
  std::string string_delimiters;
  std::string number_pattern;
  std::string parser_name;
  std::string query_file_path;
  std::vector<std::string> queries;
  bool builtin = true;
};

class SyntaxConfigLoader
{
public:
  SyntaxConfigLoader();

  // Load from filesystem
  bool loadFromRegistry(const std::string &registry_path);
  bool loadAllLanguageConfigs(const std::string &config_directory);
  bool loadLanguageConfig(const std::string &language_name,
                          const std::string &config_path);

  // NEW: Load from embedded string
  bool loadFromString(const std::string &yaml_content);

  // Getters
  const LanguageConfig *
  getLanguageConfig(const std::string &language_name) const;
  std::string getLanguageFromExtension(const std::string &extension) const;

  // Debug
  void debugCurrentState() const;

  // Public for reload callback access
  std::map<std::string, std::unique_ptr<LanguageConfig>> language_configs_;
  std::unordered_map<std::string, std::string> extension_to_language_;

private:
  bool parseYamlFile(const std::string &filepath, LanguageConfig &config);
  bool parseRegistryFile(const std::string &filepath);
};