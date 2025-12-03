// src/modes/editor_mode.cpp
#include "editor_mode.h"
#include "src/core/application.h"
#include "src/core/args_parser.h"
#include "src/core/config_manager.h"
#include "src/core/editor.h"
#include "src/core/editor_loop.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/input_handler.h"
#include "src/ui/style_manager.h"
#include <iostream>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

int EditorMode::run(const std::string &filename, const ProgramArgs &args)
{
  SyntaxMode syntax_mode = args.force_no_highlighting
                               ? SyntaxMode::NONE
                               : ConfigManager::getSyntaxMode();

  SyntaxHighlighter syntaxHighlighter;
  SyntaxHighlighter *highlighterPtr = nullptr;

  if (syntax_mode != SyntaxMode::NONE)
  {
    std::string syntax_dir = ConfigManager::getSyntaxRulesDir();
    if (syntaxHighlighter.initialize(syntax_dir))
    {
      highlighterPtr = &syntaxHighlighter;
    }
    else
    {
      std::cerr << "Warning: Syntax highlighter init failed" << std::endl;
    }
  }

  Editor editor(highlighterPtr);
  editor.setDeltaUndoEnabled(true);
  editor.beginDeltaGroup();

  if (!editor.loadFile(filename))
  {
    std::cerr << "Warning: Could not open file " << filename << std::endl;
  }

  if (!Application::initialize())
  {
    return 1;
  }

  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    if (!g_style_manager.load_theme_from_file(theme_file))
    {
      std::cerr << "FATAL: Theme load failed" << std::endl;
    }
  }

  Application::setupMouse();

  if (highlighterPtr)
  {
    editor.initializeViewportHighlighting();
  }

  InputHandler inputHandler(editor);
  editor.setCursorMode();

  editor.display();
  wnoutrefresh(stdscr);
  doupdate();
  curs_set(1);

  if (highlighterPtr)
  {
    highlighterPtr->scheduleBackgroundParse(editor.getBuffer());
  }

  if (!ConfigManager::startWatchingConfig())
  {
    std::cerr << "Warning: Config watching failed" << std::endl;
  }

  EditorLoop::run(editor, inputHandler);

  Application::cleanup();
  return 0;
}