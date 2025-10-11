// src/modes/editor_mode.h
#ifndef EDITOR_MODE_H
#define EDITOR_MODE_H

#include <string>

struct CommandLineArgs;

class EditorMode
{
public:
  static int run(const std::string &filename, const CommandLineArgs &args);
};

#endif // EDITOR_MODE_H