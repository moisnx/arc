// editor_validation.h - Add this new header for validation utilities
#ifndef EDITOR_VALIDATION_H
#define EDITOR_VALIDATION_H

#include <sstream>
#include <string>

// Validation result that tells us exactly what's wrong
struct ValidationResult
{
  bool valid;
  std::string error;

  ValidationResult() : valid(true) {}
  ValidationResult(const std::string &err) : valid(false), error(err) {}

  operator bool() const { return valid; }
};

// Snapshot of editor state for comparison
struct EditorSnapshot
{
  int lineCount;
  int cursorLine;
  int cursorCol;
  int viewportTop;
  int viewportLeft;
  size_t bufferSize;
  std::string firstLine;
  std::string lastLine;
  std::string cursorLineContent;

  std::string toString() const
  {
    std::ostringstream oss;
    oss << "Lines: " << lineCount << " | Cursor: (" << cursorLine << ","
        << cursorCol << ")" << " | Viewport: (" << viewportTop << ","
        << viewportLeft << ")" << " | BufferSize: " << bufferSize;
    return oss.str();
  }
};

#endif // EDITOR_VALIDATION_H