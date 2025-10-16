#include "args_parser.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

namespace
{
bool has_flag(const std::vector<std::string> &args, const std::string &flag)
{
  return std::find(args.begin(), args.end(), flag) != args.end();
}

void print_usage(const char *program_name)
{
  std::cerr
      << "Usage: " << program_name << " <filename> [options]\n"
      << "\nOptions:\n"
      << "  --none                   Disable syntax highlighting\n"
      << "  --quit                   Quit immediately after loading\n"
      << "\nBenchmark options:\n"
      << "  --bench-startup          Benchmark startup to interactive\n"
      << "  --bench-startup-nosyntax Benchmark without syntax highlighting\n"
      << "  --bench-file-only        Benchmark only file loading\n";
}
} // namespace

ProgramArgs ProgramArgs::parse(int argc, char *argv[])
{
  if (argc < 2)
  {
    print_usage(argv[0]);
    throw std::runtime_error("Insufficient arguments");
  }

  ProgramArgs args;
  std::vector<std::string> arg_vec(argv + 1, argv + argc);

  args.filename = std::filesystem::absolute(argv[1]).string();
  args.is_directory = std::filesystem::is_directory(args.filename);

  args.force_no_highlighting = has_flag(arg_vec, "--none");
  args.quit_immediately = has_flag(arg_vec, "--quit");
  args.bench_startup = has_flag(arg_vec, "--bench-startup");
  args.bench_startup_nosyntax = has_flag(arg_vec, "--bench-startup-nosyntax");

  return args;
}