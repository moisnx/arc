// src/features/injection_manager.h
#pragma once

#include "src/features/syntax_config_loader.h"
#include <set>
#ifdef TREE_SITTER_ENABLED

#include "color_span.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <tree_sitter/api.h>
#include <unordered_map>
#include <vector>

struct InjectionLayer
{
  std::string language;
  TSTree *tree = nullptr;
  TSQuery *query = nullptr;
  uint32_t start_row;
  uint32_t end_row;
  uint32_t start_byte;
  uint32_t end_byte;
  std::string content; // Extracted content for this layer

  ~InjectionLayer()
  {
    if (tree)
      ts_tree_delete(tree);
    if (query)
      ts_query_delete(query);
  }
};

class SyntaxHighlighter; // Forward declaration

class InjectionManager
{
public:
  InjectionManager();
  ~InjectionManager();

  // Initialize with parent highlighter
  void initialize(SyntaxHighlighter *parent);

  // Parse injections for a given tree
  void parseInjections(const TSTree *parent_tree,
                       const std::string &parent_language,
                       const std::string &buffer_content);

  // Get highlight spans for a line, considering all injection layers
  std::vector<ColorSpan>
  getHighlightSpansForLine(int line_num, const std::string &line_content) const;

  // Clear all injection layers
  void clearInjections();

  // Check if a line is within an injection
  bool isLineInjected(int line_num) const;
  std::set<int> getAffectedLines() { return affected_lines_; }

private:
  SyntaxHighlighter *parent_highlighter_ = nullptr;
  std::vector<std::unique_ptr<InjectionLayer>> layers_;
  mutable std::mutex injection_mutex_;
  std::set<int> affected_lines_;

  // Cache: line number -> injection layer index
  mutable std::map<int, int> line_to_layer_;

  // Find which injection layer contains a given line
  const InjectionLayer *findLayerForLine(int line_num) const;

  // Create an injection layer from a content node
  void createInjectionLayer(const std::string &lang_name, TSNode content_node,
                            const std::string &buffer_content);

  // Execute a query on an injection layer
  std::vector<ColorSpan>
  executeInjectionQuery(const InjectionLayer *layer, int line_num,
                        const std::string &line_content) const;

  // Helper methods that don't need SyntaxHighlighter to be fully defined
  std::string normalizeLanguageName(const std::string &name) const;
  std::string extractLanguageFromPattern(const std::string &query_str,
                                         uint32_t pattern_index) const;

  // These will call into SyntaxHighlighter, defined in .cpp
  const TSLanguage *getLanguageForInjection(const std::string &lang_name) const;
  TSQuery *getQueryForInjection(const std::string &lang_name,
                                const TSLanguage *ts_lang) const;
  int getColorPairForCapture(const std::string &capture_name) const;
  const LanguageConfig *
  getLanguageConfigFromParent(const std::string &lang_name) const;
};

#endif // TREE_SITTER_ENABLED