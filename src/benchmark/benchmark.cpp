#include "benchmark.h"
#include "src/core/application.h"
#include "src/core/config_manager.h"
#include "src/core/editor.h"
#include "src/features/syntax_highlighter.h"
#include "src/ui/style_manager.h"
#include <iostream>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

void BenchmarkResult::print(std::ostream &os) const
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

int Benchmark::runStartup(const std::string &filename,
                          bool enable_syntax_highlighting)
{
  try
  {
    BenchmarkResult result =
        runStartupInteractive(filename, enable_syntax_highlighting);
    result.print(std::cerr);
    return 0;
  }
  catch (const std::exception &e)
  {
    std::cerr << "Benchmark failed: " << e.what() << std::endl;
    return 1;
  }
}

int Benchmark::runQuickStartup()
{
  auto start = std::chrono::high_resolution_clock::now();

  if (!Application::initialize())
  {
    return 1;
  }

  auto after_init = std::chrono::high_resolution_clock::now();

  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    g_style_manager.load_theme_from_file(theme_file);
  }

  auto after_theme = std::chrono::high_resolution_clock::now();

  wnoutrefresh(stdscr);
  doupdate();
  auto after_render = std::chrono::high_resolution_clock::now();

  Application::cleanup();

  auto ms = [](auto s, auto e)
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count();
  };

  std::cerr << "Init: " << ms(start, after_init) << "ms, "
            << "Theme: " << ms(after_init, after_theme) << "ms, "
            << "Render: " << ms(after_theme, after_render) << "ms, "
            << "Total: " << ms(start, after_render) << "ms" << std::endl;

  return 0;
}

BenchmarkResult
Benchmark::runStartupInteractive(const std::string &filename,
                                 bool enable_syntax_highlighting)
{
  BenchmarkResult result{};
  auto start = std::chrono::high_resolution_clock::now();

  if (!Application::initialize())
  {
    throw std::runtime_error("Application initialization failed");
  }

  auto after_init = std::chrono::high_resolution_clock::now();
  result.init_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(after_init - start);

  std::string active_theme = ConfigManager::getActiveTheme();
  std::string theme_file = ConfigManager::getThemeFile(active_theme);
  if (!theme_file.empty())
  {
    g_style_manager.load_theme_from_file(theme_file);
  }

  auto after_theme = std::chrono::high_resolution_clock::now();
  result.theme_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      after_theme - after_init);

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

  // if (highlighterPtr)
  // {
  //   editor.initializeViewportHighlighting();
  // }

  auto after_syntax = std::chrono::high_resolution_clock::now();
  result.syntax_highlight_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(after_syntax -
                                                            after_file_load);

  editor.setCursorMode();
  editor.display();
  wnoutrefresh(stdscr);

  auto after_render = std::chrono::high_resolution_clock::now();
  result.first_render_time =
      std::chrono::duration_cast<std::chrono::milliseconds>(after_render -
                                                            after_syntax);

  result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
      after_render - start);

  endwin();

  return result;
}