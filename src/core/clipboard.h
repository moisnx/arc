#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include <string>
#ifdef _WIN32
#include <Windows.h>
#endif

class Clipboard
{
public:
  static bool copyToSystemClipboard(const std::string &text);
  static std::string getFromSystemClipboard();
};

#endif