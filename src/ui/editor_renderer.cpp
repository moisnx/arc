#include "editor_renderer.h"
#include "src/core/buffer.h"
#include "src/features/markdown_renderer.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/style_manager.h"
#include "src/utils/binary_detector.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <ncursesw/ncurses.h>
#include <vector>

// =================================================================
// RenderContext Helpers
// =================================================================

bool RenderContext::isSelected(int line, int col) const
{
  if (!hasSelection)
    return false;

  if (selStartLine == selEndLine)
  {
    return line == selStartLine && col >= selStartCol && col < selEndCol;
  }

  if (line == selStartLine)
    return col >= selStartCol;
  if (line == selEndLine)
    return col < selEndCol;
  return line > selStartLine && line < selEndLine;
}

// =================================================================
// EditorRenderer Implementation
// =================================================================

EditorRenderer::EditorRenderer() {}

std::string EditorRenderer::expandTabs(const std::string &line, int tabSize)
{
  std::string result;
  result.reserve(line.length() * 1.5);
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

void EditorRenderer::render(const RenderContext &ctx)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int lineNumWidth = ctx.showLineNumbers
                         ? std::to_string(ctx.buffer.getLineCount()).length()
                         : 0;
  int contentStartCol = ctx.showLineNumbers ? (lineNumWidth + 3) : 0;
  int contentWidth = cols - contentStartCol;

  int endLine =
      std::min(ctx.viewportTop + ctx.viewportHeight, ctx.buffer.getLineCount());

  if (ctx.highlighter)
  {
    ctx.highlighter->markViewportLines(ctx.viewportTop, endLine - 1);
  }

  bool isMarkdown =
      ctx.highlighter && ctx.highlighter->getCurrentLanguage() == "markdown";

  for (int i = ctx.viewportTop; i < endLine; i++)
  {
    int screenRow = i - ctx.viewportTop;
    bool isCurrentLine = (ctx.cursorLine == i);

    move(screenRow, 0);
    attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));

    // 1. Enhanced Line Numbers with separator
    if (ctx.showLineNumbers)
    {
      int ln_colorPair = isCurrentLine ? ColorPairs::LINE_NUMBERS_ACTIVE
                                       : ColorPairs::LINE_NUMBERS;
      attron(COLOR_PAIR(ln_colorPair));
      printw("%*d", lineNumWidth, i + 1);
      attroff(COLOR_PAIR(ln_colorPair));

      // Sophisticated separator
      attron(COLOR_PAIR(ColorPairs::UI_BORDER));
      if (isCurrentLine)
      {
        printw(" ▏ ");
      }
      else
      {
        printw(" │ ");
      }
      attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
    }

    // 2. Content Preparation
    std::string rawLine = ctx.buffer.getLine(i);
    std::string displayLine = expandTabs(rawLine, ctx.tabSize);
    std::vector<ColorSpan> currentLineSpans;
    int leftPadding = 0;

    if (isMarkdown && ctx.markdown && ctx.markdown->isEnabled())
    {
      auto renderInfo = ctx.markdown->renderLine(
          rawLine, i, ctx.cursorLine, ctx.buffer, ctx.highlighter->getTree());

      if (renderInfo.hide_line)
      {
        displayLine = "";
      }
      else
      {
        displayLine = expandTabs(renderInfo.display_text, ctx.tabSize);
        leftPadding = renderInfo.left_padding;

        if (!renderInfo.is_code_block && !renderInfo.spans.empty())
        {
          currentLineSpans = renderInfo.spans;
        }
        else if (ctx.highlighter)
        {
          try
          {
            currentLineSpans =
                ctx.highlighter->getHighlightSpans(displayLine, i, ctx.buffer);
          }
          catch (...)
          {
          }
        }
      }
    }
    else if (ctx.highlighter)
    {
      try
      {
        currentLineSpans =
            ctx.highlighter->getHighlightSpans(displayLine, i, ctx.buffer);
      }
      catch (...)
      {
      }
    }

    std::vector<RenderSpan> renderSpans =
        buildRenderSpans(displayLine, currentLineSpans, ctx, i, contentWidth);

    // 3. Draw Line
    int screenCol = 0;

    // Padding (Markdown)
    for (int p = 0; p < leftPadding && screenCol < contentWidth; p++)
    {
      addch(' ');
      screenCol++;
    }

    for (const auto &span : renderSpans)
    {
      if (screenCol >= contentWidth)
        break;

      if (span.isSelected)
      {
        attron(COLOR_PAIR(ColorPairs::STATE_SELECTED) | A_REVERSE);
      }
      else if (span.colorPair >= 0)
      {
        attron(COLOR_PAIR(span.colorPair));
        if (span.attribute != 0)
          attron(span.attribute);
      }
      else
      {
        attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
      }

      for (int col = span.start; col < span.end && screenCol < contentWidth;
           ++col)
      {
        int fileCol = ctx.viewportLeft + col;
        char ch = ' ';
        if (fileCol >= 0 && fileCol < (int)displayLine.length())
        {
          ch = displayLine[fileCol];
          if (ch < 32 || ch > 126)
            ch = ' ';
        }
        addch(ch);
        screenCol++;
      }

      if (span.isSelected)
      {
        attroff(COLOR_PAIR(ColorPairs::STATE_SELECTED) | A_REVERSE);
      }
      else if (span.colorPair >= 0)
      {
        if (span.attribute != 0)
          attroff(span.attribute);
        attroff(COLOR_PAIR(span.colorPair));
      }
    }
    attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
    clrtoeol();
  }

  // Clear empty lines with tilde indicators (like Vim)
  attrset(COLOR_PAIR(ColorPairs::STATE_DISABLED));
  for (int i = endLine - ctx.viewportTop; i < ctx.viewportHeight; i++)
  {
    move(i, 0);
    if (ctx.showLineNumbers)
    {
      int lineNumWidth = std::to_string(ctx.buffer.getLineCount()).length();
      for (int j = 0; j < lineNumWidth; j++)
        addch(' ');
      printw(" │ ");
    }
    addch('~');
    clrtoeol();
  }
}

std::vector<EditorRenderer::RenderSpan> EditorRenderer::buildRenderSpans(
    const std::string &line, const std::vector<ColorSpan> &highlightSpans,
    const RenderContext &ctx, int currentLine, int contentWidth)
{
  std::vector<RenderSpan> spans;
  int spanStart = 0;
  int currentColorPair = -1;
  int currentAttribute = 0;
  bool currentlySelected = false;

  for (int screenCol = 0; screenCol < contentWidth; ++screenCol)
  {
    int fileCol = ctx.viewportLeft + screenCol;
    bool selected = ctx.isSelected(currentLine, fileCol);

    const ColorSpan *highlight = nullptr;
    if (fileCol >= 0 && fileCol < (int)line.length())
    {
      for (const auto &s : highlightSpans)
      {
        if (fileCol >= s.start && fileCol < s.end)
        {
          highlight = &s;
          break;
        }
      }
    }

    int colorPair = highlight ? highlight->colorPair : -1;
    int attribute = highlight ? highlight->attribute : 0;

    if ((selected != currentlySelected || colorPair != currentColorPair ||
         attribute != currentAttribute) &&
        screenCol > spanStart)
    {
      spans.push_back({spanStart, screenCol, currentColorPair, currentAttribute,
                       currentlySelected});
      spanStart = screenCol;
    }

    currentlySelected = selected;
    currentColorPair = colorPair;
    currentAttribute = attribute;
  }

  if (spanStart < contentWidth)
  {
    spans.push_back({spanStart, contentWidth, currentColorPair,
                     currentAttribute, currentlySelected});
  }
  return spans;
}

// =================================================================
// ENHANCED STATUS BAR - Modern Design
// =================================================================

void EditorRenderer::drawStatusBar(const RenderContext &ctx,
                                   const std::string &filename,
                                   const std::string &fileLang, bool isModified,
                                   bool isBinary)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int statusRow = rows - 1;

  move(statusRow, 0);
  attrset(COLOR_PAIR(ColorPairs::STATUS_BAR));

  // Fill entire status bar
  for (int i = 0; i < cols; i++)
    addch(' ');

  move(statusRow, 0);

  // === LEFT SECTION ===
  int currentCol = 1;

  // 1. Mode indicator (like Neovim)
  attron(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE) | A_BOLD);
  printw(" NORMAL ");
  attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE) | A_BOLD);
  currentCol += 8;

  // Separator
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  mvprintw(statusRow, currentCol, "▕");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
  currentCol += 2;

  // 2. File Name with icon
  move(statusRow, currentCol);
  attron(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));

  if (filename.empty())
  {
    printw("󰈙 [No Name]");
    currentCol += 11;
  }
  else
  {
    size_t lastSlash = filename.find_last_of("/\\");
    std::string display = (lastSlash != std::string::npos)
                              ? filename.substr(lastSlash + 1)
                              : filename;

    // Truncate if too long
    if (display.length() > 30)
      display = display.substr(0, 27) + "...";

    printw("󰈙 %s", display.c_str());
    currentCol += display.length() + 3;
  }
  attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));

  // 3. Modified indicator
  if (isModified)
  {
    move(statusRow, currentCol);
    attron(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);
    printw(" ● ");
    attroff(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);
    currentCol += 3;
  }

  // === CENTER SECTION ===
  // Git branch (placeholder for future implementation)
  int centerStart = cols / 2 - 10;
  if (centerStart > currentCol + 5)
  {
    move(statusRow, centerStart);
    attron(COLOR_PAIR(ColorPairs::UI_INFO));
    printw("  main");
    attroff(COLOR_PAIR(ColorPairs::UI_INFO));
  }

  // === RIGHT SECTION ===
  std::string rightSection;

  // 1. Language/File type
  if (isBinary)
  {
    rightSection = " BINARY ";
  }
  else
  {
    rightSection = " " + fileLang + " ";
  }

  // 2. Encoding
  rightSection += "▕ UTF-8 ";

  // 3. Selection info
  if (ctx.hasSelection)
  {
    if (ctx.selStartLine == ctx.selEndLine)
    {
      int chars = abs(ctx.selEndCol - ctx.selStartCol);
      rightSection += "▕  " + std::to_string(chars) + " sel ";
    }
    else
    {
      int lines = abs(ctx.selEndLine - ctx.selStartLine) + 1;
      rightSection += "▕  " + std::to_string(lines) + " lines ";
    }
  }

  // 4. Cursor position
  int percent = (ctx.buffer.getLineCount() == 0)
                    ? 0
                    : ((ctx.cursorLine + 1) * 100 / ctx.buffer.getLineCount());

  char posBuffer[32];
  snprintf(posBuffer, sizeof(posBuffer), "▕  %d:%d ", ctx.cursorLine + 1,
           ctx.cursorCol + 1);
  rightSection += posBuffer;

  // 5. Percentage
  snprintf(posBuffer, sizeof(posBuffer), "▕ %3d%% ", percent);
  rightSection += posBuffer;

  // Draw right section
  int rightStartCol = cols - rightSection.length();
  if (rightStartCol > currentCol)
  {
    move(statusRow, rightStartCol);
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));
    printw("%s", rightSection.c_str());
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_TEXT));
  }
}

// =================================================================
// MODERN MODAL SYSTEM
// =================================================================

void EditorRenderer::drawUnsavedChangesModal(const std::string &filename,
                                             int rows, int cols)
{
  // Modern floating modal with shadow effect
  int modalWidth = 60;
  int modalHeight = 14;
  int centerRow = (rows - modalHeight) / 2;
  int centerCol = (cols - modalWidth) / 2;

  // Draw shadow/dim overlay
  attrset(COLOR_PAIR(ColorPairs::STATE_DISABLED));
  for (int r = 0; r < rows - 1; r++)
  {
    move(r, 0);
    for (int c = 0; c < cols; c++)
    {
      // Skip modal area
      if (r >= centerRow && r < centerRow + modalHeight && c >= centerCol &&
          c < centerCol + modalWidth)
        continue;
      addch(' ');
    }
  }

  // Clear modal background
  attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
  for (int r = centerRow; r < centerRow + modalHeight; r++)
  {
    move(r, centerCol);
    for (int c = 0; c < modalWidth; c++)
      addch(' ');
  }

  // === HEADER ===
  move(centerRow, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);

  // Top border with rounded corners
  printw("╭");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("╮");

  // Title line
  move(centerRow + 1, centerCol);
  printw("│");
  int titlePad = (modalWidth - 22) / 2;
  for (int i = 0; i < titlePad; i++)
    addch(' ');
  printw("⚠  UNSAVED CHANGES  ⚠");
  move(centerRow + 1, centerCol + modalWidth - 1);
  printw("│");

  attroff(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);

  // Separator
  move(centerRow + 2, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("├");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("┤");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // === CONTENT ===
  auto drawLine = [&](int row, const std::string &text,
                      int colorPair = ColorPairs::UI_PRIMARY)
  {
    move(row, centerCol);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

    int textPad = (modalWidth - 2 - text.length()) / 2;
    for (int i = 0; i < textPad; i++)
      addch(' ');

    attron(COLOR_PAIR(colorPair));
    printw("%s", text.c_str());
    attroff(COLOR_PAIR(colorPair));

    move(row, centerCol + modalWidth - 1);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
  };

  drawLine(centerRow + 3, "");
  drawLine(centerRow + 4,
           "Your changes will be lost if you quit without saving.");
  drawLine(centerRow + 5, "");

  // File info
  std::string displayName = filename.empty() ? "[No Name]" : filename;
  if (displayName.length() > 50)
    displayName = "..." + displayName.substr(displayName.length() - 47);

  move(centerRow + 6, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("│  ");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
  attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
  printw("File: ");
  attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));
  attron(COLOR_PAIR(ColorPairs::UI_ACCENT) | A_BOLD);
  printw("%s", displayName.c_str());
  attroff(COLOR_PAIR(ColorPairs::UI_ACCENT) | A_BOLD);
  move(centerRow + 6, centerCol + modalWidth - 1);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("│");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  drawLine(centerRow + 7, "");

  // Separator
  move(centerRow + 8, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("├");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("┤");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // === ACTIONS ===
  move(centerRow + 9, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("│");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // Center the buttons
  int btnStart = (modalWidth - 46) / 2;
  move(centerRow + 9, centerCol + btnStart);

  attron(COLOR_PAIR(ColorPairs::UI_SUCCESS) | A_BOLD);
  printw(" [S] Save & Quit ");
  attroff(COLOR_PAIR(ColorPairs::UI_SUCCESS) | A_BOLD);

  printw("   ");

  attron(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);
  printw(" [Q] Discard ");
  attroff(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);

  move(centerRow + 9, centerCol + modalWidth - 1);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("│");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  drawLine(centerRow + 10, "");

  move(centerRow + 11, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("│");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  int cancelStart = (modalWidth - 16) / 2;
  move(centerRow + 11, centerCol + cancelStart);
  attron(COLOR_PAIR(ColorPairs::UI_INFO));
  printw("[ESC] Cancel");
  attroff(COLOR_PAIR(ColorPairs::UI_INFO));

  move(centerRow + 11, centerCol + modalWidth - 1);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("│");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // Bottom border
  move(centerRow + 12, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);
  printw("╰");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("╯");
  attroff(COLOR_PAIR(ColorPairs::UI_WARNING) | A_BOLD);

  // Hint at bottom
  move(centerRow + 13, centerCol);
  attron(COLOR_PAIR(ColorPairs::STATE_DISABLED));
  int hintPad = (modalWidth - 28) / 2;
  for (int i = 0; i < hintPad; i++)
    addch(' ');
  printw("Tip: Ctrl+S to save anytime");
  attroff(COLOR_PAIR(ColorPairs::STATE_DISABLED));

  wnoutrefresh(stdscr);
  doupdate();
}

// =================================================================
// SLEEK BINARY WARNING
// =================================================================

void EditorRenderer::drawBinaryWarning(const std::string &filename, int rows,
                                       int cols)
{
  attrset(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
  clear();

  int modalWidth = 66;
  int modalHeight = 18;
  int centerRow = (rows - modalHeight) / 2;
  int centerCol = (cols - modalWidth) / 2;

  // Clear background
  for (int r = centerRow; r < centerRow + modalHeight; r++)
  {
    move(r, centerCol);
    for (int c = 0; c < modalWidth; c++)
      addch(' ');
  }

  // === HEADER ===
  move(centerRow, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);
  printw("╭");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("╮");

  // Icon and title
  move(centerRow + 1, centerCol);
  printw("│");
  int titlePad = (modalWidth - 24) / 2;
  for (int i = 0; i < titlePad; i++)
    addch(' ');
  printw("🔒  BINARY FILE WARNING");
  move(centerRow + 1, centerCol + modalWidth - 1);
  printw("│");
  attroff(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);

  // Separator
  move(centerRow + 2, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("├");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("┤");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  auto drawInfoLine = [&](int row, const std::string &label,
                          const std::string &value, bool highlight = false)
  {
    move(row, centerCol);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│  ");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

    attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
    printw("%-10s", label.c_str());
    attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

    if (highlight)
      attron(COLOR_PAIR(ColorPairs::UI_ACCENT) | A_BOLD);
    else
      attron(COLOR_PAIR(ColorPairs::UI_INFO));

    printw("%s", value.c_str());

    if (highlight)
      attroff(COLOR_PAIR(ColorPairs::UI_ACCENT) | A_BOLD);
    else
      attroff(COLOR_PAIR(ColorPairs::UI_INFO));

    move(row, centerCol + modalWidth - 1);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
  };

  auto drawEmptyLine = [&](int row)
  {
    move(row, centerCol);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│");
    move(row, centerCol + modalWidth - 1);
    printw("│");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
  };

  auto drawTextLine = [&](int row, const std::string &text)
  {
    move(row, centerCol);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│  ");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

    attron(COLOR_PAIR(ColorPairs::UI_PRIMARY));
    printw("%s", text.c_str());
    attroff(COLOR_PAIR(ColorPairs::UI_PRIMARY));

    move(row, centerCol + modalWidth - 1);
    attron(COLOR_PAIR(ColorPairs::UI_BORDER));
    printw("│");
    attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
  };

  // Content
  drawEmptyLine(centerRow + 3);
  drawTextLine(centerRow + 4,
               "This file contains binary data that cannot be safely");
  drawTextLine(centerRow + 5, "displayed or edited as text.");
  drawEmptyLine(centerRow + 6);

  // File info
  std::string displayName = filename;
  if (displayName.length() > 50)
    displayName = "..." + displayName.substr(displayName.length() - 47);

  drawInfoLine(centerRow + 7, "File:", displayName, true);

  std::string fileType = BinaryDetector::detectFileType(filename);
  std::string fileSize = BinaryDetector::getFileSize(filename);

  drawInfoLine(centerRow + 8, "Type:", fileType);
  drawInfoLine(centerRow + 9, "Size:", fileSize);

  drawEmptyLine(centerRow + 10);

  // Separator
  move(centerRow + 11, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  printw("├");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("┤");
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // Suggestions
  drawEmptyLine(centerRow + 12);
  drawTextLine(centerRow + 13, "💡 Suggestions:");
  drawTextLine(centerRow + 14, "   • Use a hex editor (e.g., xxd, hexdump)");
  drawTextLine(centerRow + 15, "   • Use appropriate binary editing tools");
  drawEmptyLine(centerRow + 16);

  // Bottom border
  move(centerRow + 17, centerCol);
  attron(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);
  printw("╰");
  for (int i = 1; i < modalWidth - 1; i++)
    printw("─");
  printw("╯");
  attroff(COLOR_PAIR(ColorPairs::UI_ERROR) | A_BOLD);

  // Prompt
  move(rows - 2, (cols - 30) / 2);
  attron(COLOR_PAIR(ColorPairs::UI_SUCCESS) | A_BOLD);
  printw("Press any key to continue...");
  attroff(COLOR_PAIR(ColorPairs::UI_SUCCESS) | A_BOLD);

  wnoutrefresh(stdscr);
  doupdate();
}