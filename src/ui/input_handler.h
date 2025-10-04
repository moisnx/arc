#pragma once

#include "src/core/editor.h"

#ifdef _WIN32
#include <curses.h>
#else
#include <ncurses.h>
#endif

// Forward declarations
class Editor;

/**
 * Centralized input handling for the editor
 * Manages key bindings, mode-specific behavior, and global shortcuts
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

  // Check if key is a global shortcut (works in any mode)
  bool isGlobalShortcut(int key) const;

  // Enable/disable specific key categories
  void setMouseEnabled(bool enabled) { mouse_enabled_ = enabled; }
  void setGlobalShortcutsEnabled(bool enabled)
  {
    global_shortcuts_enabled_ = enabled;
  }

private:
  Editor &editor_;
  bool mouse_enabled_;
  bool global_shortcuts_enabled_;

  // Mode-specific handlers
  KeyResult handleNormalMode(int key);
  KeyResult handleInsertMode(int key);
  KeyResult handleVisualMode(int key);

  // Special input types
  KeyResult handleMouseEvent();
  KeyResult handleResizeEvent();

  // Global shortcuts that work in any mode
  KeyResult handleGlobalShortcut(int key);

  // Movement key processing (shared between modes)
  // New parameter 'allow_char_keys' controls if 'h,j,k,l,0,$' etc. are treated
  // as movement.
  bool handleMovementKey(int key, bool extend_selection = false,
                         bool allow_char_keys = true); // <-- MODIFIED

  // Utility functions
  bool isMovementKey(int key) const;
  bool isPrintableChar(int key) const;
};