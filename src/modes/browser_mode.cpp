// src/modes/browser_mode.cpp
#include "browser_mode.h"
#include "src/core/application.h"
#include "src/core/config_manager.h"
#include "src/core/editor.h"
#include "src/core/editor_loop.h"
#include "src/core/file_browser.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/browser_renderer.h"
#include "src/ui/input_handler.h"
#include "src/ui/style_manager.h"
#include <iostream>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

#define CTRL(c) ((c) & 0x1F)

int BrowserMode::run(const std::string &folder_path)
{
  if (!Application::initialize())
  {
    return 1;
  }

  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    g_style_manager.load_theme_from_file(theme_file);
  }

  Application::setupMouse();

  FileBrowser browser(folder_path);
  BrowserRenderer renderer;
  renderer.setIconStyle(IconStyle::AUTO);

  if (!ConfigManager::startWatchingConfig())
  {
    std::cerr << "Warning: Config watching failed" << std::endl;
  }

  bool running = true;
  while (running)
  {
    browser.updateScroll(renderer.getViewportHeight());
    renderer.render(browser);

    int key = getch();

    switch (key)
    {
    case KEY_UP:
    case 'k':
      browser.selectPrevious();
      break;
    case KEY_DOWN:
    case 'j':
      browser.selectNext();
      break;
    case KEY_HOME:
    case 'g':
      browser.selectFirst();
      break;
    case KEY_END:
    case 'G':
      browser.selectLast();
      break;
    case KEY_PPAGE:
    case CTRL('b'):
      browser.pageUp(renderer.getViewportHeight());
      break;
    case KEY_NPAGE:
    case CTRL('f'):
      browser.pageDown(renderer.getViewportHeight());
      break;
    case KEY_LEFT:
    case 'h':
      browser.navigateUp();
      break;
    case '.':
    case 'H':
      browser.toggleHidden();
      break;
    case 's':
      browser.cycleSortMode();
      break;
    case 'r':
    case KEY_F(5):
      browser.refresh();
      break;
    case CTRL('q'):
    case 27:
      running = false;
      break;
    case KEY_RESIZE:
      break;

    case KEY_RIGHT:
    case KEY_ENTER:
    case 10:
    case 13:
    {
      if (browser.isSelectedDirectory())
      {
        browser.navigateInto(browser.getSelectedIndex());
      }
      else
      {
        auto path = browser.getSelectedPath();
        if (!path.has_value())
          break;

        Application::cleanup();

        SyntaxHighlighter syntaxHighlighter;
        SyntaxHighlighter *highlighterPtr = nullptr;

        if (ConfigManager::getSyntaxMode() != SyntaxMode::NONE)
        {
          if (syntaxHighlighter.initialize(ConfigManager::getSyntaxRulesDir()))
          {
            highlighterPtr = &syntaxHighlighter;
          }
        }

        Editor editor(highlighterPtr);
        editor.setDeltaUndoEnabled(true);
        editor.beginDeltaGroup();

        if (!editor.loadFile(path.value().string()))
        {
          std::cerr << "Failed to load: " << path.value() << std::endl;
          return 1;
        }

        if (!Application::initialize())
        {
          return 1;
        }
        if (!theme_file.empty())
        {
          g_style_manager.load_theme_from_file(theme_file);
        }
        Application::setupMouse();

        if (highlighterPtr)
        {
          editor.initializeViewportHighlighting();
        }

        InputHandler inputHandler(editor);
        editor.setCursorMode();
        editor.display();
        doupdate();
        curs_set(1);

        if (highlighterPtr)
        {
          highlighterPtr->scheduleBackgroundParse(editor.getBuffer());
        }
        ConfigManager::startWatchingConfig();

        auto exit_reason = EditorLoop::run(editor, inputHandler);

        Application::cleanup();

        if (exit_reason == EditorLoop::ExitReason::QUIT)
        {
          break;
        }
      }
      break;
    }
    }
  }

  Application::cleanup();
  return 0;
}