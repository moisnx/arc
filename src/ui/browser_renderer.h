#ifndef BROWSER_RENDERER_H
#define BROWSER_RENDERER_H

#include "../core/file_browser.h"
#include "src/ui/icon_provider.h"

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

class BrowserRenderer
{
public:
  BrowserRenderer();

  void render(const FileBrowser &browser);
  size_t getViewportHeight() const { return viewport_height_; }
  int getFileTypeColor(const std::string &filename) const;
  void setIconStyle(IconStyle style) { icon_provider_ = IconProvider(style); }

  IconStyle getIconStyle() const { return icon_provider_.getStyle(); }

private:
  int height_, width_;
  size_t viewport_height_;

  void renderHeader(const FileBrowser &browser);
  void renderFileList(const FileBrowser &browser);
  void renderStatus(const FileBrowser &browser);

  std::string formatSize(uintmax_t size) const;
  std::string getIcon(const FileBrowser::FileEntry &entry) const;
  // std::string getIcon(const FileBrowser::FileEntry &entry) const;
  IconProvider icon_provider_;
};

#endif