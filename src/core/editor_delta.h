// editor_delta.h - New file for delta-based undo/redo
#ifndef EDITOR_DELTA_H
#define EDITOR_DELTA_H

#include <chrono>
#include <string>
#include <vector>

// Represents a single atomic edit operation
struct EditDelta
{
  // Operation types
  enum OpType
  {
    INSERT_CHAR, // Single character insertion
    DELETE_CHAR, // Single character deletion
    INSERT_TEXT, // Multi-character insertion (paste, etc)
    DELETE_TEXT, // Multi-character deletion
    SPLIT_LINE,  // Enter/newline - splits one line into two
    JOIN_LINES,  // Backspace at line start - joins two lines
    REPLACE_LINE // Entire line replacement (e.g., replaceLine call)
  };

  OpType operation;

  // === Position Information ===
  // Where the edit occurred (line/col BEFORE the edit)
  int startLine;
  int startCol;

  // For multi-line operations, where it ended
  int endLine;
  int endCol;

  // === Content ===
  // Text that was deleted (needed to undo insertion)
  std::string deletedContent;

  // Text that was inserted (needed to redo/undo deletion)
  std::string insertedContent;

  // For SPLIT_LINE: content of line before split
  std::string lineBeforeSplit;

  // For JOIN_LINES: content of both lines before join
  std::string firstLineBeforeJoin;
  std::string secondLineBeforeJoin;

  // === Cursor State ===
  // Where cursor was BEFORE edit
  int preCursorLine;
  int preCursorCol;

  // Where cursor is AFTER edit
  int postCursorLine;
  int postCursorCol;

  // === Structural Changes ===
  // How many lines were added (positive) or removed (negative)
  int lineCountDelta;

  // === Viewport State ===
  int preViewportTop;
  int preViewportLeft;
  int postViewportTop;
  int postViewportLeft;

  // === Metadata ===
  std::chrono::steady_clock::time_point timestamp;

  // Constructor
  EditDelta()
      : operation(INSERT_CHAR), startLine(0), startCol(0), endLine(0),
        endCol(0), preCursorLine(0), preCursorCol(0), postCursorLine(0),
        postCursorCol(0), lineCountDelta(0), preViewportTop(0),
        preViewportLeft(0), postViewportTop(0), postViewportLeft(0),
        timestamp(std::chrono::steady_clock::now())
  {
  }

  // Helper: Get memory size for debugging
  size_t getMemorySize() const
  {
    return sizeof(*this) + deletedContent.capacity() +
           insertedContent.capacity() + lineBeforeSplit.capacity() +
           firstLineBeforeJoin.capacity() + secondLineBeforeJoin.capacity();
  }

  // Debug string
  std::string toString() const
  {
    std::ostringstream oss;
    oss << "Op: ";
    switch (operation)
    {
    case INSERT_CHAR:
      oss << "INSERT_CHAR";
      break;
    case DELETE_CHAR:
      oss << "DELETE_CHAR";
      break;
    case INSERT_TEXT:
      oss << "INSERT_TEXT";
      break;
    case DELETE_TEXT:
      oss << "DELETE_TEXT";
      break;
    case SPLIT_LINE:
      oss << "SPLIT_LINE";
      break;
    case JOIN_LINES:
      oss << "JOIN_LINES";
      break;
    case REPLACE_LINE:
      oss << "REPLACE_LINE";
      break;
    }
    oss << " | Pos: (" << startLine << "," << startCol << ")";
    oss << " | Cursor: (" << preCursorLine << "," << preCursorCol << ") -> ("
        << postCursorLine << "," << postCursorCol << ")";
    oss << " | Lines: " << (lineCountDelta >= 0 ? "+" : "") << lineCountDelta;
    return oss.str();
  }
};

// A compound state that groups related deltas together
// This is what gets pushed to undo/redo stacks
struct DeltaGroup
{
  std::vector<EditDelta> deltas;
  std::chrono::steady_clock::time_point timestamp;

  // Initial state (for safety/validation)
  int initialLineCount;
  size_t initialBufferSize;

  DeltaGroup()
      : timestamp(std::chrono::steady_clock::now()), initialLineCount(0),
        initialBufferSize(0)
  {
  }

  void addDelta(const EditDelta &delta) { deltas.push_back(delta); }

  bool isEmpty() const { return deltas.empty(); }

  size_t getMemorySize() const
  {
    size_t total = sizeof(*this);
    for (const auto &delta : deltas)
    {
      total += delta.getMemorySize();
    }
    return total;
  }

  std::string toString() const
  {
    std::ostringstream oss;
    oss << "DeltaGroup: " << deltas.size() << " delta(s), " << getMemorySize()
        << " bytes\n";
    for (size_t i = 0; i < deltas.size(); ++i)
    {
      oss << "  [" << i << "] " << deltas[i].toString() << "\n";
    }
    return oss.str();
  }
};

#endif // EDITOR_DELTA_H