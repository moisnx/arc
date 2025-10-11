#include "browser_renderer.h"
#include "src/ui/icon_provider.h"
#include "style_manager.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

BrowserRenderer::BrowserRenderer()
    : height_(0), width_(0), viewport_height_(0),
      icon_provider_(IconStyle::AUTO)
{
}

void BrowserRenderer::render(const FileBrowser &browser)
{
  getmaxyx(stdscr, height_, width_);
  viewport_height_ = std::max(1, height_ - 4);

  erase();
  renderHeader(browser);
  renderFileList(browser);
  renderStatus(browser);
  refresh();
}

void BrowserRenderer::renderHeader(const FileBrowser &browser)
{
  // Top bar with title
  attron(COLOR_PAIR(ColorPairs::STATUS_BAR));
  mvprintw(0, 0, " ");

  attron(A_BOLD);
  printw("File Browser");
  attroff(A_BOLD);

  // File counts in secondary color
  size_t dirs = browser.getDirectoryCount();
  size_t files = browser.getFileCount();
  printw("  ");
  attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
  printw("%zu dirs, %zu files", dirs, files);
  attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

  attron(COLOR_PAIR(ColorPairs::STATUS_BAR));
  clrtoeol();
  attroff(COLOR_PAIR(ColorPairs::STATUS_BAR));

  // Current path with icon
  move(1, 0);
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));

  std::string path = browser.getCurrentPath().string();
  int max_path_len = width_ - 6;

  if (path.length() > static_cast<size_t>(max_path_len))
  {
    path = "..." + path.substr(path.length() - (max_path_len - 3));
  }

  printw("  %s", path.c_str());
  clrtoeol();
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));
}

void BrowserRenderer::renderFileList(const FileBrowser &browser)
{
  const auto &entries = browser.getEntries();
  size_t selected = browser.getSelectedIndex();
  size_t scroll = browser.getScrollOffset();

  int start_y = 2;
  size_t visible_end = std::min(scroll + viewport_height_, entries.size());

  for (size_t i = scroll; i < visible_end; ++i)
  {
    int y = start_y + (i - scroll);
    const auto &entry = entries[i];
    bool is_selected = (i == selected);

    move(y, 0);

    // Determine colors based on file type
    int name_color = ColorPairs::FOREGROUND_PAIR;
    int bg_color = ColorPairs::BACKGROUND_PAIR;
    bool use_bold = false;
    std::string icon;

    // **FIX: Use IconProvider instead of hardcoded icons**
    if (entry.is_directory)
    {
      if (entry.name == "..")
      {
        icon = icon_provider_.getParentIcon();
        name_color = ColorPairs::UI_ACCENT;
        use_bold = true;
      }
      else
      {
        icon = icon_provider_.getDirectoryIcon();
        name_color = ColorPairs::UI_INFO;
        use_bold = true;
      }
    }
    else if (entry.is_executable)
    {
      icon = icon_provider_.getExecutableIcon();
      name_color = ColorPairs::UI_SUCCESS;
      use_bold = true;
    }
    else if (entry.is_symlink)
    {
      icon = icon_provider_.getSymlinkIcon();
      name_color = ColorPairs::UI_ACCENT;
    }
    else if (entry.is_hidden)
    {
      icon = icon_provider_.getHiddenIcon();
      name_color = ColorPairs::STATE_DISABLED;
    }
    else
    {
      // Use file extension-based icon
      icon = icon_provider_.getFileIcon(entry.name);
    }

    // Selection styling
    if (is_selected)
    {
      attron(COLOR_PAIR(ColorPairs::STATE_SELECTED));
      printw(" %s ", icon.c_str());

      if (use_bold)
      {
        attron(COLOR_PAIR(name_color) | A_BOLD | A_REVERSE);
      }
      else
      {
        attron(COLOR_PAIR(name_color) | A_REVERSE);
      }
    }
    else
    {
      attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
      printw(" %s ", icon.c_str());
      attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

      if (use_bold)
      {
        attron(COLOR_PAIR(name_color) | A_BOLD);
      }
      else
      {
        attron(COLOR_PAIR(name_color));
      }
    }

    // Calculate max name width
    int max_name_width = width_ - 18;
    std::string display_name = entry.name;

    if (display_name.length() > static_cast<size_t>(max_name_width))
    {
      display_name = display_name.substr(0, max_name_width - 3) + "...";
    }

    printw("%s", display_name.c_str());

    // Calculate position for size (right-aligned)
    int current_x = getcurx(stdscr);
    int size_width = 11;
    int target_x = width_ - size_width;

    // Fill space between name and size
    if (is_selected)
    {
      for (int p = current_x; p < target_x; ++p)
      {
        addch(' ');
      }
    }
    else
    {
      if (use_bold)
      {
        attroff(COLOR_PAIR(name_color) | A_BOLD);
      }
      else
      {
        attroff(COLOR_PAIR(name_color));
      }

      for (int p = current_x; p < target_x; ++p)
      {
        addch(' ');
      }
    }

    // Print size info
    if (is_selected)
    {
      attroff(COLOR_PAIR(name_color) | A_REVERSE | A_BOLD);
      attron(COLOR_PAIR(ColorPairs::STATE_SELECTED));
    }

    attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));

    if (!entry.is_directory && entry.name != "..")
    {
      printw("%10s", formatSize(entry.size).c_str());
    }
    else if (entry.is_directory && entry.name != "..")
    {
      printw("     <DIR>");
    }
    else
    {
      printw("          ");
    }

    attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));

    if (is_selected)
    {
      addch(' ');
      attroff(COLOR_PAIR(ColorPairs::STATE_SELECTED));
    }

    clrtoeol();
  }

  // Clear remaining viewport
  for (int y = start_y + (visible_end - scroll); y < height_ - 2; ++y)
  {
    move(y, 0);
    clrtoeol();
  }
}

void BrowserRenderer::renderStatus(const FileBrowser &browser)
{
  int status_y = height_ - 2;

  // Subtle separator
  attron(COLOR_PAIR(ColorPairs::UI_BORDER));
  move(status_y - 1, 0);
  for (int i = 0; i < width_; ++i)
  {
    addch(ACS_HLINE);
  }
  attroff(COLOR_PAIR(ColorPairs::UI_BORDER));

  // Status line
  if (browser.hasError())
  {
    move(status_y, 0);
    attron(COLOR_PAIR(ColorPairs::UI_ERROR));
    printw(" ! ");
    attron(A_BOLD);
    printw("%s", browser.getErrorMessage().c_str());
    attroff(A_BOLD);
    clrtoeol();
    attroff(COLOR_PAIR(ColorPairs::UI_ERROR));
  }
  else
  {
    move(status_y, 0);
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR));

    size_t selected = browser.getSelectedIndex() + 1;
    size_t total = browser.getTotalEntries();

    // Position indicator
    printw(" ");
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE) | A_BOLD);
    printw("%zu", selected);
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE) | A_BOLD);
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR));
    printw("/");
    attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
    printw("%zu", total);
    attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR));

    // Flags
    if (browser.getShowHidden())
    {
      printw("  ");
      attron(COLOR_PAIR(ColorPairs::UI_WARNING));
      printw("[hidden]");
      attroff(COLOR_PAIR(ColorPairs::UI_WARNING));
      attron(COLOR_PAIR(ColorPairs::STATUS_BAR));
    }

    // Sort mode
    printw("  ");
    attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));
    printw("sort:");
    attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR));
    printw(" ");
    attron(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE));
    switch (browser.getSortMode())
    {
    case FileBrowser::SortMode::NAME:
      printw("name");
      break;
    case FileBrowser::SortMode::SIZE:
      printw("size");
      break;
    case FileBrowser::SortMode::DATE:
      printw("date");
      break;
    case FileBrowser::SortMode::TYPE:
      printw("type");
      break;
    }
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR_ACTIVE));

    clrtoeol();
    attroff(COLOR_PAIR(ColorPairs::STATUS_BAR));
  }

  // Help line - cleaner layout
  status_y++;
  move(status_y, 0);

  attron(COLOR_PAIR(ColorPairs::UI_SECONDARY));

  // Group related commands
  printw(" ");
  attron(A_BOLD);
  printw("j");
  attroff(A_BOLD);
  printw("/");
  attron(A_BOLD);
  printw("k");
  attroff(A_BOLD);
  printw(" move");

  printw("  ");
  attron(A_BOLD);
  printw("Enter");
  attroff(A_BOLD);
  printw(" open");

  printw("  ");
  attron(A_BOLD);
  printw("h");
  attroff(A_BOLD);
  printw(" back");

  printw("  ");
  attron(A_BOLD);
  printw(".");
  attroff(A_BOLD);
  printw(" hidden");

  printw("  ");
  attron(A_BOLD);
  printw("s");
  attroff(A_BOLD);
  printw(" sort");

  printw("  ");
  attron(A_BOLD);
  printw("q");
  attroff(A_BOLD);
  printw(" quit");

  clrtoeol();
  attroff(COLOR_PAIR(ColorPairs::UI_SECONDARY));
}

std::string BrowserRenderer::formatSize(uintmax_t size) const
{
  const char *units[] = {"B", "K", "M", "G", "T"};
  int unit = 0;
  double s = static_cast<double>(size);

  while (s >= 1024.0 && unit < 4)
  {
    s /= 1024.0;
    unit++;
  }

  std::ostringstream oss;
  if (unit == 0)
  {
    oss << size << units[unit];
  }
  else
  {
    oss << std::fixed << std::setprecision(1) << s << units[unit];
  }

  return oss.str();
}