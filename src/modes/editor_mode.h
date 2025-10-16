// src/modes/editor_mode.h
#ifndef EDITOR_MODE_H
#define EDITOR_MODE_H

#include <string>

struct ProgramArgs;

class EditorMode
{
public:
  static int run(const std::string &filename, const ProgramArgs &args);
};

#endif // EDITOR_MODE_H