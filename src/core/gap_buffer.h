#ifndef GAP_BUFFER_H
#define GAP_BUFFER_H

#include <string>
#include <utility>
#include <vector>

class GapBuffer
{
public:
  // Constructor
  GapBuffer();
  explicit GapBuffer(const std::string &initialText);

  // File operations
  bool loadFromFile(const std::string &filename);
  bool saveToFile(const std::string &filename) const;
  void clear();

  // Line-based interface (for compatibility with existing Editor code)
  int getLineCount() const;
  std::string getLine(int lineNum) const;
  size_t getLineLength(int lineNum) const;
  bool isEmpty() const;

  // Position conversion utilities
  size_t lineColToPos(int line, int col) const;
  std::pair<int, int> posToLineCol(size_t pos) const;

  // Editing operations
  void insertChar(size_t pos, char c);
  void insertText(size_t pos, const std::string &text);
  void deleteChar(size_t pos);
  void deleteRange(size_t start, size_t length);

  // Line-based editing (for easier migration)
  void insertLine(int lineNum, const std::string &line);
  void deleteLine(int lineNum);
  void replaceLine(int lineNum, const std::string &newLine);

  // Get text content
  std::string getText() const;
  std::string getTextRange(size_t start, size_t length) const;

  // Statistics
  size_t size() const;
  size_t getBufferSize() const;

private:
  std::vector<char> buffer;
  size_t gapStart;
  size_t gapSize;

  // Line index cache for performance
  mutable std::vector<size_t> lineIndex;
  mutable bool lineIndexDirty;

  // Internal operations
  void moveGapTo(size_t pos);
  void expandGap(size_t minSize = 1024);
  void rebuildLineIndex() const;
  void invalidateLineIndex();

  // Utilities
  size_t gapEnd() const { return gapStart + gapSize; }
  size_t textSize() const { return buffer.size() - gapSize; }
  char charAt(size_t pos) const;

  // Constants
  static const size_t DEFAULT_GAP_SIZE = 1024;
  static const size_t MIN_GAP_SIZE = 512;
};

#endif // GAP_BUFFER_H