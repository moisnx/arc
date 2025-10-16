// src/core/args_parser.h
#ifndef ARGS_PARSER_H
#define ARGS_PARSER_H

#include <string>

struct ProgramArgs
{
  std::string filename;
  bool force_no_highlighting = false;
  bool quit_immediately = false;
  bool bench_startup = false;
  bool bench_startup_nosyntax = false;
  bool is_directory = false;

  static ProgramArgs parse(int argc, char *argv[]);
};

#endif // ARGS_PARSER_H