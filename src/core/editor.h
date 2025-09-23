#ifndef EDITOR_H
#define EDITOR_H

// Core C++ Libraries
#include <iostream>
#include <stack>
#include <string>
#include <vector>

#ifdef _WIN32
#include <curses.h> // pdcurses on Windows
#else
#include <ncurses.h> // ncurses on Unix-like systems
#endif

// Local Project Headers
#include "gap_buffer.h"
#include "src/features/syntax_highlighter.h"

// Editor modes
enum EditorMode
{
  NORMAL, // Navigate, select, commands
  INSERT, // Text editing
  VISUAL  // Selection mode
};

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
  // Public API
  Editor(SyntaxHighlighter *highlighter);
  void setSyntaxHighlighter(SyntaxHighlighter *highlighter);
  bool loadFile(const std::string &fname);
  bool saveFile(const std::string &fname = "");
  void display();
  void drawStatusBar();
  void handleResize();
  void handleMouse(MEVENT &event);
  void clearSelection();

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

  // Mode management
  void setMode(EditorMode newMode);
  EditorMode getMode() const { return currentMode; }

  // Text editing
  void insertChar(char ch);
  void insertNewline();
  void deleteChar(); // Delete character at cursor (Delete key)
  void backspace();  // Delete character before cursor (Backspace key)
  void deleteLine();
  void deleteSelection();

  // Undo/Redo
  void saveState();
  void undo();
  void redo();

  // Utility
  bool hasUnsavedChanges() const { return isModified; }

private:
  // Core Data
  GapBuffer buffer;
  std::string filename;
  SyntaxHighlighter *syntaxHighlighter;

  // Mode system
  EditorMode currentMode = EditorMode::NORMAL;

  // Viewport and Cursor State
  int viewportTop = 0;
  int viewportLeft = 0;
  int viewportHeight;
  int cursorLine = 0;
  int cursorCol = 0;

  // Selection State
  int selectionStartLine = 0;
  int selectionStartCol = 0;
  int selectionEndLine = 0;
  int selectionEndCol = 0;
  bool hasSelection = false;
  bool isSelecting = false;

  // Undo/Redo system
  std::stack<EditorState> undoStack;
  std::stack<EditorState> redoStack;
  static const size_t MAX_UNDO_LEVELS = 100;

  // File state
  bool isModified = false;

  // Configuration
  int tabSize = 4;

  // Private Helper Methods
  std::string expandTabs(const std::string &line, int tabSize = 4);
  std::string getFileExtension();
  bool isPositionSelected(int line, int col);
  void positionCursor();
  bool mouseToFilePos(int mouseRow, int mouseCol, int &fileRow, int &fileCol);
  void updateCursorAndViewport(int newLine, int newCol);

  // Internal editing helpers
  void markModified();
  void splitLineAtCursor();
  void joinLineWithNext();
  EditorState getCurrentState();
  void restoreState(const EditorState &state);
  void limitUndoStack();

  // Selection helpers
  std::pair<std::pair<int, int>, std::pair<int, int>> getNormalizedSelection();
  std::string getSelectedText();

  void updateCursorStyle();
  void restoreDefaultCursor();
};

#endif // EDITOR_H