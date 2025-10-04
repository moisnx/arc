#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Language configuration structure
struct LanguageConfig
{
  std::string name;
  std::vector<std::string> extensions;
  bool builtin; // NEW: Is this a built-in language?

  // Tree-sitter specific configuration
  std::string parser_name;     // e.g., "python" -> maps to tree_sitter_python()
  std::string query_file_path; // e.g., "syntax_rules/python.scm"
  std::vector<std::string>
      queries; // e.g., "javascript" -> queries/ecma/highlights.scm,
               // queries/javascript/highlights.scm
};

class SyntaxConfigLoader
{
public:
  SyntaxConfigLoader();

  // NEW: Load from unified registry
  bool loadFromRegistry(const std::string &registry_path);

  // Legacy: Load individual language config (for backward compatibility)
  bool loadLanguageConfig(const std::string &language_name,
                          const std::string &config_path);

  // Get loaded language configuration
  const LanguageConfig *
  getLanguageConfig(const std::string &language_name) const;

  // Get language name from file extension
  std::string getLanguageFromExtension(const std::string &extension) const;

  // Load all language configurations from a directory (DEPRECATED - use
  // loadFromRegistry)
  bool loadAllLanguageConfigs(const std::string &config_directory);

  // Debug functionality
  void debugCurrentState() const;

  // Public for direct access (needed by syntax_highlighter)
  std::unordered_map<std::string, std::unique_ptr<LanguageConfig>>
      language_configs_;
  std::unordered_map<std::string, std::string> extension_to_language_;

private:
  // Parse YAML registry file
  bool parseRegistryFile(const std::string &filepath);

  // Simplified YAML parsing for individual configs (legacy)
  bool parseYamlFile(const std::string &filepath, LanguageConfig &config);
};