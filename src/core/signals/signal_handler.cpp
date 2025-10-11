#include "signal_handler.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <curses.h>
#else
#include <ncursesw/ncurses.h>
#endif

void terminate_gracefully(int sig)
{
  // Call ncurses cleanup functions
  curs_set(1);
  attrset(A_NORMAL);
  // endwin() is declared extern by ncurses/curses.h
  endwin();

  fprintf(stderr, "\nERROR: Caught fatal signal %d. Shutting down.\n", sig);

  // Re-raise the signal for core dump/debugging
  signal(sig, SIG_DFL);
  raise(sig);
}

void install_signal_handlers()
{
  signal(SIGSEGV, terminate_gracefully);
  signal(SIGABRT, terminate_gracefully);
  signal(SIGFPE, terminate_gracefully);
  signal(SIGILL, terminate_gracefully);
  signal(SIGTERM, terminate_gracefully);
  signal(SIGINT, terminate_gracefully);
}