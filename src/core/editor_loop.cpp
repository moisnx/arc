// src/core/editor_loop.cpp - Add injection reparsing
#include "editor_loop.h"
#include "config_manager.h"
#include "editor.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/input_handler.h"

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

EditorLoop::ExitReason EditorLoop::run(Editor &editor,
                                       InputHandler &inputHandler)
{
  bool running = true;

  while (running)
  {
    if (ConfigManager::isReloadPending())
    {
      curs_set(0);
      editor.display();
      wnoutrefresh(stdscr);
      doupdate();
      editor.positionCursor();
      curs_set(1);
    }

    static bool was_highlighting_ready = false;
    bool is_highlighting_ready = editor.getSyntaxHighlighter()->isReady();

    if (!was_highlighting_ready && is_highlighting_ready)
    {
      curs_set(0);
      editor.display();
      wnoutrefresh(stdscr);
      doupdate();
      editor.positionCursor();
      curs_set(1);
    }

    was_highlighting_ready = is_highlighting_ready;

    // Check if injections need reparsing (debounced, non-blocking)
    if (auto *highlighter = editor.getSyntaxHighlighter())
    {
      highlighter->reparseInjectionsIfNeeded();

      if (highlighter->needsRedraw())
      {
        curs_set(0);
        editor.display();
        wnoutrefresh(stdscr);
        doupdate();
        editor.positionCursor();
        curs_set(1);
      }
    }

    int key = getch();
    InputHandler::KeyResult result = inputHandler.handleKey(key);

    switch (result)
    {
    case InputHandler::KeyResult::QUIT:
      running = false;
      break;
    case InputHandler::KeyResult::REDRAW:
    case InputHandler::KeyResult::HANDLED:
      curs_set(0);
      editor.display();
      wnoutrefresh(stdscr);
      doupdate();
      editor.positionCursor();
      curs_set(1);
      break;
    case InputHandler::KeyResult::NOT_HANDLED:
      break;
    }
  }

  return ExitReason::QUIT;
}