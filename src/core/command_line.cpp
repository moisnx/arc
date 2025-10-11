#include "command_line.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

CommandLineArgs CommandLineArgs::parse(int argc, char *argv[])
{
  CommandLineArgs args;

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
    throw std::runtime_error("Insufficient arguments");
  }

  std::vector<std::string> arg_vec(argv, argv + argc);
  args.filename = std::filesystem::absolute(argv[1]).string();
  args.is_directory = std::filesystem::is_directory(args.filename);

  args.force_no_highlighting =
      std::any_of(arg_vec.begin(), arg_vec.end(),
                  [](const auto &arg) { return arg == "--none"; });

  args.quit_immediately =
      std::any_of(arg_vec.begin(), arg_vec.end(),
                  [](const auto &arg) { return arg == "--quit"; });

  args.bench_startup =
      std::any_of(arg_vec.begin(), arg_vec.end(),
                  [](const auto &arg) { return arg == "--bench-startup"; });

  args.bench_startup_nosyntax =
      std::any_of(arg_vec.begin(), arg_vec.end(), [](const auto &arg)
                  { return arg == "--bench-startup-nosyntax"; });

  return args;
}