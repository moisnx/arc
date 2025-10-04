#include "renderer.h"
#include "src/ui/style_manager.h"
#include <algorithm>
#include <cstring>
#include <iostream>

Renderer::Renderer() : tab_size_(4) {}

Renderer::~Renderer() { restoreDefaultCursor(); }

void Renderer::renderEditor(const EditorState &state, const GapBuffer &buffer,
                            const SyntaxHighlighter *highlighter)
{
  // Set background color for entire screen
  setDefaultColors();
  clear();

  // Render main content area
  renderContent(state, buffer, highlighter);

  // Render status bar
  renderStatusBar(state, buffer);

  // Position cursor
  positionCursor(state.cursor, state.viewport);

  // Update cursor style based on mode
  updateCursorStyle(state.mode);

  refresh();
}

void Renderer::renderContent(const EditorState &state, const GapBuffer &buffer,
                             const SyntaxHighlighter *highlighter)
{
  const ViewportInfo &viewport = state.viewport;

  int line_num_width = calculateLineNumberWidth(buffer.getLineCount());
  int end_line =
      std::min(viewport.top + viewport.height, buffer.getLineCount());

  // Pre-calculate syntax highlighting for visible lines
  std::vector<std::vector<ColorSpan>> line_spans(end_line - viewport.top);
  if (highlighter)
  {
    for (int i = viewport.top; i < end_line; i++)
    {
      try
      {
        std::string line = buffer.getLine(i);
        std::string expanded_line = expandTabs(line, tab_size_);
        line_spans[i - viewport.top] =
            highlighter->getHighlightSpans(expanded_line, i, buffer);
      }
      catch (const std::exception &e)
      {
        std::cerr << "Syntax highlighting error on line " << i << ": "
                  << e.what() << std::endl;
        line_spans[i - viewport.top].clear();
      }
    }
  }

  // Render each visible line
  for (int i = viewport.top; i < end_line; i++)
  {
    int screen_row = i - viewport.top;
    move(screen_row, 0);

    // Set background for entire line
    setDefaultColors();

    // Render line number
    bool is_current_line = (state.cursor.line == i);
    renderLineNumbers(i, i + 1, state.cursor.line, line_num_width,
                      viewport.top);

    // Render line content
    std::string line = buffer.getLine(i);
    std::string expanded_line = expandTabs(line, tab_size_);
    const std::vector<ColorSpan> &spans =
        highlighter ? line_spans[i - viewport.top] : std::vector<ColorSpan>();

    renderLine(expanded_line, i, spans, viewport, state);

    // Clear to end of line with proper background
    setDefaultColors();
    clrtoeol();
  }

  // Fill remaining screen with background color
  setDefaultColors();
  for (int i = end_line - viewport.top; i < viewport.height; i++)
  {
    move(i, 0);
    clrtoeol();
  }
}

void Renderer::renderLine(const std::string &line, int line_number,
                          const std::vector<ColorSpan> &spans,
                          const ViewportInfo &viewport,
                          const EditorState &state)
{
  int content_width = viewport.width - viewport.content_start_col;

  for (int screen_col = 0; screen_col < content_width; screen_col++)
  {
    int file_col = viewport.left + screen_col;

    bool char_exists =
        (file_col >= 0 && file_col < static_cast<int>(line.length()));
    char ch = char_exists ? line[file_col] : ' ';

    // Filter non-printable characters
    if (char_exists && (ch < 32 || ch > 126))
    {
      ch = ' ';
    }

    // Check for selection
    bool is_selected = isPositionSelected(line_number, file_col, state);

    if (is_selected)
    {
      attron(COLOR_PAIR(SELECTION) | A_REVERSE);
      addch(ch);
      attroff(COLOR_PAIR(SELECTION) | A_REVERSE);
    }
    else
    {
      // Apply syntax highlighting
      bool color_applied = false;

      if (char_exists && !spans.empty())
      {
        for (const auto &span : spans)
        {
          if (file_col >= span.start && file_col < span.end)
          {
            if (span.colorPair >= 0 && span.colorPair < COLOR_PAIRS)
            {
              applyColorSpan(span, ch);
              color_applied = true;
              break;
            }
          }
        }
      }

      if (!color_applied)
      {
        setDefaultColors();
        addch(ch);
      }
    }
  }
}

void Renderer::renderStatusBar(const EditorState &state,
                               const GapBuffer &buffer)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int status_row = rows - 1;

  move(status_row, 0);

  // Set status bar background
  attrset(COLOR_PAIR(STATUS_BAR));
  clrtoeol();

  move(status_row, 0);

  // Render mode
  renderStatusMode(state.mode);

  // Render file info
  renderStatusFile(state.filename, state.is_modified);

  // Render position info (right-aligned)
  renderStatusPosition(state.cursor, buffer.getLineCount(), state.has_selection,
                       0); // TODO: calculate selection size
}

void Renderer::renderStatusMode(EditorMode mode)
{
  std::string mode_str;
  int mode_color;

  switch (mode)
  {
  case EditorMode::NORMAL:
    mode_str = " NORMAL ";
    mode_color = STATUS_BAR;
    break;
  case EditorMode::INSERT:
    mode_str = " INSERT ";
    mode_color = STATUS_BAR_ACTIVE;
    break;
  case EditorMode::VISUAL:
    mode_str = " VISUAL ";
    mode_color = STATUS_BAR_ACTIVE;
    break;
  }

  attron(COLOR_PAIR(mode_color) | A_BOLD);
  printw("%s", mode_str.c_str());
  attroff(COLOR_PAIR(mode_color) | A_BOLD);

  attron(COLOR_PAIR(STATUS_BAR));
  printw(" ");
}

void Renderer::renderStatusFile(const std::string &filename, bool is_modified)
{
  attron(COLOR_PAIR(STATUS_BAR_CYAN) | A_BOLD);
  if (filename.empty())
  {
    printw("[No Name]");
  }
  else
  {
    size_t last_slash = filename.find_last_of("/\\");
    std::string display_name = (last_slash != std::string::npos)
                                   ? filename.substr(last_slash + 1)
                                   : filename;
    printw("%s", display_name.c_str());
  }
  attroff(COLOR_PAIR(STATUS_BAR_CYAN) | A_BOLD);

  if (is_modified)
  {
    attron(COLOR_PAIR(STATUS_BAR_ACTIVE) | A_BOLD);
    printw(" [+]");
    attroff(COLOR_PAIR(STATUS_BAR_ACTIVE) | A_BOLD);
  }
}

void Renderer::renderStatusPosition(const CursorInfo &cursor, int total_lines,
                                    bool has_selection, int selection_size)
{
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  int status_row = rows - 1;

  char position_info[256];
  int percentage =
      total_lines == 0 ? 0 : ((cursor.line + 1) * 100 / total_lines);

  if (has_selection)
  {
    snprintf(position_info, sizeof(position_info),
             "[selection] %d:%d %d/%d %d%% ", cursor.line + 1, cursor.col + 1,
             cursor.line + 1, total_lines, percentage);
  }
  else
  {
    snprintf(position_info, sizeof(position_info), "%d:%d %d/%d %d%% ",
             cursor.line + 1, cursor.col + 1, cursor.line + 1, total_lines,
             percentage);
  }

  int info_len = strlen(position_info);
  int current_pos = getcurx(stdscr);
  int right_start = cols - info_len;

  if (right_start <= current_pos)
  {
    right_start = current_pos + 2;
  }

  // Fill middle space
  attron(COLOR_PAIR(STATUS_BAR));
  for (int i = current_pos; i < right_start && i < cols; i++)
  {
    move(status_row, i);
    addch(' ');
  }

  // Render position info
  if (right_start < cols)
  {
    move(status_row, right_start);
    attron(COLOR_PAIR(STATUS_BAR_YELLOW) | A_BOLD);
    printw("%s", position_info);
    attroff(COLOR_PAIR(STATUS_BAR_YELLOW) | A_BOLD);
  }
}

void Renderer::renderLineNumbers(int start_line, int end_line, int current_line,
                                 int line_num_width, int viewport_top)
{
  int line_index = start_line - viewport_top;
  bool is_current = (start_line == current_line);

  int color_pair = is_current ? LINE_NUMBERS_ACTIVE : LINE_NUMBERS;
  attron(COLOR_PAIR(color_pair));
  printw("%*d ", line_num_width, start_line + 1);
  attroff(COLOR_PAIR(color_pair));

  // Separator
  attron(COLOR_PAIR(LINE_NUMBERS_DIM));
  addch(' ');
  attroff(COLOR_PAIR(LINE_NUMBERS_DIM));
  addch(' ');
}

void Renderer::positionCursor(const CursorInfo &cursor,
                              const ViewportInfo &viewport)
{
  int screen_row = cursor.line - viewport.top;
  if (screen_row >= 0 && screen_row < viewport.height)
  {
    int screen_col = viewport.content_start_col + cursor.col - viewport.left;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    if (screen_col >= viewport.content_start_col && screen_col < cols)
    {
      move(screen_row, screen_col);
    }
    else
    {
      move(screen_row, viewport.content_start_col);
    }
  }
}

void Renderer::updateCursorStyle(EditorMode mode)
{
  switch (mode)
  {
  case EditorMode::NORMAL:
    printf("\033[2 q"); // Block cursor
    break;
  case EditorMode::INSERT:
    printf("\033[6 q"); // Vertical bar cursor
    break;
  case EditorMode::VISUAL:
    printf("\033[4 q"); // Underline cursor
    break;
  }
  fflush(stdout);
}

void Renderer::restoreDefaultCursor()
{
  printf("\033[0 q");
  fflush(stdout);
}

void Renderer::clear() { ::clear(); }

void Renderer::refresh() { ::refresh(); }

void Renderer::handleResize()
{
  clear();
  refresh();
}

Renderer::ViewportInfo Renderer::calculateViewport() const
{
  ViewportInfo viewport;
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  viewport.height = rows - 1; // Reserve last row for status bar
  viewport.width = cols;

  // Calculate content start column (after line numbers)
  int max_lines = 1000; // Reasonable default, should be passed as parameter
  int line_num_width = calculateLineNumberWidth(max_lines);
  viewport.content_start_col = line_num_width + 3; // +3 for space and separator

  return viewport;
}

int Renderer::calculateLineNumberWidth(int max_line) const
{
  if (max_line <= 0)
    return 1;

  int width = 0;
  int num = max_line;
  while (num > 0)
  {
    width++;
    num /= 10;
  }
  return std::max(width, 3); // Minimum width of 3
}

bool Renderer::isPositionSelected(int line, int col,
                                  const EditorState &state) const
{
  if (!state.has_selection)
  {
    return false;
  }

  int start_line = state.selection_start_line;
  int start_col = state.selection_start_col;
  int end_line = state.selection_end_line;
  int end_col = state.selection_end_col;

  // Normalize selection
  if (start_line > end_line || (start_line == end_line && start_col > end_col))
  {
    std::swap(start_line, end_line);
    std::swap(start_col, end_col);
  }

  if (line < start_line || line > end_line)
  {
    return false;
  }

  if (start_line == end_line)
  {
    return col >= start_col && col < end_col;
  }
  else if (line == start_line)
  {
    return col >= start_col;
  }
  else if (line == end_line)
  {
    return col < end_col;
  }
  else
  {
    return true;
  }
}

std::string Renderer::expandTabs(const std::string &line, int tab_size) const
{
  std::string result;
  for (char c : line)
  {
    if (c == '\t')
    {
      int spaces_to_add = tab_size - (result.length() % tab_size);
      result.append(spaces_to_add, ' ');
    }
    else if (c >= 32 && c <= 126)
    {
      result += c;
    }
    else
    {
      result += ' ';
    }
  }
  return result;
}

void Renderer::applyColorSpan(const ColorSpan &span, char ch)
{
  int attrs = COLOR_PAIR(span.colorPair);
  if (span.attribute != 0)
  {
    attrs |= span.attribute;
  }

  attron(attrs);
  addch(ch);
  attroff(attrs);
}

void Renderer::setDefaultColors()
{
  attrset(COLOR_PAIR(0)); // Use default background
}