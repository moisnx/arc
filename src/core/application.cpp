// src/core/application.cpp
#include "application.h"
#include "config_manager.h"
// #include "src/core/logger.h"
#include "src/ui/style_manager.h"
#include <filesystem>
#include <iostream>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

bool Application::initialize()

{

  if (!initializeNcurses())
  {
    return false;
  }
  if (!initializeThemes())
  {
    cleanup();
    return false;
  }
  return true;
}

void Application::cleanup()
{
  cleanupMouse();
  attrset(A_NORMAL);
  curs_set(1);
  endwin();
}

void Application::setupMouse()
{
  mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
#ifndef _WIN32
  printf("\033[?1003h");
  fflush(stdout);
#endif
}

void Application::cleanupMouse()
{
#ifndef _WIN32
  printf("\033[?1003l");
  fflush(stdout);
#endif
}

bool Application::initializeNcurses()
{
  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();
  curs_set(1);

#ifdef _WIN32
  nodelay(stdscr, FALSE);
  PDC_set_blink(FALSE);
  PDC_return_key_modifiers(TRUE);
  PDC_save_key_modifiers(TRUE);
  scrollok(stdscr, FALSE);
  leaveok(stdscr, FALSE);
  raw();
  meta(stdscr, TRUE);
  intrflush(stdscr, FALSE);
#else
  timeout(50);
#endif

  if (!has_colors())
  {
    endwin();
    std::cerr << "Error: Terminal does not support colors" << std::endl;
    return false;
  }

  if (start_color() == ERR)
  {
    endwin();
    std::cerr << "Error: Could not initialize colors" << std::endl;
    return false;
  }

  if (use_default_colors() == ERR)
  {
    assume_default_colors(COLOR_WHITE, COLOR_BLACK);
  }

  return true;
}

bool Application::initializeThemes()
{
  g_style_manager.initialize();

  ConfigManager::registerReloadCallback(
      []()
      {
        std::string active_theme = ConfigManager::getActiveTheme();
        std::string theme_file = ConfigManager::getThemeFile(active_theme);

        if (!theme_file.empty())
        {
          if (!g_style_manager.load_theme_from_file(theme_file))
          {
            std::cerr << "ERROR: Theme reload failed for: " << active_theme
                      << std::endl;
          }
        }
      });

  return true;
}