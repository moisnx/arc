#include "editor.h"
#include "src/core/clipboard.h"
#include "src/core/config_manager.h"
#include "src/features/indent_manager.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric>
#ifdef _WIN32
#include <curses.h>
#include <windows.h>
#undef min
#undef max
#else
#include <ncursesw/ncurses.h>
#endif
#include "src/utils/binary_detector.h"
#include <iostream>
#include <magika/magika.hpp>
#include <sstream>
#include <string>
#include <utility>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

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

#ifdef TREE_SITTER_ENABLED
  config_loader_ = std::make_unique<SyntaxConfigLoader>();
  std::string syntax_dir = ConfigManager::getSyntaxRulesDir();
  config_loader_->loadAllLanguageConfigs(syntax_dir);

  // Initialize FileManager with the loader
  fileManager_ = std::make_unique<FileManager>(config_loader_.get());

  indentManager_ = std::make_unique<IndentManager>();
  markdownRenderer_ = std::make_unique<MarkdownRenderer>();
  markdownRenderer_->setEnabled(false);
  indentManager_->setTabSize(tabSize);
#endif

  // No need to init stacks manually, history_ ctor does it
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

  auto [start, end] = getNormalizedSelection();
  int startL = start.first;
  int startC = start.second;
  int endL = end.first;
  int endC = end.second;

  // Single point selection (no actual selection)
  if (startL == endL && startC == endC)
    return false;

  // Before selection
  if (line < startL)
    return false;

  // After selection
  if (line > endL)
    return false;

  // On start line
  if (line == startL && line == endL)
  {
    return col >= startC && col < endC;
  }

  if (line == startL)
  {
    return col >= startC;
  }

  // On end line
  if (line == endL)
  {
    return col < endC;
  }

  // Between start and end lines
  return true;
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
}

bool Editor::mouseToFilePos(int mouseRow, int mouseCol, int &fileRow,
                            int &fileCol)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  // Don't process clicks on status bar
  if (mouseRow >= rows - 1)
    return false;

  bool show_line_numbers = ConfigManager::getLineNumbers();
  int lineNumWidth =
      show_line_numbers ? std::to_string(buffer.getLineCount()).length() : 0;
  int contentStartCol = show_line_numbers ? (lineNumWidth + 3) : 0;

  // Clicks in line number area should still work
  if (mouseCol < contentStartCol)
  {
    mouseCol = contentStartCol;
  }

  // Convert screen row to file row
  fileRow = viewportTop + mouseRow;

  // Clamp to valid line range
  if (fileRow < 0)
    fileRow = 0;
  if (fileRow >= buffer.getLineCount())
    fileRow = buffer.getLineCount() - 1;

  // Convert screen col to file col
  fileCol = viewportLeft + (mouseCol - contentStartCol);

  // Clamp to valid column range (allow position at end of line)
  if (fileCol < 0)
    fileCol = 0;

  std::string line = buffer.getLine(fileRow);
  int lineLen = static_cast<int>(line.length());

  if (fileCol > lineLen)
    fileCol = lineLen;

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

void Editor::displayBinaryWarning()
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  // Clear with background
  attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
  clear();

  int centerRow = rows / 2 - 4;
  int centerCol = cols / 2;

  // Top border with ERROR color
  attron(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);
  mvprintw(centerRow, centerCol - 25,
           "╔═════════════════════════════════════════════════╗");
  mvprintw(centerRow + 1, centerCol - 25, "║");
  mvprintw(centerRow + 1, centerCol + 24, "║");
  attroff(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);

  // Icon and title with WARNING color
  attron(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);
  mvprintw(centerRow + 1, centerCol - 10, "⚠️  BINARY FILE  ⚠️");
  attroff(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);

  // Separator
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  mvprintw(centerRow + 2, centerCol - 25,
           "╟─────────────────────────────────────────────────╢");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // Main message with PRIMARY color
  attron(COLOR_PAIR(ColorPairs::UI_PRIMARY));
  mvprintw(centerRow + 4, centerCol - 22,
           "This file contains binary data that cannot");
  mvprintw(centerRow + 5, centerCol - 22,
           "be safely displayed or edited as text.");
  attroff(COLOR_PAIR(ColorPairs::UI_PRIMARY));

  // File info section
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  mvprintw(centerRow + 7, centerCol - 25,
           "╟─────────────────────────────────────────────────╢");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // Filename with ACCENT color
  attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
  mvprintw(centerRow + 8, centerCol - 22, "File:");
  attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

  attron(COLOR_PAIR(ColorPairs::UI_ACCENT) | A_BOLD);
  // Truncate long filenames
  std::string displayName = filename;
  if (displayName.length() > 40)
  {
    displayName = "..." + displayName.substr(displayName.length() - 37);
  }
  mvprintw(centerRow + 8, centerCol - 16, "%s", displayName.c_str());
  attroff(COLOR_PAIR(ColorPairs::UI_ACCENT) | A_BOLD);

  // File type and size detection
  std::string fileType = BinaryDetector::detectFileType(filename);
  std::string fileSize = BinaryDetector::getFileSize(filename);

  attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
  mvprintw(centerRow + 9, centerCol - 22, "Type:");
  attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

  attron(COLOR_PAIR(ColorPairs::UI_INFO));
  mvprintw(centerRow + 9, centerCol - 16, "%s", fileType.c_str());
  attroff(COLOR_PAIR(ColorPairs::UI_INFO));

  // Show file size
  attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
  mvprintw(centerRow + 10, centerCol - 22, "Size:");
  attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

  attron(COLOR_PAIR(ColorPairs::UI_INFO));
  mvprintw(centerRow + 10, centerCol - 16, "%s", fileSize.c_str());
  attroff(COLOR_PAIR(ColorPairs::UI_INFO));

  // Help text with DISABLED state color
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  mvprintw(centerRow + 12, centerCol - 25,
           "╟─────────────────────────────────────────────────╢");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  attron(COLOR_PAIR(ColorPairs::STATE_DISABLED));
  mvprintw(centerRow + 13, centerCol - 22,
           "To edit this file, use a hex editor or");
  mvprintw(centerRow + 14, centerCol - 22, "appropriate binary editing tool.");
  attroff(COLOR_PAIR(ColorPairs::STATE_DISABLED));

  // Bottom border with ERROR color
  attron(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);
  mvprintw(centerRow + 15, centerCol - 25,
           "╚═════════════════════════════════════════════════╝");
  attroff(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);

  // Action hint at bottom with SUCCESS color
  attron(COLOR_PAIR(ColorPairs::UI_SUCCESS));
  mvprintw(rows - 3, centerCol - 15, "Press any key to continue...");
  attroff(COLOR_PAIR(ColorPairs::UI_SUCCESS));

  drawStatusBar();

  wnoutrefresh(stdscr);
  doupdate();
}

void Editor::display()
{
  if (isBinaryFile)
  {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    renderer_->drawBinaryWarning(filename, rows, cols);
    renderer_->drawStatusBar(buildRenderContext(), filename, "Binary", false,
                             true);
    return;
  }

  if (!validateEditorState())
  {
    validateCursorAndViewport();
  }

  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  viewportHeight = rows - 1; // Update internal state

  // Create Context
  RenderContext ctx = buildRenderContext();
  ctx.viewportHeight = viewportHeight;
  ctx.viewportWidth = cols;

  // Delegate to Renderer
  renderer_->render(ctx);
  renderer_->drawStatusBar(ctx, filename, getFileLang(), hasUnsavedChanges(),
                           false);

  positionCursor(); // Ncurses cursor move
}

#include "editor.h"
#include "src/core/config_manager.h"

// ...

RenderContext Editor::buildRenderContext() const
{
  RenderContext ctx{
      buffer,
      syntaxHighlighter,
      markdownRenderer_.get(),
      cursorLine,
      cursorCol,
      viewportTop,
      viewportLeft,
      0, // viewportHeight (filled in display)
      0, // viewportWidth (filled in display)
      ConfigManager::getLineNumbers(),
      tabSize,
      (hasSelection || isSelecting),
      0,
      0,
      0,
      0 // Selection place holders
  };

  if (ctx.hasSelection)
  {
    auto sel = const_cast<Editor *>(this)->getNormalizedSelection();
    ctx.selStartLine = sel.first.first;
    ctx.selStartCol = sel.first.second;
    ctx.selEndLine = sel.second.first;
    ctx.selEndCol = sel.second.second;
  }

  return ctx;
}

UnsavedModalResult Editor::handleUnsavedChangesModal()
{
  if (!hasUnsavedChanges())
    return UnsavedModalResult::QUIT_WITHOUT_SAVE;

  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  // Initial Draw
  display(); // Draw background editor
  renderer_->drawUnsavedChangesModal(filename, rows, cols);

  while (true)
  {
    int key = getch();
    switch (key)
    {
    case 's':
    case 'S':
      return UnsavedModalResult::SAVE_AND_QUIT;
    case 'q':
    case 'Q':
      return UnsavedModalResult::QUIT_WITHOUT_SAVE;
    case 27:
      return UnsavedModalResult::CANCEL;
    case KEY_RESIZE:
      handleResize(); // Updates globals
      getmaxyx(stdscr, rows, cols);
      display();
      renderer_->drawUnsavedChangesModal(filename, rows, cols);
      break;
    }
  }
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
  if (history_.hasUnsavedChanges())
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

void Editor::startSelectionIfNeeded()
{
  if (!hasSelection && !isSelecting)
  {
    startSelection(cursorLine, cursorCol);
  }
}

void Editor::updateSelectionEnd() { extendSelection(cursorLine, cursorCol); }

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

std::string Editor::find_magika_models()
{
  // Try environment variable first
  if (const char *env_path = std::getenv("MAGIKA_MODEL_PATH"))
  {
    if (fs::exists(env_path))
    {
      return env_path;
    }
  }

  // Try common install locations
  std::vector<std::string> search_paths = {
      "/usr/local/share/magika/models/standard_v3_3",
      "/usr/share/magika/models/standard_v3_3",
      "../../assets/models/standard_v3_3", // Development build
      "./models/standard_v3_3",            // Bundled
      "../models/standard_v3_3"            // Build directory
  };

  for (const auto &path : search_paths)
  {
    if (fs::exists(path + "/model.onnx"))
    {
      return path;
    }
  }

  throw std::runtime_error(
      "Magika models not found. Tried:\n" +
      std::accumulate(search_paths.begin(), search_paths.end(), std::string(),
                      [](const std::string &a, const std::string &b)
                      { return a + "  - " + b + "\n"; }) +
      "\nInstall with: sudo cmake --install build\n"
      "Or set MAGIKA_MODEL_PATH=/path/to/models/standard_v3_3");
}

bool Editor::loadFile(const std::string &fname)
{
  // Delegate to FileManager
  auto result = fileManager_->loadFile(fname, buffer);

  filename = result.filename;
  isBinaryFile = result.isBinary;

  // Handle Result
  if (result.isBinary)
  {
    buffer.clear();
    buffer.insertLine(0, "");
    history_.markSaved();
    return true;
  }

  if (!result.success)
  {
    // Buffer already cleared in fileManager if failed
    return false;
  }

  // Handle Syntax/Language
  if (syntaxHighlighter)
  {
    // Markdown special handling
    if (result.detectedLanguage == "markdown" && markdownRenderer_)
    {
      markdownRenderer_->updateState(buffer, syntaxHighlighter->getTree());
    }

    syntaxHighlighter->setLanguage(result.detectedLanguage);

    // Force immediate re-highlight
    syntaxHighlighter->invalidateLineRange(0, buffer.getLineCount());
    syntaxHighlighter->forceFullReparse(buffer);
  }

  history_.clear(); // Reset undo history
  return true;
}

bool Editor::saveFile()
{
  if (filename.empty())
    return false;

  isSaving = true; // Block undo history generation
  bool success = fileManager_->saveFile(filename, buffer);

  if (success)
  {
    history_.markSaved();
  }

  isSaving = false;
  return success;
}

// =================================================================
// Text Editing Operations
// =================================================================

void Editor::insertChar(char ch)
{
  if (isBinaryFile)
    return;
  if (cursorLine < 0 || cursorLine >= buffer.getLineCount())
    return;

  // 1. Create Delta representing the intent
  EditDelta delta = createDeltaForInsertChar(ch);

  // 2. Modify Buffer
  std::string line = buffer.getLine(cursorLine);
  if (cursorCol > (int)line.length())
    cursorCol = line.length();
  size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);
  line.insert(cursorCol, 1, ch);
  buffer.replaceLine(cursorLine, line);
  cursorCol++;

  // 3. Update Syntax
  if (syntaxHighlighter)
  {
    syntaxHighlighter->updateTreeAfterEdit(
        buffer, byte_pos, 0, 1, cursorLine, cursorCol - 1, cursorLine,
        cursorCol - 1, cursorLine, cursorCol);
    syntaxHighlighter->invalidateLineCache(cursorLine);
  }

  // 4. Update Viewport
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int contentWidth =
      cols - (ConfigManager::getLineNumbers() ? 5 : 0); // Simplified calc
  if (contentWidth > 0 && cursorCol >= viewportLeft + contentWidth)
  {
    viewportLeft = cursorCol - contentWidth + 1;
  }

  // 5. Finalize Delta
  delta.postCursorLine = cursorLine;
  delta.postCursorCol = cursorCol;
  delta.postViewportTop = viewportTop;
  delta.postViewportLeft = viewportLeft;

  // 6. Push to History Manager
  history_.addDelta(delta);

  // 7. Handle Batching (Grouping)
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - history_.getCurrentGroupTimestamp())
                     .count();

  bool is_boundary_char = (ch == '>' || ch == ')' || ch == '}' || ch == ']' ||
                           ch == ';' || ch == ',' || ch == ' ');

  if (elapsed > UNDO_GROUP_TIMEOUT_MS || is_boundary_char)
  {
    history_.commitDeltaGroup();
    history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());
  }

  history_.markModified();
}

void Editor::insertNewline()
{
  if (isBinaryFile)
    return;

  EditDelta delta = createDeltaForNewline();
  size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);

  // Buffer Ops
  splitLineAtCursor();
  cursorLine++;
  cursorCol = 0;

  // Indentation Logic
#ifdef TREE_SITTER_ENABLED
  int indent_spaces = 0;
  if (!isPasting_ && indentManager_ && syntaxHighlighter &&
      syntaxHighlighter->hasValidTree())
  {
    indent_spaces = indentManager_->calculateIndentAfterLine(
        cursorLine - 1, buffer, syntaxHighlighter->getTree());

    if (indent_spaces > 0)
    {
      std::string line = buffer.getLine(cursorLine);
      std::string indent_str(indent_spaces, ' ');
      line = indent_str + line;
      buffer.replaceLine(cursorLine, line);
      cursorCol = indent_spaces;
    }
  }
#endif

  // Syntax Update
  if (syntaxHighlighter)
  {
    int bytes_inserted = 1 + cursorCol;
    syntaxHighlighter->updateTreeAfterEdit(
        buffer, byte_pos, 0, bytes_inserted, delta.preCursorLine,
        delta.preCursorCol, delta.preCursorLine, delta.preCursorCol, cursorLine,
        cursorCol);
    syntaxHighlighter->invalidateLineRange(cursorLine - 1,
                                           buffer.getLineCount() - 1);
  }

  // Viewport
  if (cursorLine >= viewportTop + viewportHeight)
  {
    viewportTop = cursorLine - viewportHeight + 1;
  }
  viewportLeft = 0;

  // History
  delta.postCursorLine = cursorLine;
  delta.postCursorCol = cursorCol;
  delta.postViewportTop = viewportTop;
  delta.postViewportLeft = viewportLeft;

  history_.addDelta(delta);
  history_.commitDeltaGroup(); // Always commit on newline
  history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());

  history_.markModified();
  updateMarkdownRendering();
}

void Editor::deleteChar()
{
  if (isBinaryFile)
    return;

  EditDelta delta = createDeltaForDeleteChar();
  std::string line = buffer.getLine(cursorLine);

  if (cursorCol < (int)line.length())
  {
    // Delete char in line
    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol);
    line.erase(cursorCol, 1);
    buffer.replaceLine(cursorLine, line);

    if (syntaxHighlighter)
    {
      syntaxHighlighter->updateTreeAfterEdit(
          buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine,
          cursorCol + 1, cursorLine, cursorCol);
      syntaxHighlighter->invalidateLineCache(cursorLine);
    }
  }
  else if (cursorLine < buffer.getLineCount() - 1)
  {
    // Join lines
    size_t byte_pos = buffer.lineColToPos(cursorLine, line.length());
    std::string nextLine = buffer.getLine(cursorLine + 1);
    buffer.replaceLine(cursorLine, line + nextLine);
    buffer.deleteLine(cursorLine + 1);

    if (syntaxHighlighter)
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

  history_.addDelta(delta);
  // Commit if we joined lines, otherwise keep grouping
  if (delta.operation == EditDelta::JOIN_LINES)
  {
    history_.commitDeltaGroup();
    history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());
  }

  history_.markModified();
  updateMarkdownRendering();
}

void Editor::backspace()
{
  if (isBinaryFile)
    return;

  EditDelta delta = createDeltaForBackspace();

  if (cursorCol > 0)
  {
    // Simple backspace
    std::string line = buffer.getLine(cursorLine);
    size_t byte_pos = buffer.lineColToPos(cursorLine, cursorCol - 1);
    line.erase(cursorCol - 1, 1);
    buffer.replaceLine(cursorLine, line);
    cursorCol--;

    if (syntaxHighlighter)
    {
      syntaxHighlighter->updateTreeAfterEdit(
          buffer, byte_pos, 1, 0, cursorLine, cursorCol, cursorLine,
          cursorCol + 1, cursorLine, cursorCol);
      syntaxHighlighter->invalidateLineCache(cursorLine);
    }
    if (cursorCol < viewportLeft)
      viewportLeft = cursorCol;
  }
  else if (cursorLine > 0)
  {
    // Join with prev line
    std::string currentLine = buffer.getLine(cursorLine);
    std::string prevLine = buffer.getLine(cursorLine - 1);
    size_t byte_pos = buffer.lineColToPos(cursorLine - 1, prevLine.length());
    int oldCursorLine = cursorLine;

    cursorCol = (int)prevLine.length();
    cursorLine--;

    buffer.replaceLine(cursorLine, prevLine + currentLine);
    buffer.deleteLine(cursorLine + 1);

    if (syntaxHighlighter)
    {
      syntaxHighlighter->updateTreeAfterEdit(buffer, byte_pos, 1, 0, cursorLine,
                                             cursorCol, oldCursorLine, 0,
                                             cursorLine, cursorCol);
      syntaxHighlighter->invalidateLineRange(cursorLine,
                                             buffer.getLineCount() - 1);
    }
  }

  delta.postCursorLine = cursorLine;
  delta.postCursorCol = cursorCol;
  delta.postViewportTop = viewportTop;
  delta.postViewportLeft = viewportLeft;

  history_.addDelta(delta);
  if (delta.operation == EditDelta::JOIN_LINES)
  {
    history_.commitDeltaGroup();
    history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());
  }

  history_.markModified();
  updateMarkdownRendering();
}

void Editor::deleteSelection()
{
  if (isBinaryFile || !isSelectionActive())
    return;

  // Use a group for atomic selection deletion
  history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());

  EditDelta delta = createDeltaForDeleteSelection();

  auto [start, end] = getNormalizedSelection();
  size_t start_byte = buffer.lineColToPos(start.first, start.second);
  size_t end_byte = buffer.lineColToPos(end.first, end.second);
  size_t delete_bytes = (end_byte > start_byte) ? (end_byte - start_byte) : 0;

  if (start.first == end.first)
  {
    // Single line
    std::string line = buffer.getLine(start.first);
    line.erase(start.second, end.second - start.second);
    buffer.replaceLine(start.first, line);
  }
  else
  {
    // Multi line
    std::string first = buffer.getLine(start.first);
    std::string last = buffer.getLine(end.first);
    buffer.replaceLine(start.first,
                       first.substr(0, start.second) + last.substr(end.second));
    for (int i = end.first; i > start.first; i--)
    {
      buffer.deleteLine(i);
    }
  }

  // Syntax
  if (syntaxHighlighter)
  {
    syntaxHighlighter->updateTreeAfterEdit(
        buffer, start_byte, delete_bytes, 0, start.first, start.second,
        end.first, end.second, start.first, start.second);
    syntaxHighlighter->invalidateLineRange(start.first,
                                           buffer.getLineCount() - 1);
  }

  updateCursorAndViewport(start.first, start.second);
  clearSelection();

  delta.postCursorLine = cursorLine;
  delta.postCursorCol = cursorCol;
  delta.postViewportTop = viewportTop;
  delta.postViewportLeft = viewportLeft;

  history_.addDelta(delta);
  history_.commitDeltaGroup();
  history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());

  history_.markModified();
  updateMarkdownRendering();
}

// =================================================================
// Undo/Redo (DELEGATION)
// =================================================================

void Editor::undo()
{
  if (isBinaryFile)
    return;
  bool changed = history_.undo(buffer, cursorLine, cursorCol, viewportTop,
                               viewportLeft, syntaxHighlighter);
  if (changed)
  {
    validateCursorAndViewport();
    updateMarkdownRendering();
  }
}

void Editor::redo()
{
  if (isBinaryFile)
    return;
  bool changed = history_.redo(buffer, cursorLine, cursorCol, viewportTop,
                               viewportLeft, syntaxHighlighter);
  if (changed)
  {
    validateCursorAndViewport();
    updateMarkdownRendering();
  }
}

// =================================================================
// MISSING IMPLEMENTATIONS - Append to src/core/editor.cpp
// =================================================================

// --- Selection Helpers ---

std::pair<std::pair<int, int>, std::pair<int, int>>
Editor::getNormalizedSelection()
{
  int startLine = selectionStartLine;
  int startCol = selectionStartCol;
  int endLine = selectionEndLine;
  int endCol = selectionEndCol;

  if (startLine > endLine || (startLine == endLine && startCol > endCol))
  {
    std::swap(startLine, endLine);
    std::swap(startCol, endCol);
  }

  return {{startLine, startCol}, {endLine, endCol}};
}

void Editor::startSelection(int line, int col)
{
  selectionStartLine = line;
  selectionStartCol = col;
  selectionEndLine = line;
  selectionEndCol = col;
  isSelecting = true;
  hasSelection = false;
}

void Editor::extendSelection(int line, int col)
{
  if (!isSelecting && !hasSelection)
  {
    startSelection(line, col);
    return;
  }
  selectionEndLine = line;
  selectionEndCol = col;
  if (selectionStartLine != selectionEndLine ||
      selectionStartCol != selectionEndCol)
  {
    hasSelection = true;
  }
}

void Editor::finalizeSelection()
{
  if (selectionStartLine == selectionEndLine &&
      selectionStartCol == selectionEndCol)
  {
    clearSelection();
  }
  else
  {
    hasSelection = true;
    isSelecting = false;
  }
}

void Editor::selectAll()
{
  if (buffer.getLineCount() == 0)
    return;
  startSelection(0, 0);
  int lastLine = buffer.getLineCount() - 1;
  extendSelection(lastLine, buffer.getLine(lastLine).length());
  finalizeSelection();
}

std::string Editor::getSelectedText()
{
  if (!hasSelection && !isSelecting)
    return "";
  auto [start, end] = getNormalizedSelection();

  std::ostringstream result;
  if (start.first == end.first)
  {
    std::string line = buffer.getLine(start.first);
    int len = std::min((int)line.length(), end.second) - start.second;
    if (len > 0)
      result << line.substr(start.second, len);
  }
  else
  {
    for (int i = start.first; i <= end.first; i++)
    {
      std::string line = buffer.getLine(i);
      if (i == start.first)
        result << line.substr(start.second);
      else if (i == end.first)
        result << line.substr(0, std::min((int)line.length(), end.second));
      else
        result << line;
      if (i < end.first)
        result << "\n";
    }
  }
  return result.str();
}

// --- Clipboard ---

void Editor::copySelection()
{
  if (!isSelectionActive())
    return;
  std::string text = getSelectedText();
  if (!text.empty())
  {
    clipboard = text;
    Clipboard::copyToSystemClipboard(text);
  }
}

void Editor::cutSelection()
{
  if (!isSelectionActive())
    return;
  copySelection();
  deleteSelection();
}

void Editor::pasteFromClipboard()
{
  std::string text = Clipboard::getFromSystemClipboard();
  if (text.empty())
    text = clipboard;
  if (text.empty())
    return;

  history_.commitDeltaGroup();
  history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());

  if (isSelectionActive())
    deleteSelection();

  size_t start_byte = buffer.lineColToPos(cursorLine, cursorCol);
  int start_line = cursorLine;
  int start_col = cursorCol;

  EditDelta delta;
  delta.operation = EditDelta::INSERT_TEXT;
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;
  delta.startLine = cursorLine;
  delta.startCol = cursorCol;
  delta.insertedContent = text;
  delta.deletedContent = "";

  buffer.insertText(start_byte, text);

  int newlines = std::count(text.begin(), text.end(), '\n');
  if (newlines == 0)
  {
    cursorCol += text.length();
  }
  else
  {
    cursorLine += newlines;
    size_t last_nl = text.rfind('\n');
    cursorCol = text.length() - last_nl - 1;
  }
  delta.endLine = cursorLine;
  delta.endCol = cursorCol;

  if (syntaxHighlighter)
  {
    syntaxHighlighter->updateTreeAfterEdit(buffer, start_byte, 0, text.length(),
                                           start_line, start_col, start_line,
                                           start_col, cursorLine, cursorCol);
    syntaxHighlighter->invalidateLineRange(start_line,
                                           buffer.getLineCount() - 1);
  }

  validateCursorAndViewport();

  delta.postCursorLine = cursorLine;
  delta.postCursorCol = cursorCol;
  delta.postViewportTop = viewportTop;
  delta.postViewportLeft = viewportLeft;

  history_.addDelta(delta);
  history_.commitDeltaGroup();
  history_.beginDeltaGroup(buffer.getLineCount(), buffer.size());

  history_.markModified();
  forceSyntaxResync();
}

// --- Text Helpers ---

void Editor::splitLineAtCursor()
{
  std::string line = buffer.getLine(cursorLine);
  std::string left = line.substr(0, cursorCol);
  std::string right = line.substr(cursorCol);
  buffer.replaceLine(cursorLine, left);
  buffer.insertLine(cursorLine + 1, right);
}

void Editor::joinLineWithNext()
{
  if (cursorLine < buffer.getLineCount() - 1)
  {
    std::string current = buffer.getLine(cursorLine);
    std::string next = buffer.getLine(cursorLine + 1);
    buffer.replaceLine(cursorLine, current + next);
    buffer.deleteLine(cursorLine + 1);
  }
}

// --- Delta Creators ---

EditDelta Editor::createDeltaForInsertChar(char ch)
{
  EditDelta delta;
  delta.operation = EditDelta::INSERT_CHAR;
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;
  delta.startLine = cursorLine;
  delta.startCol = cursorCol;
  delta.endLine = cursorLine;
  delta.endCol = cursorCol;
  delta.insertedContent = std::string(1, ch);
  return delta;
}

EditDelta Editor::createDeltaForDeleteChar()
{
  EditDelta delta;
  delta.operation = EditDelta::DELETE_CHAR;
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;
  delta.startLine = cursorLine;
  delta.startCol = cursorCol;

  std::string line = buffer.getLine(cursorLine);
  if (cursorCol < (int)line.length())
  {
    delta.deletedContent = std::string(1, line[cursorCol]);
    delta.endLine = cursorLine;
    delta.endCol = cursorCol + 1;
  }
  else if (cursorLine < buffer.getLineCount() - 1)
  {
    delta.operation = EditDelta::JOIN_LINES;
    delta.deletedContent = "\n";
    delta.endLine = cursorLine + 1;
    delta.endCol = 0;
    delta.firstLineBeforeJoin = line;
    delta.secondLineBeforeJoin = buffer.getLine(cursorLine + 1);
  }
  return delta;
}

EditDelta Editor::createDeltaForBackspace()
{
  EditDelta delta;
  delta.operation = EditDelta::DELETE_CHAR;
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;

  if (cursorCol > 0)
  {
    std::string line = buffer.getLine(cursorLine);
    delta.deletedContent = std::string(1, line[cursorCol - 1]);
    delta.startLine = cursorLine;
    delta.startCol = cursorCol - 1;
    delta.endLine = cursorLine;
    delta.endCol = cursorCol;
  }
  else if (cursorLine > 0)
  {
    delta.operation = EditDelta::JOIN_LINES;
    delta.deletedContent = "\n";
    delta.startLine = cursorLine - 1;
    std::string prevLine = buffer.getLine(cursorLine - 1);
    delta.startCol = prevLine.length();
    delta.endLine = cursorLine;
    delta.endCol = 0;
    delta.firstLineBeforeJoin = prevLine;
    delta.secondLineBeforeJoin = buffer.getLine(cursorLine);
  }
  return delta;
}

EditDelta Editor::createDeltaForNewline()
{
  EditDelta delta;
  delta.operation = EditDelta::SPLIT_LINE;
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;
  delta.startLine = cursorLine;
  delta.startCol = cursorCol;
  delta.endLine = cursorLine + 1;
  delta.endCol = 0;
  delta.lineBeforeSplit = buffer.getLine(cursorLine);
  delta.insertedContent = "\n";
  return delta;
}

EditDelta Editor::createDeltaForDeleteSelection()
{
  EditDelta delta;
  delta.operation = EditDelta::DELETE_TEXT;
  delta.preCursorLine = cursorLine;
  delta.preCursorCol = cursorCol;
  delta.preViewportTop = viewportTop;
  delta.preViewportLeft = viewportLeft;
  auto [start, end] = getNormalizedSelection();
  delta.startLine = start.first;
  delta.startCol = start.second;
  delta.endLine = end.first;
  delta.endCol = end.second;
  delta.deletedContent = getSelectedText();
  return delta;
}

// --- Features ---

void Editor::updateMarkdownRendering()
{
#ifdef TREE_SITTER_ENABLED
  if (syntaxHighlighter && markdownRenderer_ &&
      syntaxHighlighter->getCurrentLanguage() == "markdown")
  {
    markdownRenderer_->updateState(buffer, syntaxHighlighter->getTree());
  }
#endif
}

void Editor::toggleMarkdownRendering()
{
#ifdef TREE_SITTER_ENABLED
  if (markdownRenderer_)
  {
    markdownRenderer_->setEnabled(!markdownRenderer_->isEnabled());
    updateMarkdownRendering();
  }
#endif
}

bool Editor::isMarkdownRenderingEnabled() const
{
#ifdef TREE_SITTER_ENABLED
  return markdownRenderer_ ? markdownRenderer_->isEnabled() : false;
#else
  return false;
#endif
}

void Editor::setCursorMode()
{
  // Simple terminal cursor shape changing
  // 2 = block (Normal), 6 = beam (Insert), 4 = underline (Visual)
  if (currentMode == NORMAL)
    printf("\033[2 q");
  else if (currentMode == INSERT)
    printf("\033[6 q");
  else if (currentMode == VISUAL)
    printf("\033[4 q");
  fflush(stdout);
}

void Editor::initializeViewportHighlighting()
{
  if (syntaxHighlighter)
  {
    syntaxHighlighter->parseViewportOnly(buffer, viewportTop);
  }
}

void Editor::forceSyntaxResync()
{
  if (syntaxHighlighter)
  {
    syntaxHighlighter->scheduleBackgroundParse(buffer);
  }
}

// Stub for unused method signature if needed
void Editor::insertTextAtCursor(const std::string &text)
{
  // Basic implementation using clipboard logic
  clipboard = text;
  pasteFromClipboard();
}

int Editor::removePreviousIndent(int amount)
{
  // Simplistic implementation for now
  if (cursorCol >= amount)
  {
    cursorCol -= amount; // Just move back?
                         // Real implementation would delete chars.
    // Keeping stub to satisfy linker if it was declared but not used.
  }
  return 0;
}