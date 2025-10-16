#include "src/benchmark/benchmark.h"
#include "src/core/application.h"
#include "src/core/args_parser.h"
#include "src/core/config_manager.h"
// #include "src/core/logger.h"
#include "src/core/signals/signal_handler.h"
#include "src/features/query_manager.h"
#include "src/modes/browser_mode.h"
#include "src/modes/editor_mode.h"
#include <iostream>
#include <locale.h>

int main(int argc, char *argv[])
{
  install_signal_handlers();

  // std::cerr.flush();
  try
  {
    ProgramArgs args = ProgramArgs::parse(argc, argv);
    setlocale(LC_ALL, "");

    if (!ConfigManager::ensureConfigStructure())
    {
      std::cerr << "Warning: Failed to ensure config structure" << std::endl;
    }
    ConfigManager::copyProjectFilesToConfig();
    ConfigManager::loadConfig();

    if (!args.bench_startup && !args.bench_startup_nosyntax)
    {
      QueryManager::warmupCache(
          {"c", "cpp", "python", "rust", "go", "javascript"});
    }

    if (args.bench_startup || args.bench_startup_nosyntax)
    {
      return Benchmark::runStartup(args.filename, !args.bench_startup_nosyntax);
    }

    if (args.quit_immediately)
    {
      return Benchmark::runQuickStartup();
    }

    if (args.is_directory)
    {
      return BrowserMode::run(args.filename);
    }

    return EditorMode::run(args.filename, args);
  }
  catch (const std::exception &e)
  {
    Application::cleanup();
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  catch (...)
  {
    Application::cleanup();
    std::cerr << "An unknown fatal error occurred." << std::endl;
    return 1;
  }
}