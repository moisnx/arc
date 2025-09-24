#include "editor.h"
// #include "src/ui/colors.h"
#include "src/ui/theme_manager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
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
}

// =================================================================
// Mode Management
// =================================================================

void Editor::setMode(EditorMode newMode)
{
  if (currentMode != newMode)
  {
    // Save state before mode change for potential undo
    if (newMode == EditorMode::INSERT)
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
    int lineNumWidth = std::to_string(buffer.getLineCount()).length();
    int contentStartCol = lineNumWidth + 3;
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
}

bool Editor::mouseToFilePos(int mouseRow, int mouseCol, int &fileRow,
                            int &fileCol)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  if (mouseRow >= rows - 1)
    return false;

  int lineNumWidth = std::to_string(buffer.getLineCount()).length();
  int contentStartCol = lineNumWidth + 3;

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

  std::string expandedLine = expandTabs(buffer.getLine(cursorLine), tabSize);
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
  int lineNumWidth = std::to_string(buffer.getLineCount()).length();
  int contentWidth = cols - lineNumWidth - 3;

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
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  viewportHeight = rows - 1;

  clear();

  int lineNumWidth = std::to_string(buffer.getLineCount()).length();
  int contentStartCol = lineNumWidth + 3;
  int contentWidth = cols - contentStartCol;

  int endLine = viewportTop + viewportHeight;
  if (endLine > buffer.getLineCount())
  {
    endLine = buffer.getLineCount();
  }

  // Pre-calculate syntax highlighting for visible lines ONCE
  std::vector<std::vector<ColorSpan>> lineSpans(endLine - viewportTop);
  if (syntaxHighlighter)
  {
    for (int i = viewportTop; i < endLine; i++)
    {
      std::string expandedLine = expandTabs(buffer.getLine(i), tabSize);
      lineSpans[i - viewportTop] =
          syntaxHighlighter->getHighlightSpans(expandedLine, i, buffer);
    }
  }

  // Render all lines
  for (int i = viewportTop; i < endLine; i++)
  {
    int screenRow = i - viewportTop;
    bool isCurrentLine = (cursorLine == i);

    move(screenRow, 0);

    // Line numbers with consistent color pairs
    int ln_colorPair = isCurrentLine ? LINE_NUMBERS_ACTIVE : LINE_NUMBERS;
    attron(COLOR_PAIR(ln_colorPair));
    printw("%*d ", lineNumWidth, i + 1);
    attroff(COLOR_PAIR(ln_colorPair));

    move(screenRow, contentStartCol);

    std::string expandedLine = expandTabs(buffer.getLine(i), tabSize);
    const std::vector<ColorSpan> &currentLineSpans = lineSpans[i - viewportTop];

    // Render line content
    for (int screenCol = 0; screenCol < contentWidth; screenCol++)
    {
      int fileCol = viewportLeft + screenCol;

      bool charExists =
          (fileCol >= 0 && fileCol < static_cast<int>(expandedLine.length()));
      char ch = charExists ? expandedLine[fileCol] : ' ';

      // Find applicable syntax highlighting
      int currentSyntaxColor = -1;
      int currentSyntaxAttr = 0;
      bool hasSyntaxHighlight = false;

      if (charExists)
      {
        for (const auto &span : currentLineSpans)
        {
          if (fileCol >= span.start && fileCol < span.end)
          {
            currentSyntaxColor = span.colorPair;
            currentSyntaxAttr = span.attribute;
            hasSyntaxHighlight = true;
            break; // Use first matching span
          }
        }
      }

      bool isSelected = isPositionSelected(i, fileCol);

      // Apply attributes efficiently - avoid redundant calls
      if (hasSyntaxHighlight)
      {
        attron(COLOR_PAIR(currentSyntaxColor) | currentSyntaxAttr);
      }

      if (isSelected)
      {
        attron(A_STANDOUT);
      }

      addch(ch);

      // Remove attributes in reverse order
      if (isSelected)
      {
        attroff(A_STANDOUT);
      }

      if (hasSyntaxHighlight)
      {
        attroff(COLOR_PAIR(currentSyntaxColor) | currentSyntaxAttr);
      }
    }
  }

  // Draw status bar
  drawStatusBar();
  positionCursor();
  updateCursorStyle();
}

void Editor::drawStatusBar()
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int statusRow = rows - 1;

  move(statusRow, 0);
  attron(COLOR_PAIR(STATUS_BAR));
  for (int i = 0; i < cols; i++)
  {
    addch(' ');
  }
  attroff(COLOR_PAIR(STATUS_BAR));

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
    if (isSelecting)
    {
      modeStr = " VISUAL ";
      modeColor = STATUS_BAR_ACTIVE;
    }
    else if (hasSelection)
    {
      modeStr = " VISUAL ";
      modeColor = STATUS_BAR_ACTIVE;
    }
    else
    {
      modeStr = " VISUAL ";
      modeColor = STATUS_BAR_ACTIVE;
    }
    break;
  }

  attron(COLOR_PAIR(modeColor) | A_BOLD);
  printw("%s", modeStr.c_str());
  attroff(COLOR_PAIR(modeColor) | A_BOLD);

  attron(COLOR_PAIR(modeColor));
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
  attroff(COLOR_PAIR(STATUS_BAR_ACTIVE) | A_BOLD);

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

  attron(COLOR_PAIR(STATUS_BAR_CYAN));
  for (int i = currentPos; i < rightStart && i < cols; i++)
  {
    move(statusRow, i);
    addch(' ');
  }
  attroff(COLOR_PAIR(STATUS_BAR_CYAN));

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
  refresh();
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
    std::string expandedLine = expandTabs(buffer.getLine(cursorLine), tabSize);
    cursorCol = expandedLine.length();

    if (cursorLine < viewportTop)
    {
      viewportTop = cursorLine;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int lineNumWidth = std::to_string(buffer.getLineCount()).length();
    int contentWidth = cols - lineNumWidth - 3;

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
    // Update selection end position
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
      std::string expandedLine = expandTabs(line, tabSize);
      if (cursorCol < static_cast<int>(expandedLine.length()))
      {
        cursorCol++;
      }
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int lineNumWidth = std::to_string(buffer.getLineCount()).length();
    int contentWidth = cols - lineNumWidth - 3;

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
    // Update selection end position
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
  std::string expandedLine = expandTabs(buffer.getLine(cursorLine), tabSize);
  cursorCol = static_cast<int>(expandedLine.length());

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int lineNumWidth = std::to_string(buffer.getLineCount()).length();
  int contentWidth = cols - lineNumWidth - 3;

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

bool Editor::loadFile(const std::string &fname)
{
  filename = fname;

  if (!buffer.loadFromFile(filename))
  {
    buffer.clear();
    // Add one empty line for new files
    buffer.insertLine(0, "");
    return false;
  }

  if (syntaxHighlighter)
  {
    syntaxHighlighter->setLanguage(getFileExtension());
  }

  isModified = false;
  return true;
}

bool Editor::saveFile(const std::string &fname)
{
  std::string targetFile = fname.empty() ? filename : fname;

  if (targetFile.empty())
  {
    return false; // No filename specified
  }

  std::ofstream file(targetFile);
  if (!file.is_open())
  {
    return false;
  }

  for (int i = 0; i < buffer.getLineCount(); i++)
  {
    file << buffer.getLine(i);
    if (i < buffer.getLineCount() - 1)
    {
      file << "\n";
    }
  }

  file.close();

  if (!fname.empty())
  {
    filename = fname;
  }

  isModified = false;
  return true;
}

// =================================================================
// Text Editing Operations
// =================================================================

void Editor::insertChar(char ch)
{
  if (cursorLine < 0 || cursorLine >= buffer.getLineCount())
  {
    return; // Invalid line
  }

  std::string line = buffer.getLine(cursorLine);

  // CRITICAL: Fix cursor position if it's out of bounds
  if (cursorCol > static_cast<int>(line.length()))
  {
    cursorCol = line.length(); // Clamp to end of line
  }
  if (cursorCol < 0)
  {
    cursorCol = 0;
  }

  line.insert(cursorCol, 1, ch);
  buffer.replaceLine(cursorLine, line);

  cursorCol++;
  markModified();

  // Auto-adjust viewport
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
  splitLineAtCursor();
  cursorLine++;
  cursorCol = 0;

  // Auto-adjust viewport
  if (cursorLine >= viewportTop + viewportHeight)
  {
    viewportTop = cursorLine - viewportHeight + 1;
  }

  viewportLeft = 0;
  markModified();
}

void Editor::deleteChar()
{
  std::string line = buffer.getLine(cursorLine);

  if (cursorCol < static_cast<int>(line.length()))
  {
    // Delete character at cursor position
    line.erase(cursorCol, 1);
    buffer.replaceLine(cursorLine, line);
    markModified();
  }
  else if (cursorLine < buffer.getLineCount() - 1)
  {
    // Join with next line
    joinLineWithNext();
    markModified();
  }
}

void Editor::backspace()
{
  if (cursorCol > 0)
  {
    // Delete character before cursor
    std::string line = buffer.getLine(cursorLine);
    line.erase(cursorCol - 1, 1);
    buffer.replaceLine(cursorLine, line);

    cursorCol--;
    if (cursorCol < viewportLeft)
    {
      viewportLeft = cursorCol;
    }
    markModified();
  }
  else if (cursorLine > 0)
  {
    // Join with previous line
    std::string currentLine = buffer.getLine(cursorLine);
    std::string prevLine = buffer.getLine(cursorLine - 1);

    cursorCol = static_cast<int>(prevLine.length());
    cursorLine--;

    buffer.replaceLine(cursorLine, prevLine + currentLine);
    buffer.deleteLine(cursorLine + 1);

    // Auto-adjust viewport
    if (cursorLine < viewportTop)
    {
      viewportTop = cursorLine;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int lineNumWidth = std::to_string(buffer.getLineCount()).length();
    int contentWidth = cols - lineNumWidth - 3;

    if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
    {
      viewportLeft = cursorCol - contentWidth + 1;
      if (viewportLeft < 0)
        viewportLeft = 0;
    }

    markModified();
  }
}

void Editor::deleteLine()
{
  if (buffer.getLineCount() == 1)
  {
    // Clear the only line
    buffer.replaceLine(0, "");
    cursorCol = 0;
  }
  else
  {
    buffer.deleteLine(cursorLine);

    // Adjust cursor position
    if (cursorLine >= buffer.getLineCount())
    {
      cursorLine = buffer.getLineCount() - 1;
    }

    // Ensure cursor column is valid
    std::string line = buffer.getLine(cursorLine);
    if (cursorCol > static_cast<int>(line.length()))
    {
      cursorCol = static_cast<int>(line.length());
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

    // Combine parts before selection start and after selection end
    std::string newLine =
        firstLine.substr(0, startCol) + lastLine.substr(endCol);
    buffer.replaceLine(startLine, newLine);

    // Delete the lines in between (including the last line)
    for (int i = endLine; i > startLine; i--)
    {
      buffer.deleteLine(i);
    }
  }

  // Position cursor at selection start
  updateCursorAndViewport(startLine, startCol);
  clearSelection();
  markModified();
}

// =================================================================
// Undo/Redo System
// =================================================================

void Editor::saveState()
{
  EditorState state = getCurrentState();

  undoStack.push(state);
  limitUndoStack();

  // Clear redo stack when new state is saved
  while (!redoStack.empty())
  {
    redoStack.pop();
  }
}

void Editor::undo()
{
  if (!undoStack.empty())
  {
    // Save current state to redo stack
    redoStack.push(getCurrentState());

    // Restore previous state
    EditorState state = undoStack.top();
    undoStack.pop();
    restoreState(state);

    isModified = true; // Mark as modified since we changed something
  }
}

void Editor::redo()
{
  if (!redoStack.empty())
  {
    // Save current state to undo stack
    undoStack.push(getCurrentState());

    // Restore next state
    EditorState state = redoStack.top();
    redoStack.pop();
    restoreState(state);

    isModified = true;
  }
}

EditorState Editor::getCurrentState()
{
  EditorState state;

  // Serialize buffer content
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
    // Vertical bar cursor (thin line like VSCode/modern editors)
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