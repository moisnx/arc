#ifndef HISTORY_MANAGER_H
#define HISTORY_MANAGER_H

#include <chrono>
#include <memory>
#include <stack>
#include <vector>

// Forward declarations
class GapBuffer;
class SyntaxHighlighter;

// Include the delta definitions
// (Assuming editor_delta.h is in the root or include path)
#include "editor_delta.h"

class HistoryManager
{
public:
  HistoryManager();

  // Lifecycle of an edit group (batching characters)
  void beginDeltaGroup(int initialLineCount, size_t initialBufferSize);
  void addDelta(const EditDelta &delta);
  void commitDeltaGroup();
  bool isCurrentGroupEmpty() const;
  std::chrono::steady_clock::time_point getCurrentGroupTimestamp() const;

  // Core Undo/Redo
  // Returns true if an undo actually occurred
  bool undo(GapBuffer &buffer, int &cursorLine, int &cursorCol,
            int &viewportTop, int &viewportLeft,
            SyntaxHighlighter *highlighter);

  // Returns true if a redo actually occurred
  bool redo(GapBuffer &buffer, int &cursorLine, int &cursorCol,
            int &viewportTop, int &viewportLeft,
            SyntaxHighlighter *highlighter);

  // State Management
  void markModified();
  void markSaved();
  bool hasUnsavedChanges() const { return isModified_; }
  void clear(); // Clear stacks (e.g., on file load)

  // Stats
  size_t getUndoMemoryUsage() const;
  size_t getRedoMemoryUsage() const;

private:
  static const size_t MAX_UNDO_LEVELS = 100;

  std::stack<DeltaGroup> undoStack_;
  std::stack<DeltaGroup> redoStack_;
  DeltaGroup currentDeltaGroup_;

  bool isModified_ = false;
  bool isUndoRedoing_ = false; // Guard to prevent recursive delta creation

  // Internal helpers (Moved from Editor)
  void applyDeltaForward(GapBuffer &buffer, const EditDelta &delta,
                         int &cursorLine, int &cursorCol, int &viewportTop,
                         int &viewportLeft, SyntaxHighlighter *highlighter);

  void applyDeltaReverse(GapBuffer &buffer, const EditDelta &delta,
                         int &cursorLine, int &cursorCol, int &viewportTop,
                         int &viewportLeft, SyntaxHighlighter *highlighter);

  void notifyTreeSitterEdit(const EditDelta &delta, bool isReverse,
                            GapBuffer &buffer, SyntaxHighlighter *highlighter);

  // Helper to validate viewport after moves
  void validateCursorAndViewport(const GapBuffer &buffer, int &cursorLine,
                                 int &cursorCol, int &viewportTop,
                                 int &viewportLeft, int viewportHeight);
};

#endif // HISTORY_MANAGER_H