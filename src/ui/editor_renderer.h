#ifndef EDITOR_RENDERER_H
#define EDITOR_RENDERER_H

#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

// Forward declarations
class GapBuffer;
class SyntaxHighlighter;
class MarkdownRenderer;
struct ColorSpan;

// Snapshot of state needed for one frame of rendering
struct RenderContext
{
  const GapBuffer &buffer;
  SyntaxHighlighter *highlighter;
  MarkdownRenderer *markdown;

  // Viewport & Cursor
  int cursorLine;
  int cursorCol;
  int viewportTop;
  int viewportLeft;
  int viewportHeight;
  int viewportWidth;

  // Settings
  bool showLineNumbers;
  int tabSize;

  // Selection
  bool hasSelection;
  int selStartLine;
  int selEndLine;
  int selStartCol;
  int selEndCol;

  // Computed helper to check if a specific line/col is selected
  bool isSelected(int line, int col) const;
};

class EditorRenderer
{
public:
  EditorRenderer();

  // Main render function (replaces display())
  void render(const RenderContext &ctx);

  // Sub-components
  void drawStatusBar(const RenderContext &ctx, const std::string &filename,
                     const std::string &fileLang, bool isModified,
                     bool isBinary);

  // Modal Drawing (Output only, input handling stays in Editor)
  void drawUnsavedChangesModal(const std::string &filename, int rows, int cols);
  void drawBinaryWarning(const std::string &filename, int rows, int cols);

  // Utility
  static std::string expandTabs(const std::string &line, int tabSize);

private:
  struct RenderSpan
  {
    int start;       // Screen column start
    int end;         // Screen column end
    int colorPair;   // Color pair to use
    int attribute;   // Attribute flags
    bool isSelected; // Whether this span is selected
  };

  // Internal rendering helpers
  std::vector<RenderSpan> buildRenderSpans(
      const std::string &line, const std::vector<ColorSpan> &highlightSpans,
      const RenderContext &ctx, int currentLineIdx, int contentWidth);
};

#endif // EDITOR_RENDERER_H