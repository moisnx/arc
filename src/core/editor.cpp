#include "editor.h"
// #include "src/ui/colors.h"
#include "src/core/config_manager.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
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

Editor::Editor(SyntaxHighlighter *highlighter) : syntaxHighlighter(highlighter)
{
  tabSize = ConfigManager::getTabSize();
}
void Editor::reloadConfig()
{
  tabSize = ConfigManager::getTabSize();
  // Trigger redisplay to reflect changes
}

// =================================================================
// Mode Management
// =================================================================

void Editor::setMode(EditorMode newMode)
{
  if (currentMode != newMode)
  {
    // Only save state for INSERT mode if we're not in the middle of a save
    // operation
    if (newMode == EditorMode::INSERT && currentMode == EditorMode::NORMAL &&
        !isSaving)
    {
      saveState();
    }
    currentMode = newMode;
    updateCursorStyle();
  }
}

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
#ifdef _WIN32
      // Force cursor update on Windows
      refresh();
#endif
    }
    else
    {
      move(screenRow, contentStartCol);
#ifdef _WIN32
      refresh();
#endif
    }
  }
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

// In editor.cpp - Modified display() function
void Editor::display()
{
  if (!validateEditorState())
  {
    std::cerr << "DISPLAY ERROR: Invalid state detected!" << std::endl;
    validateCursorAndViewport();
    if (!validateEditorState())
    {
      std::cerr << "CRITICAL: Could not fix invalid state!" << std::endl;
      return;
    }
  }

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  viewportHeight = rows - 1;

  bool show_line_numbers = ConfigManager::getLineNumbers();
  int lineNumWidth =
      show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
  int contentStartCol = show_line_numbers ? (lineNumWidth + 3) : 0;
  int contentWidth = cols - contentStartCol;

  int endLine = viewportTop + viewportHeight;
  if (endLine > buffer.getLineCount())
  {
    endLine = buffer.getLineCount();
  }

  // Pre-compute selection bounds ONCE
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

  if (syntaxHighlighter)
  {
    syntaxHighlighter->markViewportLines(viewportTop, endLine - 1);
  }

  int currentTabSize = ConfigManager::getTabSize();

  // Render lines
  for (int i = viewportTop; i < endLine; i++)
  {
    int screenRow = i - viewportTop;
    bool isCurrentLine = (cursorLine == i);

    move(screenRow, 0);
    attrset(COLOR_PAIR(0));

    // Render line numbers
    if (show_line_numbers)
    {
      int ln_colorPair = isCurrentLine ? 3 : 2;
      attron(COLOR_PAIR(ln_colorPair));
      printw("%*d ", lineNumWidth, i + 1);
      attroff(COLOR_PAIR(ln_colorPair));

      attron(COLOR_PAIR(4));
      addch(' ');
      attroff(COLOR_PAIR(4));
      addch(' ');
    }

    attrset(COLOR_PAIR(0));

    std::string expandedLine = expandTabs(buffer.getLine(i), currentTabSize);
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

    int current_span_idx = 0;
    int num_spans = currentLineSpans.size();

    // Content rendering
    for (int screenCol = 0; screenCol < contentWidth; screenCol++)
    {
      int fileCol = viewportLeft + screenCol;
      bool charExists =
          (fileCol >= 0 && fileCol < static_cast<int>(expandedLine.length()));
      char ch = charExists ? expandedLine[fileCol] : ' ';

      if (charExists && (ch < 32 || ch > 126))
      {
        ch = ' ';
      }

      bool isSelected = false;
      if (lineHasSelection && charExists)
      {
        if (sel_start_line == sel_end_line)
        {
          isSelected = (fileCol >= sel_start_col && fileCol < sel_end_col);
        }
        else if (i == sel_start_line)
        {
          isSelected = (fileCol >= sel_start_col);
        }
        else if (i == sel_end_line)
        {
          isSelected = (fileCol < sel_end_col);
        }
        else
        {
          isSelected = true;
        }
      }

      if (isSelected)
      {
        attron(COLOR_PAIR(14) | A_REVERSE);
        addch(ch);
        attroff(COLOR_PAIR(14) | A_REVERSE);
      }
      else
      {
        bool colorApplied = false;

        if (charExists && num_spans > 0)
        {
          while (current_span_idx < num_spans &&
                 currentLineSpans[current_span_idx].end <= fileCol)
          {
            current_span_idx++;
          }

          if (current_span_idx < num_spans)
          {
            const auto &span = currentLineSpans[current_span_idx];
            if (fileCol >= span.start && fileCol < span.end)
            {
              if (span.colorPair >= 0 && span.colorPair < COLOR_PAIRS)
              {
                attron(COLOR_PAIR(span.colorPair));
                if (span.attribute != 0)
                {
                  attron(span.attribute);
                }
                addch(ch);
                if (span.attribute != 0)
                {
                  attroff(span.attribute);
                }
                attroff(COLOR_PAIR(span.colorPair));
                colorApplied = true;
              }
            }
          }
        }

        if (!colorApplied)
        {
          attrset(COLOR_PAIR(0));
          addch(ch);
        }
      }
    }

    attrset(COLOR_PAIR(0));
    clrtoeol();
  }

  attrset(COLOR_PAIR(0));
  for (int i = endLine - viewportTop; i < viewportHeight; i++)
  {
    move(i, 0);
    clrtoeol();
  }

  drawStatusBar();
  positionCursor();

  // REMOVED: positionCursor() call
  // Cursor positioning now happens AFTER doupdate() in main loop
}

// Also fix the drawStatusBar function
void Editor::drawStatusBar()
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int statusRow = rows - 1;

  move(statusRow, 0);

  // CRITICAL: Set status bar background before clearing
  attrset(COLOR_PAIR(STATUS_BAR));
  clrtoeol(); // Clear entire line with status bar background

  move(statusRow, 0);

  // Show current mode
  std::string modeStr;
  int modeColor;

  switch (currentMode)
  {
  case EditorMode::NORMAL:
    modeStr = " NORMAL ";
    modeColor = STATUS_BAR;
    break;
  case EditorMode::INSERT:
    modeStr = " INSERT ";
    modeColor = STATUS_BAR_ACTIVE;
    break;
  case EditorMode::VISUAL:
    modeStr = " VISUAL ";
    modeColor = STATUS_BAR_ACTIVE;
    break;
  }

  attron(COLOR_PAIR(modeColor) | A_BOLD);
  printw("%s", modeStr.c_str());
  attroff(COLOR_PAIR(modeColor) | A_BOLD);

  attron(COLOR_PAIR(STATUS_BAR));
  printw(" ");

  attron(COLOR_PAIR(STATUS_BAR_CYAN) | A_BOLD);
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
  attroff(COLOR_PAIR(STATUS_BAR_CYAN) | A_BOLD);

  // Show modified indicator
  if (isModified)
  {
    attron(COLOR_PAIR(STATUS_BAR_ACTIVE) | A_BOLD);
    printw(" [+]");
    attroff(COLOR_PAIR(STATUS_BAR_ACTIVE) | A_BOLD);
  }

  std::string ext = getFileExtension();
  if (!ext.empty())
  {
    attron(COLOR_PAIR(STATUS_BAR_ACTIVE));
    printw(" [%s]", ext.c_str());
    attroff(COLOR_PAIR(STATUS_BAR_ACTIVE));
  }

  // Right section with position info
  char rightSection[256];
  if (hasSelection || currentMode == EditorMode::VISUAL)
  {
    int startL = selectionStartLine, startC = selectionStartCol;
    int endL = selectionEndLine, endC = selectionEndCol;
    if (startL > endL || (startL == endL && startC > endC))
    {
      std::swap(startL, endL);
      std::swap(startC, endC);
    }

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
  attron(COLOR_PAIR(STATUS_BAR));
  for (int i = currentPos; i < rightStart && i < cols; i++)
  {
    move(statusRow, i);
    addch(' ');
  }
  attroff(COLOR_PAIR(STATUS_BAR));

  // Right section
  if (rightStart < cols)
  {
    move(statusRow, rightStart);

    if (hasSelection || currentMode == EditorMode::VISUAL)
    {
      attron(COLOR_PAIR(STATUS_BAR_GREEN) | A_BOLD);
    }
    else
    {
      attron(COLOR_PAIR(STATUS_BAR_YELLOW) | A_BOLD);
    }

    int remaining = cols - rightStart;
    if (remaining > 0)
    {
      std::string truncated = rightSection;
      if (static_cast<int>(truncated.length()) > remaining)
      {
        truncated = truncated.substr(0, remaining - 3) + "...";
      }
      printw("%s", truncated.c_str());
    }

    attroff(COLOR_PAIR(STATUS_BAR_GREEN) | A_BOLD);
    attroff(COLOR_PAIR(STATUS_BAR_YELLOW) | A_BOLD);
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
      if (currentMode == EditorMode::VISUAL)
      {
        hasSelection = false;
        isSelecting = true;
        selectionStartLine = fileRow;
        selectionStartCol = fileCol;
        selectionEndLine = fileRow;
        selectionEndCol = fileCol;
      }
      else
      {
        clearSelection();
      }
      updateCursorAndViewport(fileRow, fileCol);
    }
  }
  else if (event.bstate & BUTTON1_RELEASED)
  {
    if (isSelecting && currentMode == EditorMode::VISUAL)
    {
      int fileRow, fileCol;
      if (mouseToFilePos(event.y, event.x, fileRow, fileCol))
      {
        selectionEndLine = fileRow;
        selectionEndCol = fileCol;
        if (selectionStartLine != selectionEndLine ||
            selectionStartCol != selectionEndCol)
        {
          hasSelection = true;
        }
        updateCursorAndViewport(fileRow, fileCol);
      }
      isSelecting = false;
    }
  }
  else if ((event.bstate & REPORT_MOUSE_POSITION) && isSelecting &&
           currentMode == EditorMode::VISUAL)
  {
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
    int fileRow, fileCol;
    if (mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      updateCursorAndViewport(fileRow, fileCol);
      if (currentMode != EditorMode::VISUAL)
      {
        clearSelection();
      }
    }
  }
  else if (event.bstate & BUTTON4_PRESSED)
  {
    scrollUp();
  }
  else if (event.bstate & BUTTON5_PRESSED)
  {
    scrollDown();
  }
}

void Editor::clearSelection()
{
  hasSelection = false;
  isSelecting = false;
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

  // Only clear selection if not in visual mode
  if (currentMode != EditorMode::VISUAL && (hasSelection || isSelecting))
  {
    clearSelection();
  }
  else if (currentMode == EditorMode::VISUAL)
  {
    // Update selection end position
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
  }
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

  // Only clear selection if not in visual mode
  if (currentMode != EditorMode::VISUAL && (hasSelection || isSelecting))
  {
    clearSelection();
  }
  else if (currentMode == EditorMode::VISUAL)
  {
    // Update selection end position
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
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

  // Only clear selection if not in visual mode
  if (currentMode != EditorMode::VISUAL && (hasSelection || isSelecting))
  {
    clearSelection();
  }
  else if (currentMode == EditorMode::VISUAL)
  {
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
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

  // Only clear selection if not in visual mode
  if (currentMode != EditorMode::VISUAL && (hasSelection || isSelecting))
  {
    clearSelection();
  }
  else if (currentMode == EditorMode::VISUAL)
  {
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
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

  if (currentMode != EditorMode::VISUAL && (hasSelection || isSelecting))
  {
    clearSelection();
  }
  else if (currentMode == EditorMode::VISUAL)
  {
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
  }
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

  if (currentMode != EditorMode::VISUAL && (hasSelection || isSelecting))
  {
    clearSelection();
  }
  else if (currentMode == EditorMode::VISUAL)
  {
    selectionEndLine = cursorLine;
    selectionEndCol = cursorCol;
    hasSelection = true;
  }
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
  std::cerr << "currentMode: " << (int)currentMode << std::endl;

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

  std::string line = buffer.getLine(cursorLine);
  if (cursorCol > static_cast<int>(line.length()))
    cursorCol = line.length();
  if (cursorCol < 0)
    cursorCol = 0;

  size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);

  // FIRST: Modify the buffer
  line.insert(cursorCol, 1, ch);
  buffer.replaceLine(cursorLine, line);

  // THEN: Notify Tree-sitter with the complete new content
  if (syntaxHighlighter)
  {
    // Get fresh buffer content
    syntaxHighlighter->updateTreeAfterEdit(buffer, byte_pos, 0, 1, cursorLine,
                                           cursorCol, cursorLine, cursorCol,
                                           cursorLine, cursorCol + 1);
    syntaxHighlighter->invalidateLineRange(cursorLine, cursorLine);
  }

  cursorCol++;
  markModified();

  // REMOVED: Don't call notifyBufferChanged() - we already notified
  // Tree-sitter notifyBufferChanged();

  // Auto-adjust viewport (unchanged)
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int lineNumWidth = std::to_string(buffer.getLineCount()).length();
  int contentWidth = cols - lineNumWidth - 3;
  if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
  {
    viewportLeft = cursorCol - contentWidth + 1;
  }
}

void Editor::insertNewline()
{
  // Calculate position BEFORE split
  size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);

  // Notify Tree-sitter BEFORE modifying buffer
  if (syntaxHighlighter)
  {
    syntaxHighlighter->notifyEdit(
        byte_pos, 0, 1,        // Inserting newline (1 byte)
        cursorLine, cursorCol, // Start position
        cursorLine, cursorCol, // Old end (same as start)
        cursorLine + 1, 0      // New end (new line created)
    );
  }

  // Now modify buffer
  splitLineAtCursor();
  cursorLine++;
  cursorCol = 0;

  if (cursorLine >= viewportTop + viewportHeight)
  {
    viewportTop = cursorLine - viewportHeight + 1;
  }

  viewportLeft = 0;
  markModified();

  // Invalidate affected lines only
  if (syntaxHighlighter)
  {
    syntaxHighlighter->invalidateLineRange(cursorLine - 1,
                                           buffer.getLineCount() - 1);
  }
}

void Editor::deleteChar()
{
  std::string line = buffer.getLine(cursorLine);

  if (cursorCol < static_cast<int>(line.length()))
  {
    // Calculate position BEFORE deletion
    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);

    // Notify Tree-sitter
    if (syntaxHighlighter)
    {
      syntaxHighlighter->notifyEdit(byte_pos, 1, 0,        // Deleting 1 byte
                                    cursorLine, cursorCol, // Start position
                                    cursorLine, cursorCol + 1, // Old end
                                    cursorLine,
                                    cursorCol // New end (same as start)
      );
    }

    line.erase(cursorCol, 1);
    buffer.replaceLine(cursorLine, line);
    markModified();

    // Invalidate only current line
    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(cursorLine, cursorLine);
    }
  }
  else if (cursorLine < buffer.getLineCount() - 1)
  {
    // Joining lines - calculate position at end of current line
    size_t byte_pos = buffer.lineColToPos(cursorLine, line.length());

    // Notify Tree-sitter about newline deletion
    if (syntaxHighlighter)
    {
      std::string nextLine = buffer.getLine(cursorLine + 1);
      syntaxHighlighter->notifyEdit(
          byte_pos, 1, 0,                      // Deleting newline (1 byte)
          cursorLine, (uint32_t)line.length(), // Start position
          cursorLine + 1, 0,                   // Old end (start of next line)
          cursorLine, (uint32_t)line.length()  // New end (same position)
      );
    }

    joinLineWithNext();
    markModified();

    // Invalidate from current line to end
    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(cursorLine,
                                             buffer.getLineCount() - 1);
    }
  }
}

void Editor::backspace()
{
  if (cursorCol > 0)
  {
    // NEW: Calculate position BEFORE deletion
    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol - 1);

    std::string line = buffer.getLine(cursorLine);

    // Notify Tree-sitter about deletion
    if (syntaxHighlighter)
    {
      syntaxHighlighter->notifyEdit(
          byte_pos, 1, 0,            // Deleting 1 byte
          cursorLine, cursorCol - 1, // Start position
          cursorLine, cursorCol,     // Old end
          cursorLine, cursorCol - 1  // New end (same as start after delete)
      );
    }

    line.erase(cursorCol - 1, 1);
    buffer.replaceLine(cursorLine, line);

    cursorCol--;
    if (cursorCol < viewportLeft)
    {
      viewportLeft = cursorCol;
    }
    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(0, 0);
    }
    markModified();
    // notifyBufferChanged();
  }
  else if (cursorLine > 0)
  {
    std::string currentLine = buffer.getLine(cursorLine);
    std::string prevLine = buffer.getLine(cursorLine - 1);

    // Calculate byte position for line join
    size_t byte_pos = buffer.lineColToPos(cursorLine - 1, prevLine.length());

    if (syntaxHighlighter)
    {
      syntaxHighlighter->notifyEdit(byte_pos, 1, 0, // Deleting newline (1 byte)
                                    cursorLine - 1, (uint32_t)prevLine.length(),
                                    cursorLine, 0, cursorLine - 1,
                                    (uint32_t)prevLine.length());
    }

    cursorCol = static_cast<int>(prevLine.length());
    cursorLine--;

    buffer.replaceLine(cursorLine, prevLine + currentLine);
    buffer.deleteLine(cursorLine + 1);
    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(0, 0);
    }
    markModified();
  }
}

void Editor::deleteLine()
{
  if (buffer.getLineCount() == 1)
  {
    // Clearing the only line
    std::string line = buffer.getLine(0);
    size_t byte_pos = 0;

    if (syntaxHighlighter)
    {
      syntaxHighlighter->notifyEdit(byte_pos, line.length(),
                                    0,    // Deleting entire line
                                    0, 0, // Start
                                    0, (uint32_t)line.length(), // Old end
                                    0, 0                        // New end
      );
    }

    buffer.replaceLine(0, "");
    cursorCol = 0;

    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(0, 0);
    }
  }
  else
  {
    // Calculate byte position of line start
    size_t byte_pos = buffer.lineColToPos(cursorLine, 0);
    std::string line = buffer.getLine(cursorLine);
    size_t line_length = line.length();

    // Include newline in deletion if not last line
    bool has_newline = (cursorLine < buffer.getLineCount() - 1);
    size_t delete_bytes = line_length + (has_newline ? 1 : 0);

    if (syntaxHighlighter)
    {
      syntaxHighlighter->notifyEdit(
          byte_pos, delete_bytes, 0, // Deleting line + newline
          cursorLine, 0,             // Start of line
          cursorLine + (has_newline ? 1 : 0),
          has_newline ? 0 : (uint32_t)line_length, // Old end
          cursorLine, 0                            // New end
      );
    }

    buffer.deleteLine(cursorLine);

    // Adjust cursor position
    if (cursorLine >= buffer.getLineCount())
    {
      cursorLine = buffer.getLineCount() - 1;
    }

    // Ensure cursor column is valid
    line = buffer.getLine(cursorLine);
    if (cursorCol > static_cast<int>(line.length()))
    {
      cursorCol = static_cast<int>(line.length());
    }

    // Invalidate from current line to end
    if (syntaxHighlighter)
    {
      syntaxHighlighter->invalidateLineRange(cursorLine,
                                             buffer.getLineCount() - 1);
    }
  }

  validateCursorAndViewport();
  markModified();
}

void Editor::deleteSelection()
{
  if (!hasSelection && !isSelecting)
    return;

  auto selection = getNormalizedSelection();
  int startLine = selection.first.first;
  int startCol = selection.first.second;
  int endLine = selection.second.first;
  int endCol = selection.second.second;

  // Calculate byte position and length
  size_t start_byte = buffer.lineColToPos(startLine, startCol);
  size_t end_byte = buffer.lineColToPos(endLine, endCol);
  size_t delete_bytes = end_byte - start_byte;

  // Notify Tree-sitter BEFORE modification
  if (syntaxHighlighter)
  {
    syntaxHighlighter->notifyEdit(start_byte, delete_bytes,
                                  0,                   // Deleting selection
                                  startLine, startCol, // Start
                                  endLine, endCol,     // Old end
                                  startLine, startCol // New end (back to start)
    );
  }

  if (startLine == endLine)
  {
    // Single line selection
    std::string line = buffer.getLine(startLine);
    line.erase(startCol, endCol - startCol);
    buffer.replaceLine(startLine, line);
  }
  else
  {
    // Multi-line selection
    std::string firstLine = buffer.getLine(startLine);
    std::string lastLine = buffer.getLine(endLine);

    std::string newLine =
        firstLine.substr(0, startCol) + lastLine.substr(endCol);
    buffer.replaceLine(startLine, newLine);

    // Delete the lines in between
    for (int i = endLine; i > startLine; i--)
    {
      buffer.deleteLine(i);
    }
  }

  // Position cursor at selection start
  updateCursorAndViewport(startLine, startCol);
  clearSelection();
  markModified();

  // Invalidate affected lines
  if (syntaxHighlighter)
  {
    syntaxHighlighter->invalidateLineRange(startLine,
                                           buffer.getLineCount() - 1);
  }
}

// =================================================================
// Undo/Redo System
// =================================================================

void Editor::saveState()
{
  if (isSaving)
    return; // Skip saving state during file operations

  EditorState state = getCurrentState();
  undoStack.push(state);
  limitUndoStack();

  while (!redoStack.empty())
  {
    redoStack.pop();
  }
}

void Editor::undo()
{
  if (!undoStack.empty())
  {
    redoStack.push(getCurrentState());
    EditorState state = undoStack.top();
    undoStack.pop();
    restoreState(state);

    // Full reparse needed after undo
    if (syntaxHighlighter)
    {
      syntaxHighlighter->bufferChanged(buffer);
    }

    isModified = true;
  }
}

void Editor::redo()
{
  if (!redoStack.empty())
  {
    undoStack.push(getCurrentState());
    EditorState state = redoStack.top();
    redoStack.pop();
    restoreState(state);

    // Full reparse needed after redo
    if (syntaxHighlighter)
    {
      syntaxHighlighter->bufferChanged(buffer);
    }

    isModified = true;
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

  // Normalize selection (ensure start comes before end)
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
    return "";

  auto [start, end] = getNormalizedSelection();
  int startLine = start.first, startCol = start.second;
  int endLine = end.first, endCol = end.second;

  std::ostringstream result;

  if (startLine == endLine)
  {
    // Single line selection
    std::string line = buffer.getLine(startLine);
    result << line.substr(startCol, endCol - startCol);
  }
  else
  {
    // Multi-line selection
    for (int i = startLine; i <= endLine; i++)
    {
      std::string line = buffer.getLine(i);

      if (i == startLine)
      {
        result << line.substr(startCol);
      }
      else if (i == endLine)
      {
        result << line.substr(0, endCol);
      }
      else
      {
        result << line;
      }

      if (i < endLine)
      {
        result << "\n";
      }
    }
  }

  // CRITICAL FIX: Actually return the result!
  return result.str();
}

void Editor::updateCursorStyle()
{
  switch (currentMode)
  {
  case EditorMode::NORMAL:
    // Block cursor (solid block)
    printf("\033[2 q");
    fflush(stdout);
    break;
  case EditorMode::INSERT:
    // Vertical bar cursor (thin lifne like VSCode/modern editors)
    printf("\033[6 q");
    fflush(stdout);
    break;
  case EditorMode::VISUAL:
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

void Editor::restoreDefaultCursor()
{
  // Restore default cursor (usually blinking block)
  printf("\033[0 q");
  fflush(stdout);
}

void Editor::initializeViewportHighlighting()
{
  if (syntaxHighlighter)
  {
    // Pre-parse viewport so first display() is instant
    syntaxHighlighter->parseViewportOnly(buffer, viewportTop);
  }
}