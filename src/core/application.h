#ifndef APPLICATION_H
#define APPLICATION_H

#include <string>

class Application
{
public:
  static bool initialize();
  static void cleanup();
  static void setupMouse();
  static void cleanupMouse();

private:
  static bool initializeNcurses();
  static bool initializeThemes();
};

#endif // APPLICATION_H