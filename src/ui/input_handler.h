#pragma once

#include "src/core/editor.h"
#include <optional>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

class Editor;

/**
 * Simplified input handler for modeless editing
 * Handles standard editor keybindings without vim-style modes
 */
class InputHandler
{
public:
  enum class KeyResult
  {
    HANDLED,     // Key was processed successfully
    NOT_HANDLED, // Key was not recognized
    QUIT,        // User requested quit
    REDRAW       // Screen needs redraw
  };

  explicit InputHandler(Editor &editor);

  // Main input processing
  KeyResult handleKey(int key);

  // Enable/disable specific key categories
  void setMouseEnabled(bool enabled) { mouse_enabled_ = enabled; }

private:
  Editor &editor_;
  bool mouse_enabled_;

  // Special input types
  KeyResult handleMouseEvent();
  KeyResult handleResizeEvent();

  // Movement and editing
  bool handleMovementKey(int key, bool shift_held);
  bool handleEditingKey(int key);
  std::optional<KeyResult> handleGlobalShortcut(int key);

  // Utility functions
  bool isPrintableChar(int key) const;
};