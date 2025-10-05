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

enum class BenchmarkMode
{
  NONE,
  STARTUP_ONLY,        // Just ncurses + theme init (your current --quit)
  STARTUP_INTERACTIVE, // Full init to first input ready (RECOMMENDED)
  FILE_LOAD,           // Measure file loading separately
  FULL_CYCLE           // Complete startup + single operation + exit
};

struct BenchmarkResult
{
  std::chrono::milliseconds init_time;
  std::chrono::milliseconds theme_time;
  std::chrono::milliseconds editor_creation_time;
  std::chrono::milliseconds file_load_time;
  std::chrono::milliseconds first_render_time;
  std::chrono::milliseconds syntax_highlight_time;
  std::chrono::milliseconds total_time;

  void print(std::ostream &os) const
  {
    os << "=== Benchmark Results ===" << std::endl;
    os << "Init (ncurses):        " << init_time.count() << "ms" << std::endl;
    os << "Theme load:            " << theme_time.count() << "ms" << std::endl;
    os << "Editor creation:       " << editor_creation_time.count() << "ms"
       << std::endl;
    os << "File load:             " << file_load_time.count() << "ms"
       << std::endl;
    os << "Syntax highlighting:   " << syntax_highlight_time.count() << "ms"
       << std::endl;
    os << "First render:          " << first_render_time.count() << "ms"
       << std::endl;
    os << "------------------------" << std::endl;
    os << "TOTAL (user-perceived): " << total_time.count() << "ms" << std::endl;
  }
};

BenchmarkResult runStartupInteractiveBenchmark(const std::string &filename,
                                               bool enable_syntax_highlighting)
{

  BenchmarkResult result{};
  auto start = std::chrono::high_resolution_clock::now();

  // Phase 1: Initialize ncurses
  disableXonXoff();
  if (!initializeNcurses())
  {
    throw std::runtime_error("ncurses init failed");
  }
  if (!initializeThemes())
  {
    endwin();
    throw std::runtime_error("theme init failed");
  }

  auto after_init = std::chrono::high_resolution_clock::now();
  result.init_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(after_init - start);

  // Phase 2: Load theme
  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    g_style_manager.load_theme_from_file(theme_file);
  }

  auto after_theme = std::chrono::high_resolution_clock::now();
  result.theme_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      after_theme - after_init);

  // Phase 3: Create syntax highlighter (if enabled)
  SyntaxHighlighter syntaxHighlighter;
  SyntaxHighlighter *highlighterPtr = nullptr;

  if (enable_syntax_highlighting)
  {
    std::string syntax_dir = ConfigManager::getSyntaxRulesDir();
    if (syntaxHighlighter.initialize(syntax_dir))
    {
      highlighterPtr = &syntaxHighlighter;
    }
  }

  auto after_highlighter_init = std::chrono::high_resolution_clock::now();

  // Phase 4: Create editor and load file
  Editor editor(highlighterPtr);

  auto after_editor_creation = std::chrono::high_resolution_clock::now();
  result.editor_creation_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          after_editor_creation - after_theme);

  if (!editor.loadFile(filename))
  {
    endwin();
    throw std::runtime_error("Failed to load file");
  }

  auto after_file_load = std::chrono::high_resolution_clock::now();
  result.file_load_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      after_file_load - after_editor_creation);

  // Phase 5: Initialize syntax highlighting for viewport
  if (highlighterPtr)
  {
    editor.initializeViewportHighlighting();
  }

  auto after_syntax = std::chrono::high_resolution_clock::now();
  result.syntax_highlight_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(after_syntax -
                                                            after_file_load);

  // Phase 6: Render first display
  editor.setMode(EditorMode::INSERT);
  editor.display();
  refresh();

  auto after_render = std::chrono::high_resolution_clock::now();
  result.first_render_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(after_render -
                                                            after_syntax);

  // This is the "interactive ready" point - user can now type
  result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      after_render - start);

  // Cleanup
  endwin();

  return result;
}

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    std::cerr << "Usage: " << argv[0] << " <filename> [options]" << std::endl;
    std::cerr << "\nBenchmark options:" << std::endl;
    std::cerr << "  --bench-startup          Benchmark startup to interactive"
              << std::endl;
    std::cerr
        << "  --bench-startup-nosyntax Same but without syntax highlighting"
        << std::endl;
    std::cerr << "  --bench-file-only        Benchmark only file loading"
              << std::endl;
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

  //   Bechmark
  bool bench_startup = std::any_of(args.begin(), args.end(), [](const auto &arg)
                                   { return arg == "--bench-startup"; });
  bool bench_startup_nosyntax =
      std::any_of(args.begin(), args.end(), [](const auto &arg)
                  { return arg == "--bench-startup-nosyntax"; });

  std::string filename = std::filesystem::absolute(argv[1]).string();

  // Initialize config
  if (!ConfigManager::ensureConfigStructure())
  {
    std::cerr << "Warning: Failed to ensure config structure" << std::endl;
  }
  ConfigManager::copyProjectFilesToConfig();
  ConfigManager::loadConfig();

  if (bench_startup || bench_startup_nosyntax)
  {
    try
    {
      BenchmarkResult result =
          runStartupInteractiveBenchmark(filename, !bench_startup_nosyntax);

      result.print(std::cerr);
      return 0;
    }
    catch (const std::exception &e)
    {
      std::cerr << "Benchmark failed: " << e.what() << std::endl;
      return 1;
    }
  }
  // BENCHMARK PATH: Load file and quit immediately
  if (quit_immediately)
  {
    auto start = std::chrono::high_resolution_clock::now();

    // Initialize ncurses (users see this cost)
    disableXonXoff();
    if (!initializeNcurses())
      return 1;
    if (!initializeThemes())
      return 1;

    auto after_init = std::chrono::high_resolution_clock::now();

    // Load theme (users see this)
    std::string active_theme = ConfigManager::getActiveTheme();
    std::string theme_file = ConfigManager::getThemeFile(active_theme);
    if (!theme_file.empty())
    {
      g_style_manager.load_theme_from_file(theme_file);
    }

    auto after_theme = std::chrono::high_resolution_clock::now();

    // Render once (users see this)
    // editor.display();
    refresh();

    auto after_render = std::chrono::high_resolution_clock::now();

    // Cleanup
    endwin();

    // Print timings to stderr (won't interfere with hyperfine)
    auto ms = [](auto s, auto e)
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(e - s)
          .count();
    };

    std::cerr << "Init: " << ms(start, after_init) << "ms, "
              << "Theme: " << ms(after_init, after_theme) << "ms, "
              << "Render: " << ms(after_theme, after_render) << "ms, "
              << "Total: " << ms(start, after_render) << "ms" << std::endl;

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
#ifndef _WIN32
      refresh();
#endif
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
#ifdef _WIN32
// On Windows, display() already calls refresh()
// Don't call it again here
#else
      refresh();
#endif
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
#ifdef _WIN32
  timeout(100);                   // Longer timeout on Windows
  PDC_set_blink(FALSE);           // Disable blinking (PDCurses specific)
  PDC_return_key_modifiers(TRUE); // Better key handling
#else
  timeout(50);
#endif

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
#ifndef _WIN32
  printf("\033[?1003h");
  fflush(stdout);
#endif
}

void cleanupMouse()
{
#ifndef _WIN32
  printf("\033[?1003l");
  fflush(stdout);
#endif
}