#pragma once

#include "src/core/buffer.h"
#include "src/core/editor.h"
#include "src/features/syntax_highlighter.h"
#include <string>
#include <vector>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

// Forward declarations
class Editor;
struct ColorSpan;

/**
 * Handles all rendering/display logic for the editor
 * Separated from Editor to keep business logic clean
 */
class Renderer
{
public:
  struct ViewportInfo
  {
    int top;
    int left;
    int height;
    int width;
    int content_start_col; // Where content starts after line numbers
  };

  struct CursorInfo
  {
    int line;
    int col;
    int screen_row;
    int screen_col;
  };

  struct EditorState
  {
    ViewportInfo viewport;
    CursorInfo cursor;
    EditorMode mode;
    bool has_selection;
    int selection_start_line;
    int selection_start_col;
    int selection_end_line;
    int selection_end_col;
    std::string filename;
    bool is_modified;
  };

  Renderer();
  ~Renderer();

  // Main rendering functions
  void renderEditor(const EditorState &state, const GapBuffer &buffer,
                    const SyntaxHighlighter *highlighter);

  // Individual component rendering
  void renderContent(const EditorState &state, const GapBuffer &buffer,
                     const SyntaxHighlighter *highlighter);
  void renderStatusBar(const EditorState &state, const GapBuffer &buffer);
  void renderLineNumbers(int start_line, int end_line, int current_line,
                         int line_num_width, int viewport_top);

  // Cursor management
  void positionCursor(const CursorInfo &cursor, const ViewportInfo &viewport);
  void updateCursorStyle(EditorMode mode);
  void restoreDefaultCursor();

  // Screen management
  void clear();
  void refresh();
  void handleResize();
  ViewportInfo calculateViewport() const;

  // Utility functions
  int calculateLineNumberWidth(int max_line) const;
  bool isPositionSelected(int line, int col, const EditorState &state) const;
  std::string expandTabs(const std::string &line, int tab_size = 4) const;

private:
  int tab_size_;

  // Internal rendering helpers
  void renderLine(const std::string &line, int line_number,
                  const std::vector<ColorSpan> &spans,
                  const ViewportInfo &viewport, const EditorState &state);

  void renderStatusMode(EditorMode mode);
  void renderStatusFile(const std::string &filename, bool is_modified);
  void renderStatusPosition(const CursorInfo &cursor, int total_lines,
                            bool has_selection, int selection_size);

  // Color/attribute helpers
  void applyColorSpan(const ColorSpan &span, char ch);
  void setDefaultColors();
};