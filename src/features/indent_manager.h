#pragma once

#ifdef TREE_SITTER_ENABLED

#include "query_manager.h"
#include "src/core/buffer.h"
#include <iostream>
#include <string>
#include <tree_sitter/api.h>
#include <unordered_map>
#include <vector>

/**
 * IndentManager - Calculates indentation using Tree-sitter indent queries
 *
 * Supports standard captures:
 *   @indent.begin    - Increase indent after this node
 *   @indent.end      - Decrease indent for this node
 *   @indent.align    - Align with parent node
 *   @indent.dedent   - Dedent this line immediately
 *   @indent.branch   - Conditional indent (used with predicates)
 */
class IndentManager
{
public:
  IndentManager()
      : tree_(nullptr), query_(nullptr), language_(nullptr), tab_size_(4),
        debug_(false)
  {
  }

  ~IndentManager() { cleanup(); }

  void setTabSize(int size) { tab_size_ = size; }
  void setDebugMode(bool enable) { debug_ = enable; }

  /**
   * Set the language and load indent queries
   */
  bool setLanguage(const std::string &lang, const TSLanguage *ts_language)
  {
    cleanup();

    language_ = ts_language;
    language_name_ = lang;

    // Load indent query from embedded or filesystem
    std::string query_src = QueryManager::getQuery(lang, "indents");

    if (query_src.empty())
    {
      if (debug_)
      {
        std::cerr << "No indent query found for " << lang << ", using fallback"
                  << std::endl;
      }
      // No indent query available - use fallback heuristics
      return false;
    }

    // Compile query
    uint32_t error_offset;
    TSQueryError error_type;
    query_ = ts_query_new(language_, query_src.c_str(), query_src.length(),
                          &error_offset, &error_type);

    if (!query_)
    {
      std::cerr << "Failed to compile indent query for " << lang
                << " at offset " << error_offset
                << ", error type: " << error_type << std::endl;
      return false;
    }

    // Cache capture indices
    cacheCaptures();

    if (debug_)
    {
      std::cerr << "Indent query loaded for " << lang << std::endl;
      std::cerr << "  indent.begin: "
                << (indent_begin_idx_ != UINT32_MAX ? "yes" : "no")
                << std::endl;
      std::cerr << "  indent.end: "
                << (indent_end_idx_ != UINT32_MAX ? "yes" : "no") << std::endl;
      std::cerr << "  indent.dedent: "
                << (indent_dedent_idx_ != UINT32_MAX ? "yes" : "no")
                << std::endl;
    }

    return true;
  }

  /**
   * Calculate indent for a new line after lineNum
   * Called when user presses Enter
   */
  int calculateIndentAfterLine(int lineNum, const GapBuffer &buffer,
                               TSTree *tree)
  {
    if (!enabled_)
    {
      return 0;
    }

    if (!tree)
    {
      return fallbackIndent(lineNum, buffer);
    }

    tree_ = tree;

    // Get the current line content
    std::string current_line = buffer.getLine(lineNum);
    int base_indent = getLineIndent(lineNum, buffer);

    if (debug_)
    {
      std::cerr << "\n=== calculateIndentAfterLine ===" << std::endl;
      std::cerr << "Line " << lineNum << ": '" << current_line << "'"
                << std::endl;
      std::cerr << "Base indent: " << base_indent << std::endl;
    }

    // If no query, use fallback
    if (!query_)
    {
      return fallbackIndent(lineNum, buffer);
    }

    // Get byte position at end of line
    uint32_t line_end_col = current_line.length();
    uint32_t byte_offset = buffer.lineColToPos(lineNum, line_end_col);

    // Get root node
    TSNode root = ts_tree_root_node(tree);

    // Find node at end of current line
    TSNode node =
        ts_node_descendant_for_byte_range(root, byte_offset, byte_offset);

    if (ts_node_is_null(node))
    {
      if (debug_)
        std::cerr << "Node is null, using fallback" << std::endl;
      return fallbackIndent(lineNum, buffer);
    }

    // Calculate indent by checking all parent nodes
    int indent_level = 0;
    TSNode current = node;

    while (!ts_node_is_null(current))
    {
      int node_delta = getIndentDeltaForNode(current, buffer, lineNum, true);
      indent_level += node_delta;

      if (debug_ && node_delta != 0)
      {
        const char *type = ts_node_type(current);
        std::cerr << "  Node: " << type << " -> delta: " << node_delta
                  << " (total: " << indent_level << ")" << std::endl;
      }

      current = ts_node_parent(current);
    }

    int final_indent = std::max(0, indent_level * tab_size_);

    if (debug_)
    {
      std::cerr << "Indent level: " << indent_level
                << " -> spaces: " << final_indent << std::endl;
    }

    return final_indent;
  }

  /**
   * Calculate dedent amount for a closing bracket
   * Returns number of spaces to remove
   */
  int calculateDedentAmount(int lineNum, const GapBuffer &buffer)
  {
    if (!enabled_)
    {
      return 0;
    }

    if (!query_ || !tree_)
    {
      return fallbackDedentAmount(lineNum, buffer);
    }

    // Get current line indent
    int current_indent = getLineIndent(lineNum, buffer);

    // Get byte position at start of line (after any whitespace)
    std::string line = buffer.getLine(lineNum);
    int first_non_space = 0;
    while (first_non_space < line.length() &&
           (line[first_non_space] == ' ' || line[first_non_space] == '\t'))
    {
      first_non_space++;
    }

    uint32_t byte_offset = buffer.lineColToPos(lineNum, first_non_space);
    TSNode root = ts_tree_root_node(tree_);
    TSNode node =
        ts_node_descendant_for_byte_range(root, byte_offset, byte_offset);

    if (ts_node_is_null(node))
    {
      return fallbackDedentAmount(lineNum, buffer);
    }

    // Calculate what the indent SHOULD be by querying indent rules
    int target_indent_level = 0;
    TSNode current = node;
    TSNode parent = ts_node_parent(current);

    while (!ts_node_is_null(parent))
    {
      int delta = getIndentDeltaForNode(parent, buffer, lineNum, false);
      target_indent_level += delta;

      // For Python and similar, check if parent is a block that should dedent
      const char *parent_type = ts_node_type(parent);
      std::string type_str(parent_type);

      // Stop at the containing block
      if (type_str.find("block") != std::string::npos ||
          type_str.find("body") != std::string::npos ||
          type_str.find("suite") != std::string::npos) // Python uses "suite"
      {
        break;
      }

      current = parent;
      parent = ts_node_parent(current);
    }

    int target_indent = std::max(0, target_indent_level * tab_size_);

    if (debug_)
    {
      std::cerr << "\n=== calculateDedentAmount ===" << std::endl;
      std::cerr << "Current indent: " << current_indent << std::endl;
      std::cerr << "Target indent: " << target_indent << std::endl;
    }

    // Return how much to remove
    return std::max(0, current_indent - target_indent);
  }

  /**
   * Check if character should trigger auto-dedent (e.g., '}' in C)
   */
  bool shouldDedentOnChar(char ch)
  {
    // Common dedent triggers
    return ch == '}' || ch == ']' || ch == ')';
  }

  void setEnabled(bool enable) { enabled_ = enable; }
  bool isEnabled() const { return enabled_; }

private:
  TSTree *tree_;
  TSQuery *query_;
  const TSLanguage *language_;
  std::string language_name_;
  int tab_size_;
  bool debug_;
  bool enabled_ = true;

  // Cached capture indices
  uint32_t indent_begin_idx_ = UINT32_MAX;
  uint32_t indent_end_idx_ = UINT32_MAX;
  uint32_t indent_align_idx_ = UINT32_MAX;
  uint32_t indent_dedent_idx_ = UINT32_MAX;
  uint32_t indent_branch_idx_ = UINT32_MAX;

  void cleanup()
  {
    if (query_)
    {
      ts_query_delete(query_);
      query_ = nullptr;
    }
    tree_ = nullptr;
    language_ = nullptr;
  }

  void cacheCaptures()
  {
    if (!query_)
      return;

    uint32_t count = ts_query_capture_count(query_);

    for (uint32_t i = 0; i < count; ++i)
    {
      uint32_t len;
      const char *name = ts_query_capture_name_for_id(query_, i, &len);
      std::string capture_name(name, len);

      if (capture_name == "indent.begin" || capture_name == "indent")
      {
        indent_begin_idx_ = i;
      }
      else if (capture_name == "indent.end")
      {
        indent_end_idx_ = i;
      }
      else if (capture_name == "indent.align")
      {
        indent_align_idx_ = i;
      }
      else if (capture_name == "indent.dedent")
      {
        indent_dedent_idx_ = i;
      }
      else if (capture_name == "indent.branch")
      {
        indent_branch_idx_ = i;
      }
    }
  }

  /**
   * Get indent delta for a specific node
   * after_newline: true when calculating indent for new line, false for dedent
   */
  int getIndentDeltaForNode(TSNode node, const GapBuffer &buffer,
                            int current_line, bool after_newline)
  {
    if (!query_)
      return 0;

    TSQueryCursor *cursor = ts_query_cursor_new();
    ts_query_cursor_exec(cursor, query_, node);

    int delta = 0;
    TSQueryMatch match;

    // Get node position
    TSPoint start_point = ts_node_start_point(node);
    TSPoint end_point = ts_node_end_point(node);
    int node_start_line = start_point.row;
    int node_end_line = end_point.row;

    while (ts_query_cursor_next_match(cursor, &match))
    {
      for (uint16_t i = 0; i < match.capture_count; ++i)
      {
        TSQueryCapture cap = match.captures[i];
        TSNode cap_node = cap.node;
        TSPoint cap_start = ts_node_start_point(cap_node);

        if (cap.index == indent_begin_idx_)
        {
          // Only apply if the node is before or on current line
          if (node_end_line <= current_line)
          {
            delta += 1;
          }
        }
        else if (cap.index == indent_end_idx_)
        {
          // Apply dedent if:
          // 1. We're calculating dedent (not after_newline) and node is on
          // current line
          // 2. We're after newline and node ends before current line
          if (!after_newline && node_start_line <= current_line)
          {
            delta -= 1;
          }
          else if (after_newline && node_end_line < current_line)
          {
            delta -= 1;
          }
        }
        else if (cap.index == indent_dedent_idx_)
        {
          // Immediate dedent for this line
          if (cap_start.row == current_line)
          {
            delta -= 1;
          }
        }
      }
    }

    ts_query_cursor_delete(cursor);
    return delta;
  }

  /**
   * Get current indentation of a line (count leading spaces/tabs)
   */
  int getLineIndent(int lineNum, const GapBuffer &buffer)
  {
    if (lineNum < 0 || lineNum >= buffer.getLineCount())
    {
      return 0;
    }

    std::string line = buffer.getLine(lineNum);
    int indent = 0;

    for (char ch : line)
    {
      if (ch == ' ')
      {
        indent++;
      }
      else if (ch == '\t')
      {
        indent += tab_size_;
      }
      else
      {
        break;
      }
    }

    return indent;
  }

  /**
   * Fallback indent when no query available (simple heuristic)
   */
  int fallbackIndent(int lineNum, const GapBuffer &buffer)
  {
    if (lineNum < 0 || lineNum >= buffer.getLineCount())
      return 0;

    std::string line = buffer.getLine(lineNum);
    int current_indent = getLineIndent(lineNum, buffer);

    // Remove trailing whitespace
    std::string trimmed = line;
    size_t end = trimmed.find_last_not_of(" \t\n\r");
    if (end != std::string::npos)
    {
      trimmed = trimmed.substr(0, end + 1);
    }

    if (!trimmed.empty())
    {
      char last = trimmed.back();

      // Increase indent after opening brackets or colon (Python)
      if (last == '{' || last == '[' || last == '(' || last == ':')
      {
        return current_indent + tab_size_;
      }

      // Decrease indent for closing brackets at start of line
      if ((last == '}' || last == ']' || last == ')') &&
          trimmed.find_first_not_of(" \t") == trimmed.length() - 1)
      {
        return std::max(0, current_indent - tab_size_);
      }
    }

    return current_indent;
  }

  /**
   * Fallback dedent amount (simple heuristic)
   */
  int fallbackDedentAmount(int lineNum, const GapBuffer &buffer)
  {
    std::string line = buffer.getLine(lineNum);

    // Find first non-whitespace char
    size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos)
      return 0;

    char first_char = line[first];

    // If line starts with closing bracket, dedent by one level
    if (first_char == '}' || first_char == ']' || first_char == ')')
    {
      int current_indent = getLineIndent(lineNum, buffer);
      return std::min(tab_size_, current_indent);
    }

    return 0;
  }
};

#endif // TREE_SITTER_ENABLED
