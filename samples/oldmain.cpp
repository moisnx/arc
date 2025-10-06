#include <algorithm>
#include <curses.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// Windows/PDCurses compatibility
#ifdef _WIN32
#include <windows.h>
#endif

enum class EditorMode
{
  NORMAL,
  INSERT,
  VISUAL,
  COMMAND
};

// Color pairs enum
enum ColorPairs
{
  HEADER_PAIR = 1,
  LINE_NUM_PAIR,
  CONTENT_PAIR,
  STATUS_PAIR,
  CURSOR_PAIR,
  ACCENT_PAIR,
  BORDER_PAIR,
  COMMAND_PAIR,
  VISUAL_PAIR
};

// Theme structure
struct Theme
{
  std::string name;
  struct
  {
    short fg, bg;
  } header, line_num, content, status, cursor, accent, border, command, visual;

  Theme(const std::string &theme_name = "default") : name(theme_name)
  {
    // Default theme
    header = {COLOR_BLACK, COLOR_CYAN};
    line_num = {COLOR_BLUE, COLOR_BLACK};
    content = {COLOR_WHITE, COLOR_BLACK};
    status = {COLOR_BLACK, COLOR_GREEN};
    cursor = {COLOR_BLACK, COLOR_WHITE};
    accent = {COLOR_MAGENTA, COLOR_BLACK};
    border = {COLOR_BLUE, COLOR_BLACK};
    command = {COLOR_YELLOW, COLOR_BLACK};
    visual = {COLOR_BLACK, COLOR_YELLOW};
  }
};

class ThemeManager
{
private:
  std::vector<Theme> themes;
  int current_theme_index = 0;

public:
  ThemeManager()
  {
    // Add default themes
    themes.push_back(Theme("default"));

    // Dark theme
    Theme dark("dark");
    dark.header = {COLOR_WHITE, COLOR_BLACK};
    dark.line_num = {COLOR_CYAN, COLOR_BLACK};
    dark.content = {COLOR_WHITE, COLOR_BLACK};
    dark.status = {COLOR_BLACK, COLOR_CYAN};
    dark.cursor = {COLOR_BLACK, COLOR_WHITE};
    dark.accent = {COLOR_YELLOW, COLOR_BLACK};
    dark.border = {COLOR_CYAN, COLOR_BLACK};
    dark.command = {COLOR_GREEN, COLOR_BLACK};
    dark.visual = {COLOR_BLACK, COLOR_YELLOW};
    themes.push_back(dark);

    // Monokai-inspired theme
    Theme monokai("monokai");
    monokai.header = {COLOR_BLACK, COLOR_MAGENTA};
    monokai.line_num = {COLOR_YELLOW, COLOR_BLACK};
    monokai.content = {COLOR_WHITE, COLOR_BLACK};
    monokai.status = {COLOR_BLACK, COLOR_MAGENTA};
    monokai.cursor = {COLOR_BLACK, COLOR_WHITE};
    monokai.accent = {COLOR_GREEN, COLOR_BLACK};
    monokai.border = {COLOR_MAGENTA, COLOR_BLACK};
    monokai.command = {COLOR_CYAN, COLOR_BLACK};
    monokai.visual = {COLOR_BLACK, COLOR_GREEN};
    themes.push_back(monokai);
  }

  void nextTheme()
  {
    current_theme_index = (current_theme_index + 1) % themes.size();
    applyTheme();
  }

  void applyTheme()
  {
    const Theme &theme = themes[current_theme_index];
    init_pair(HEADER_PAIR, theme.header.fg, theme.header.bg);
    init_pair(LINE_NUM_PAIR, theme.line_num.fg, theme.line_num.bg);
    init_pair(CONTENT_PAIR, theme.content.fg, theme.content.bg);
    init_pair(STATUS_PAIR, theme.status.fg, theme.status.bg);
    init_pair(CURSOR_PAIR, theme.cursor.fg, theme.cursor.bg);
    init_pair(ACCENT_PAIR, theme.accent.fg, theme.accent.bg);
    init_pair(BORDER_PAIR, theme.border.fg, theme.border.bg);
    init_pair(COMMAND_PAIR, theme.command.fg, theme.command.bg);
    init_pair(VISUAL_PAIR, theme.visual.fg, theme.visual.bg);
  }

  std::string getCurrentThemeName() const
  {
    return themes[current_theme_index].name;
  }
};

class ModeManager
{
private:
  EditorMode current_mode = EditorMode::NORMAL;

public:
  void switchMode(EditorMode mode) { current_mode = mode; }

  EditorMode getCurrentMode() const { return current_mode; }

  std::string getModeString() const
  {
    switch (current_mode)
    {
    case EditorMode::NORMAL:
      return "NORMAL";
    case EditorMode::INSERT:
      return "INSERT";
    case EditorMode::VISUAL:
      return "VISUAL";
    case EditorMode::COMMAND:
      return "COMMAND";
    default:
      return "UNKNOWN";
    }
  }
};

struct Cursor
{
  int line = 0;
  int column = 0;
  int preferred_column = 0;

  void moveUp(int line_count)
  {
    if (line > 0)
    {
      line--;
      column = preferred_column;
    }
    (void)line_count; // Suppress unused parameter warning
  }

  void moveDown(int line_count)
  {
    if (line < line_count - 1)
    {
      line++;
      column = preferred_column;
    }
  }

  void moveLeft(const std::vector<std::string> &lines)
  {
    if (column > 0)
    {
      column--;
      preferred_column = column;
    }
    else if (line > 0)
    {
      line--;
      column = lines[line].length();
      preferred_column = column;
    }
  }

  void moveRight(const std::vector<std::string> &lines)
  {
    if (line < (int)lines.size() && column < (int)lines[line].length())
    {
      column++;
      preferred_column = column;
    }
    else if (line < (int)lines.size() - 1)
    {
      line++;
      column = 0;
      preferred_column = column;
    }
  }

  void moveToLineStart()
  {
    column = 0;
    preferred_column = column;
  }

  void moveToLineEnd(const std::vector<std::string> &lines)
  {
    if (line < (int)lines.size())
    {
      column = lines[line].length();
      preferred_column = column;
    }
  }

  void clampToBuffer(const std::vector<std::string> &lines)
  {
    if (lines.empty())
    {
      line = column = preferred_column = 0;
      return;
    }

    line = std::max(0, std::min(line, (int)lines.size() - 1));
    if (line < (int)lines.size())
    {
      column = std::max(0, std::min(column, (int)lines[line].length()));
      preferred_column =
          std::max(0, std::min(preferred_column, (int)lines[line].length()));
    }
  }
};

class EditableTextBuffer
{
private:
  std::vector<std::string> lines;
  std::string filename;
  bool modified = false;

public:
  bool loadFile(const std::string &filepath)
  {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
      return false;
    }

    lines.clear();
    std::string content;

    // Read entire file
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    if (size > 0)
    {
      content.resize(size);
      file.seekg(0, std::ios::beg);
      file.read(&content[0], size);
    }
    file.close();

    // Split into lines
    if (content.empty())
    {
      lines.push_back("");
    }
    else
    {
      std::istringstream stream(content);
      std::string line;
      while (std::getline(stream, line))
      {
        lines.push_back(line);
      }

      // If file doesn't end with newline, we still have all lines
      if (lines.empty())
      {
        lines.push_back("");
      }
    }

    filename = filepath;
    modified = false;
    return true;
  }

  bool saveFile(const std::string &filepath = "")
  {
    std::string save_path = filepath.empty() ? filename : filepath;

    std::ofstream file(save_path, std::ios::binary);
    if (!file.is_open())
    {
      return false;
    }

    for (size_t i = 0; i < lines.size(); ++i)
    {
      file << lines[i];
      if (i < lines.size() - 1)
      {
        file << '\n';
      }
    }

    file.close();

    if (!filepath.empty())
    {
      filename = filepath;
    }
    modified = false;
    return true;
  }

  void insertChar(int line, int col, char c)
  {
    if (line < 0 || line >= (int)lines.size())
      return;

    if (col < 0)
      col = 0;
    if (col > (int)lines[line].length())
      col = lines[line].length();

    lines[line].insert(col, 1, c);
    modified = true;
  }

  void deleteChar(int line, int col)
  {
    if (line < 0 || line >= (int)lines.size())
      return;
    if (col < 0 || col >= (int)lines[line].length())
      return;

    lines[line].erase(col, 1);
    modified = true;
  }

  void insertNewLine(int line, int col)
  {
    if (line < 0 || line >= (int)lines.size())
      return;

    if (col < 0)
      col = 0;
    if (col > (int)lines[line].length())
      col = lines[line].length();

    std::string current_line = lines[line];
    std::string new_line = current_line.substr(col);
    lines[line] = current_line.substr(0, col);
    lines.insert(lines.begin() + line + 1, new_line);
    modified = true;
  }

  void backspace(int line, int col)
  {
    if (line < 0 || line >= (int)lines.size())
      return;

    if (col > 0)
    {
      // Delete character before cursor
      deleteChar(line, col - 1);
    }
    else if (line > 0)
    {
      // Join with previous line
      int prev_line_length = lines[line - 1].length();
      lines[line - 1] += lines[line];
      lines.erase(lines.begin() + line);
      modified = true;
    }
  }

  const std::vector<std::string> &getLines() const { return lines; }
  std::string getFilename() const { return filename; }
  size_t getLineCount() const { return lines.size(); }
  bool isModified() const { return modified; }

  // Get display width of a UTF-8 string (approximation)
  int getDisplayWidth(const std::string &str) const
  {
    int width = 0;
    for (size_t i = 0; i < str.length(); ++i)
    {
      unsigned char c = str[i];
      if ((c & 0x80) == 0)
      {
        width++;
      }
      else if ((c & 0xE0) == 0xC0)
      {
        width++;
        i++;
      }
      else if ((c & 0xF0) == 0xE0)
      {
        width += 2;
        i += 2;
      }
      else if ((c & 0xF8) == 0xF0)
      {
        width++;
        i += 3;
      }
    }
    return width;
  }
};

class CommandProcessor
{
private:
  std::vector<std::string> command_history;

public:
  enum CommandResult
  {
    CMD_SUCCESS,
    CMD_ERROR,
    CMD_QUIT,
    CMD_UNKNOWN_COMMAND
  };

  CommandResult executeCommand(const std::string &command,
                               EditableTextBuffer &buffer)
  {
    if (command.empty())
      return CMD_SUCCESS;

    command_history.push_back(command);

    if (command == "q" || command == "quit")
    {
      if (buffer.isModified())
      {
        // Should show warning, but for now just quit
        return CMD_QUIT;
      }
      return CMD_QUIT;
    }
    else if (command == "q!" || command == "quit!")
    {
      return CMD_QUIT;
    }
    else if (command == "w" || command == "write")
    {
      return buffer.saveFile() ? CMD_SUCCESS : CMD_ERROR;
    }
    else if (command == "wq" || command == "x")
    {
      if (buffer.saveFile())
      {
        return CMD_QUIT;
      }
      return CMD_ERROR;
    }
    else if (command.substr(0, 2) == "w ")
    {
      std::string filename = command.substr(2);
      return buffer.saveFile(filename) ? CMD_SUCCESS : CMD_ERROR;
    }
    else if (command == "help")
    {
      // Could display help - for now just return success
      return CMD_SUCCESS;
    }

    return CMD_UNKNOWN_COMMAND;
  }

  const std::vector<std::string> &getCommandHistory() const
  {
    return command_history;
  }
};

class Menu
{
private:
  std::string title;
  std::vector<std::string> items;
  int selected_item = 0;
  bool is_open = false;

public:
  Menu(const std::string &menu_title,
       const std::vector<std::string> &menu_items)
      : title(menu_title), items(menu_items)
  {
  }

  void draw(int x, int y)
  {
    if (!is_open)
      return;

    int width = title.length() + 4;
    for (const auto &item : items)
    {
      width = std::max(width, (int)item.length() + 4);
    }

    // Draw menu background
    attron(COLOR_PAIR(HEADER_PAIR));
    for (int i = 0; i < (int)items.size() + 2; ++i)
    {
      mvhline(y + i, x, ' ', width);
    }

    // Draw title
    attron(A_BOLD);
    mvprintw(y, x + 2, "%s", title.c_str());
    attroff(A_BOLD);

    // Draw separator
    mvhline(y + 1, x, '-', width);

    // Draw items
    for (int i = 0; i < (int)items.size(); ++i)
    {
      if (i == selected_item)
      {
        attron(A_REVERSE);
      }
      mvprintw(y + 2 + i, x + 2, "%s", items[i].c_str());
      if (i == selected_item)
      {
        attroff(A_REVERSE);
      }
    }

    attroff(COLOR_PAIR(HEADER_PAIR));
  }

  bool handleInput(int key)
  {
    if (!is_open)
      return false;

    switch (key)
    {
    case KEY_UP:
      selected_item = (selected_item - 1 + items.size()) % items.size();
      return true;
    case KEY_DOWN:
      selected_item = (selected_item + 1) % items.size();
      return true;
    case '\n':
    case '\r':
      is_open = false;
      return true;
    case 27: // ESC
      is_open = false;
      return true;
    }
    return false;
  }

  void open() { is_open = true; }
  void close() { is_open = false; }
  bool isOpen() const { return is_open; }
  int getSelectedItem() const { return selected_item; }
};

class ArcEditor
{
private:
  EditableTextBuffer buffer;
  ModeManager mode_manager;
  ThemeManager theme_manager;
  CommandProcessor command_processor;

  Cursor cursor;
  int scroll_offset = 0;
  bool running = true;
  bool show_line_numbers = true;
  int line_number_width = 4;

  std::string command_line;
  std::string status_message;

  // Menu system
  std::unique_ptr<Menu> file_menu;
  std::unique_ptr<Menu> edit_menu;
  std::unique_ptr<Menu> view_menu;
  int active_menu = -1;

public:
  void init()
  {
#ifndef _WIN32
    setlocale(LC_ALL, "");
#else
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

#ifdef PDC_WIDE
    mouse_set(ALL_MOUSE_EVENTS);
#elif defined(_WIN32)
    mouseinterval(0);
#else
    mousemask(ALL_MOUSE_EVENTS, NULL);
#endif

    curs_set(1); // Show cursor for editing

    if (has_colors())
    {
      start_color();
#ifndef _WIN32
      use_default_colors();
#endif
      theme_manager.applyTheme();
    }

    // Initialize menus
    file_menu = std::make_unique<Menu>(
        "File",
        std::vector<std::string>{"New", "Open", "Save", "Save As", "Quit"});
    edit_menu = std::make_unique<Menu>(
        "Edit",
        std::vector<std::string>{"Cut", "Copy", "Paste", "Find", "Replace"});
    view_menu = std::make_unique<Menu>(
        "View", std::vector<std::string>{"Toggle Line Numbers", "Next Theme",
                                         "Zoom In", "Zoom Out"});
  }

  void cleanup() { endwin(); }

  bool loadFile(const std::string &filepath)
  {
    bool result = buffer.loadFile(filepath);
    if (result)
    {
      line_number_width =
          std::max(4, (int)std::to_string(buffer.getLineCount()).length() + 1);
      cursor = Cursor(); // Reset cursor
      scroll_offset = 0;
    }
    return result;
  }

  void drawMenuBar()
  {
    int width = getmaxx(stdscr);

    attron(COLOR_PAIR(HEADER_PAIR));
    mvhline(0, 0, ' ', width);

    // Menu titles
    std::vector<std::string> menu_titles = {"File", "Edit", "View"};
    int x = 2;
    for (int i = 0; i < (int)menu_titles.size(); ++i)
    {
      if (active_menu == i)
      {
        attron(A_REVERSE);
      }
      mvprintw(0, x, " %s ", menu_titles[i].c_str());
      if (active_menu == i)
      {
        attroff(A_REVERSE);
      }
      x += menu_titles[i].length() + 3;
    }

    // Editor title and filename
    std::string title = "Arc Editor";
    std::string filename = buffer.getFilename();
    if (buffer.isModified())
      filename += " [Modified]";

    mvprintw(0, width - title.length() - filename.length() - 5, "%s - %s",
             title.c_str(), filename.c_str());

    attroff(COLOR_PAIR(HEADER_PAIR));

    // Draw open menus
    if (file_menu->isOpen())
      file_menu->draw(2, 1);
    if (edit_menu->isOpen())
      edit_menu->draw(8, 1);
    if (view_menu->isOpen())
      view_menu->draw(15, 1);
  }

  void drawBorder()
  {
    int width = getmaxx(stdscr);
    int height = getmaxy(stdscr);

    attron(COLOR_PAIR(BORDER_PAIR));

#ifdef _WIN32
    mvhline(1, 0, '-', width);
    for (int i = 2; i < height - 2; ++i)
    {
      mvaddch(i, 0, '|');
      mvaddch(i, width - 1, '|');
    }
    mvhline(height - 2, 0, '-', width);
    mvaddch(1, 0, '+');
    mvaddch(1, width - 1, '+');
    mvaddch(height - 2, 0, '+');
    mvaddch(height - 2, width - 1, '+');
#else
    mvhline(1, 0, ACS_HLINE, width);
    mvaddch(1, 0, ACS_ULCORNER);
    mvaddch(1, width - 1, ACS_URCORNER);

    for (int i = 2; i < height - 2; ++i)
    {
      mvaddch(i, 0, ACS_VLINE);
      mvaddch(i, width - 1, ACS_VLINE);
    }

    mvhline(height - 2, 0, ACS_HLINE, width);
    mvaddch(height - 2, 0, ACS_LLCORNER);
    mvaddch(height - 2, width - 1, ACS_LRCORNER);
#endif

    attroff(COLOR_PAIR(BORDER_PAIR));
  }

  void drawContent()
  {
    int width = getmaxx(stdscr);
    int height = getmaxy(stdscr);

    const auto &lines = buffer.getLines();
    int content_height = height - 4;
    int content_start_x = show_line_numbers ? line_number_width + 3 : 2;
    int content_width = width - content_start_x - 2;

    // Ensure cursor is visible
    while (cursor.line < scroll_offset)
    {
      scroll_offset--;
    }
    while (cursor.line >= scroll_offset + content_height)
    {
      scroll_offset++;
    }

    for (int i = 0;
         i < content_height && (i + scroll_offset) < (int)lines.size(); ++i)
    {
      int line_num = i + scroll_offset + 1;
      int screen_y = i + 2;

      // Draw line number
      if (show_line_numbers)
      {
        if (line_num == cursor.line + 1)
        {
          attron(COLOR_PAIR(ACCENT_PAIR) | A_BOLD);
        }
        else
        {
          attron(COLOR_PAIR(LINE_NUM_PAIR));
        }

        mvprintw(screen_y, 1, "%*d |", line_number_width, line_num);
        attroff(COLOR_PAIR(LINE_NUM_PAIR) | COLOR_PAIR(ACCENT_PAIR) | A_BOLD);
      }

      // Draw line content
      const std::string &line = lines[i + scroll_offset];

      // Highlight current line in visual mode or show cursor
      if ((mode_manager.getCurrentMode() == EditorMode::VISUAL ||
           mode_manager.getCurrentMode() == EditorMode::INSERT) &&
          i + scroll_offset == cursor.line)
      {
        attron(COLOR_PAIR(VISUAL_PAIR));
        for (int j = content_start_x; j < width - 1; ++j)
        {
          mvaddch(screen_y, j, ' ');
        }
        attroff(COLOR_PAIR(VISUAL_PAIR));
      }

      attron(COLOR_PAIR(CONTENT_PAIR));

      // Truncate line if too long
      std::string display_line = line;
      if (buffer.getDisplayWidth(line) > content_width)
      {
        display_line = line.substr(0, content_width - 3) + "...";
      }

      mvprintw(screen_y, content_start_x, "%s", display_line.c_str());
      attroff(COLOR_PAIR(CONTENT_PAIR));
    }

    // Position cursor
    if (cursor.line >= scroll_offset &&
        cursor.line < scroll_offset + content_height)
    {
      int cursor_screen_y = cursor.line - scroll_offset + 2;
      int cursor_screen_x = content_start_x + cursor.column;
      move(cursor_screen_y, cursor_screen_x);
    }
  }

  void drawStatusBar()
  {
    int width = getmaxx(stdscr);
    int height = getmaxy(stdscr);

    // Status bar background
    attron(COLOR_PAIR(STATUS_PAIR) | A_BOLD);
    for (int i = 0; i < width; ++i)
      mvaddch(height - 1, i, ' ');

    // Left side: mode and file info
    mvprintw(height - 1, 2, "%s | %s | Line: %d Col: %d",
             mode_manager.getModeString().c_str(), buffer.getFilename().c_str(),
             cursor.line + 1, cursor.column + 1);

    // Center: status message or command line
    if (mode_manager.getCurrentMode() == EditorMode::COMMAND)
    {
      int center_x = width / 2 - 20;
      mvprintw(height - 1, center_x, ":%s", command_line.c_str());
    }
    else if (!status_message.empty())
    {
      int center_x = width / 2 - status_message.length() / 2;
      mvprintw(height - 1, center_x, "%s", status_message.c_str());
    }

    // Right side: theme and encoding
    std::string right_info = theme_manager.getCurrentThemeName() + " | UTF-8";
    mvprintw(height - 1, width - right_info.length() - 2, "%s",
             right_info.c_str());

    attroff(COLOR_PAIR(STATUS_PAIR) | A_BOLD);
  }

  void draw()
  {
    clear();

    // drawMenuBar();
    // drawBorder();
    drawContent();
    drawStatusBar();

    refresh();
  }

  void handleNormalMode(int ch)
  {
    const auto &lines = buffer.getLines();

    switch (ch)
    {
    // Movement
    case 'h':
    case KEY_LEFT:
      cursor.moveLeft(lines);
      break;
    case 'j':
    case KEY_DOWN:
      cursor.moveDown(buffer.getLineCount());
      break;
    case 'k':
    case KEY_UP:
      cursor.moveUp(buffer.getLineCount());
      break;
    case 'l':
    case KEY_RIGHT:
      cursor.moveRight(lines);
      break;
    case '0':
      cursor.moveToLineStart();
      break;
    case '$':
      cursor.moveToLineEnd(lines);
      break;
    case 'g':
      // Go to first line
      cursor.line = 0;
      cursor.column = 0;
      cursor.preferred_column = 0;
      scroll_offset = 0;
      break;
    case 'G':
      // Go to last line
      cursor.line = buffer.getLineCount() - 1;
      cursor.column = 0;
      cursor.preferred_column = 0;
      break;

    // Mode switches
    case 'i':
      mode_manager.switchMode(EditorMode::INSERT);
      curs_set(1);
      break;
    case 'a':
      cursor.moveRight(lines);
      mode_manager.switchMode(EditorMode::INSERT);
      curs_set(1);
      break;
    case 'o':
      // Insert new line below and enter insert mode
      buffer.insertNewLine(cursor.line, lines[cursor.line].length());
      cursor.line++;
      cursor.column = 0;
      cursor.preferred_column = 0;
      mode_manager.switchMode(EditorMode::INSERT);
      curs_set(1);
      break;
    case 'O':
      // Insert new line above and enter insert mode
      buffer.insertNewLine(cursor.line - 1,
                           cursor.line > 0 ? lines[cursor.line - 1].length()
                                           : 0);
      cursor.column = 0;
      cursor.preferred_column = 0;
      mode_manager.switchMode(EditorMode::INSERT);
      curs_set(1);
      break;
    case 'v':
      mode_manager.switchMode(EditorMode::VISUAL);
      break;
    case ':':
      mode_manager.switchMode(EditorMode::COMMAND);
      command_line.clear();
      break;

    // File operations
    case 's':
      if (buffer.saveFile())
      {
        status_message = "File saved";
      }
      else
      {
        status_message = "Error saving file";
      }
      break;

    // Themes
    case 't':
      theme_manager.nextTheme();
      status_message = "Theme: " + theme_manager.getCurrentThemeName();
      break;

    // Toggle line numbers
    case 'n':
      show_line_numbers = !show_line_numbers;
      status_message =
          show_line_numbers ? "Line numbers on" : "Line numbers off";
      break;

    // Menu
    case KEY_F(1):
      active_menu = 0;
      file_menu->open();
      break;
    case KEY_F(2):
      active_menu = 1;
      edit_menu->open();
      break;
    case KEY_F(3):
      active_menu = 2;
      view_menu->open();
      break;

    // Quit
    case 'q':
      if (!buffer.isModified())
      {
        running = false;
      }
      else
      {
        status_message = "File modified. Use :q! to force quit or :w to save";
      }
      break;
    }

    cursor.clampToBuffer(lines);
  }

  void handleInsertMode(int ch)
  {
    const auto &lines = buffer.getLines();

    switch (ch)
    {
    case 27: // ESC
      mode_manager.switchMode(EditorMode::NORMAL);
      curs_set(0);
      break;
    case KEY_BACKSPACE:
    case '\b':
    case 127:
      if (cursor.column > 0)
      {
        buffer.deleteChar(cursor.line, cursor.column - 1);
        cursor.column--;
        cursor.preferred_column = cursor.column;
      }
      else if (cursor.line > 0)
      {
        // Join with previous line
        int prev_line_length = lines[cursor.line - 1].length();
        buffer.backspace(cursor.line, cursor.column);
        cursor.line--;
        cursor.column = prev_line_length;
        cursor.preferred_column = cursor.column;
      }
      break;
    case '\n':
    case '\r':
      buffer.insertNewLine(cursor.line, cursor.column);
      cursor.line++;
      cursor.column = 0;
      cursor.preferred_column = 0;
      break;
    case KEY_LEFT:
      cursor.moveLeft(lines);
      break;
    case KEY_RIGHT:
      cursor.moveRight(lines);
      break;
    case KEY_UP:
      cursor.moveUp(buffer.getLineCount());
      break;
    case KEY_DOWN:
      cursor.moveDown(buffer.getLineCount());
      break;
    default:
      if (ch >= 32 && ch <= 126)
      { // Printable characters
        buffer.insertChar(cursor.line, cursor.column, (char)ch);
        cursor.column++;
        cursor.preferred_column = cursor.column;
      }
      break;
    }

    cursor.clampToBuffer(lines);
  }

  void handleVisualMode(int ch)
  {
    const auto &lines = buffer.getLines();

    switch (ch)
    {
    case 27: // ESC
    case 'v':
      mode_manager.switchMode(EditorMode::NORMAL);
      break;
    case 'h':
    case KEY_LEFT:
      cursor.moveLeft(lines);
      break;
    case 'j':
    case KEY_DOWN:
      cursor.moveDown(buffer.getLineCount());
      break;
    case 'k':
    case KEY_UP:
      cursor.moveUp(buffer.getLineCount());
      break;
    case 'l':
    case KEY_RIGHT:
      cursor.moveRight(lines);
      break;
    case 'i':
      mode_manager.switchMode(EditorMode::INSERT);
      curs_set(1);
      break;
    }

    cursor.clampToBuffer(lines);
  }

  void handleCommandMode(int ch)
  {
    switch (ch)
    {
    case 27: // ESC
      mode_manager.switchMode(EditorMode::NORMAL);
      command_line.clear();
      status_message.clear();
      break;
    case '\n':
    case '\r':
    {
      auto result = command_processor.executeCommand(command_line, buffer);
      switch (result)
      {
      case CommandProcessor::CMD_SUCCESS:
        status_message = "Command executed";
        break;
      case CommandProcessor::CMD_ERROR:
        status_message = "Error executing command";
        break;
      case CommandProcessor::CMD_QUIT:
        running = false;
        break;
      case CommandProcessor::CMD_UNKNOWN_COMMAND:
        status_message = "Unknown command: " + command_line;
        break;
      }
      mode_manager.switchMode(EditorMode::NORMAL);
      command_line.clear();
    }
    break;
    case KEY_BACKSPACE:
    case '\b':
    case 127:
      if (!command_line.empty())
      {
        command_line.pop_back();
      }
      break;
    default:
      if (ch >= 32 && ch <= 126)
      { // Printable characters
        command_line += (char)ch;
      }
      break;
    }
  }

  void handleMenuInput(int ch)
  {
    bool menu_handled = false;

    if (file_menu->isOpen())
    {
      menu_handled = file_menu->handleInput(ch);
      if (ch == '\n' || ch == '\r')
      {
        int selected = file_menu->getSelectedItem();
        switch (selected)
        {
        case 0: // New
          // Create new file
          status_message = "New file functionality not implemented";
          break;
        case 1: // Open
          status_message = "Open file functionality not implemented";
          break;
        case 2: // Save
          if (buffer.saveFile())
          {
            status_message = "File saved";
          }
          else
          {
            status_message = "Error saving file";
          }
          break;
        case 3: // Save As
          status_message = "Save As functionality not implemented";
          break;
        case 4: // Quit
          if (!buffer.isModified())
          {
            running = false;
          }
          else
          {
            status_message = "File modified. Save first or use force quit";
          }
          break;
        }
        active_menu = -1;
      }
    }
    else if (edit_menu->isOpen())
    {
      menu_handled = edit_menu->handleInput(ch);
      if (ch == '\n' || ch == '\r')
      {
        status_message = "Edit menu functionality not implemented";
        active_menu = -1;
      }
    }
    else if (view_menu->isOpen())
    {
      menu_handled = view_menu->handleInput(ch);
      if (ch == '\n' || ch == '\r')
      {
        int selected = view_menu->getSelectedItem();
        switch (selected)
        {
        case 0: // Toggle Line Numbers
          show_line_numbers = !show_line_numbers;
          status_message =
              show_line_numbers ? "Line numbers on" : "Line numbers off";
          break;
        case 1: // Next Theme
          theme_manager.nextTheme();
          status_message = "Theme: " + theme_manager.getCurrentThemeName();
          break;
        case 2: // Zoom In
          status_message = "Zoom functionality not implemented";
          break;
        case 3: // Zoom Out
          status_message = "Zoom functionality not implemented";
          break;
        }
        active_menu = -1;
      }
    }

    if (menu_handled && (ch == 27))
    {
      active_menu = -1;
    }

    // Handle menu navigation
    if (!menu_handled)
    {
      switch (ch)
      {
      case KEY_F(1):
        active_menu = 0;
        file_menu->open();
        edit_menu->close();
        view_menu->close();
        break;
      case KEY_F(2):
        active_menu = 1;
        edit_menu->open();
        file_menu->close();
        view_menu->close();
        break;
      case KEY_F(3):
        active_menu = 2;
        view_menu->open();
        file_menu->close();
        edit_menu->close();
        break;
      case 27: // ESC
        active_menu = -1;
        file_menu->close();
        edit_menu->close();
        view_menu->close();
        break;
      }
    }
  }

  void handleInput()
  {
    int ch = getch();

    // Clear status message after some time
    static int status_timer = 0;
    if (!status_message.empty())
    {
      status_timer++;
      if (status_timer > 50)
      { // Clear after ~50 key presses
        status_message.clear();
        status_timer = 0;
      }
    }

    // Handle menu input first
    if (active_menu != -1 || file_menu->isOpen() || edit_menu->isOpen() ||
        view_menu->isOpen())
    {
      handleMenuInput(ch);
      return;
    }

    // Handle global keys
    switch (ch)
    {
    case KEY_RESIZE:
      endwin();
      refresh();
      clear();
      return;
    case KEY_F(10):        // F10 to quit
    case 'Q' + ('Q' << 8): // Ctrl+Q (doesn't work in all terminals)
      running = false;
      return;
    }

    // Handle mode-specific input
    switch (mode_manager.getCurrentMode())
    {
    case EditorMode::NORMAL:
      handleNormalMode(ch);
      break;
    case EditorMode::INSERT:
      handleInsertMode(ch);
      break;
    case EditorMode::VISUAL:
      handleVisualMode(ch);
      break;
    case EditorMode::COMMAND:
      handleCommandMode(ch);
      break;
    }
  }

  void run()
  {
    while (running)
    {
      draw();
      handleInput();
    }
  }

  // Additional utility methods
  void showHelp()
  {
    status_message = "F1:File F2:Edit F3:View | Normal: hjkl/arrows:move "
                     "i:insert v:visual ::command q:quit";
  }
};

int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    std::cout << "Usage: arc <filename>" << std::endl;
    std::cout << "Arc Editor - A feature-rich terminal text editor"
              << std::endl;
    std::cout << "\nKey bindings:" << std::endl;
    std::cout << "  Normal Mode:" << std::endl;
    std::cout << "    h,j,k,l or arrows - Movement" << std::endl;
    std::cout << "    i - Insert mode" << std::endl;
    std::cout << "    a - Insert mode (after cursor)" << std::endl;
    std::cout << "    o/O - New line and insert mode" << std::endl;
    std::cout << "    v - Visual mode" << std::endl;
    std::cout << "    : - Command mode" << std::endl;
    std::cout << "    s - Save file" << std::endl;
    std::cout << "    t - Next theme" << std::endl;
    std::cout << "    n - Toggle line numbers" << std::endl;
    std::cout << "    q - Quit (if no changes)" << std::endl;
    std::cout << "    F1/F2/F3 - File/Edit/View menus" << std::endl;
    std::cout << "\n  Command Mode:" << std::endl;
    std::cout << "    :q - Quit" << std::endl;
    std::cout << "    :q! - Force quit" << std::endl;
    std::cout << "    :w - Save" << std::endl;
    std::cout << "    :w filename - Save as" << std::endl;
    std::cout << "    :wq or :x - Save and quit" << std::endl;
    return 1;
  }

  ArcEditor editor;

  if (!editor.loadFile(argv[1]))
  {
    std::cout << "Error: Could not open file '" << argv[1] << "'" << std::endl;
    std::cout << "Creating new file..." << std::endl;
    // Could create empty file here
  }

  editor.init();
  editor.run();
  editor.cleanup();

  return 0;
}
