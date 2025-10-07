#include "input_handler.h"
#include <fstream>
#include <iostream>
#include <optional>

// PDCursesMod key code compatibility
#define CTRL(x) ((x) & 0x1f)
#define KEY_TAB 9
#define KEY_ENTER 10
#define KEY_ESC 27
#define KEY_BACKSPACE_ALT 127

#ifdef _WIN32
#define GETMOUSE_FUNC nc_getmouse

// PDCursesMod VT-mode extended key codes
#define PDC_KEY_UP 60418
#define PDC_KEY_DOWN 60419
#define PDC_KEY_RIGHT 60420
#define PDC_KEY_LEFT 60421

#else
#define GETMOUSE_FUNC getmouse
#endif

InputHandler::InputHandler(Editor &editor)
    : editor_(editor), mouse_enabled_(true)
{
}

InputHandler::KeyResult InputHandler::handleKey(int key)
{
  // Handle special events first
  if (key == KEY_MOUSE && mouse_enabled_)
  {
    return handleMouseEvent();
  }

  if (key == KEY_RESIZE)
  {
    return handleResizeEvent();
  }

  // Global shortcuts (Ctrl+S, Ctrl+Z, etc.)
  if (auto result = handleGlobalShortcut(key))
  {
    return *result; // Return whatever KeyResult it gave us
  }

  // Movement keys (handles both normal and shift+movement for selection)
  if (handleMovementKey(key, false))
  {
    return KeyResult::REDRAW;
  }

  // Editing keys (backspace, delete, enter, tab)
  if (handleEditingKey(key))
  {
    return KeyResult::REDRAW;
  }

  // Insert printable characters
  if (isPrintableChar(key))
  {
    // Delete selection first if one exists
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }

    editor_.insertChar(static_cast<char>(key));
    return KeyResult::REDRAW;
  }

  return KeyResult::NOT_HANDLED;
}

std::optional<InputHandler::KeyResult>
InputHandler::handleGlobalShortcut(int key)
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
    // // TODO: Check for unsaved changes and prompt
    // if (editor_.hasUnsavedChanges())
    // {
    //   // For now, just quit - add confirmation dialog later
    //   exit(0);
    // }
    // exit(0);
    // return true;
    return KeyResult::QUIT;

  case CTRL('c'):
    editor_.copySelection();
    return KeyResult::REDRAW;

  case CTRL('x'):
    editor_.cutSelection();
    return KeyResult::REDRAW;

  case CTRL('v'):
    editor_.pasteFromClipboard();
    return KeyResult::REDRAW;

  case CTRL('a'):
    editor_.selectAll();
    return KeyResult::REDRAW;

  default:
    return std::nullopt; // No shortcut handled
  }
}

bool InputHandler::handleMovementKey(int key, bool shift_held)
{
  // Detect if shift is being held for this key
  bool extending_selection = false;

#ifdef _WIN32
  if (PDC_get_key_modifiers() & PDC_KEY_MODIFIER_SHIFT)
  {
    extending_selection = true;
  }
#endif

  // Check for shift-modified arrow keys (Unix/ncurses)
  switch (key)
  {
  case KEY_SLEFT:
  case KEY_SRIGHT:
  case KEY_SR:
  case KEY_SF:
#ifdef _WIN32
  case KEY_SUP:
  case KEY_SDOWN:
#endif
    extending_selection = true;
    break;
  }

  // IMPROVED: Start selection BEFORE movement if shift held
  if (extending_selection)
  {
    editor_.startSelectionIfNeeded();
  }
  else
  {
    // Clear selection BEFORE movement if no shift
    editor_.clearSelection();
  }

  // Handle the actual movement
  bool moved = false;

  switch (key)
  {
  case KEY_LEFT:
  case KEY_SLEFT:
#ifdef _WIN32
  case PDC_KEY_LEFT:
#endif
    editor_.moveCursorLeft();
    moved = true;
    break;

  case KEY_RIGHT:
  case KEY_SRIGHT:
#ifdef _WIN32
  case PDC_KEY_RIGHT:
#endif
    editor_.moveCursorRight();
    moved = true;
    break;

  case KEY_UP:
  case KEY_SR:
#ifdef _WIN32
  case PDC_KEY_UP:
  case KEY_SUP:
#endif
    editor_.moveCursorUp();
    moved = true;
    break;

  case KEY_DOWN:
  case KEY_SF:
#ifdef _WIN32
  case PDC_KEY_DOWN:
  case KEY_SDOWN:
#endif
    editor_.moveCursorDown();
    moved = true;
    break;

  case KEY_HOME:
    editor_.moveCursorToLineStart();
    moved = true;
    break;

  case KEY_END:
    editor_.moveCursorToLineEnd();
    moved = true;
    break;

  case KEY_PPAGE:
    editor_.pageUp();
    moved = true;
    break;

  case KEY_NPAGE:
    editor_.pageDown();
    moved = true;
    break;

  default:
    return false;
  }

  // IMPROVED: Always update selection end if extending
  if (moved && extending_selection)
  {
    editor_.updateSelectionEnd();
  }

  return moved;
}
bool InputHandler::handleEditingKey(int key)
{
  switch (key)
  {
  case KEY_BACKSPACE:
  case KEY_BACKSPACE_ALT:
  case 8: // Ctrl+H
    // If there's a selection, delete it instead of single backspace
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }
    else
    {
      editor_.backspace();
    }
    return true;

  case KEY_DC: // Delete key
    // If there's a selection, delete it
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }
    else
    {
      editor_.deleteChar();
    }
    return true;

  case KEY_ENTER:
  case '\r':
    // case '\n':
    // Delete selection first if one exists
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }
    editor_.insertNewline();
    return true;

  case KEY_TAB:
    // Delete selection first if one exists
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }
    // Insert 4 spaces instead of tab
    for (int i = 0; i < 4; i++)
    {
      editor_.insertChar(' ');
    }
    return true;

  case KEY_ESC:
    // Clear selection on escape
    editor_.clearSelection();
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
  flushinp();
  return KeyResult::REDRAW;
}

bool InputHandler::isPrintableChar(int key) const
{
  return key >= 32 && key <= 126;
}