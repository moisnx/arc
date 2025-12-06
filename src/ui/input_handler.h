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
  struct MouseState {
    bool isDragging = false;
    bool wasClick = true;
    int clickStartX = -1;
    int clickStartY = -1;
    static constexpr int DRAG_THRESHOLD = 2; // pixels before it's considered a drag
  };

  explicit InputHandler(Editor &editor);

  // Main input processing
  KeyResult handleKey(int key);

  // Enable/disable specific key categories
  void setMouseEnabled(bool enabled) { mouse_enabled_ = enabled; }
  void displayStatusMessage(const std::string &message);
  void showHelpScreen();

private:
  Editor &editor_;
  bool mouse_enabled_;
  MouseState mouseState_;


  // Sequence
  int pending_sequence_key_ = 0;
  std::optional<InputHandler::KeyResult> handleKeySequence(int first_key,
                                                           int second_key);

  // Special input types
  KeyResult handleMouseEvent();
  KeyResult handleResizeEvent();

  // Movement and editing
  bool handleMovementKey(int key);
  bool handleEditingKey(int key);
  std::optional<KeyResult> handleGlobalShortcut(int key);

  // Utility functions
  bool isPrintableChar(int key) const;
};
