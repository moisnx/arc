#include "input_handler.h"
#include <fstream>
#include <iostream>

// PDCursesMod key code compatibility
#define CTRL(x) ((x) & 0x1f)
#define KEY_TAB 9
#define KEY_ENTER 10
#define KEY_ESC 27
#define KEY_BACKSPACE_ALT 127

#ifdef _WIN32
#define GETMOUSE_FUNC nc_getmouse

// PDCursesMod VT-mode extended key codes
#define PDC_KEY_UP 60418    // 0xec02
#define PDC_KEY_DOWN 60419  // 0xec03
#define PDC_KEY_RIGHT 60420 // 0xec04
#define PDC_KEY_LEFT 60421  // 0xec05

#else
#define GETMOUSE_FUNC getmouse
#endif

InputHandler::InputHandler(Editor &editor)
    : editor_(editor), mouse_enabled_(true), global_shortcuts_enabled_(true)
{
}

InputHandler::KeyResult InputHandler::handleKey(int key)
{
  static std::ofstream debug("keylog.txt", std::ios::app);
  debug << "Key: " << key << " (hex: 0x" << std::hex << key << std::dec << ")"
        << std::endl;

  // Handle special events first
  if (key == KEY_MOUSE && mouse_enabled_)
  {
    return handleMouseEvent();
  }

  if (key == KEY_RESIZE)
  {
    return handleResizeEvent();
  }

  // Handle global shortcuts (work in any mode)
  if (global_shortcuts_enabled_ && isGlobalShortcut(key))
  {
    return handleGlobalShortcut(key);
  }

  // Handle mode-specific keys
  EditorMode current_mode = editor_.getMode();

  switch (current_mode)
  {
  case EditorMode::NORMAL:
    return handleNormalMode(key);
  case EditorMode::INSERT:
    return handleInsertMode(key);
  case EditorMode::VISUAL:
    return handleVisualMode(key);
  default:
    return KeyResult::NOT_HANDLED;
  }
}

bool InputHandler::isGlobalShortcut(int key) const
{
  switch (key)
  {
  case CTRL('s'): // Save
  case CTRL('z'): // Undo
  case CTRL('y'): // Redo
  case CTRL('q'): // Quit (future)
    return true;
  default:
    return false;
  }
}

InputHandler::KeyResult InputHandler::handleGlobalShortcut(int key)
{
  switch (key)
  {
  case CTRL('s'):
    editor_.saveFile();
    return KeyResult::REDRAW;

  case CTRL('z'):
    editor_.undo();
    return KeyResult::REDRAW;

  case CTRL('y'):
    editor_.redo();
    return KeyResult::REDRAW;

  case CTRL('q'):
    // TODO: Check for unsaved changes
    return KeyResult::QUIT;

  default:
    return KeyResult::NOT_HANDLED;
  }
}

InputHandler::KeyResult InputHandler::handleNormalMode(int key)
{
  // Mode switching keys
  switch (key)
  {
  case 'i':
    editor_.setMode(EditorMode::INSERT);
    return KeyResult::REDRAW;

  case 'I':
    editor_.setMode(EditorMode::INSERT);
    editor_.moveCursorToLineStart();
    return KeyResult::REDRAW;

  case 'a':
    editor_.setMode(EditorMode::INSERT);
    editor_.moveCursorRight();
    return KeyResult::REDRAW;

  case 'A':
    editor_.setMode(EditorMode::INSERT);
    editor_.moveCursorToLineEnd();
    return KeyResult::REDRAW;

  case 'o':
    editor_.moveCursorToLineEnd();
    editor_.insertNewline();
    editor_.setMode(EditorMode::INSERT);
    return KeyResult::REDRAW;

  case 'O':
    editor_.moveCursorToLineStart();
    editor_.insertNewline();
    editor_.moveCursorUp();
    editor_.setMode(EditorMode::INSERT);
    return KeyResult::REDRAW;

  case 'v':
  case 'V':
    editor_.setMode(EditorMode::VISUAL);
    return KeyResult::REDRAW;

  // Editing commands
  case 'x':
    editor_.deleteChar();
    return KeyResult::REDRAW;

  case 'd':
    editor_.deleteLine();
    return KeyResult::REDRAW;
  }

  // Movement keys (allow character keys like 'h', 'j', 'k', 'l')
  if (handleMovementKey(key, false, true))
  {
    return KeyResult::REDRAW;
  }

  return KeyResult::NOT_HANDLED;
}

InputHandler::KeyResult InputHandler::handleInsertMode(int key)
{
  // Mode switching
  if (key == KEY_ESC)
  {
    editor_.setMode(EditorMode::NORMAL);
    editor_.moveCursorLeft(); // Vim-like behavior
    return KeyResult::REDRAW;
  }

  // Text editing
  switch (key)
  {
  case KEY_BACKSPACE:
  case KEY_BACKSPACE_ALT:
  case 8: // Ctrl+H
    editor_.backspace();
    return KeyResult::REDRAW;

  case KEY_DC:
    editor_.deleteChar();
    return KeyResult::REDRAW;

  case KEY_ENTER:
  case '\r':
    editor_.insertNewline();
    return KeyResult::REDRAW;

  case KEY_TAB:
    // Insert 4 spaces instead of tab
    for (int i = 0; i < 4; i++)
    {
      editor_.insertChar(' ');
    }
    return KeyResult::REDRAW;
  }

  // Movement keys work in insert mode (ONLY non-character keys like arrows)
  if (handleMovementKey(
          key, false, false)) // <-- MODIFIED: Pass 'false' to disable char keys
  {
    return KeyResult::REDRAW;
  }

  // Insert printable characters
  if (isPrintableChar(key))
  {
    editor_.insertChar(static_cast<char>(key));
    return KeyResult::REDRAW;
  }

  return KeyResult::NOT_HANDLED;
}

InputHandler::KeyResult InputHandler::handleVisualMode(int key)
{
  // Exit visual mode
  switch (key)
  {
  case KEY_ESC:
  case 'v':
    editor_.setMode(EditorMode::NORMAL);
    editor_.clearSelection();
    return KeyResult::REDRAW;

  case 'x':
  case KEY_DC:
    editor_.deleteSelection();
    editor_.setMode(EditorMode::NORMAL);
    return KeyResult::REDRAW;

  case 'i':
    editor_.deleteSelection();
    editor_.setMode(EditorMode::INSERT);
    return KeyResult::REDRAW;
  }

  // Movement keys extend selection in visual mode (allow character keys)
  if (handleMovementKey(key, true, true))
  {
    return KeyResult::REDRAW;
  }

  return KeyResult::NOT_HANDLED;
}

// MODIFIED: Added 'allow_char_keys' parameter with default 'true'
bool InputHandler::handleMovementKey(int key, bool extend_selection,
                                     bool allow_char_keys)
{
  switch (key)
  {
  // Basic movement
  case 'h':
    if (!allow_char_keys)
      return false;
  case KEY_LEFT:
#ifdef _WIN32
  case PDC_KEY_LEFT: // 60421
#endif
    editor_.moveCursorLeft();
    return true;

  case 'j':
    if (!allow_char_keys)
      return false;
  case KEY_DOWN:
#ifdef _WIN32
  case PDC_KEY_DOWN: // 60419
#endif
    editor_.moveCursorDown();
    return true;

  case 'k':
    if (!allow_char_keys)
      return false;
  case KEY_UP:
#ifdef _WIN32
  case PDC_KEY_UP: // 60418
#endif
    editor_.moveCursorUp();
    return true;

  case 'l':
    if (!allow_char_keys)
      return false;
  case KEY_RIGHT:
#ifdef _WIN32
  case PDC_KEY_RIGHT: // 60420
#endif
    editor_.moveCursorRight();
    return true;

  // Line movement
  case '0':
    if (!allow_char_keys)
      return false;
    editor_.moveCursorToLineStart();
    return true;

  case '$':
    if (!allow_char_keys)
      return false;
    editor_.moveCursorToLineEnd();
    return true;

  // Page movement (always allowed)
  case KEY_PPAGE:
    editor_.pageUp();
    return true;

  case KEY_NPAGE:
    editor_.pageDown();
    return true;

  // Extended keys (always allowed)
  case KEY_HOME:
    editor_.moveCursorToLineStart();
    return true;

  case KEY_END:
    editor_.moveCursorToLineEnd();
    return true;

  default:
    return false;
  }
}

InputHandler::KeyResult InputHandler::handleMouseEvent()
{
  MEVENT event;
  if (GETMOUSE_FUNC(&event) == OK)
  {
    editor_.handleMouse(event);
    return KeyResult::REDRAW;
  }
  return KeyResult::NOT_HANDLED;
}

InputHandler::KeyResult InputHandler::handleResizeEvent()
{
  editor_.handleResize();
  flushinp(); // Clear input buffer
  return KeyResult::REDRAW;
}

bool InputHandler::isMovementKey(int key) const
{
  switch (key)
  {
  case 'h':
  case 'j':
  case 'k':
  case 'l':
  case '0':
  case '$':
  case KEY_LEFT:
  case KEY_RIGHT:
  case KEY_UP:
  case KEY_DOWN:
  case KEY_HOME:
  case KEY_END:
  case KEY_PPAGE:
  case KEY_NPAGE:
    return true;
  default:
    return false;
  }
}

bool InputHandler::isPrintableChar(int key) const
{
  return key >= 32 && key <= 126;
}