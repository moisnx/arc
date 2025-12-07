// src/modes/browser_mode.cpp
#include "browser_mode.h"
#include "src/core/application.h"
#include "src/core/config_manager.h"
#include "src/core/editor.h"
#include "src/core/editor_loop.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/flux_theme_bridge.h"
#include "src/ui/input_handler.h"
#include "src/ui/style_manager.h"

// Flux integration
#include <flux/core/browser.h>
#include <flux/ui/renderer.h>

#include <iostream>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

#define CTRL(c) ((c) & 0x1F)

int BrowserMode::run(const std::string &folder_path)
{
  // Step 1: Initialize Arc's application and style system FIRST
  if (!Application::initialize())
  {
    return 1;
  }

  // Step 2: Load Arc's theme - this sets up all COLOR_PAIR() indices (0-79)
  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    g_style_manager.load_theme_from_file(theme_file);
  }

  Application::setupMouse();

  // Step 3: Initialize Flux with Arc's existing color pairs
  flux::Browser browser(folder_path);
  flux::Renderer renderer;

  // CRITICAL: Use the bridge to tell Flux which COLOR_PAIR indices to use
  // This does NOT create new pairs - Flux will use Arc's pre-configured ones
  flux::Theme fluxTheme = arc::FluxThemeBridge::createFluxThemeFromArc();
  renderer.setTheme(fluxTheme);

  // Debug: Show what pairs we're using (remove in production)
  // arc::FluxThemeBridge::debugPrintMapping();

  // Set the terminal background to Arc's background pair
  // This ensures the entire screen uses Arc's theme consistently
  bkgd(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));

  // IMPORTANT: Don't let Flux override this with its own background
  // The renderer should respect Arc's background setting

  if (!ConfigManager::startWatchingConfig())
  {
    std::cerr << "Warning: Config watching failed" << std::endl;
  }

  bool running = true;
  while (running)
  {
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    int viewportHeight = maxY - 2;

    browser.updateScroll(viewportHeight);

    // Before rendering, ensure background is still set correctly
    // (in case Flux tries to change it)
    bkgd(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));

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
      browser.pageUp(viewportHeight);
      break;
    case KEY_NPAGE:
    case CTRL('f'):
      browser.pageDown(viewportHeight);
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
      // Re-apply background after resize
      bkgd(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
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
        std::string selectedPath = browser.getSelectedPath()->string();

        // Clean up Flux UI before transitioning to editor
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
        // editor.setDeltaUndoEnabled(true);
        // editor.beginDeltaGroup();

        if (!editor.loadFile(selectedPath))
        {
          std::cerr << "Failed to load: " << selectedPath << std::endl;
          return 1;
        }

        // Reinitialize Arc's system for editor
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

        // If returning to browser, need to reinitialize Arc and Flux
        if (exit_reason != EditorLoop::ExitReason::QUIT)
        {
          if (!Application::initialize())
          {
            return 1;
          }
          if (!theme_file.empty())
          {
            g_style_manager.load_theme_from_file(theme_file);
          }
          Application::setupMouse();

          // Re-apply Flux theme with Arc's color pairs
          fluxTheme = arc::FluxThemeBridge::createFluxThemeFromArc();
          renderer.setTheme(fluxTheme);

          // Critical: Set background again after returning from editor
          bkgd(COLOR_PAIR(ColorPairs::BACKGROUND_PAIR));
        }
        else
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