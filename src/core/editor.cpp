#include "editor.h"
// #include "src/ui/colors.h"
#include "src/core/clipboard.h"
#include "src/core/config_manager.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <curses.h>
#include <windows.h>
#else
#include <ncursesw/ncurses.h>
#endif
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

// Windows-specific mouse codes
#ifndef BUTTON4_PRESSED
#define BUTTON4_PRESSED 0x00200000L
#endif
#ifndef BUTTON5_PRESSED
#define BUTTON5_PRESSED 0x00100000L
#endif

// =================================================================
// Constructor
// =================================================================

// Clipboard::Clipboard();

Editor::Editor(SyntaxHighlighter *highlighter) : syntaxHighlighter(highlighter)
{
  tabSize = ConfigManager::getTabSize();
}

EditorSnapshot Editor::captureSnapshot() const
{
  EditorSnapshot snap;
  snap.lineCount = buffer.getLineCount();
  snap.cursorLine = cursorLine;
  snap.cursorCol = cursorCol;
  snap.viewportTop = viewportTop;
  snap.viewportLeft = viewportLeft;
  snap.bufferSize = buffer.size();

  if (snap.lineCount > 0)
  {
    snap.firstLine = buffer.getLine(0);
    snap.lastLine = buffer.getLine(snap.lineCount - 1);
    if (cursorLine < snap.lineCount)
    {
      snap.cursorLineContent = buffer.getLine(cursorLine);
    }
  }

  return snap;
}

ValidationResult Editor::validateState(const std::string &context) const
{
  // Check buffer is not empty
  if (buffer.getLineCount() == 0)
  {
    return ValidationResult("Buffer has 0 lines at: " + context);
  }

  // Check cursor line bounds
  if (cursorLine < 0 || cursorLine >= buffer.getLineCount())
  {
    std::ostringstream oss;
    oss << "Cursor line " << cursorLine << " out of bounds [0, "
        << buffer.getLineCount() - 1 << "] at: " << context;
    return ValidationResult(oss.str());
  }

  // Check cursor column bounds
  std::string line = buffer.getLine(cursorLine);
  if (cursorCol < 0 || cursorCol > static_cast<int>(line.length()))
  {
    std::ostringstream oss;
    oss << "Cursor col " << cursorCol << " out of bounds [0, " << line.length()
        << "] at: " << context;
    return ValidationResult(oss.str());
  }

  // Check viewport bounds
  if (viewportTop < 0)
  {
    return ValidationResult("Viewport top negative at: " + context);
  }

  if (viewportLeft < 0)
  {
    return ValidationResult("Viewport left negative at: " + context);
  }

  // Check viewport can contain cursor
  if (cursorLine < viewportTop)
  {
    std::ostringstream oss;
    oss << "Cursor line " << cursorLine << " above viewport " << viewportTop
        << " at: " + context;
    return ValidationResult(oss.str());
  }

  return ValidationResult(); // All valid
}

// Compare two snapshots and report differences
std::string Editor::compareSnapshots(const EditorSnapshot &before,
                                     const EditorSnapshot &after) const
{
  std::ostringstream oss;

  if (before.lineCount != after.lineCount)
  {
    oss << "LineCount: " << before.lineCount << " -> " << after.lineCount
        << "\n";
  }
  if (before.cursorLine != after.cursorLine)
  {
    oss << "CursorLine: " << before.cursorLine << " -> " << after.cursorLine
        << "\n";
  }
  if (before.cursorCol != after.cursorCol)
  {
    oss << "CursorCol: " << before.cursorCol << " -> " << after.cursorCol
        << "\n";
  }
  if (before.bufferSize != after.bufferSize)
  {
    oss << "BufferSize: " << before.bufferSize << " -> " << after.bufferSize
        << "\n";
  }
  if (before.cursorLineContent != after.cursorLineContent)
  {
    oss << "CursorLine content changed\n";
    oss << "  Before: '" << before.cursorLineContent << "'\n";
    oss << "  After:  '" << after.cursorLineContent << "'\n";
  }

  return oss.str();
}

void Editor::reloadConfig()
{
  tabSize = ConfigManager::getTabSize();
  // Trigger redisplay to reflect changes
}

// =================================================================
// Mode Management
// =================================================================

// =================================================================
// Private Helper Methods (from original code)
// =================================================================

std::string Editor::expandTabs(const std::string &line, int tabSize)
{
  std::string result;
  for (char c : line)
  {
    if (c == '\t')
    {
      int spacesToAdd = tabSize - (result.length() % tabSize);
      result.append(spacesToAdd, ' ');
    }
    else if (c >= 32 && c <= 126)
    {
      result += c;
    }
    else
    {
      result += ' ';
    }
  }
  return result;
}

std::string Editor::getFileExtension()
{
  if (filename.empty())
    return "";

  size_t dot = filename.find_last_of(".");
  if (dot == std::string::npos)
    return "";

  std::string ext = filename.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  return ext;
}

bool Editor::isPositionSelected(int line, int col)
{
  if (!hasSelection && !isSelecting)
    return false;

  int startL = selectionStartLine;
  int startC = selectionStartCol;
  int endL = selectionEndLine;
  int endC = selectionEndCol;

  if (startL > endL || (startL == endL && startC > endC))
  {
    std::swap(startL, endL);
    std::swap(startC, endC);
  }

  if (line < startL || line > endL)
    return false;

  if (startL == endL)
  {
    return col >= startC && col < endC;
  }
  else if (line == startL)
  {
    return col >= startC;
  }
  else if (line == endL)
  {
    return col < endC;
  }
  else
  {
    return true;
  }
}

void Editor::positionCursor()
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int screenRow = cursorLine - viewportTop;
  if (screenRow >= 0 && screenRow < viewportHeight)
  {
    bool show_line_numbers = ConfigManager::getLineNumbers();
    int lineNumWidth =
        show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
    int contentStartCol = show_line_numbers ? (lineNumWidth + 3) : 0;
    int screenCol = contentStartCol + cursorCol - viewportLeft;

    if (screenCol >= contentStartCol && screenCol < cols)
    {
      move(screenRow, screenCol);
    }
    else
    {
      move(screenRow, contentStartCol);
    }
  }
  // REMOVED: All #ifdef _WIN32 refresh() calls
}

bool Editor::mouseToFilePos(int mouseRow, int mouseCol, int &fileRow,
                            int &fileCol)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  if (mouseRow >= rows - 1)
    return false;

  bool show_line_numbers = ConfigManager::getLineNumbers();
  int lineNumWidth =
      show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
  int contentStartCol = show_line_numbers ? (lineNumWidth + 3) : 0;

  if (mouseCol < contentStartCol)
  {
    mouseCol = contentStartCol;
  }

  fileRow = viewportTop + mouseRow;
  if (fileRow < 0)
    fileRow = 0;
  if (fileRow >= buffer.getLineCount())
    fileRow = buffer.getLineCount() - 1;

  fileCol = viewportLeft + (mouseCol - contentStartCol);
  if (fileCol < 0)
    fileCol = 0;

  return true;
}

void Editor::updateCursorAndViewport(int newLine, int newCol)
{
  cursorLine = newLine;

  int currentTabSize = ConfigManager::getTabSize();
  std::string expandedLine =
      expandTabs(buffer.getLine(cursorLine), currentTabSize);
  cursorCol = std::min(newCol, static_cast<int>(expandedLine.length()));

  if (cursorLine < viewportTop)
  {
    viewportTop = cursorLine;
  }
  else if (cursorLine >= viewportTop + viewportHeight)
  {
    viewportTop = cursorLine - viewportHeight + 1;
  }

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  bool show_line_numbers = ConfigManager::getLineNumbers();
  int lineNumWidth =
      show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
  int contentWidth = cols - (show_line_numbers ? (lineNumWidth + 3) : 0);

  if (cursorCol < viewportLeft)
  {
    viewportLeft = cursorCol;
  }
  else if (cursorCol >= viewportLeft + contentWidth)
  {
    viewportLeft = cursorCol - contentWidth + 1;
  }
}

// =================================================================
// Public API Methods
// =================================================================

void Editor::setSyntaxHighlighter(SyntaxHighlighter *highlighter)
{
  syntaxHighlighter = highlighter;
}

void Editor::display()
{
  // Validate state
  if (!validateEditorState())
  {
    validateCursorAndViewport();
    if (!validateEditorState())
      return;
  }

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  viewportHeight = rows - 1;

  bool show_line_numbers = ConfigManager::getLineNumbers();
  int lineNumWidth =
      show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
  int contentStartCol = show_line_numbers ? (lineNumWidth + 3) : 0;
  int contentWidth = cols - contentStartCol;

  int endLine = std::min(viewportTop + viewportHeight, buffer.getLineCount());

  // OPTIMIZATION: Pre-mark viewport lines for priority parsing
  if (syntaxHighlighter)
  {
    syntaxHighlighter->markViewportLines(viewportTop, endLine - 1);
  }

  // Pre-compute selection
  bool hasActiveSelection = (hasSelection || isSelecting);
  int sel_start_line = -1, sel_start_col = -1;
  int sel_end_line = -1, sel_end_col = -1;

  if (hasActiveSelection)
  {
    auto [start, end] = getNormalizedSelection();
    sel_start_line = start.first;
    sel_start_col = start.second;
    sel_end_line = end.first;
    sel_end_col = end.second;
  }

  int currentTabSize = ConfigManager::getTabSize();

  // OPTIMIZATION: Batch render - minimize attribute changes
  for (int i = viewportTop; i < endLine; i++)
  {
    int screenRow = i - viewportTop;
    bool isCurrentLine = (cursorLine == i);

    move(screenRow, 0);
    attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));

    // Render line numbers
    if (show_line_numbers)
    {
      int ln_colorPair = isCurrentLine ? ColorPairs::LINE_NUMBERS_ACTIVE
                                       : ColorPairs::LINE_NUMBERS;
      attron(COLOR_PAIR(ln_colorPair));
      printw("%*d ", lineNumWidth, i + 1);
      attroff(COLOR_PAIR(ln_colorPair));

      attron(COLOR_PAIR(ColorPairs::UI_BORDER));
      addch(' ');
      attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
      addch(' ');
    }

    // Get line content
    std::string expandedLine = expandTabs(buffer.getLine(i), currentTabSize);

    // OPTIMIZATION: Get highlighting spans (cached if available)
    std::vector<ColorSpan> currentLineSpans;
    if (syntaxHighlighter)
    {
      try
      {
        currentLineSpans =
            syntaxHighlighter->getHighlightSpans(expandedLine, i, buffer);
      }
      catch (...)
      {
        currentLineSpans.clear();
      }
    }

    bool lineHasSelection =
        hasActiveSelection && i >= sel_start_line && i <= sel_end_line;

    // Build render spans that combine highlighting + selection
    std::vector<RenderSpan> renderSpans =
        buildRenderSpans(expandedLine, currentLineSpans, lineHasSelection,
                         sel_start_line, sel_end_line, sel_start_col,
                         sel_end_col, i, viewportLeft, contentWidth);

    // Render each span as a batch
    int screenCol = 0;
    for (const auto &span : renderSpans)
    {
      if (screenCol >= contentWidth)
        break;

      // Set attributes once per span (not per character!)
      if (span.isSelected)
      {
        attron(COLOR_PAIR(ColorPairs::STATE_SELECTED) | A_REVERSE);
      }
      else if (span.colorPair >= 0)
      {
        attron(COLOR_PAIR(span.colorPair));
        if (span.attribute != 0)
        {
          attron(span.attribute);
        }
      }
      else
      {
        attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
      }

      // Render all characters in this span
      for (int col = span.start; col < span.end && screenCol < contentWidth;
           ++col)
      {
        int fileCol = viewportLeft + screenCol;
        char ch = ' ';

        if (fileCol >= 0 && fileCol < (int)expandedLine.length())
        {
          ch = expandedLine[fileCol];
          if (ch < 32 || ch > 126)
            ch = ' ';
        }

        addch(ch);
        screenCol++;
      }

      // Clear attributes once per span
      if (span.isSelected)
      {
        attroff(COLOR_PAIR(ColorPairs::STATE_SELECTED) | A_REVERSE);
      }
      else if (span.colorPair >= 0)
      {
        if (span.attribute != 0)
        {
          attroff(span.attribute);
        }
        attroff(COLOR_PAIR(span.colorPair));
      }
    }

    attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
    clrtoeol();
  }

  // Clear remaining lines
  attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
  for (int i = endLine - viewportTop; i < viewportHeight; i++)
  {
    move(i, 0);
    clrtoeol();
  }

  drawStatusBar();
  positionCursor();
}

std::vector<Editor::RenderSpan>
Editor::buildRenderSpans(const std::string &line,
                         const std::vector<ColorSpan> &highlightSpans,
                         bool lineHasSelection, int sel_start_line,
                         int sel_end_line, int sel_start_col, int sel_end_col,
                         int currentLine, int viewportLeft, int contentWidth)
{
  std::vector<RenderSpan> spans;

  // Track current position and state
  int spanStart = 0;
  int currentColorPair = -1;
  int currentAttribute = 0;
  bool currentlySelected = false;

  // Helper to check selection at a file column
  auto isColSelected = [&](int fileCol) -> bool
  {
    if (!lineHasSelection)
      return false;

    if (sel_start_line == sel_end_line)
    {
      return fileCol >= sel_start_col && fileCol < sel_end_col;
    }
    else if (currentLine == sel_start_line)
    {
      return fileCol >= sel_start_col;
    }
    else if (currentLine == sel_end_line)
    {
      return fileCol < sel_end_col;
    }
    else
    {
      return true; // Middle line, fully selected
    }
  };

  // Helper to find highlight span for a file column
  auto findHighlightSpan = [&](int fileCol) -> const ColorSpan *
  {
    for (const auto &span : highlightSpans)
    {
      if (fileCol >= span.start && fileCol < span.end)
      {
        return &span;
      }
    }
    return nullptr;
  };

  // Scan through visible columns and build batched spans
  for (int screenCol = 0; screenCol < contentWidth; ++screenCol)
  {
    int fileCol = viewportLeft + screenCol;

    bool selected = isColSelected(fileCol);
    const ColorSpan *highlight = (fileCol >= 0 && fileCol < (int)line.length())
                                     ? findHighlightSpan(fileCol)
                                     : nullptr;

    int colorPair = highlight ? highlight->colorPair : -1;
    int attribute = highlight ? highlight->attribute : 0;

    // Check if we need to start a new span
    bool stateChanged = (selected != currentlySelected) ||
                        (colorPair != currentColorPair) ||
                        (attribute != currentAttribute);

    if (stateChanged && screenCol > spanStart)
    {
      // Finish current span
      spans.push_back({spanStart, screenCol, currentColorPair, currentAttribute,
                       currentlySelected});
      spanStart = screenCol;
    }

    // Update current state
    currentlySelected = selected;
    currentColorPair = colorPair;
    currentAttribute = attribute;
  }

  // Finish last span
  if (spanStart < contentWidth)
  {
    spans.push_back({spanStart, contentWidth, currentColorPair,
                     currentAttribute, currentlySelected});
  }

  return spans;
}

void Editor::drawStatusBar()
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int statusRow = rows - 1;

  move(statusRow, 0);
  attrset(COLOR_PAIR(ColorPairs::STATUS_BAR));
  clrtoeol();

  move(statusRow, 0);

  // Show filename using STATUS_BAR_ACTIVE for main UI elements
  attron(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE) | A_BOLD);
  if (filename.empty())
  {
    printw("[No Name]");
  }
  else
  {
    size_t lastSlash = filename.find_last_of("/\\");
    std::string displayName = (lastSlash != std::string::npos)
                                  ? filename.substr(lastSlash + 1)
                                  : filename;
    printw("%s", displayName.c_str());
  }
  attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE) | A_BOLD);

  // Show modified indicator - use text color with bold
  if (isModified)
  {
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT) | A_BOLD);
    printw(" [+]");
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT) | A_BOLD);
  }

  // Show file extension
  std::string ext = getFileExtension();
  if (!ext.empty())
  {
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));
    printw(" [%s]", ext.c_str());
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));
  }

  // Right section with position info
  char rightSection[256];
  if (hasSelection)
  {
    auto [start, end] = getNormalizedSelection();
    int startL = start.first, startC = start.second;
    int endL = end.first, endC = end.second;

    if (startL == endL)
    {
      int selectionSize = endC - startC;
      snprintf(rightSection, sizeof(rightSection),
               "[%d chars] %d:%d %d/%d %d%% ", selectionSize, cursorLine + 1,
               cursorCol + 1, cursorLine + 1, buffer.getLineCount(),
               buffer.getLineCount() == 0
                   ? 0
                   : ((cursorLine + 1) * 100 / buffer.getLineCount()));
    }
    else
    {
      int lineCount = endL - startL + 1;
      snprintf(rightSection, sizeof(rightSection),
               "[%d lines] %d:%d %d/%d %d%% ", lineCount, cursorLine + 1,
               cursorCol + 1, cursorLine + 1, buffer.getLineCount(),
               buffer.getLineCount() == 0
                   ? 0
                   : ((cursorLine + 1) * 100 / buffer.getLineCount()));
    }
  }
  else
  {
    snprintf(rightSection, sizeof(rightSection), "%d:%d %d/%d %d%% ",
             cursorLine + 1, cursorCol + 1, cursorLine + 1,
             buffer.getLineCount(),
             buffer.getLineCount() == 0
                 ? 0
                 : ((cursorLine + 1) * 100 / buffer.getLineCount()));
  }

  int rightLen = strlen(rightSection);
  int currentPos = getcurx(stdscr);
  int rightStart = cols - rightLen;

  if (rightStart <= currentPos)
  {
    rightStart = currentPos + 2;
  }

  // Fill middle space with status bar background
  for (int i = currentPos; i < rightStart && i < cols; i++)
  {
    move(statusRow, i);
    addch(' ' | COLOR_PAIR(ColorPairs::STATUS_BAR));
  }

  // Right section using STATUS_BAR_TEXT for position information
  if (rightStart < cols)
  {
    move(statusRow, rightStart);
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));
    printw("%s", rightSection);
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));
  }
}

void Editor::handleResize()
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  viewportHeight = rows - 1;

  if (cursorLine >= viewportTop + viewportHeight)
  {
    viewportTop = cursorLine - viewportHeight + 1;
  }
  if (viewportTop < 0)
  {
    viewportTop = 0;
  }
  clear();
  display();

  wnoutrefresh(stdscr); // Mark stdscr as ready
  doupdate();           // Execute the single, clean flush
}

void Editor::handleMouse(MEVENT &event)
{
  if (event.bstate & BUTTON1_PRESSED)
  {
    int fileRow, fileCol;
    if (mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      // Start a new selection on mouse press
      clearSelection();
      isSelecting = true;
      selectionStartLine = fileRow;
      selectionStartCol = fileCol;
      selectionEndLine = fileRow;
      selectionEndCol = fileCol;
      updateCursorAndViewport(fileRow, fileCol);
    }
  }
  else if (event.bstate & BUTTON1_RELEASED)
  {
    if (isSelecting)
    {
      int fileRow, fileCol;
      if (mouseToFilePos(event.y, event.x, fileRow, fileCol))
      {
        selectionEndLine = fileRow;
        selectionEndCol = fileCol;
        // Only keep selection if it's not just a click (start != end)
        if (selectionStartLine != selectionEndLine ||
            selectionStartCol != selectionEndCol)
        {
          hasSelection = true;
        }
        else
        {
          // Just a click, no drag - clear selection
          clearSelection();
        }
        updateCursorAndViewport(fileRow, fileCol);
      }
      isSelecting = false;
    }
  }
  else if ((event.bstate & REPORT_MOUSE_POSITION) && isSelecting)
  {
    // Mouse drag - extend selection
    int fileRow, fileCol;
    if (mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      selectionEndLine = fileRow;
      selectionEndCol = fileCol;
      updateCursorAndViewport(fileRow, fileCol);
    }
  }
  else if (event.bstate & BUTTON1_CLICKED)
  {
    // Single click - move cursor and clear selection
    int fileRow, fileCol;
    if (mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      clearSelection();
      updateCursorAndViewport(fileRow, fileCol);
    }
  }
  else if (event.bstate & BUTTON4_PRESSED)
  {
    // Scroll up
    scrollUp();
  }
  else if (event.bstate & BUTTON5_PRESSED)
  {
    // Scroll down
    scrollDown();
  }
}

void Editor::clearSelection()
{
  hasSelection = false;
  isSelecting = false;
  selectionStartLine = 0;
  selectionStartCol = 0;
  selectionEndLine = 0;
  selectionEndCol = 0;
}

void Editor::moveCursorUp()
{
  if (cursorLine > 0)
  {
    cursorLine--;
    if (cursorLine < viewportTop)
    {
      viewportTop = cursorLine;
    }

    if (cursorCol > 0)
    {
      std::string line = buffer.getLine(cursorLine);
      int lineLen = static_cast<int>(line.length());
      if (cursorCol > lineLen)
      {
        std::string expandedLine = expandTabs(line, tabSize);
        cursorCol =
            std::min(cursorCol, static_cast<int>(expandedLine.length()));
      }
    }
  }
  // Note: Selection handling now done in InputHandler
}

void Editor::moveCursorDown()
{
  int maxLine = buffer.getLineCount() - 1;
  if (cursorLine < maxLine)
  {
    cursorLine++;
    if (cursorLine >= viewportTop + viewportHeight)
    {
      viewportTop = cursorLine - viewportHeight + 1;
    }

    if (cursorCol > 0)
    {
      std::string line = buffer.getLine(cursorLine);
      int lineLen = static_cast<int>(line.length());
      if (cursorCol > lineLen)
      {
        std::string expandedLine = expandTabs(line, tabSize);
        cursorCol =
            std::min(cursorCol, static_cast<int>(expandedLine.length()));
      }
    }
  }
}

void Editor::moveCursorLeft()
{
  if (cursorCol > 0)
  {
    cursorCol--;
    if (cursorCol < viewportLeft)
    {
      viewportLeft = cursorCol;
    }
  }
  else if (cursorLine > 0)
  {
    cursorLine--;
    int currentTabSize = ConfigManager::getTabSize();
    std::string expandedLine =
        expandTabs(buffer.getLine(cursorLine), currentTabSize);
    cursorCol = expandedLine.length();

    if (cursorLine < viewportTop)
    {
      viewportTop = cursorLine;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    bool show_line_numbers = ConfigManager::getLineNumbers();
    int lineNumWidth =
        show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
    int contentWidth = cols - (show_line_numbers ? (lineNumWidth + 3) : 0);

    if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
    {
      viewportLeft = cursorCol - contentWidth + 1;
      if (viewportLeft < 0)
        viewportLeft = 0;
    }
  }
}

void Editor::moveCursorRight()
{
  std::string line = buffer.getLine(cursorLine);

  if (cursorCol < static_cast<int>(line.length()))
  {
    if (line[cursorCol] != '\t')
    {
      cursorCol++;
    }
    else
    {
      int currentTabSize = ConfigManager::getTabSize();
      std::string expandedLine = expandTabs(line, currentTabSize);
      if (cursorCol < static_cast<int>(expandedLine.length()))
      {
        cursorCol++;
      }
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    bool show_line_numbers = ConfigManager::getLineNumbers();
    int lineNumWidth =
        show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
    int contentWidth = cols - (show_line_numbers ? (lineNumWidth + 3) : 0);

    if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
    {
      viewportLeft = cursorCol - contentWidth + 1;
    }
  }
  else if (cursorLine < buffer.getLineCount() - 1)
  {
    cursorLine++;
    cursorCol = 0;

    if (cursorLine >= viewportTop + viewportHeight)
    {
      viewportTop = cursorLine - viewportHeight + 1;
    }

    viewportLeft = 0;
  }
}

void Editor::pageUp()
{
  for (int i = 0; i < 10; i++)
  {
    moveCursorUp();
  }
}

void Editor::pageDown()
{
  for (int i = 0; i < 10; i++)
  {
    moveCursorDown();
  }
}

void Editor::moveCursorToLineStart()
{
  cursorCol = 0;
  if (cursorCol < viewportLeft)
  {
    viewportLeft = 0;
  }

  // Selection handling is done in InputHandler, not here
  // This method just moves the cursor
}

void Editor::moveCursorToLineEnd()
{
  int currentTabSize = ConfigManager::getTabSize();
  std::string expandedLine =
      expandTabs(buffer.getLine(cursorLine), currentTabSize);
  cursorCol = static_cast<int>(expandedLine.length());

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  bool show_line_numbers = ConfigManager::getLineNumbers();
  int lineNumWidth =
      show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
  int contentWidth = cols - (show_line_numbers ? (lineNumWidth + 3) : 0);

  if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
  {
    viewportLeft = cursorCol - contentWidth + 1;
    if (viewportLeft < 0)
      viewportLeft = 0;
  }

  // Selection handling is done in InputHandler, not here
  // This method just moves the cursor
}

void Editor::scrollUp(int linesToScroll)
{
  viewportTop -= linesToScroll;
  if (viewportTop < 0)
    viewportTop = 0;

  if (cursorLine < viewportTop)
  {
    cursorLine = viewportTop;
    if (cursorLine < 0)
      cursorLine = 0;
    if (cursorLine >= buffer.getLineCount())
    {
      cursorLine = buffer.getLineCount() - 1;
    }

    std::string expandedLine = expandTabs(buffer.getLine(cursorLine), tabSize);
    cursorCol = std::min(cursorCol, static_cast<int>(expandedLine.length()));
  }
}

void Editor::scrollDown(int linesToScroll)
{
  int maxViewportTop = buffer.getLineCount() - viewportHeight;
  if (maxViewportTop < 0)
    maxViewportTop = 0;

  viewportTop += linesToScroll;
  if (viewportTop > maxViewportTop)
    viewportTop = maxViewportTop;
  if (viewportTop < 0)
    viewportTop = 0;

  if (cursorLine >= viewportTop + viewportHeight)
  {
    cursorLine = viewportTop + viewportHeight - 1;

    int maxLine = buffer.getLineCount() - 1;
    if (cursorLine > maxLine)
      cursorLine = maxLine;
    if (cursorLine < 0)
      cursorLine = 0;

    std::string expandedLine = expandTabs(buffer.getLine(cursorLine), tabSize);
    cursorCol = std::min(cursorCol, static_cast<int>(expandedLine.length()));
  }
}

void Editor::validateCursorAndViewport()
{
  if (buffer.getLineCount() == 0)
    return;

  int maxLine = buffer.getLineCount() - 1;
  if (cursorLine < 0)
    cursorLine = 0;
  if (cursorLine > maxLine)
    cursorLine = maxLine;

  std::string expandedLine = expandTabs(buffer.getLine(cursorLine), tabSize);
  if (cursorCol < 0)
    cursorCol = 0;
  if (cursorCol > static_cast<int>(expandedLine.length()))
  {
    cursorCol = static_cast<int>(expandedLine.length());
  }

  int maxViewportTop = buffer.getLineCount() - viewportHeight;
  if (maxViewportTop < 0)
    maxViewportTop = 0;

  if (viewportTop < 0)
    viewportTop = 0;
  if (viewportTop > maxViewportTop)
    viewportTop = maxViewportTop;
  if (viewportLeft < 0)
    viewportLeft = 0;

  if (cursorLine < viewportTop)
  {
    viewportTop = cursorLine;
  }
  else if (cursorLine >= viewportTop + viewportHeight)
  {
    viewportTop = cursorLine - viewportHeight + 1;
    if (viewportTop < 0)
      viewportTop = 0;
    if (viewportTop > maxViewportTop)
      viewportTop = maxViewportTop;
  }
}

// =================================================================
// File Operations
// =================================================================

void Editor::debugPrintState(const std::string &context)
{
  std::cerr << "=== EDITOR STATE DEBUG: " << context << " ===" << std::endl;
  std::cerr << "cursorLine: " << cursorLine << std::endl;
  std::cerr << "cursorCol: " << cursorCol << std::endl;
  std::cerr << "viewportTop: " << viewportTop << std::endl;
  std::cerr << "viewportLeft: " << viewportLeft << std::endl;
  std::cerr << "buffer.getLineCount(): " << buffer.getLineCount() << std::endl;
  std::cerr << "buffer.size(): " << buffer.size() << std::endl;
  std::cerr << "isModified: " << isModified << std::endl;
  // std::cerr << "currentMode: " << (int)currentMode << std::endl;

  if (cursorLine < buffer.getLineCount())
  {
    std::string currentLine = buffer.getLine(cursorLine);
    std::cerr << "currentLine length: " << currentLine.length() << std::endl;
    std::cerr << "currentLine content: '" << currentLine << "'" << std::endl;
  }
  else
  {
    std::cerr << "ERROR: cursorLine out of bounds!" << std::endl;
  }

  std::cerr << "hasSelection: " << hasSelection << std::endl;
  std::cerr << "isSelecting: " << isSelecting << std::endl;
  std::cerr << "undoStack.size(): " << undoStack.size() << std::endl;
  std::cerr << "redoStack.size(): " << redoStack.size() << std::endl;
  std::cerr << "=== END DEBUG ===" << std::endl;
}

bool Editor::validateEditorState()
{
  bool valid = true;

  if (cursorLine < 0 || cursorLine >= buffer.getLineCount())
  {
    std::cerr << "INVALID: cursorLine out of bounds: " << cursorLine
              << " (max: " << buffer.getLineCount() - 1 << ")" << std::endl;
    valid = false;
  }

  if (cursorCol < 0)
  {
    std::cerr << "INVALID: cursorCol negative: " << cursorCol << std::endl;
    valid = false;
  }

  if (cursorLine >= 0 && cursorLine < buffer.getLineCount())
  {
    std::string line = buffer.getLine(cursorLine);
    if (cursorCol > static_cast<int>(line.length()))
    {
      std::cerr << "INVALID: cursorCol past end of line: " << cursorCol
                << " (line length: " << line.length() << ")" << std::endl;
      valid = false;
    }
  }

  if (viewportTop < 0)
  {
    std::cerr << "INVALID: viewportTop negative: " << viewportTop << std::endl;
    valid = false;
  }

  if (viewportLeft < 0)
  {
    std::cerr << "INVALID: viewportLeft negative: " << viewportLeft
              << std::endl;
    valid = false;
  }

  return valid;
}

bool Editor::loadFile(const std::string &fname)
{
  filename = fname;

  if (syntaxHighlighter)
  {
    std::string extension = getFileExtension();
    syntaxHighlighter->setLanguage(extension);
  }

  if (!buffer.loadFromFile(filename))
  {
    buffer.clear();
    buffer.insertLine(0, "");
    return false;
  }

  // Set language but DON'T parse yet - parsing happens on first display

  isModified = false;
  return true;
}

bool Editor::saveFile()
{
  if (filename.empty())
  {
    return false;
  }

  // Set flag to prevent saveState() during file operations
  isSaving = true;

  bool success = buffer.saveToFile(filename);

  if (success)
  {
    isModified = false;
  }

  isSaving = false; // Reset flag
  return success;
}
// =================================================================
// Text Editing Operations
// =================================================================

void Editor::insertChar(char ch)
{
  if (cursorLine < 0 || cursorLine >= buffer.getLineCount())
    return;

  if (useDeltaUndo_ && !isUndoRedoing)
  {
    EditDelta delta = createDeltaForInsertChar(ch);
    std::string line = buffer.getLine(cursorLine);
    if (cursorCol > static_cast<int>(line.length()))
      cursorCol = line.length();
    if (cursorCol < 0)
      cursorCol = 0;

    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);
    line.insert(cursorCol, 1, ch);
    buffer.replaceLine(cursorLine, line);
    cursorCol++;

    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(
          buffer, byte_pos, 0, 1, cursorLine, cursorCol - 1, cursorLine,
          cursorCol - 1, cursorLine, cursorCol);

      // NEW: Always invalidate cache after edit
      syntaxHighlighter->invalidateLineCache(cursorLine);
    }

    // Update viewport
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    bool show_line_numbers = ConfigManager::getLineNumbers();
    int lineNumWidth =
        show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
    int contentWidth = cols - (show_line_numbers ? (lineNumWidth + 3) : 0);

    if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
    {
      viewportLeft = cursorCol - contentWidth + 1;
    }

    // Complete delta
    delta.postCursorLine = cursorLine;
    delta.postCursorCol = cursorCol;
    delta.postViewportTop = viewportTop;
    delta.postViewportLeft = viewportLeft;

    addDelta(delta);

    // FIX: Auto-commit on timeout OR boundary characters for immediate
    // highlighting
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - currentDeltaGroup_.timestamp)
                       .count();

    // Boundary characters that should trigger immediate commit
    bool is_boundary_char = (ch == '>' || ch == ')' || ch == '}' || ch == ']' ||
                             ch == ';' || ch == ',');

    if (elapsed > UNDO_GROUP_TIMEOUT_MS || is_boundary_char)
    {
      commitDeltaGroup();
      beginDeltaGroup();
    }

    markModified();
  }
  else if (!isUndoRedoing)
  {
    // OLD: Full-state undo (fallback)
    saveState();

    std::string line = buffer.getLine(cursorLine);
    if (cursorCol > static_cast<int>(line.length()))
      cursorCol = line.length();
    if (cursorCol < 0)
      cursorCol = 0;

    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);
    line.insert(cursorCol, 1, ch);
    buffer.replaceLine(cursorLine, line);

    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(buffer, byte_pos, 0, 1, cursorLine,
                                             cursorCol, cursorLine, cursorCol,
                                             cursorLine, cursorCol + 1);
      syntaxHighlighter->invalidateLineRange(cursorLine, cursorLine);
    }

    cursorCol++;
    markModified();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int lineNumWidth = std::to_string(buffer.getLineCount()).length();
    int contentWidth = cols - lineNumWidth - 3;
    if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
    {
      viewportLeft = cursorCol - contentWidth + 1;
    }
  }
}

void Editor::insertNewline()
{
  if (useDeltaUndo_ && !isUndoRedoing)
  {
    EditorSnapshot before = captureSnapshot();
    EditDelta delta = createDeltaForNewline();

    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);

    // 1. MODIFY BUFFER FIRST
    splitLineAtCursor();
    cursorLine++;
    cursorCol = 0;

    // 2. THEN notify Tree-sitter AFTER buffer change
    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(
          buffer, byte_pos, 0, 1,                  // Inserted 1 byte (newline)
          delta.preCursorLine, delta.preCursorCol, // OLD position
          delta.preCursorLine, delta.preCursorCol, cursorLine,
          0); // NEW position

      // Invalidate from split point onwards
      syntaxHighlighter->invalidateLineRange(cursorLine - 1,
                                             buffer.getLineCount() - 1);
    }

    if (cursorLine >= viewportTop + viewportHeight)
    {
      viewportTop = cursorLine - viewportHeight + 1;
    }
    viewportLeft = 0;

    // Complete delta
    delta.postCursorLine = cursorLine;
    delta.postCursorCol = cursorCol;
    delta.postViewportTop = viewportTop;
    delta.postViewportLeft = viewportLeft;

    ValidationResult valid = validateState("After insertNewline");
    if (valid)
    {
      addDelta(delta);
      // Newlines always commit the current group
      commitDeltaGroup();
      beginDeltaGroup();
    }
    else
    {
      std::cerr << "VALIDATION FAILED in insertNewline\n";
      std::cerr << valid.error << "\n";
    }

    markModified();
  }
  else if (!isUndoRedoing)
  {
    // OLD: Full-state undo
    saveState();

    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);

    splitLineAtCursor();
    cursorLine++;
    cursorCol = 0;

    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(buffer, byte_pos, 0, 1,
                                             cursorLine - 1, 0, cursorLine - 1,
                                             0, cursorLine, 0);
      syntaxHighlighter->invalidateLineRange(cursorLine - 1,
                                             buffer.getLineCount() - 1);
    }

    if (cursorLine >= viewportTop + viewportHeight)
    {
      viewportTop = cursorLine - viewportHeight + 1;
    }
    viewportLeft = 0;
    markModified();
  }
}

void Editor::deleteChar()
{
  if (useDeltaUndo_ && !isUndoRedoing)
  {
    EditorSnapshot before = captureSnapshot();
    EditDelta delta = createDeltaForDeleteChar();
    std::string line = buffer.getLine(cursorLine);

    if (cursorCol < static_cast<int>(line.length()))
    {
      size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);
      line.erase(cursorCol, 1);
      buffer.replaceLine(cursorLine, line);

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine,
            cursorCol + 1, cursorLine, cursorCol);

        // NEW: Always invalidate cache after edit
        syntaxHighlighter->invalidateLineCache(cursorLine);
      }
    }
    else if (cursorLine < buffer.getLineCount() - 1)
    {
      size_t byte_pos = buffer.lineColToPos(cursorLine, line.length());
      std::string nextLine = buffer.getLine(cursorLine + 1);
      buffer.replaceLine(cursorLine, line + nextLine);
      buffer.deleteLine(cursorLine + 1);

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, (uint32_t)line.length(),
            cursorLine + 1, 0, cursorLine, (uint32_t)line.length());

        syntaxHighlighter->invalidateLineRange(cursorLine,
                                               buffer.getLineCount() - 1);
      }
    }

    delta.postCursorLine = cursorLine;
    delta.postCursorCol = cursorCol;
    delta.postViewportTop = viewportTop;
    delta.postViewportLeft = viewportLeft;

    ValidationResult valid = validateState("After deleteChar");
    if (valid)
    {
      addDelta(delta);
      if (delta.operation == EditDelta::JOIN_LINES)
      {
        commitDeltaGroup();
        beginDeltaGroup();
      }
    }
    else
    {
      std::cerr << "VALIDATION FAILED in deleteChar\n";
      std::cerr << valid.error << "\n";
    }

    markModified();
  }
  else if (!isUndoRedoing)
  {
    saveState();
    std::string line = buffer.getLine(cursorLine);

    if (cursorCol < static_cast<int>(line.length()))
    {
      size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);
      line.erase(cursorCol, 1);
      buffer.replaceLine(cursorLine, line);

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine,
            cursorCol + 1, cursorLine, cursorCol);
        // NEW: Always invalidate cache after edit
        syntaxHighlighter->invalidateLineCache(cursorLine);
      }

      markModified();
    }
    else if (cursorLine < buffer.getLineCount() - 1)
    {
      size_t byte_pos = buffer.lineColToPos(cursorLine, line.length());
      std::string nextLine = buffer.getLine(cursorLine + 1);
      buffer.replaceLine(cursorLine, line + nextLine);
      buffer.deleteLine(cursorLine + 1);

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, (uint32_t)line.length(),
            cursorLine + 1, 0, cursorLine, (uint32_t)line.length());
        syntaxHighlighter->invalidateLineRange(cursorLine,
                                               buffer.getLineCount() - 1);
      }

      markModified();
    }
  }
}

void Editor::backspace()
{
  if (useDeltaUndo_ && !isUndoRedoing)
  {
    EditorSnapshot before = captureSnapshot();
    EditDelta delta = createDeltaForBackspace();

    if (cursorCol > 0)
    {
      std::string line = buffer.getLine(cursorLine);
      size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol - 1);
      line.erase(cursorCol - 1, 1);
      buffer.replaceLine(cursorLine, line);
      cursorCol--;

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine,
            cursorCol + 1, cursorLine, cursorCol);

        // NEW: Always invalidate cache after edit
        syntaxHighlighter->invalidateLineCache(cursorLine);
      }

      if (cursorCol < viewportLeft)
      {
        viewportLeft = cursorCol;
      }
    }
    else if (cursorLine > 0)
    {
      std::string currentLine = buffer.getLine(cursorLine);
      std::string prevLine = buffer.getLine(cursorLine - 1);
      size_t byte_pos = buffer.lineColToPos(cursorLine - 1, prevLine.length());

      int oldCursorLine = cursorLine;
      cursorCol = static_cast<int>(prevLine.length());
      cursorLine--;

      buffer.replaceLine(cursorLine, prevLine + currentLine);
      buffer.deleteLine(cursorLine + 1);

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, cursorCol, oldCursorLine, 0,
            cursorLine, cursorCol);

        syntaxHighlighter->invalidateLineRange(cursorLine,
                                               buffer.getLineCount() - 1);
      }
    }

    delta.postCursorLine = cursorLine;
    delta.postCursorCol = cursorCol;
    delta.postViewportTop = viewportTop;
    delta.postViewportLeft = viewportLeft;

    ValidationResult valid = validateState("After backspace");
    if (valid)
    {
      addDelta(delta);
      if (delta.operation == EditDelta::JOIN_LINES)
      {
        commitDeltaGroup();
        beginDeltaGroup();
      }
    }
    else
    {
      std::cerr << "VALIDATION FAILED in backspace\n";
      std::cerr << valid.error << "\n";
    }

    markModified();
  }
  else if (!isUndoRedoing)
  {
    saveState();

    if (cursorCol > 0)
    {
      size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol - 1);
      std::string line = buffer.getLine(cursorLine);
      line.erase(cursorCol - 1, 1);
      buffer.replaceLine(cursorLine, line);
      cursorCol--;

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine,
            cursorCol + 1, cursorLine, cursorCol);
        // NEW: Always invalidate cache after edit
        syntaxHighlighter->invalidateLineCache(cursorLine);
      }

      if (cursorCol < viewportLeft)
      {
        viewportLeft = cursorCol;
      }

      markModified();
    }
    else if (cursorLine > 0)
    {
      std::string currentLine = buffer.getLine(cursorLine);
      std::string prevLine = buffer.getLine(cursorLine - 1);
      size_t byte_pos = buffer.lineColToPos(cursorLine - 1, prevLine.length());

      cursorCol = static_cast<int>(prevLine.length());
      cursorLine--;

      buffer.replaceLine(cursorLine, prevLine + currentLine);
      buffer.deleteLine(cursorLine + 1);

      if (syntaxHighlighter && !isUndoRedoing)
      {
        syntaxHighlighter->updateTreeAfterEdit(
            buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine + 1, 0,
            cursorLine, cursorCol);
        syntaxHighlighter->invalidateLineRange(cursorLine,
                                               buffer.getLineCount() - 1);
      }

      markModified();
    }
  }
}

void Editor::deleteLine()
{
  // SAVE STATE BEFORE MODIFICATION
  if (!isUndoRedoing)
  {
    saveState();
  }

  if (buffer.getLineCount() == 1)
  {
    std::string line = buffer.getLine(0);
    size_t byte_pos = 0;

    buffer.replaceLine(0, "");
    cursorCol = 0;

    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(buffer, byte_pos, line.length(), 0,
                                             0, 0, 0, (uint32_t)line.length(),
                                             0, 0);
      syntaxHighlighter->invalidateLineRange(0, 0);
    }

    // buffer.replaceLine(0, "");
    // cursorCol = 0;
  }
  else
  {
    size_t byte_pos = buffer.lineColToPos(cursorLine, 0);
    std::string line = buffer.getLine(cursorLine);
    size_t line_length = line.length();

    bool has_newline = (cursorLine < buffer.getLineCount() - 1);
    size_t delete_bytes = line_length + (has_newline ? 1 : 0);

    buffer.deleteLine(cursorLine);

    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(
          buffer, byte_pos, delete_bytes, 0, cursorLine, 0,
          cursorLine + (has_newline ? 1 : 0),
          has_newline ? 0 : (uint32_t)line_length, cursorLine, 0);
      syntaxHighlighter->invalidateLineRange(cursorLine,
                                             buffer.getLineCount() - 1);
    }

    if (cursorLine >= buffer.getLineCount())
    {
      cursorLine = buffer.getLineCount() - 1;
    }

    line = buffer.getLine(cursorLine);
    if (cursorCol > static_cast<int>(line.length()))
    {
      cursorCol = static_cast<int>(line.length());
    }
  }

  validateCursorAndViewport();
  markModified();
}

// =================================================================
// Undo/Redo System
// =================================================================

void Editor::deleteSelection()
{
  if (!hasSelection && !isSelecting)
  {
    return;
  }

  auto selection = getNormalizedSelection();
  int startLine = selection.first.first;
  int startCol = selection.first.second;
  int endLine = selection.second.first;
  int endCol = selection.second.second;

  // Validate bounds
  if (startLine < 0 || startLine >= buffer.getLineCount() || endLine < 0 ||
      endLine >= buffer.getLineCount())
  {
    std::cerr << "Warning: Cannot delete - selection out of bounds\n";
    clearSelection();
    return;
  }

  if (useDeltaUndo_ && !isUndoRedoing)
  {
    EditorSnapshot before = captureSnapshot();
    EditDelta delta = createDeltaForDeleteSelection();

    size_t start_byte = buffer.lineColToPos(startLine, startCol);
    size_t end_byte = buffer.lineColToPos(endLine, endCol);
    size_t delete_bytes = (end_byte > start_byte) ? (end_byte - start_byte) : 0;

    // Perform the deletion
    if (startLine == endLine)
    {
      // Single line deletion
      std::string line = buffer.getLine(startLine);
      startCol =
          std::max(0, std::min(startCol, static_cast<int>(line.length())));
      endCol = std::max(0, std::min(endCol, static_cast<int>(line.length())));

      if (endCol > startCol)
      {
        line.erase(startCol, endCol - startCol);
        buffer.replaceLine(startLine, line);
      }
    }
    else
    {
      // Multi-line deletion
      std::string firstLine = buffer.getLine(startLine);
      std::string lastLine = buffer.getLine(endLine);

      startCol =
          std::max(0, std::min(startCol, static_cast<int>(firstLine.length())));
      endCol =
          std::max(0, std::min(endCol, static_cast<int>(lastLine.length())));

      std::string newLine =
          firstLine.substr(0, startCol) + lastLine.substr(endCol);
      buffer.replaceLine(startLine, newLine);

      // Delete intermediate lines
      for (int i = endLine; i > startLine; i--)
      {
        buffer.deleteLine(i);
      }
    }

    // Update syntax highlighter
    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(buffer, start_byte, delete_bytes,
                                             0, startLine, startCol, endLine,
                                             endCol, startLine, startCol);
      syntaxHighlighter->invalidateLineRange(startLine,
                                             buffer.getLineCount() - 1);
    }

    updateCursorAndViewport(startLine, startCol);
    clearSelection();

    delta.postCursorLine = cursorLine;
    delta.postCursorCol = cursorCol;
    delta.postViewportTop = viewportTop;
    delta.postViewportLeft = viewportLeft;

    ValidationResult valid = validateState("After deleteSelection");
    if (valid)
    {
      addDelta(delta);
      commitDeltaGroup();
      beginDeltaGroup();
    }
    else
    {
      std::cerr << "VALIDATION FAILED in deleteSelection\n";
      std::cerr << valid.error << "\n";
    }

    markModified();
  }
  else if (!isUndoRedoing)
  {
    // Fallback: full-state undo
    saveState();

    size_t start_byte = buffer.lineColToPos(startLine, startCol);
    size_t end_byte = buffer.lineColToPos(endLine, endCol);
    size_t delete_bytes = (end_byte > start_byte) ? (end_byte - start_byte) : 0;

    if (startLine == endLine)
    {
      std::string line = buffer.getLine(startLine);
      startCol =
          std::max(0, std::min(startCol, static_cast<int>(line.length())));
      endCol = std::max(0, std::min(endCol, static_cast<int>(line.length())));

      if (endCol > startCol)
      {
        line.erase(startCol, endCol - startCol);
        buffer.replaceLine(startLine, line);
      }
    }
    else
    {
      std::string firstLine = buffer.getLine(startLine);
      std::string lastLine = buffer.getLine(endLine);

      startCol =
          std::max(0, std::min(startCol, static_cast<int>(firstLine.length())));
      endCol =
          std::max(0, std::min(endCol, static_cast<int>(lastLine.length())));

      std::string newLine =
          firstLine.substr(0, startCol) + lastLine.substr(endCol);
      buffer.replaceLine(startLine, newLine);

      for (int i = endLine; i > startLine; i--)
      {
        buffer.deleteLine(i);
      }
    }

    if (syntaxHighlighter && !isUndoRedoing)
    {
      syntaxHighlighter->updateTreeAfterEdit(buffer, start_byte, delete_bytes,
                                             0, startLine, startCol, endLine,
                                             endCol, startLine, startCol);
      syntaxHighlighter->invalidateLineRange(startLine,
                                             buffer.getLineCount() - 1);
    }

    updateCursorAndViewport(startLine, startCol);
    clearSelection();
    markModified();
  }
}

void Editor::undo()
{
  if (useDeltaUndo_)
  {
    // Commit any pending delta group first
    if (!currentDeltaGroup_.isEmpty())
    {
      commitDeltaGroup();
    }

    if (deltaUndoStack_.empty())
    {
      return;
    }

#ifdef DEBUG_DELTA_UNDO
    std::cerr << "\n=== UNDO START ===\n";
    EditorSnapshot beforeUndo = captureSnapshot();
#endif

    // Get the delta group to undo
    DeltaGroup group = deltaUndoStack_.top();
    deltaUndoStack_.pop();

#ifdef DEBUG_DELTA_UNDO
    std::cerr << "Undoing group:\n" << group.toString() << "\n";
#endif

    // Track affected line range for incremental highlighting
    int minAffectedLine = buffer.getLineCount();
    int maxAffectedLine = 0;

    // Apply deltas in REVERSE order
    for (auto it = group.deltas.rbegin(); it != group.deltas.rend(); ++it)
    {
      // Track which lines are affected
      minAffectedLine =
          std::min(minAffectedLine, std::min(it->startLine, it->preCursorLine));
      maxAffectedLine =
          std::max(maxAffectedLine, std::max(it->endLine, it->postCursorLine));

      applyDeltaReverse(*it);

#ifdef DEBUG_DELTA_UNDO
      ValidationResult valid = validateState("After undo delta");
      if (!valid)
      {
        std::cerr << "CRITICAL: Validation failed during undo!\n";
        std::cerr << valid.error << "\n";
      }
#endif
    }

    // Save to redo stack
    deltaRedoStack_.push(group);

    // FIXED: Incremental syntax update instead of full reparse
    if (syntaxHighlighter)
    {
      // Only invalidate affected line range, not entire cache
      syntaxHighlighter->invalidateLineRange(minAffectedLine,
                                             buffer.getLineCount() - 1);

      // Use viewport-only parsing for immediate visual update
      syntaxHighlighter->parseViewportOnly(buffer, viewportTop);

      // Schedule background full reparse (non-blocking)
      syntaxHighlighter->scheduleBackgroundParse(buffer);
    }

    isModified = true;

#ifdef DEBUG_DELTA_UNDO
    EditorSnapshot afterUndo = captureSnapshot();
    std::cerr << "Affected lines: " << minAffectedLine << " to "
              << maxAffectedLine << "\n";
    std::cerr << "=== UNDO END ===\n\n";
#endif
  }
  else
  {
    // OLD: Full-state undo (fallback)
    if (undoStack.empty())
      return;

    isUndoRedoing = true;
    redoStack.push(getCurrentState());
    EditorState state = undoStack.top();
    undoStack.pop();
    restoreState(state);

    if (syntaxHighlighter)
    {
      syntaxHighlighter->bufferChanged(buffer);
    }

    isModified = true;
    isUndoRedoing = false;
  }
}

void Editor::redo()
{
  if (useDeltaUndo_)
  {
    if (deltaRedoStack_.empty())
    {
      return;
    }

#ifdef DEBUG_DELTA_UNDO
    std::cerr << "\n=== REDO START ===\n";
    EditorSnapshot beforeRedo = captureSnapshot();
#endif

    // Get the delta group to redo
    DeltaGroup group = deltaRedoStack_.top();
    deltaRedoStack_.pop();

#ifdef DEBUG_DELTA_UNDO
    std::cerr << "Redoing group:\n" << group.toString() << "\n";
#endif

    // Track affected line range
    int minAffectedLine = buffer.getLineCount();
    int maxAffectedLine = 0;

    // Apply deltas in FORWARD order
    for (const auto &delta : group.deltas)
    {
      minAffectedLine = std::min(
          minAffectedLine, std::min(delta.startLine, delta.preCursorLine));
      maxAffectedLine = std::max(maxAffectedLine,
                                 std::max(delta.endLine, delta.postCursorLine));

      applyDeltaForward(delta);

#ifdef DEBUG_DELTA_UNDO
      ValidationResult valid = validateState("After redo delta");
      if (!valid)
      {
        std::cerr << "CRITICAL: Validation failed during redo!\n";
        std::cerr << valid.error << "\n";
      }
#endif
    }

    // Save to undo stack
    deltaUndoStack_.push(group);

    // FIXED: Incremental syntax update
    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(minAffectedLine,
                                             buffer.getLineCount() - 1);
      syntaxHighlighter->parseViewportOnly(buffer, viewportTop);
      syntaxHighlighter->scheduleBackgroundParse(buffer);
    }

    isModified = true;

#ifdef DEBUG_DELTA_UNDO
    EditorSnapshot afterRedo = captureSnapshot();
    std::cerr << "Affected lines: " << minAffectedLine << " to "
              << maxAffectedLine << "\n";
    std::cerr << "=== REDO END ===\n\n";
#endif
  }
  else
  {
    // OLD: Full-state redo (fallback)
    if (redoStack.empty())
      return;

    isUndoRedoing = true;
    undoStack.push(getCurrentState());
    EditorState state = redoStack.top();
    redoStack.pop();
    restoreState(state);

    if (syntaxHighlighter)
    {
      syntaxHighlighter->bufferChanged(buffer);
    }

    isModified = true;
    isUndoRedoing = false;
  }
}

EditorState Editor::getCurrentState()
{
  EditorState state;

  // Your existing serialization code
  std::ostringstream oss;
  for (int i = 0; i < buffer.getLineCount(); i++)
  {
    oss << buffer.getLine(i);
    if (i < buffer.getLineCount() - 1)
    {
      oss << "\n";
    }
  }
  state.content = oss.str();

  // Save cursor/viewport state regardless
  state.cursorLine = cursorLine;
  state.cursorCol = cursorCol;
  state.viewportTop = viewportTop;
  state.viewportLeft = viewportLeft;

  return state;
}

void Editor::restoreState(const EditorState &state)
{
  // Clear buffer and reload content
  buffer.clear();

  std::istringstream iss(state.content);
  std::string line;
  int lineNum = 0;

  while (std::getline(iss, line))
  {
    buffer.insertLine(lineNum++, line);
  }

  // If no lines were added, add empty line
  if (buffer.getLineCount() == 0)
  {
    buffer.insertLine(0, "");
  }

  // Restore cursor and viewport
  cursorLine = state.cursorLine;
  cursorCol = state.cursorCol;
  viewportTop = state.viewportTop;
  viewportLeft = state.viewportLeft;

  validateCursorAndViewport();
}

void Editor::limitUndoStack()
{
  while (undoStack.size() > MAX_UNDO_LEVELS)
  {
    // Remove oldest state (bottom of stack)
    std::stack<EditorState> temp;
    bool first = true;

    while (!undoStack.empty())
    {
      if (first)
      {
        first = false;
        undoStack.pop(); // Skip the oldest
      }
      else
      {
        temp.push(undoStack.top());
        undoStack.pop();
      }
    }

    // Restore stack in correct order
    while (!temp.empty())
    {
      undoStack.push(temp.top());
      temp.pop();
    }
  }
}

// =================================================================
// Internal Helpers
// =================================================================

void Editor::markModified() { isModified = true; }

void Editor::splitLineAtCursor()
{
  std::string line = buffer.getLine(cursorLine);
  std::string leftPart = line.substr(0, cursorCol);
  std::string rightPart = line.substr(cursorCol);

  buffer.replaceLine(cursorLine, leftPart);
  buffer.insertLine(cursorLine + 1, rightPart);
}

void Editor::joinLineWithNext()
{
  if (cursorLine < buffer.getLineCount() - 1)
  {
    std::string currentLine = buffer.getLine(cursorLine);
    std::string nextLine = buffer.getLine(cursorLine + 1);

    buffer.replaceLine(cursorLine, currentLine + nextLine);
    buffer.deleteLine(cursorLine + 1);
  }
}

std::pair<std::pair<int, int>, std::pair<int, int>>
Editor::getNormalizedSelection()
{
  int startLine = selectionStartLine;
  int startCol = selectionStartCol;
  int endLine = selectionEndLine;
  int endCol = selectionEndCol;

  // Always normalize so start < end
  if (startLine > endLine || (startLine == endLine && startCol > endCol))
  {
    std::swap(startLine, endLine);
    std::swap(startCol, endCol);
  }

  return {{startLine, startCol}, {endLine, endCol}};
}

std::string Editor::getSelectedText()
{
  if (!hasSelection && !isSelecting)
  {
    return "";
  }

  auto [start, end] = getNormalizedSelection();
  int startLine = start.first;
  int startCol = start.second;
  int endLine = end.first;
  int endCol = end.second;

  // Validate bounds
  if (startLine < 0 || startLine >= buffer.getLineCount() || endLine < 0 ||
      endLine >= buffer.getLineCount())
  {
    std::cerr << "Warning: Selection out of bounds\n";
    return "";
  }

  std::ostringstream result;

  if (startLine == endLine)
  {
    // Single line selection
    std::string line = buffer.getLine(startLine);

    // Clamp columns to line length
    startCol = std::max(0, std::min(startCol, static_cast<int>(line.length())));
    endCol = std::max(0, std::min(endCol, static_cast<int>(line.length())));

    if (endCol > startCol)
    {
      result << line.substr(startCol, endCol - startCol);
    }
  }
  else
  {
    // Multi-line selection
    for (int i = startLine; i <= endLine; i++)
    {
      std::string line = buffer.getLine(i);

      if (i == startLine)
      {
        // First line: from startCol to end
        startCol =
            std::max(0, std::min(startCol, static_cast<int>(line.length())));
        result << line.substr(startCol);
      }
      else if (i == endLine)
      {
        // Last line: from start to endCol
        endCol = std::max(0, std::min(endCol, static_cast<int>(line.length())));
        result << line.substr(0, endCol);
      }
      else
      {
        // Middle lines: entire line
        result << line;
      }

      // Add newline between lines (but not after last line)
      if (i < endLine)
      {
        result << "\n";
      }
    }
  }

  return result.str();
}

// Selection management
void Editor::startSelectionIfNeeded()
{
  if (!hasSelection && !isSelecting)
  {
    isSelecting = true;
    selectionStartLine = cursorLine;
    selectionStartCol = cursorCol;
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
  }
}

void Editor::updateSelectionEnd()
{
  if (isSelecting || hasSelection)
  {
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
  }
}

// Clipboard operations
void Editor::copySelection()
{
  if (!hasSelection && !isSelecting)
  {
    return;
  }

  // Get the selected text
  std::string selectedText = getSelectedText();

  if (selectedText.empty())
  {
    std::cerr << "Warning: No text selected for copy\n";
    return;
  }

  // Store in internal clipboard (fallback)
  clipboard = selectedText;

  // Try to copy to system clipboard
  if (Clipboard::copyToSystemClipboard(selectedText))
  {
    // Success - optionally show a status message
    // std::cerr << "Copied " << selectedText.length() << " characters\n";
  }
  else
  {
    std::cerr << "Warning: Could not access system clipboard, using internal "
                 "clipboard\n";
  }
}

void Editor::cutSelection()
{
  if (!hasSelection && !isSelecting)
    return;

  copySelection();
  deleteSelection();
}

void Editor::pasteFromClipboard()
{
  // Try to get from system clipboard first
  std::string systemClipboard = Clipboard::getFromSystemClipboard();

  if (!systemClipboard.empty())
  {
    clipboard = systemClipboard;
  }

  if (clipboard.empty())
  {
    return;
  }

  // Create a delta group for the entire paste operation
  if (useDeltaUndo_ && !isUndoRedoing)
  {
    beginDeltaGroup();
  }
  else if (!isUndoRedoing)
  {
    saveState();
  }

  // Delete selection if any
  if (hasSelection || isSelecting)
  {
    deleteSelection();
  }

  // Insert clipboard content
  size_t start_byte = buffer.lineColToPos(cursorLine, cursorCol);
  int start_line = cursorLine;
  int start_col = cursorCol;

  // Split clipboard by lines and insert
  std::istringstream iss(clipboard);
  std::string line;
  bool firstLine = true;

  while (std::getline(iss, line))
  {
    if (!firstLine)
    {
      insertNewline();
    }

    // Insert each character of the line
    for (char ch : line)
    {
      insertChar(ch);
    }

    firstLine = false;
  }

  // If clipboard ended with newline, add it
  if (!clipboard.empty() && clipboard.back() == '\n')
  {
    insertNewline();
  }

  // Commit the paste as a single undo operation
  if (useDeltaUndo_ && !isUndoRedoing)
  {
    commitDeltaGroup();
    beginDeltaGroup();
  }

  markModified();
}

void Editor::selectAll()
{
  if (buffer.getLineCount() == 0)
    return;

  selectionStartLine = 0;
  selectionStartCol = 0;

  selectionEndLine = buffer.getLineCount() - 1;
  std::string lastLine = buffer.getLine(selectionEndLine);
  selectionEndCol = static_cast<int>(lastLine.length());

  hasSelection = true;
  isSelecting = false;
}

void Editor::initializeViewportHighlighting()
{
  if (syntaxHighlighter)
  {
    // Pre-parse viewport so first display() is instant
    syntaxHighlighter->parseViewportOnly(buffer, viewportTop);
  }
}

// Cursor
void Editor::setCursorMode()
{
  switch (currentMode)
  {
  case CursorMode::NORMAL:
    // Block cursor (solid block)
    printf("\033[2 q");
    fflush(stdout);
    break;
  case CursorMode::INSERT:
    // Vertical bar cursor (thin lifne like VSCode/modern editors)
    printf("\033[6 q");
    fflush(stdout);
    break;
  case CursorMode::VISUAL:
    // Underline cursor for visual mode
    printf("\033[4 q");
    fflush(stdout);
    break;
  default:
    printf("\033[6 q");
    fflush(stdout);
    break;
  }
}

// === Delta Group Management ===

void Editor::beginDeltaGroup()
{
  currentDeltaGroup_ = DeltaGroup();
  currentDeltaGroup_.initialLineCount = buffer.getLineCount();
  currentDeltaGroup_.initialBufferSize = buffer.size();
  currentDeltaGroup_.timestamp = std::chrono::steady_clock::now();
}

void Editor::addDelta(const EditDelta &delta)
{
  currentDeltaGroup_.addDelta(delta);

// DEBUG: Validate after every delta in debug builds
#ifdef DEBUG_DELTA_UNDO
  ValidationResult valid = validateState("After adding delta");
  if (!valid)
  {
    std::cerr << "VALIDATION FAILED after delta:\n";
    std::cerr << delta.toString() << "\n";
    std::cerr << "Error: " << valid.error << "\n";
    std::cerr << "Current state: " << captureSnapshot().toString() << "\n";
  }
#endif
}

void Editor::commitDeltaGroup()
{
  if (currentDeltaGroup_.isEmpty())
  {
    return;
  }

  // Validate before committing
  ValidationResult valid = validateState("Before committing delta group");
  if (!valid)
  {
    std::cerr << "WARNING: Invalid state before commit, discarding group\n";
    std::cerr << valid.error << "\n";
    currentDeltaGroup_ = DeltaGroup();
    return;
  }

  deltaUndoStack_.push(currentDeltaGroup_);

  // Clear redo stack on new edit
  while (!deltaRedoStack_.empty())
  {
    deltaRedoStack_.pop();
  }

  // Limit stack size
  while (deltaUndoStack_.size() > MAX_UNDO_LEVELS)
  {
    // Remove oldest (bottom of stack)
    std::stack<DeltaGroup> temp;
    bool first = true;
    while (!deltaUndoStack_.empty())
    {
      if (first)
      {
        first = false;
        deltaUndoStack_.pop(); // Discard oldest
      }
      else
      {
        temp.push(deltaUndoStack_.top());
        deltaUndoStack_.pop();
      }
    }
    while (!temp.empty())
    {
      deltaUndoStack_.push(temp.top());
      temp.pop();
    }
  }

  currentDeltaGroup_ = DeltaGroup();
}

// === Delta Creation for Each Operation ===

EditDelta Editor::createDeltaForInsertChar(char ch)
{
  EditDelta delta;
  delta.operation = EditDelta::INSERT_CHAR;

  // Capture state BEFORE edit
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;

  delta.startLine = cursorLine;
  delta.startCol = cursorCol;
  delta.endLine = cursorLine;
  delta.endCol = cursorCol;

  // Content: what we're inserting
  delta.insertedContent = std::string(1, ch);
  delta.deletedContent = ""; // Nothing deleted

  // No structural change
  delta.lineCountDelta = 0;

  // Post-state will be filled after edit completes
  return delta;
}

EditDelta Editor::createDeltaForDeleteChar()
{
  EditDelta delta;
  delta.operation = EditDelta::DELETE_CHAR;

  // Capture state BEFORE deletion
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;

  delta.startLine = cursorLine;
  delta.startCol = cursorCol;

  // Capture what we're about to delete
  std::string line = buffer.getLine(cursorLine);

  if (cursorCol < static_cast<int>(line.length()))
  {
    // Deleting a character on current line
    delta.deletedContent = std::string(1, line[cursorCol]);
    delta.endLine = cursorLine;
    delta.endCol = cursorCol + 1;
    delta.lineCountDelta = 0;
  }
  else if (cursorLine < buffer.getLineCount() - 1)
  {
    // Deleting newline - will join lines
    delta.operation = EditDelta::JOIN_LINES;
    delta.deletedContent = "\n";
    delta.endLine = cursorLine + 1;
    delta.endCol = 0;
    delta.lineCountDelta = -1;

    // Save line contents for reversal
    delta.firstLineBeforeJoin = line;
    delta.secondLineBeforeJoin = buffer.getLine(cursorLine + 1);
  }

  delta.insertedContent = ""; // Nothing inserted

  return delta;
}

EditDelta Editor::createDeltaForBackspace()
{
  EditDelta delta;
  delta.operation = EditDelta::DELETE_CHAR;

  // Capture state BEFORE deletion
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;

  if (cursorCol > 0)
  {
    // Deleting character before cursor on same line
    std::string line = buffer.getLine(cursorLine);
    delta.deletedContent = std::string(1, line[cursorCol - 1]);

    delta.startLine = cursorLine;
    delta.startCol = cursorCol - 1;
    delta.endLine = cursorLine;
    delta.endCol = cursorCol;
    delta.lineCountDelta = 0;
  }
  else if (cursorLine > 0)
  {
    // Backspace at line start - join with previous line
    delta.operation = EditDelta::JOIN_LINES;
    delta.deletedContent = "\n";

    std::string prevLine = buffer.getLine(cursorLine - 1);
    std::string currLine = buffer.getLine(cursorLine);

    delta.startLine = cursorLine - 1;
    delta.startCol = prevLine.length();
    delta.endLine = cursorLine;
    delta.endCol = 0;
    delta.lineCountDelta = -1;

    // Save line contents for reversal
    delta.firstLineBeforeJoin = prevLine;
    delta.secondLineBeforeJoin = currLine;
  }

  delta.insertedContent = ""; // Nothing inserted

  return delta;
}

EditDelta Editor::createDeltaForNewline()
{
  EditDelta delta;
  delta.operation = EditDelta::SPLIT_LINE;

  // Capture state BEFORE split
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;

  delta.startLine = cursorLine;
  delta.startCol = cursorCol;
  delta.endLine = cursorLine + 1; // New line will be created
  delta.endCol = 0;

  // Save the line content before split
  delta.lineBeforeSplit = buffer.getLine(cursorLine);

  delta.insertedContent = "\n";
  delta.deletedContent = "";
  delta.lineCountDelta = 1; // One new line

  return delta;
}

EditDelta Editor::createDeltaForDeleteSelection()
{
  EditDelta delta;
  delta.operation = EditDelta::DELETE_TEXT;

  // Capture state
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;

  auto [start, end] = getNormalizedSelection();
  delta.startLine = start.first;
  delta.startCol = start.second;
  delta.endLine = end.first;
  delta.endCol = end.second;

  // Capture the deleted text
  delta.deletedContent = getSelectedText();
  delta.insertedContent = "";

  // Calculate line count change
  delta.lineCountDelta = -(end.first - start.first);

  return delta;
}

// === Memory Usage Stats ===

size_t Editor::getUndoMemoryUsage() const
{
  size_t total = 0;

  if (useDeltaUndo_)
  {
    // Count delta stack
    std::stack<DeltaGroup> temp = deltaUndoStack_;
    while (!temp.empty())
    {
      total += temp.top().getMemorySize();
      temp.pop();
    }
  }
  else
  {
    // Count old state stack (approximate)
    total = undoStack.size() * sizeof(EditorState);
    std::stack<EditorState> temp = undoStack;
    while (!temp.empty())
    {
      total += temp.top().content.capacity();
      temp.pop();
    }
  }

  return total;
}

size_t Editor::getRedoMemoryUsage() const
{
  size_t total = 0;

  if (useDeltaUndo_)
  {
    std::stack<DeltaGroup> temp = deltaRedoStack_;
    while (!temp.empty())
    {
      total += temp.top().getMemorySize();
      temp.pop();
    }
  }
  else
  {
    total = redoStack.size() * sizeof(EditorState);
    std::stack<EditorState> temp = redoStack;
    while (!temp.empty())
    {
      total += temp.top().content.capacity();
      temp.pop();
    }
  }

  return total;
}

void Editor::applyDeltaForward(const EditDelta &delta)
{
  isUndoRedoing = true;

#ifdef DEBUG_DELTA_UNDO
  std::cerr << "Applying delta forward: " << delta.toString() << "\n";
#endif

  // Restore cursor to PRE-edit position
  cursorLine = delta.preCursorLine;
  cursorCol = delta.preCursorCol;
  viewportTop = delta.preViewportTop;
  viewportLeft = delta.preViewportLeft;

  validateCursorAndViewport();

  // Notify Tree-sitter BEFORE applying changes
  // notifyTreeSitterEdit(delta, false); // false = forward (redo)

  switch (delta.operation)
  {
  case EditDelta::INSERT_CHAR:
  case EditDelta::INSERT_TEXT:
  {
    // Re-insert the text
    std::string line = buffer.getLine(cursorLine);
    line.insert(cursorCol, delta.insertedContent);
    buffer.replaceLine(cursorLine, line);
    cursorCol += delta.insertedContent.length();
    break;
  }

  case EditDelta::DELETE_CHAR:
  case EditDelta::DELETE_TEXT:
  {
    // Re-delete the text
    if (delta.startLine == delta.endLine)
    {
      std::string line = buffer.getLine(delta.startLine);
      line.erase(delta.startCol, delta.deletedContent.length());
      buffer.replaceLine(delta.startLine, line);
    }
    else
    {
      // Multi-line deletion
      std::string firstLine = buffer.getLine(delta.startLine);
      std::string lastLine = buffer.getLine(delta.endLine);
      std::string newLine =
          firstLine.substr(0, delta.startCol) + lastLine.substr(delta.endCol);
      buffer.replaceLine(delta.startLine, newLine);

      for (int i = delta.endLine; i > delta.startLine; i--)
      {
        buffer.deleteLine(i);
      }
    }
    break;
  }

  case EditDelta::SPLIT_LINE:
  {
    // Re-split the line
    std::string line = buffer.getLine(cursorLine);
    std::string leftPart = line.substr(0, cursorCol);
    std::string rightPart = line.substr(cursorCol);

    buffer.replaceLine(cursorLine, leftPart);
    buffer.insertLine(cursorLine + 1, rightPart);

    cursorLine++;
    cursorCol = 0;
    break;
  }

  case EditDelta::JOIN_LINES:
  {
    // Re-join the lines
    if (delta.startLine + 1 < buffer.getLineCount())
    {
      std::string firstLine = buffer.getLine(delta.startLine);
      std::string secondLine = buffer.getLine(delta.startLine + 1);
      buffer.replaceLine(delta.startLine, firstLine + secondLine);
      buffer.deleteLine(delta.startLine + 1);
    }
    break;
  }

  case EditDelta::REPLACE_LINE:
  {
    if (!delta.insertedContent.empty())
    {
      buffer.replaceLine(delta.startLine, delta.insertedContent);
    }
    break;
  }
  }

  // Restore POST-edit cursor position
  cursorLine = delta.postCursorLine;
  cursorCol = delta.postCursorCol;
  viewportTop = delta.postViewportTop;
  viewportLeft = delta.postViewportLeft;

  validateCursorAndViewport();
  buffer.invalidateLineIndex();

  // Invalidate only affected lines (not entire cache)
  if (syntaxHighlighter)
  {
    int startLine = std::min(delta.startLine, delta.preCursorLine);
    int endLine = std::max(delta.endLine, delta.postCursorLine);
    syntaxHighlighter->invalidateLineRange(startLine,
                                           buffer.getLineCount() - 1);
  }

  isUndoRedoing = false;
}

// === Apply Delta Reverse (for Undo) ===

void Editor::applyDeltaReverse(const EditDelta &delta)
{
  isUndoRedoing = true;

#ifdef DEBUG_DELTA_UNDO
  std::cerr << "Applying delta reverse: " << delta.toString() << "\n";
#endif

  // Restore cursor to POST-edit position
  cursorLine = delta.postCursorLine;
  cursorCol = delta.postCursorCol;
  viewportTop = delta.postViewportTop;
  viewportLeft = delta.postViewportLeft;

  validateCursorAndViewport();

  switch (delta.operation)
  {
  case EditDelta::INSERT_CHAR:
  case EditDelta::INSERT_TEXT:
  {
    // Reverse of insert is delete
    std::string line = buffer.getLine(delta.startLine);
    if (delta.startCol + delta.insertedContent.length() <= line.length())
    {
      line.erase(delta.startCol, delta.insertedContent.length());
      buffer.replaceLine(delta.startLine, line);
    }
    break;
  }

  case EditDelta::DELETE_CHAR:
  case EditDelta::DELETE_TEXT:
  {
    // FIXED: Proper multi-line restoration
    if (delta.startLine == delta.endLine)
    {
      // Single line restoration (simple case)
      std::string line = buffer.getLine(delta.startLine);
      line.insert(delta.startCol, delta.deletedContent);
      buffer.replaceLine(delta.startLine, line);
    }
    else
    {
      // Multi-line restoration (the bug was here!)

      // Step 1: Get the current line at startLine
      std::string currentLine = buffer.getLine(delta.startLine);

      // Step 2: Split current line at insertion point
      std::string beforeInsert = currentLine.substr(0, delta.startCol);
      std::string afterInsert = currentLine.substr(delta.startCol);

      // Step 3: Split deletedContent by ACTUAL newlines, preserving them
      std::vector<std::string> linesToRestore;
      size_t pos = 0;
      size_t nextNewline;

      while ((nextNewline = delta.deletedContent.find('\n', pos)) !=
             std::string::npos)
      {
        // Include everything up to (but not including) the newline
        linesToRestore.push_back(
            delta.deletedContent.substr(pos, nextNewline - pos));
        pos = nextNewline + 1;
      }

      // Add remaining content (after last newline)
      if (pos < delta.deletedContent.length())
      {
        linesToRestore.push_back(delta.deletedContent.substr(pos));
      }

      // Step 4: Reconstruct lines correctly
      if (!linesToRestore.empty())
      {
        // First line: beforeInsert + first restored line
        buffer.replaceLine(delta.startLine, beforeInsert + linesToRestore[0]);

        // Insert middle lines (if any)
        for (size_t i = 1; i < linesToRestore.size(); ++i)
        {
          buffer.insertLine(delta.startLine + i, linesToRestore[i]);
        }

        // Handle the content after insertion point
        if (linesToRestore.size() == 1)
        {
          // Single line case: append afterInsert to same line
          std::string finalLine = buffer.getLine(delta.startLine);
          buffer.replaceLine(delta.startLine, finalLine + afterInsert);
        }
        else
        {
          // Multi-line case: append afterInsert to last restored line
          int lastLineIdx = delta.startLine + linesToRestore.size() - 1;
          std::string lastLine = buffer.getLine(lastLineIdx);
          buffer.replaceLine(lastLineIdx, lastLine + afterInsert);
        }
      }
      else
      {
        // Edge case: deletedContent was empty (shouldn't happen, but be safe)
        std::cerr << "WARNING: Empty deletedContent in multi-line restore\n";
      }
    }
    break;
  }

  case EditDelta::SPLIT_LINE:
  {
    // Reverse of split is join
    if (!delta.lineBeforeSplit.empty())
    {
      if (delta.startLine + 1 < buffer.getLineCount())
      {
        buffer.replaceLine(delta.startLine, delta.lineBeforeSplit);
        buffer.deleteLine(delta.startLine + 1);
      }
    }
    break;
  }

  case EditDelta::JOIN_LINES:
  {
    // Reverse of join is split
    if (!delta.firstLineBeforeJoin.empty() &&
        !delta.secondLineBeforeJoin.empty())
    {
      buffer.replaceLine(delta.startLine, delta.firstLineBeforeJoin);
      buffer.insertLine(delta.startLine + 1, delta.secondLineBeforeJoin);
    }
    break;
  }

  case EditDelta::REPLACE_LINE:
  {
    // Reverse of replace is restore original
    if (!delta.deletedContent.empty())
    {
      buffer.replaceLine(delta.startLine, delta.deletedContent);
    }
    break;
  }
  }

  // Restore PRE-edit cursor position
  cursorLine = delta.preCursorLine;
  cursorCol = delta.preCursorCol;
  viewportTop = delta.preViewportTop;
  viewportLeft = delta.preViewportLeft;

  validateCursorAndViewport();
  buffer.invalidateLineIndex();

  isUndoRedoing = false;
}

// Fallback
void Editor::saveState()
{
  if (isSaving || isUndoRedoing)
    return;

  auto now = std::chrono::steady_clock::now();
  auto elapsed =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEditTime)
          .count();

  // Only save if enough time has passed since last edit
  if (elapsed > UNDO_GROUP_TIMEOUT_MS || undoStack.empty())
  {
    EditorState state = getCurrentState();
    undoStack.push(state);
    limitUndoStack();

    while (!redoStack.empty())
    {
      redoStack.pop();
    }
  }

  lastEditTime = now;
}

void Editor::optimizedLineInvalidation(int startLine, int endLine)
{
  if (!syntaxHighlighter)
  {
    return;
  }

  // Only invalidate if change is significant
  int changeSize = endLine - startLine + 1;

  if (changeSize > 100)
  {
    // Large change: full reparse (but async)
    syntaxHighlighter->clearAllCache();
    syntaxHighlighter->scheduleBackgroundParse(buffer);
  }
  else if (changeSize > 10)
  {
    // Medium change: invalidate range and reparse viewport
    syntaxHighlighter->invalidateLineRange(startLine,
                                           buffer.getLineCount() - 1);
    syntaxHighlighter->parseViewportOnly(buffer, viewportTop);
  }
  else
  {
    // Small change: just invalidate the affected lines
    syntaxHighlighter->invalidateLineRange(startLine, endLine);
  }
}

// ============================================================================
// FIX 4: Add Tree-sitter Edit Notification for Delta Operations
// ============================================================================
// Add this method to track Tree-sitter edits during delta apply:

void Editor::notifyTreeSitterEdit(const EditDelta &delta, bool isReverse)
{
  if (!syntaxHighlighter)
  {
    return;
  }

  // Calculate byte positions
  size_t start_byte = buffer.lineColToPos(delta.startLine, delta.startCol);

  if (isReverse)
  {
    // Undoing: reverse the original operation
    switch (delta.operation)
    {
    case EditDelta::INSERT_CHAR:
    case EditDelta::INSERT_TEXT:
    {
      // Was an insert, now delete
      size_t len = delta.insertedContent.length();
      syntaxHighlighter->notifyEdit(start_byte, 0, len, // Inserting back
                                    delta.startLine, delta.startCol,
                                    delta.startLine, delta.startCol,
                                    delta.postCursorLine, delta.postCursorCol);
      break;
    }

    case EditDelta::DELETE_CHAR:
    case EditDelta::DELETE_TEXT:
    {
      // Was a delete, now insert
      size_t len = delta.deletedContent.length();
      syntaxHighlighter->notifyEdit(start_byte, len, 0, // Deleting
                                    delta.startLine, delta.startCol,
                                    delta.endLine, delta.endCol,
                                    delta.startLine, delta.startCol);
      break;
    }

    case EditDelta::SPLIT_LINE:
    {
      // Was a split, now join
      syntaxHighlighter->notifyEdit(start_byte, 0, 1, // Remove newline
                                    delta.startLine, delta.startCol,
                                    delta.startLine, delta.startCol,
                                    delta.startLine + 1, 0);
      break;
    }

    case EditDelta::JOIN_LINES:
    {
      // Was a join, now split
      syntaxHighlighter->notifyEdit(start_byte, 1, 0, // Add newline
                                    delta.startLine, delta.startCol,
                                    delta.startLine + 1, 0, delta.startLine,
                                    delta.startCol);
      break;
    }
    }
  }
  else
  {
    // Redoing: apply the original operation
    switch (delta.operation)
    {
    case EditDelta::INSERT_CHAR:
    case EditDelta::INSERT_TEXT:
    {
      size_t len = delta.insertedContent.length();
      syntaxHighlighter->notifyEdit(start_byte, len, 0, delta.startLine,
                                    delta.startCol, delta.postCursorLine,
                                    delta.postCursorCol, delta.startLine,
                                    delta.startCol);
      break;
    }

    case EditDelta::DELETE_CHAR:
    case EditDelta::DELETE_TEXT:
    {
      size_t len = delta.deletedContent.length();
      syntaxHighlighter->notifyEdit(
          start_byte, 0, len, delta.startLine, delta.startCol, delta.startLine,
          delta.startCol, delta.endLine, delta.endCol);
      break;
    }

    case EditDelta::SPLIT_LINE:
    {
      syntaxHighlighter->notifyEdit(start_byte, 1, 0, delta.startLine,
                                    delta.startCol, delta.startLine + 1, 0,
                                    delta.startLine, delta.startCol);
      break;
    }

    case EditDelta::JOIN_LINES:
    {
      syntaxHighlighter->notifyEdit(start_byte, 0, 1, delta.startLine,
                                    delta.startCol, delta.startLine,
                                    delta.startCol, delta.startLine + 1, 0);
      break;
    }
    }
  }
}