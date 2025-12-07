#ifndef EDITOR_H
#define EDITOR_H

#include "src/core/file_manager.h"
#include "src/ui/editor_renderer.h"

#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

// Core Systems
#include "buffer.h"
#include "editor_delta.h"
#include "editor_validation.h"
#include "src/core/history_manager.h"

// Features
#include "src/features/indent_manager.h"
#include "src/features/markdown_renderer.h"
#include "src/features/syntax_highlighter.h"

enum class UnsavedModalResult
{
  SAVE_AND_QUIT,
  QUIT_WITHOUT_SAVE,
  CANCEL
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
    int start;
    int end;
    int colorPair;
    int attribute;
    bool isSelected;
  };

  // Rendering (To be moved to EditorRenderer later)
  std::vector<RenderSpan>
  buildRenderSpans(const std::string &line,
                   const std::vector<ColorSpan> &highlightSpans,
                   bool lineHasSelection, int sel_start_line, int sel_end_line,
                   int sel_start_col, int sel_end_col, int currentLine,
                   int viewportLeft, int contentWidth);

  // Core API
  Editor(SyntaxHighlighter *highlighter);

  void setSyntaxHighlighter(SyntaxHighlighter *highlighter);
  SyntaxHighlighter *getSyntaxHighlighter() { return syntaxHighlighter; }

  bool loadFile(const std::string &fname);
  bool saveFile();
  void display();
  UnsavedModalResult handleUnsavedChangesModal();
  void drawStatusBar();
  void handleResize();
  void handleMouse(MEVENT &event);

  std::string getFilename() const { return filename; }
  std::string getFirstLine() const { return buffer.getLine(0); }

  // Markdown / Language
  void toggleMarkdownRendering();
  MarkdownRenderer *getMarkdownRenderer() { return markdownRenderer_.get(); }
  bool isMarkdownRenderingEnabled() const;
  bool setFileLang(std::string language)
  {
    filelang = language;
    return true;
  }
  std::string getFileLang() const { return filelang; }
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
  void selectAll();

  // Clipboard operations
  void copySelection();
  void cutSelection();
  void pasteFromClipboard();

  // Undo/Redo (Delegated to HistoryManager)
  void undo();
  void redo();

  // Utility
  bool hasUnsavedChanges() const { return history_.hasUnsavedChanges(); }
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

  void startSelection(int line, int col);
  void extendSelection(int line, int col);
  void finalizeSelection();
  bool isSelectionActive() const { return isSelecting || hasSelection; }
  int getCursorLine() const { return cursorLine; }
  int getCursorCol() const { return cursorCol; }

  bool mouseToFilePos(int mouseRow, int mouseCol, int &fileRow, int &fileCol);

  // Helpers being prepared for future FileManager class
  bool isImageFile(const std::string &path) const;
  std::string find_magika_models();
  static std::string find_magika_models_static();
  void displayBinaryWarning();

  // Editor cursor
  void setCursorMode();
  CursorMode getCursorMode() const { return currentMode; };

  // Validation
  EditorSnapshot captureSnapshot() const;
  ValidationResult validateState(const std::string &context) const;

  // Debug/stats
  size_t getUndoMemoryUsage() const { return history_.getUndoMemoryUsage(); }
  size_t getRedoMemoryUsage() const { return history_.getRedoMemoryUsage(); }

  // Indentation
  void insertTextAtCursor(const std::string &text);
  int removePreviousIndent(int amount);
  void forceSyntaxResync();

  // Expose Utilities
  bool isBinary() const { return isBinaryFile; }
  void updateCursorAndViewport(int newLine, int newCol);

private:
  // Core data
  GapBuffer buffer;

  std::string filename;
  SyntaxHighlighter *syntaxHighlighter;
  std::unique_ptr<EditorRenderer> renderer_;
  std::unique_ptr<IndentManager> indentManager_;
  std::unique_ptr<SyntaxConfigLoader> config_loader_;
  std::unique_ptr<FileManager> fileManager_;

  HistoryManager history_;

  // Helper to bridge data to renderer
  RenderContext buildRenderContext() const;

  bool isSaving = false;

  // Viewport and cursor
  int viewportTop = 0;
  int viewportLeft = 0;
  int viewportHeight;
  int cursorLine = 0;
  int cursorCol = 0;

  std::string filelang = "text";
  bool isPasting_ = false;
  std::string clipboard;

  // Undo/Redo Timeout Logic (Controller logic)
  static constexpr int UNDO_GROUP_TIMEOUT_MS = 2000;

  int tabSize = 4;

  // Private helpers
  std::string expandTabs(const std::string &line, int tabSize = 4);
  std::string getFileExtension();
  bool isPositionSelected(int line, int col);

  void splitLineAtCursor();
  void joinLineWithNext();

  // NOTE: These helpers create delta objects based on CURRENT Editor state.
  // They are passed to HistoryManager.
  EditDelta createDeltaForInsertChar(char ch);
  EditDelta createDeltaForDeleteChar();
  EditDelta createDeltaForBackspace();
  EditDelta createDeltaForNewline();
  EditDelta createDeltaForDeleteSelection();

  std::pair<std::pair<int, int>, std::pair<int, int>> getNormalizedSelection();
  void optimizedLineInvalidation(int startLine, int endLine);

  // Cursor Style
  CursorMode currentMode = NORMAL;

  // Indent helpers
  void autoIndentCurrentLine();
  void adjustIndentForClosingBracket();
  bool isLineOnlyWhitespace(const std::string &line);
  std::string getIndentString(int spaces);
  int countIndentSpaces(const std::string &line);
  void pasteWithSmartIndent(const std::string &text);

  bool isBinaryFile = false;

  std::unique_ptr<MarkdownRenderer> markdownRenderer_;
  void updateMarkdownRendering();

  // Internal helper
  void validateCursorAndViewport();
  // void updateCursorAndViewport(int newLine, int newCol);
};

#endif // EDITOR_H