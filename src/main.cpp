// src/main.cpp
#include "src/core/config_manager.h"
#include "src/core/editor.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/input_handler.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#include <termios.h>
#include <unistd.h>
#endif

void disableXonXoff();
bool initializeNcurses();
bool initializeThemes();
void setupMouse();
void cleanupMouse();

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0]
              << " <filename> [--bench-quit] [--none] [--quit]" << std::endl;
    return 1;
  }

  // Parse command line arguments
  std::vector<std::string> args(argv, argv + argc);
  bool bench_quit = std::any_of(args.begin(), args.end(), [](const auto &arg)
                                { return arg == "--bench-quit"; });
  bool force_no_highlighting =
      std::any_of(args.begin(), args.end(),
                  [](const auto &arg) { return arg == "--none"; });
  bool quit_immediately =
      std::any_of(args.begin(), args.end(),
                  [](const auto &arg) { return arg == "--quit"; });

  std::string filename = std::filesystem::absolute(argv[1]).string();

  // Initialize config
  if (!ConfigManager::ensureConfigStructure())
  {
    std::cerr << "Warning: Failed to ensure config structure" << std::endl;
  }
  ConfigManager::copyProjectFilesToConfig();
  ConfigManager::loadConfig();

  // BENCHMARK PATH: Load file and quit immediately
  if (bench_quit)
  {
    SyntaxHighlighter syntaxHighlighter;
    Editor editor(force_no_highlighting ? nullptr : &syntaxHighlighter);

    if (!force_no_highlighting)
    {
      std::string syntax_dir = ConfigManager::getSyntaxRulesDir();
      if (syntaxHighlighter.initialize(syntax_dir))
      {
        editor.setSyntaxHighlighter(&syntaxHighlighter);
        editor.updateSyntaxHighlighting();
      }
    }

    if (!editor.loadFile(filename))
    {
      std::cerr << "Error: Could not open file " << filename << std::endl;
      return 1;
    }

    std::cout << "Benchmark complete. File loaded successfully." << std::endl;
    return 0;
  }

  // Determine syntax mode (CLI args override config)
  SyntaxMode syntax_mode =
      force_no_highlighting ? SyntaxMode::NONE : ConfigManager::getSyntaxMode();

  // Create syntax highlighter
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

  // Create editor
  Editor editor(highlighterPtr);

  if (!editor.loadFile(filename))
  {
    std::cerr << "Warning: Could not open file " << filename << std::endl;
  }

  // Initialize ncurses
  disableXonXoff();

  if (!initializeNcurses())
  {
    return 1;
  }

  if (!initializeThemes())
  {
    endwin();
    return 1;
  }

  // Load theme
  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    if (!g_style_manager.load_theme_from_file(theme_file))
    {
      std::cerr << "FATAL: Theme load failed" << std::endl;
    }
  }

  setupMouse();

  if (highlighterPtr)
  {
    editor.initializeViewportHighlighting();
  }
  // Start editor
  InputHandler inputHandler(editor);
  editor.setMode(EditorMode::INSERT);
  editor.display();
  refresh();

  // Handle --quit for testing
  if (quit_immediately)
  {
    cleanupMouse();
    attrset(A_NORMAL);
    curs_set(1);
    endwin();
    std::cerr << "First line: " << editor.getFirstLine() << std::endl;
    return 0;
  }

  if (highlighterPtr)
  {
    highlighterPtr->scheduleBackgroundParse(editor.getBuffer());
  }
  // Start config watching
  if (!ConfigManager::startWatchingConfig())
  {
    std::cerr << "Warning: Config watching failed" << std::endl;
  }

  // Main loop
  int key;
  bool running = true;
  while (running)
  {
    if (ConfigManager::isReloadPending())
    {
      editor.display();
      refresh();
    }

    key = getch();
    if (key == 'q' || key == 'Q')
    {
      if (editor.getMode() == EditorMode::NORMAL)
      {
        running = false;
        continue;
      }
    }

    InputHandler::KeyResult result = inputHandler.handleKey(key);

    switch (result)
    {
    case InputHandler::KeyResult::QUIT:
      running = false;
      break;
    case InputHandler::KeyResult::REDRAW:
    case InputHandler::KeyResult::HANDLED:
      editor.display();
      refresh();
      break;
    case InputHandler::KeyResult::NOT_HANDLED:
      break;
    }
  }

  // Cleanup
  cleanupMouse();
  attrset(A_NORMAL);
  curs_set(1);
  endwin();
  return 0;
}

void disableXonXoff()
{
#ifndef _WIN32
  struct termios tty;
  if (tcgetattr(STDIN_FILENO, &tty) == 0)
  {
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
  }
  else
  {
    std::cerr << "Warning: Could not disable XON/XOFF flow control"
              << std::endl;
  }
#endif
}

bool initializeNcurses()
{
  initscr();
  cbreak();
  keypad(stdscr, TRUE);
  noecho();
  curs_set(1);
  timeout(50);

  if (!has_colors())
  {
    endwin();
    std::cerr << "Error: Terminal does not support colors" << std::endl;
    return false;
  }

  if (start_color() == ERR)
  {
    endwin();
    std::cerr << "Error: Could not initialize colors" << std::endl;
    return false;
  }

  if (use_default_colors() == ERR)
  {
    assume_default_colors(COLOR_WHITE, COLOR_BLACK);
  }

  return true;
}

bool initializeThemes()
{
  g_style_manager.initialize();

  // Register the callback for future config changes
  ConfigManager::registerReloadCallback(
      []()
      {
        // 1. Get current theme from the *newly loaded* config
        std::string active_theme = ConfigManager::getActiveTheme();
        std::string theme_file = ConfigManager::getThemeFile(active_theme);

        if (!theme_file.empty())
        {
          // 2. Load the theme
          if (!g_style_manager.load_theme_from_file(theme_file))
          {
            std::cerr << "ERROR: Theme reload failed for: " << active_theme
                      << std::endl;
          }
        }
      });

  // NOTE: Initial theme load logic removed from here
  return true;
}

void setupMouse()
{
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
  printf("\033[?1003h");
  fflush(stdout);
}

void cleanupMouse()
{
  printf("\033[?1003l");
  fflush(stdout);
}