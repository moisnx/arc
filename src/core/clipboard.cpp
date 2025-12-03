#include "clipboard.h"
#include <cstdio>
#include <string>

bool Clipboard::copyToSystemClipboard(const std::string &text)
{
  if (text.empty())
  {
    return false;
  }

#ifdef _WIN32
  // Windows clipboard implementation
  if (!OpenClipboard(nullptr))
  {
    return false;
  }

  EmptyClipboard();

  HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
  if (!hGlob)
  {
    CloseClipboard();
    return false;
  }

  char *pGlob = static_cast<char *>(GlobalLock(hGlob));
  if (pGlob)
  {
    memcpy(pGlob, text.c_str(), text.size() + 1);
    GlobalUnlock(hGlob);
  }

  SetClipboardData(CF_TEXT, hGlob);
  CloseClipboard();
  return true;

#else
  // Unix/Linux clipboard implementation
  // Try multiple clipboard methods for better compatibility

  // Method 1: xclip (most common)
  FILE *pipe = popen("xclip -selection clipboard -i 2>/dev/null", "w");
  if (pipe)
  {
    size_t written = fwrite(text.c_str(), 1, text.length(), pipe);
    int result = pclose(pipe);
    if (result == 0 && written == text.length())
    {
      return true;
    }
  }

  // Method 2: xsel (alternative)
  pipe = popen("xsel --clipboard --input 2>/dev/null", "w");
  if (pipe)
  {
    size_t written = fwrite(text.c_str(), 1, text.length(), pipe);
    int result = pclose(pipe);
    if (result == 0 && written == text.length())
    {
      return true;
    }
  }

  // Method 3: wl-copy (Wayland)
  pipe = popen("wl-copy 2>/dev/null", "w");
  if (pipe)
  {
    size_t written = fwrite(text.c_str(), 1, text.length(), pipe);
    int result = pclose(pipe);
    if (result == 0 && written == text.length())
    {
      return true;
    }
  }

  return false;
#endif
}

std::string Clipboard::getFromSystemClipboard()
{
#ifdef _WIN32
  // Windows clipboard implementation
  if (!OpenClipboard(nullptr))
  {
    return "";
  }

  HANDLE hData = GetClipboardData(CF_TEXT);
  if (!hData)
  {
    CloseClipboard();
    return "";
  }

  char *pData = static_cast<char *>(GlobalLock(hData));
  std::string text;
  if (pData)
  {
    text = std::string(pData);
    GlobalUnlock(hData);
  }

  CloseClipboard();
  return text;

#else
  // Unix/Linux clipboard implementation
  std::string result;

  // Try xclip first
  FILE *pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
  if (pipe)
  {
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
      result += buffer;
    }
    int exitCode = pclose(pipe);
    if (exitCode == 0 && !result.empty())
    {
      return result;
    }
  }

  // Try xsel
  result.clear();
  pipe = popen("xsel --clipboard --output 2>/dev/null", "r");
  if (pipe)
  {
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
      result += buffer;
    }
    int exitCode = pclose(pipe);
    if (exitCode == 0 && !result.empty())
    {
      return result;
    }
  }

  // Try wl-paste (Wayland)
  result.clear();
  pipe = popen("wl-paste 2>/dev/null", "r");
  if (pipe)
  {
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe))
    {
      result += buffer;
    }
    int exitCode = pclose(pipe);
    if (exitCode == 0 && !result.empty())
    {
      return result;
    }
  }

  return "";
#endif
}