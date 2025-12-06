#include "input_handler.h"
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

// PDCursesMod key code compatibility
#define CTRL(x) ((x) & 0x1f)
#define KEY_TAB 9
// #define KEY_ENTER 10
#define KEY_ESC 27
#define KEY_BACKSPACE_ALT 127

#ifdef _WIN32
#define GETMOUSE_FUNC nc_getmouse
#define PDC_KEY_UP 60418
#define PDC_KEY_DOWN 60419
#define PDC_KEY_RIGHT 60420
#define PDC_KEY_LEFT 60421
#else
#define GETMOUSE_FUNC getmouse
#endif

InputHandler::InputHandler(Editor &editor)
    : editor_(editor), mouse_enabled_(true), pending_sequence_key_(0)
{
}

// ----------------------------------------------------------------------
// Core Key Handler
// ----------------------------------------------------------------------

InputHandler::KeyResult InputHandler::handleKey(int key)
{
  if (editor_.getIsBinary() && key != CTRL('q') && key != KEY_RESIZE)
    return KeyResult::NOT_HANDLED;

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
    return *result;
  }

  // Movement keys (handles both normal and shift+movement for selection)
  if (handleMovementKey(key))
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

// ----------------------------------------------------------------------
// Global Shortcuts - SIMPLIFIED WITH DIRECT KEYS
// ----------------------------------------------------------------------

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
    if (editor_.hasUnsavedChanges())
    {
      // Show modal and get user decision
      UnsavedModalResult result = editor_.displayUnsavedChangesModal();

      switch (result)
      {
      case UnsavedModalResult::SAVE_AND_QUIT:
        // User chose to save and quit
        if (editor_.saveFile())
        {
          // Save successful, proceed with quit
          return KeyResult::QUIT;
        }
        else
        {
          // Save failed - show error and cancel quit
          displayStatusMessage("Error: Could not save file!");
          return KeyResult::REDRAW;
        }

      case UnsavedModalResult::QUIT_WITHOUT_SAVE:
        // User confirmed quit without saving
        return KeyResult::QUIT;

      case UnsavedModalResult::CANCEL:
        // User cancelled - redraw editor and continue editing
        return KeyResult::REDRAW;
      }
    }

    return KeyResult::QUIT;

  case CTRL('c'):
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.copySelection();
    }
    return KeyResult::REDRAW;

  case CTRL('x'):
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.cutSelection();
    }
    return KeyResult::REDRAW;

  case CTRL('v'):
  case CTRL('p'):
    editor_.pasteFromClipboard();
    return KeyResult::REDRAW;

  case CTRL('a'):
    editor_.selectAll();
    return KeyResult::REDRAW;

  // ========== MARKDOWN CONTROLS (SIMPLE DIRECT KEYS) ==========

  // CTRL+L: Toggle markdown rendering ON/OFF
  case CTRL('l'):
    if (editor_.getSyntaxHighlighter() &&
        editor_.getSyntaxHighlighter()->getCurrentLanguage() == "markdown")
    {
      editor_.toggleMarkdownRendering();
      std::string status = editor_.isMarkdownRenderingEnabled()
                               ? "Markdown rendering: ON"
                               : "Markdown rendering: OFF";
      displayStatusMessage(status);
      return KeyResult::REDRAW;
    }
    displayStatusMessage("Not a markdown file");
    return KeyResult::REDRAW;

  // CTRL+]: Cycle code block style
  case CTRL(']'):
    if (editor_.getSyntaxHighlighter() &&
        editor_.getSyntaxHighlighter()->getCurrentLanguage() == "markdown" &&
        editor_.isMarkdownRenderingEnabled())
    {
      auto *renderer = editor_.getMarkdownRenderer();
      if (renderer)
      {
        renderer->cycleCodeBlockStyle();
        auto config = renderer->getConfig();
        std::string style_name =
            MarkdownRenderConfig::getStyleName(config.block_style);
        displayStatusMessage("Code block: " + style_name);
        return KeyResult::REDRAW;
      }
    }
    displayStatusMessage("Enable markdown rendering first (Ctrl+L)");
    return KeyResult::REDRAW;

  // CTRL+[: Cycle heading style
  case CTRL('['):
    if (editor_.getSyntaxHighlighter() &&
        editor_.getSyntaxHighlighter()->getCurrentLanguage() == "markdown" &&
        editor_.isMarkdownRenderingEnabled())
    {
      auto *renderer = editor_.getMarkdownRenderer();
      if (renderer)
      {
        auto config = renderer->getConfig();

        // Cycle through heading styles
        switch (config.heading_style)
        {
        case HeadingStyle::MODERN:
          config.heading_style = HeadingStyle::CLASSIC;
          break;
        case HeadingStyle::CLASSIC:
          config.heading_style = HeadingStyle::MINIMAL;
          break;
        case HeadingStyle::MINIMAL:
          config.heading_style = HeadingStyle::UNDERLINED;
          break;
        case HeadingStyle::UNDERLINED:
          config.heading_style = HeadingStyle::MODERN;
          break;
        }

        renderer->setConfig(config);
        std::string style_name =
            MarkdownRenderConfig::getStyleName(config.heading_style);
        displayStatusMessage("Heading: " + style_name);
        return KeyResult::REDRAW;
      }
    }
    displayStatusMessage("Enable markdown rendering first (Ctrl+L)");
    return KeyResult::REDRAW;

  // CTRL+\: Cycle code indent style
  case CTRL('?'):
    if (editor_.getSyntaxHighlighter() &&
        editor_.getSyntaxHighlighter()->getCurrentLanguage() == "markdown" &&
        editor_.isMarkdownRenderingEnabled())
    {
      auto *renderer = editor_.getMarkdownRenderer();
      if (renderer)
      {
        auto config = renderer->getConfig();

        // Cycle through indent styles
        switch (config.block_indent)
        {
        case CodeBlockIndent::NONE:
          config.block_indent = CodeBlockIndent::MINIMAL;
          break;
        case CodeBlockIndent::MINIMAL:
          config.block_indent = CodeBlockIndent::GUTTER;
          break;
        case CodeBlockIndent::GUTTER:
          config.block_indent = CodeBlockIndent::STANDARD;
          break;
        case CodeBlockIndent::STANDARD:
          config.block_indent = CodeBlockIndent::NONE;
          break;
        }

        renderer->setConfig(config);

        std::string indent_name;
        switch (config.block_indent)
        {
        case CodeBlockIndent::NONE:
          indent_name = "none";
          break;
        case CodeBlockIndent::MINIMAL:
          indent_name = "minimal";
          break;
        case CodeBlockIndent::GUTTER:
          indent_name = "gutter";
          break;
        case CodeBlockIndent::STANDARD:
          indent_name = "standard";
          break;
        }

        displayStatusMessage("Indent: " + indent_name);
        return KeyResult::REDRAW;
      }
    }
    displayStatusMessage("Enable markdown rendering first (Ctrl+L)");
    return KeyResult::REDRAW;

  // F1: Show help screen
  case CTRL('e'):
    showHelpScreen();
    return KeyResult::REDRAW;

  default:
    return std::nullopt;
  }
}

// ----------------------------------------------------------------------
// Editing Keys
// ----------------------------------------------------------------------

bool InputHandler::handleEditingKey(int key)
{
  if (key == '\n' || key == KEY_ENTER)
  {
    // Delete selection first if one exists
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }

    // Call the core editor logic to create a new line
    editor_.insertNewline();
    return true;
  }
  switch (key)
  {
  case KEY_BACKSPACE:
  case KEY_BACKSPACE_ALT:
  case 8:
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
  case '\r': // Handle both Enter and Ctrl+M as newline
    // Delete selection first if one exists
    if (editor_.hasSelection || editor_.isSelecting)
    {
      editor_.deleteSelection();
    }
    editor_.insertNewline();
    return true;

  case KEY_TAB:
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

bool InputHandler::handleMovementKey(int key)
{
  // Detect if Shift is held using multiple methods for cross-platform reliability
  bool extending_selection = false;

#ifdef _WIN32
  if (PDC_get_key_modifiers() & PDC_KEY_MODIFIER_SHIFT)
  {
    extending_selection = true;
  }
#endif

  // Check for shift-modified arrow keys (ncurses keycodes)
  switch (key)
  {
  case KEY_SLEFT:   // Shift+Left
  case KEY_SRIGHT:  // Shift+Right
  case KEY_SR:      // Shift+Up
  case KEY_SF:      // Shift+Down
#ifdef _WIN32
  case KEY_SUP:
  case KEY_SDOWN:
#endif
    extending_selection = true;
    break;
  }

  // Determine base movement key (remove shift modifier)
  int baseKey = key;
  if (extending_selection)
  {
    switch (key)
    {
    case KEY_SLEFT:  baseKey = KEY_LEFT; break;
    case KEY_SRIGHT: baseKey = KEY_RIGHT; break;
    case KEY_SR:     baseKey = KEY_UP; break;
    case KEY_SF:     baseKey = KEY_DOWN; break;
#ifdef _WIN32
    case KEY_SUP:    baseKey = KEY_UP; break;
    case KEY_SDOWN:  baseKey = KEY_DOWN; break;
#endif
    }
  }

  // Check if this is a movement key
  bool is_movement_key = false;
  switch (baseKey)
  {
  case KEY_LEFT:
  case KEY_RIGHT:
  case KEY_UP:
  case KEY_DOWN:
  case KEY_HOME:
  case KEY_END:
  case KEY_PPAGE:
  case KEY_NPAGE:
#ifdef _WIN32
  case PDC_KEY_LEFT:
  case PDC_KEY_RIGHT:
  case PDC_KEY_UP:
  case PDC_KEY_DOWN:
#endif
    is_movement_key = true;
    break;
  }

  if (!is_movement_key)
  {
    return false;
  }

  // === SELECTION LOGIC ===
  if (extending_selection)
  {
    // Start selection if not already selecting
    if (!editor_.isSelectionActive())
    {
      editor_.startSelection(editor_.getCursorLine(), editor_.getCursorCol());
    }
  }
  else
  {
    // Moving without shift - clear selection
    editor_.clearSelection();
  }

  // === PERFORM MOVEMENT ===
  bool moved = false;

  switch (baseKey)
  {
  case KEY_LEFT:
#ifdef _WIN32
  case PDC_KEY_LEFT:
#endif
    editor_.moveCursorLeft();
    moved = true;
    break;

  case KEY_RIGHT:
#ifdef _WIN32
  case PDC_KEY_RIGHT:
#endif
    editor_.moveCursorRight();
    moved = true;
    break;

  case KEY_UP:
#ifdef _WIN32
  case PDC_KEY_UP:
#endif
    editor_.moveCursorUp();
    moved = true;
    break;

  case KEY_DOWN:
#ifdef _WIN32
  case PDC_KEY_DOWN:
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
  }

  // Update selection end point if extending
  if (moved && extending_selection)
  {
    editor_.extendSelection(editor_.getCursorLine(), editor_.getCursorCol());
  }

  return moved;
}

// ----------------------------------------------------------------------
// Event Handlers
// ----------------------------------------------------------------------

InputHandler::KeyResult InputHandler::handleMouseEvent()
{
  MEVENT event;
  if (GETMOUSE_FUNC(&event) != OK)
  {
    return KeyResult::NOT_HANDLED;
  }

  // === BUTTON1_PRESSED: Start potential selection ===
  if (event.bstate & BUTTON1_PRESSED)
  {
    int fileRow, fileCol;
    if (!editor_.mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      return KeyResult::NOT_HANDLED;
    }

    // Store click position for drag detection
    mouseState_.clickStartX = event.x;
    mouseState_.clickStartY = event.y;
    mouseState_.wasClick = true;
    mouseState_.isDragging = false;

    // ALWAYS start fresh selection on mouse press
    editor_.clearSelection();
    editor_.startSelection(fileRow, fileCol);
    editor_.updateCursorAndViewport(fileRow, fileCol);

    return KeyResult::REDRAW;
  }

  // === REPORT_MOUSE_POSITION: Drag to extend selection ===
  if (event.bstate & REPORT_MOUSE_POSITION)
  {
    // Check if button 1 is still held (dragging)
    if (mouseState_.clickStartX >= 0)
    {
      // Calculate distance from click start
      int dx = abs(event.x - mouseState_.clickStartX);
      int dy = abs(event.y - mouseState_.clickStartY);

      // If moved beyond threshold, it's a drag
      if (dx > MouseState::DRAG_THRESHOLD || dy > MouseState::DRAG_THRESHOLD)
      {
        mouseState_.isDragging = true;
        mouseState_.wasClick = false;

        int fileRow, fileCol;
        if (editor_.mouseToFilePos(event.y, event.x, fileRow, fileCol))
        {
          editor_.extendSelection(fileRow, fileCol);
          editor_.updateCursorAndViewport(fileRow, fileCol);
          return KeyResult::REDRAW;
        }
      }
    }
  }

  // === BUTTON1_RELEASED: Finalize or cancel selection ===
  if (event.bstate & BUTTON1_RELEASED)
  {
    int fileRow, fileCol;
    if (editor_.mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      if (mouseState_.isDragging)
      {
        // Complete the drag selection
        editor_.extendSelection(fileRow, fileCol);
        editor_.finalizeSelection();
      }
      else if (mouseState_.wasClick)
      {
        // Just a click - clear selection and move cursor
        editor_.clearSelection();
      }

      editor_.updateCursorAndViewport(fileRow, fileCol);
    }

    // Reset mouse state
    mouseState_.clickStartX = -1;
    mouseState_.clickStartY = -1;
    mouseState_.isDragging = false;
    mouseState_.wasClick = false;

    return KeyResult::REDRAW;
  }

  // === BUTTON1_CLICKED: Pure click (no drag) ===
  if (event.bstate & BUTTON1_CLICKED)
  {
    int fileRow, fileCol;
    if (editor_.mouseToFilePos(event.y, event.x, fileRow, fileCol))
    {
      editor_.clearSelection();
      editor_.updateCursorAndViewport(fileRow, fileCol);
      return KeyResult::REDRAW;
    }
  }

  // === SCROLL WHEEL ===
  if (event.bstate & BUTTON4_PRESSED)
  {
    editor_.scrollUp();
    return KeyResult::REDRAW;
  }

  if (event.bstate & BUTTON5_PRESSED)
  {
    editor_.scrollDown();
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

void InputHandler::showHelpScreen()
{
  // A structure to hold help screen data: Category, Key, Description
  using HelpEntry = std::tuple<std::string, std::string, std::string>;
  std::vector<HelpEntry> entries = {
      // Core Commands
      {"CORE COMMANDS", "Ctrl+Q", "Quit Editor"},
      {"", "Ctrl+S", "Save File"},
      {"", "F1 / Esc", "Close Help Screen"},
      {"", "Ctrl+L", "Toggle Markdown Rendering"},

      // Editor Actions
      {"EDITOR ACTIONS", "Ctrl+Z", "Undo"},
      {"", "Ctrl+Y", "Redo"},
      {"", "Ctrl+A", "Select All"},
      {"", "Ctrl+C", "Copy Selection"},
      {"", "Ctrl+X", "Cut Selection"},
      {"", "Ctrl+V / Ctrl+P", "Paste From Clipboard"},
      {"", "Backspace / Del", "Delete Character / Selection"},
      {"", "Tab", "Insert 4 Spaces (Soft Tab)"},

      // Markdown Configuration
      {"MARKDOWN CONFIG", "Ctrl+]", "Cycle Code Block Style"},
      {"", "Ctrl+[", "Cycle Heading Style"},
      {"", "Ctrl+\\", "Cycle Code Indent Style"},
      {"NAVIGATION", "Arrow Keys", "Move Cursor"},
      {"", "Shift + Arrows", "Select Text"},
      {"", "Home / End", "Move to Line Start / End"},
      {"", "Page Up / Down", "Scroll by Page"},
  };

  // Determine column widths
  size_t cat_w = 0, key_w = 0, desc_w = 0;
  for (auto &e : entries)
  {
    cat_w = std::max(cat_w, std::get<0>(e).length());
    key_w = std::max(key_w, std::get<1>(e).length());
    desc_w = std::max(desc_w, std::get<2>(e).length());
  }
  // Provide some minimum spacing
  cat_w = std::max(cat_w, size_t(14));
  key_w = std::max(key_w, size_t(12));

  // Print header
  std::cout << std::string(cat_w + key_w + desc_w + 6, '=') << "\n";
  std::cout << std::left << std::setw((int)cat_w) << "Category" << "  "
            << std::setw((int)key_w) << "Key" << "  " << "Description\n";
  std::cout << std::string(cat_w + key_w + desc_w + 6, '=') << "\n";

  // Print rows, grouping categories visually (blank category prints as
  // continuation)
  std::string last_category;
  for (auto &e : entries)
  {
    const auto &cat = std::get<0>(e);
    const auto &key = std::get<1>(e);
    const auto &desc = std::get<2>(e);

    if (!cat.empty())
    {
      // Category row
      std::cout << std::left << std::setw((int)cat_w) << cat << "  "
                << std::setw((int)key_w) << key << "  " << desc << "\n";
      last_category = cat;
    }
    else
    {
      // Continuation row (no repeated category)
      std::cout << std::left << std::setw((int)cat_w) << " " << "  "
                << std::setw((int)key_w) << key << "  " << desc << "\n";
    }
  }

  std::cout << std::string(cat_w + key_w + desc_w + 6, '=') << "\n";
  // Minimal hint: adapt this printing to your UI window/overlay API where
  // appropriate.
}

void InputHandler::displayStatusMessage(const std::string &message)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  // Display at bottom of screen
  move(rows - 2, 0);
  attron(A_STANDOUT);
  clrtoeol();

  if (!message.empty())
  {
    // Center the message
    int start_col = (cols - static_cast<int>(message.length())) / 2;
    if (start_col < 0)
      start_col = 0;

    mvprintw(rows - 2, start_col, "%s", message.c_str());
  }

  attroff(A_STANDOUT);
  refresh();
}
