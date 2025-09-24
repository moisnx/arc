#include "src/core/editor.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/theme_manager.h"
// #include "ui/colors.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <curses.h>
#ifndef BUTTON4_PRESSED
#define BUTTON4_PRESSED 0x00200000L
#endif
#ifndef BUTTON5_PRESSED
#define BUTTON5_PRESSED 0x00100000L
#endif
#else
#include <ncurses.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Key constants
#define CTRL(x) ((x) & 0x1f)
#define KEY_TAB 9
#define KEY_ENTER 10
#define KEY_ESC 27
#define KEY_BACKSPACE_ALT 127

#ifdef _WIN32
// Windows/PDCurses specific
#ifndef KEY_ENTER
#define KEY_ENTER 10
#endif
#define GETMOUSE_FUNC nc_getmouse
#else
// Linux/NCurses specific
// KEY_ENTER is already defined in ncurses.h, use it as-is
#define GETMOUSE_FUNC getmouse
#endif

// Helper function to check if input is available without blocking
bool hasInput()
{
  int ch = getch();
  if (ch != ERR)
  {
    ungetch(ch);
    return true;
  }
  return false;
}

// Helper function to flush input buffer
void flushInputBuffer() { flushinp(); }

// Handle keys in NORMAL mode
bool handleNormalMode(Editor &editor, int ch)
{
  switch (ch)
  {
  case 'i':
  case 'I':
    editor.setMode(EditorMode::INSERT);
    if (ch == 'I')
    {
      editor.moveCursorToLineStart();
    }
    return true;

  case 'a':
  case 'A':
    editor.setMode(EditorMode::INSERT);
    if (ch == 'A')
    {
      editor.moveCursorToLineEnd();
    }
    else
    {
      editor.moveCursorRight();
    }
    return true;

  case 'o':
    editor.moveCursorToLineEnd();
    editor.insertNewline();
    editor.setMode(EditorMode::INSERT);
    return true;

  case 'O':
    editor.moveCursorToLineStart();
    editor.insertNewline();
    editor.moveCursorUp();
    editor.setMode(EditorMode::INSERT);
    return true;

  case 'v':
  case 'V':
    editor.setMode(EditorMode::VISUAL);
    return true;

  case 'x':
    editor.deleteChar();
    return true;

  case 'd':
    // Simple implementation: delete current line
    editor.deleteLine();
    return true;

  // Movement keys
  case 'h':
  case KEY_LEFT:
    editor.moveCursorLeft();
    return true;
  case 'j':
  case KEY_DOWN:
    editor.moveCursorDown();
    return true;
  case 'k':
  case KEY_UP:
    editor.moveCursorUp();
    return true;
  case 'l':
  case KEY_RIGHT:
    editor.moveCursorRight();
    return true;

  case '0':
    editor.moveCursorToLineStart();
    return true;
  case '$':
    editor.moveCursorToLineEnd();
    return true;

  case KEY_PPAGE:
    editor.pageUp();
    return true;
  case KEY_NPAGE:
    editor.pageDown();
    return true;

  // Global commands that work in normal mode
  case CTRL('s'):
    editor.saveFile();
    return true;
  case CTRL('z'):
    editor.undo();
    return true;
  case CTRL('y'):
    editor.redo();
    return true;

  default:
    return false; // Key not handled
  }
}

// Handle keys in INSERT mode
bool handleInsertMode(Editor &editor, int ch)
{
  switch (ch)
  {
  case KEY_ESC:
    editor.setMode(EditorMode::NORMAL);
    editor.moveCursorLeft(); // Vim-like behavior: move back one char
    return true;

  case KEY_BACKSPACE:
  case KEY_BACKSPACE_ALT:
  case 8: // Ctrl+H
    editor.backspace();
    return true;

  case KEY_DC:
    editor.deleteChar();
    return true;

  case KEY_ENTER:
  // case '\n':
  case '\r':
    editor.insertNewline();
    return true;

  case KEY_TAB:
    // Insert 4 spaces instead of tab
    for (int i = 0; i < 4; i++)
    {
      editor.insertChar(' ');
    }
    return true;

  // Movement keys still work in insert mode
  case KEY_UP:
    editor.moveCursorUp();
    return true;
  case KEY_DOWN:
    editor.moveCursorDown();
    return true;
  case KEY_LEFT:
    editor.moveCursorLeft();
    return true;
  case KEY_RIGHT:
    editor.moveCursorRight();
    return true;
  case KEY_HOME:
    editor.moveCursorToLineStart();
    return true;
  case KEY_END:
    editor.moveCursorToLineEnd();
    return true;
  case KEY_PPAGE:
    editor.pageUp();
    return true;
  case KEY_NPAGE:
    editor.pageDown();
    return true;

  // Global commands
  case CTRL('s'):
    editor.saveFile();
    return true;
  case CTRL('z'):
    editor.undo();
    return true;
  case CTRL('y'):
    editor.redo();
    return true;

  default:
    // Insert regular printable characters
    if (ch >= 32 && ch <= 126)
    {
      editor.insertChar(static_cast<char>(ch));
      return true;
    }
    return false;
  }
}

// Handle keys in VISUAL mode
bool handleVisualMode(Editor &editor, int ch)
{
  switch (ch)
  {
  case KEY_ESC:
  case 'v':
    editor.setMode(EditorMode::NORMAL);
    editor.clearSelection();
    return true;

  case 'x':
  case KEY_DC:
    editor.deleteSelection();
    editor.setMode(EditorMode::NORMAL);
    return true;

  case 'i':
    editor.deleteSelection();
    editor.setMode(EditorMode::INSERT);
    return true;

  // Movement keys extend selection
  case 'h':
  case KEY_LEFT:
    editor.moveCursorLeft();
    return true;
  case 'j':
  case KEY_DOWN:
    editor.moveCursorDown();
    return true;
  case 'k':
  case KEY_UP:
    editor.moveCursorUp();
    return true;
  case 'l':
  case KEY_RIGHT:
    editor.moveCursorRight();
    return true;

  case '0':
    editor.moveCursorToLineStart();
    return true;
  case '$':
    editor.moveCursorToLineEnd();
    return true;

  case KEY_PPAGE:
    editor.pageUp();
    return true;
  case KEY_NPAGE:
    editor.pageDown();
    return true;

  // Global commands
  case CTRL('s'):
    editor.saveFile();
    return true;
  case CTRL('z'):
    editor.undo();
    return true;
  case CTRL('y'):
    editor.redo();
    return true;

  default:
    return false;
  }
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  SyntaxHighlighter syntaxHighlighter;
  Editor editor(&syntaxHighlighter);

  if (!editor.loadFile(argv[1]))
  {
    std::cerr << "Warning: Could not open file " << argv[1] << std::endl;
  }

  initscr();
  cbreak();
  keypad(stdscr, TRUE);
  noecho();
  // use_color
  g_theme_manager.initialize();
  // std::cerr << "Colors: " << COLORS << ", Color Pairs: " << COLOR_PAIRS
  //           << std::endl;
  if (!g_theme_manager.load_theme_from_file("themes/default.theme"))
  {
    std::cerr << "Failed to load theme, using default" << std::endl;
  }

  nodelay(stdscr, FALSE);
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  printf("\033[?1003h");
  fflush(stdout);

  editor.display();

  // std::cerr << "Color pair 9 (KEYWORD) after display = " <<
  // COLOR_PAIR(KEYWORD)
  //           << std::endl;
  refresh();
  refresh();

  int ch;
  bool needsRedraw = true;

  while ((ch = getch()) != 'q' && ch != 'Q')
  {
    bool shouldRedraw = false;
    bool keyHandled = false;
    int moveCount = 0;

    // Handle mouse events first
    if (ch == KEY_MOUSE)
    {
      MEVENT event;
      if (GETMOUSE_FUNC(&event) == OK)
      {
        editor.handleMouse(event);
        shouldRedraw = true;
        keyHandled = true;
      }
    }
    else if (ch == KEY_RESIZE)
    {
      editor.handleResize();
      flushInputBuffer();
      shouldRedraw = true;
      keyHandled = true;
    }
    else
    {
      // Handle mode-specific keys
      EditorMode currentMode = editor.getMode();

      switch (currentMode)
      {
      case EditorMode::NORMAL:
        keyHandled = handleNormalMode(editor, ch);
        break;
      case EditorMode::INSERT:
        keyHandled = handleInsertMode(editor, ch);
        break;
      case EditorMode::VISUAL:
        keyHandled = handleVisualMode(editor, ch);
        break;
      }

      if (keyHandled)
      {
        shouldRedraw = true;

        // Handle rapid movement batching for arrow keys in normal mode
        if (currentMode == EditorMode::NORMAL &&
            (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT ||
             ch == KEY_RIGHT))
        {
          moveCount = 1;
          nodelay(stdscr, TRUE);

          int nextCh;
          while ((nextCh = getch()) != ERR && moveCount < 50)
          {
            if (nextCh == ch)
            {
              switch (nextCh)
              {
              case KEY_UP:
                editor.moveCursorUp();
                break;
              case KEY_DOWN:
                editor.moveCursorDown();
                break;
              case KEY_LEFT:
                editor.moveCursorLeft();
                break;
              case KEY_RIGHT:
                editor.moveCursorRight();
                break;
              }
              moveCount++;
            }
            else
            {
              ungetch(nextCh);
              break;
            }
          }

          nodelay(stdscr, FALSE);
        }
      }
    }

    // Redraw if needed and no pending input
    if (shouldRedraw)
    {
      nodelay(stdscr, TRUE);
      int pendingCh = getch();
      nodelay(stdscr, FALSE);

      if (pendingCh != ERR)
      {
        ungetch(pendingCh);
      }
      else
      {
        editor.display();
        refresh();
      }
    }
  }

  printf("\033[?1003l");
  fflush(stdout);
  endwin();
  return 0;
}