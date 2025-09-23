#include "gap_buffer.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

const size_t GapBuffer::DEFAULT_GAP_SIZE;
const size_t GapBuffer::MIN_GAP_SIZE;
GapBuffer::GapBuffer()
    : gapStart(0), gapSize(DEFAULT_GAP_SIZE), lineIndexDirty(true)
{
  buffer.resize(DEFAULT_GAP_SIZE);
}

GapBuffer::GapBuffer(const std::string &initialText) : GapBuffer()
{
  if (!initialText.empty())
  {
    insertText(0, initialText);
  }
}

bool GapBuffer::loadFromFile(const std::string &filename)
{
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open())
  {
    return false;
  }

  clear();

  std::string line;
  bool first = true;
  while (std::getline(file, line))
  {
    if (!first)
    {
      insertChar(textSize(), '\n');
    }
    first = false;

    // Remove carriage returns
    if (!line.empty() && line.back() == '\r')
    {
      line.pop_back();
    }

    insertText(textSize(), line);
  }

  // If file was empty, ensure we have at least one empty line
  if (textSize() == 0)
  {
    insertChar(0, '\n');
  }

  return true;
}

bool GapBuffer::saveToFile(const std::string &filename) const
{
  std::ofstream file(filename);
  if (!file.is_open())
  {
    return false;
  }

  file << getText();
  return file.good();
}

void GapBuffer::clear()
{
  buffer.clear();
  buffer.resize(DEFAULT_GAP_SIZE);
  gapStart = 0;
  gapSize = DEFAULT_GAP_SIZE;
  invalidateLineIndex();
}

int GapBuffer::getLineCount() const
{
  if (lineIndexDirty)
  {
    rebuildLineIndex();
  }
  return static_cast<int>(lineIndex.size());
}

std::string GapBuffer::getLine(int lineNum) const
{
  if (lineIndexDirty)
  {
    rebuildLineIndex();
  }

  if (lineNum < 0 || lineNum >= static_cast<int>(lineIndex.size()))
  {
    return "";
  }

  size_t lineStart = lineIndex[lineNum];
  size_t lineEnd;

  if (lineNum + 1 < static_cast<int>(lineIndex.size()))
  {
    lineEnd = lineIndex[lineNum + 1] - 1; // -1 to exclude the newline
  }
  else
  {
    lineEnd = textSize();
  }

  if (lineEnd <= lineStart)
  {
    return "";
  }

  std::string result;
  result.reserve(lineEnd - lineStart);

  for (size_t pos = lineStart; pos < lineEnd; ++pos)
  {
    result += charAt(pos);
  }

  return result;
}

size_t GapBuffer::getLineLength(int lineNum) const
{
  if (lineIndexDirty)
  {
    rebuildLineIndex();
  }

  if (lineNum < 0 || lineNum >= static_cast<int>(lineIndex.size()))
  {
    return 0;
  }

  size_t lineStart = lineIndex[lineNum];
  size_t lineEnd;

  if (lineNum + 1 < static_cast<int>(lineIndex.size()))
  {
    lineEnd = lineIndex[lineNum + 1] - 1; // -1 to exclude the newline
  }
  else
  {
    lineEnd = textSize();
  }

  return (lineEnd > lineStart) ? lineEnd - lineStart : 0;
}

bool GapBuffer::isEmpty() const { return textSize() == 0; }

size_t GapBuffer::lineColToPos(int line, int col) const
{
  if (lineIndexDirty)
  {
    rebuildLineIndex();
  }

  if (line < 0 || line >= static_cast<int>(lineIndex.size()))
  {
    return textSize();
  }

  size_t lineStart = lineIndex[line];
  size_t maxCol = getLineLength(line);

  size_t actualCol = std::min(static_cast<size_t>(std::max(0, col)), maxCol);
  return lineStart + actualCol;
}

std::pair<int, int> GapBuffer::posToLineCol(size_t pos) const
{
  if (lineIndexDirty)
  {
    rebuildLineIndex();
  }

  pos = std::min(pos, textSize());

  // Binary search to find the line
  int line = 0;
  for (int i = static_cast<int>(lineIndex.size()) - 1; i >= 0; --i)
  {
    if (pos >= lineIndex[i])
    {
      line = i;
      break;
    }
  }

  int col = static_cast<int>(pos - lineIndex[line]);
  return std::make_pair(line, col);
}

void GapBuffer::insertChar(size_t pos, char c)
{
  pos = std::min(pos, textSize());
  moveGapTo(pos);

  if (gapSize == 0)
  {
    expandGap();
  }

  buffer[gapStart] = c;
  gapStart++;
  gapSize--;

  if (c == '\n')
  {
    invalidateLineIndex();
  }
}

void GapBuffer::insertText(size_t pos, const std::string &text)
{
  if (text.empty())
    return;

  pos = std::min(pos, textSize());
  moveGapTo(pos);

  if (gapSize < text.size())
  {
    expandGap(text.size());
  }

  bool hasNewlines = false;
  for (char c : text)
  {
    buffer[gapStart] = c;
    gapStart++;
    gapSize--;

    if (c == '\n')
    {
      hasNewlines = true;
    }
  }

  if (hasNewlines)
  {
    invalidateLineIndex();
  }
}

void GapBuffer::deleteChar(size_t pos) { deleteRange(pos, 1); }

void GapBuffer::deleteRange(size_t start, size_t length)
{
  if (length == 0)
    return;

  start = std::min(start, textSize());
  length = std::min(length, textSize() - start);

  if (length == 0)
    return;

  // Check if we're deleting newlines
  bool hasNewlines = false;
  for (size_t i = start; i < start + length; ++i)
  {
    if (charAt(i) == '\n')
    {
      hasNewlines = true;
      break;
    }
  }

  moveGapTo(start);
  gapSize += length;

  if (hasNewlines)
  {
    invalidateLineIndex();
  }
}

void GapBuffer::insertLine(int lineNum, const std::string &line)
{
  size_t pos = lineColToPos(lineNum, 0);
  insertText(pos, line + "\n");
}

void GapBuffer::deleteLine(int lineNum)
{
  if (lineNum < 0 || lineNum >= getLineCount())
    return;

  size_t lineStart = lineColToPos(lineNum, 0);
  size_t lineLength = getLineLength(lineNum);

  // Include the newline character if it exists
  if (lineNum < getLineCount() - 1)
  {
    lineLength++; // Include the newline
  }

  deleteRange(lineStart, lineLength);
}

void GapBuffer::replaceLine(int lineNum, const std::string &newLine)
{
  if (lineNum < 0 || lineNum >= getLineCount())
    return;

  // Get current state before any modifications
  size_t lineStart = lineColToPos(lineNum, 0);
  size_t lineLength = getLineLength(lineNum);

  // Move gap to line start
  moveGapTo(lineStart);

  // Expand gap if needed
  if (gapSize < newLine.length())
  {
    expandGap(newLine.length());
  }

  // Replace the line content directly
  gapSize += lineLength; // "Delete" old content by expanding gap

  // Insert new content
  for (char c : newLine)
  {
    buffer[gapStart] = c;
    gapStart++;
    gapSize--;
  }

  // Mark line index as dirty only once
  invalidateLineIndex();
}

std::string GapBuffer::getText() const
{
  std::string result;
  result.reserve(textSize());

  // Add text before gap
  for (size_t i = 0; i < gapStart; ++i)
  {
    result += buffer[i];
  }

  // Add text after gap
  for (size_t i = gapEnd(); i < buffer.size(); ++i)
  {
    result += buffer[i];
  }

  return result;
}

std::string GapBuffer::getTextRange(size_t start, size_t length) const
{
  start = std::min(start, textSize());
  length = std::min(length, textSize() - start);

  std::string result;
  result.reserve(length);

  for (size_t i = 0; i < length; ++i)
  {
    result += charAt(start + i);
  }

  return result;
}

size_t GapBuffer::size() const { return textSize(); }

size_t GapBuffer::getBufferSize() const { return buffer.size(); }

void GapBuffer::moveGapTo(size_t pos)
{
  if (pos == gapStart)
    return;

  if (pos < gapStart)
  {
    // Move gap left - move text from before new gap position to after gap
    size_t moveSize = gapStart - pos;
    size_t gapEndPos = gapEnd();

    // Use memmove to handle overlapping memory safely
    // Move text from [pos, gapStart) to [gapEndPos - moveSize, gapEndPos)
    std::memmove(&buffer[gapEndPos - moveSize], &buffer[pos], moveSize);

    gapStart = pos;
  }
  else
  {
    // Move gap right - move text from after gap to before new gap position
    size_t moveSize = pos - gapStart;
    size_t gapEndPos = gapEnd();

    // Move text from [gapEndPos, gapEndPos + moveSize) to [gapStart, gapStart +
    // moveSize)
    std::memmove(&buffer[gapStart], &buffer[gapEndPos], moveSize);

    gapStart = pos;
  }
}

void GapBuffer::expandGap(size_t minSize)
{
  size_t newGapSize = std::max(minSize, std::max(gapSize * 2, MIN_GAP_SIZE));
  size_t oldBufferSize = buffer.size();
  size_t newBufferSize = oldBufferSize + newGapSize - gapSize;

  // Create new buffer
  std::vector<char> newBuffer(newBufferSize);

  // Copy text before gap
  std::copy(buffer.begin(), buffer.begin() + gapStart, newBuffer.begin());

  // Copy text after gap
  size_t afterGapSize = oldBufferSize - gapEnd();
  std::copy(buffer.begin() + gapEnd(), buffer.end(),
            newBuffer.begin() + gapStart + newGapSize);

  buffer = std::move(newBuffer);
  gapSize = newGapSize;
}

void GapBuffer::rebuildLineIndex() const
{
  lineIndex.clear();
  lineIndex.push_back(0); // First line always starts at position 0

  for (size_t pos = 0; pos < textSize(); ++pos)
  {
    if (charAt(pos) == '\n')
    {
      lineIndex.push_back(pos + 1);
    }
  }

  // If the buffer doesn't end with a newline, we still have the last line
  // The line index is already correct as we added positions after each newline

  lineIndexDirty = false;
}

void GapBuffer::invalidateLineIndex() { lineIndexDirty = true; }

char GapBuffer::charAt(size_t pos) const
{
  if (pos >= textSize())
  {
    return '\0';
  }

  if (pos < gapStart)
  {
    return buffer[pos];
  }
  else
  {
    // Position is after gap, so add gap size to skip over it
    return buffer[pos + gapSize];
  }
}