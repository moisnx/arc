// src/core/editor_loop.h
#ifndef EDITOR_LOOP_H
#define EDITOR_LOOP_H

class Editor;
class InputHandler;

class EditorLoop
{
public:
  enum class ExitReason
  {
    QUIT,
    ERROR
  };

  static ExitReason run(Editor &editor, InputHandler &inputHandler);
};

#endif // EDITOR_LOOP_H