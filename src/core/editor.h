#ifndef EDITOR_H
#define EDITOR_H

#include <stack>
#include <string>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

#include "buffer.h"
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

class Editor
{
public:
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

private:
  // Core data
  GapBuffer buffer;
  std::string filename;
  SyntaxHighlighter *syntaxHighlighter;
  bool isSaving = false;

  // Viewport and cursor
  int viewportTop = 0;
  int viewportLeft = 0;
  int viewportHeight;
  int cursorLine = 0;
  int cursorCol = 0;

  // Clipboard
  std::string clipboard;

  // Undo/Redo
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
};

#endif // EDITOR_H