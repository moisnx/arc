// src/core/command_line.h
#ifndef COMMAND_LINE_H
#define COMMAND_LINE_H

#include <string>

struct CommandLineArgs
{
  std::string filename;
  bool force_no_highlighting = false;
  bool quit_immediately = false;
  bool bench_startup = false;
  bool bench_startup_nosyntax = false;
  bool is_directory = false;

  static CommandLineArgs parse(int argc, char *argv[]);
};

#endif // COMMAND_LINE_H