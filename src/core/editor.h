#ifndef EDITOR_H
#define EDITOR_H

#include <stack>
#include <string>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

#include "buffer.h"
#include "editor_delta.h"
#include "editor_validation.h"
#include "src/features/syntax_highlighter.h"

// Undo/Redo system
struct EditorState
{
  std::string content;
  int cursorLine;
  int cursorCol;
  int viewportTop;
  int viewportLeft;
};

enum CursorMode
{
  NORMAL,
  INSERT,
  VISUAL
};

class Editor
{
public:
  struct RenderSpan
  {
    int start;       // Screen column start
    int end;         // Screen column end
    int colorPair;   // Color pair to use
    int attribute;   // Attribute flags
    bool isSelected; // Whether this span is selected
  };

  std::vector<RenderSpan>
  buildRenderSpans(const std::string &line,
                   const std::vector<ColorSpan> &highlightSpans,
                   bool lineHasSelection, int sel_start_line, int sel_end_line,
                   int sel_start_col, int sel_end_col, int currentLine,
                   int viewportLeft, int contentWidth);

  // Core API
  Editor(SyntaxHighlighter *highlighter);
  void setSyntaxHighlighter(SyntaxHighlighter *highlighter);
  bool loadFile(const std::string &fname);
  bool saveFile();
  void display();
  void drawStatusBar();
  void handleResize();
  void handleMouse(MEVENT &event);

  std::string getFilename() const { return filename; }
  std::string getFirstLine() const { return buffer.getLine(0); }
  GapBuffer getBuffer() { return buffer; }

  // Movement
  void moveCursorUp();
  void moveCursorDown();
  void moveCursorLeft();
  void moveCursorRight();
  void pageUp();
  void pageDown();
  void moveCursorToLineStart();
  void moveCursorToLineEnd();
  void scrollUp(int lines = 3);
  void scrollDown(int lines = 3);
  void validateCursorAndViewport();
  void positionCursor();

  // Text editing
  void insertChar(char ch);
  void insertNewline();
  void deleteChar();
  void backspace();
  void deleteLine();

  // Selection management
  void clearSelection();
  void startSelectionIfNeeded();
  void updateSelectionEnd();
  void deleteSelection();
  std::string getSelectedText();

  // Clipboard operations
  void copySelection();
  void cutSelection();
  void pasteFromClipboard();
  void selectAll();

  // Undo/Redo

  void saveState();
  void undo();
  void redo();

  // Utility
  bool hasUnsavedChanges() const { return isModified; }
  void reloadConfig();
  void initializeViewportHighlighting();
  void updateSyntaxHighlighting();

  // Debug
  void debugPrintState(const std::string &context);
  bool validateEditorState();

  // Selection state
  int selectionStartLine = 0;
  int selectionStartCol = 0;
  int selectionEndLine = 0;
  int selectionEndCol = 0;
  bool hasSelection = false;
  bool isSelecting = false;

  // Editor cursor
  void setCursorMode();
  CursorMode getCursorMode() const { return currentMode; };

  // Validation
  EditorSnapshot captureSnapshot() const;
  ValidationResult validateState(const std::string &context) const;
  std::string compareSnapshots(const EditorSnapshot &before,
                               const EditorSnapshot &after) const;

  // Delta undo configuration
  void beginDeltaGroup();
  void setDeltaUndoEnabled(bool enabled) { useDeltaUndo_ = enabled; }
  bool isDeltaUndoEnabled() const { return useDeltaUndo_; }

  // Debug/stats
  size_t getUndoMemoryUsage() const;
  size_t getRedoMemoryUsage() const;

private:
  // Core data
  GapBuffer buffer;
  std::string filename;
  SyntaxHighlighter *syntaxHighlighter;
  bool isSaving = false;

  // Delta
  bool useDeltaUndo_ = false; // Feature flag - start disabled!
  std::stack<DeltaGroup> deltaUndoStack_;
  std::stack<DeltaGroup> deltaRedoStack_;
  DeltaGroup currentDeltaGroup_; // Accumulates deltas for grouping

  // Delta operations
  void addDelta(const EditDelta &delta);
  void commitDeltaGroup();
  void applyDeltaForward(const EditDelta &delta);
  void applyDeltaReverse(const EditDelta &delta);
  // Helper to create delta from current operation
  EditDelta createDeltaForInsertChar(char ch);
  EditDelta createDeltaForDeleteChar();
  EditDelta createDeltaForBackspace();
  EditDelta createDeltaForNewline();
  EditDelta createDeltaForDeleteSelection();
  // Viewport and cursor
  int viewportTop = 0;
  int viewportLeft = 0;
  int viewportHeight;
  int cursorLine = 0;
  int cursorCol = 0;

  // Clipboard
  std::string clipboard;

  // Undo/Redo
  std::chrono::steady_clock::time_point lastEditTime;
  static constexpr int UNDO_GROUP_TIMEOUT_MS = 2000;
  bool isUndoRedoing = false; // Add this flag
  std::stack<EditorState> undoStack;
  std::stack<EditorState> redoStack;
  static const size_t MAX_UNDO_LEVELS = 100;

  // File state
  bool isModified = false;
  int tabSize = 4;

  // Private helpers
  std::string expandTabs(const std::string &line, int tabSize = 4);
  std::string getFileExtension();
  bool isPositionSelected(int line, int col);
  bool mouseToFilePos(int mouseRow, int mouseCol, int &fileRow, int &fileCol);
  void updateCursorAndViewport(int newLine, int newCol);
  void markModified();
  void splitLineAtCursor();
  void joinLineWithNext();
  EditorState getCurrentState();
  void restoreState(const EditorState &state);
  void limitUndoStack();
  std::pair<std::pair<int, int>, std::pair<int, int>> getNormalizedSelection();

  void notifyTreeSitterEdit(const EditDelta &delta, bool isReverse);
  void optimizedLineInvalidation(int startLine, int endLine);

  // Cursor Style
  CursorMode currentMode = NORMAL;
};

#endif // EDITOR_H