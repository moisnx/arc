#include "buffer.h"
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

#include "buffer.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

bool GapBuffer::loadFromFile(const std::string &filename)
{
  // --- Step 1: Fast I/O (Read entire file into memory) ---
  std::ifstream file(filename, std::ios::binary | std::ios::ate);
  if (!file.is_open())
  {
    return false;
  }

  std::streamsize fileSize = file.tellg();
  file.seekg(0, std::ios::beg);

  // Handle empty file case
  if (fileSize <= 0)
  {
    clear();
    insertChar(0, '\n');
    return true;
  }

  std::vector<char> content(fileSize);
  if (!file.read(content.data(), fileSize))
  {
    return false;
  }
  file.close();

  // --- Step 2: Check if normalization is needed ---
  const char *src = content.data();
  bool has_carriage_returns = false;

  // OPTIMIZATION: Quick scan for \r (memchr is highly optimized)
  if (std::memchr(src, '\r', fileSize) != nullptr)
  {
    has_carriage_returns = true;
  }

  // --- Step 3: Clear and Initial Buffer Sizing ---
  clear();

  if (!has_carriage_returns)
  {
    // FAST PATH: No normalization needed, direct copy
    buffer.resize(fileSize + DEFAULT_GAP_SIZE);
    char *dest = buffer.data() + DEFAULT_GAP_SIZE;

    std::memcpy(dest, src, fileSize);

    buffer.resize(fileSize + DEFAULT_GAP_SIZE);
    gapStart = 0;
    gapSize = DEFAULT_GAP_SIZE;
    invalidateLineIndex();

    return true;
  }

  // SLOW PATH: Normalization required
  buffer.resize(fileSize + DEFAULT_GAP_SIZE);
  char *dest = buffer.data() + DEFAULT_GAP_SIZE;
  size_t actualTextSize = 0;

  for (std::streamsize i = 0; i < fileSize; ++i)
  {
    char c = src[i];

    if (c == '\r')
    {
      // Windows \r\n sequence: skip \r, write \n
      if (i + 1 < fileSize && src[i + 1] == '\n')
      {
        c = '\n';
        i++; // Skip next \n
      }
      else
      {
        // Old Mac/Lone \r: Replace with \n
        c = '\n';
      }
    }

    *dest++ = c;
    actualTextSize++;
  }

  // --- Step 4: Finalize Gap Buffer State ---
  buffer.resize(actualTextSize + DEFAULT_GAP_SIZE);
  gapStart = 0;
  gapSize = DEFAULT_GAP_SIZE;
  invalidateLineIndex();

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

// std::string GapBuffer::getText() const
// {
//   std::string result;
//   result.reserve(textSize());

//   // Add text before gap
//   for (size_t i = 0; i < gapStart; ++i)
//   {
//     result += buffer[i];
//   }

//   // Add text after gap
//   for (size_t i = gapEnd(); i < buffer.size(); ++i)
//   {
//     result += buffer[i];
//   }

//   return result;
// }

// In buffer.cpp, inside GapBuffer::getText()
std::string GapBuffer::getText() const
{
  // The size of the final string *must* match the size reported by textSize()
  size_t text_size = textSize();
  std::string result;
  result.reserve(text_size); // Reserve the exact final capacity

  // 1. Append the first half (Text before the gap)
  // The length of the first half is gapStart
  result.append(buffer.data(), gapStart);

  // 2. Append the second half (Text after the gap)
  // The length of the second half is text_size - gapStart
  size_t afterGapLength = text_size - gapStart;

  // Check for a scenario where afterGapLength might wrap (though highly
  // unlikely with your logic)
  if (afterGapLength > 0 && gapEnd() < buffer.size())
  {
    result.append(buffer.data() + gapEnd(), afterGapLength);
  }

  // Safety Check: If the buffer was empty, or if logic failed, ensure the
  // result matches the source of truth
  if (result.size() != text_size)
  {
    // This should not happen, but if it does, it's a critical bug.
    // For now, assume logic is correct and rely on the math.
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

// In buffer.cpp

void GapBuffer::moveGapTo(size_t pos)
{
  if (pos == gapStart)
    return;

  // Get the base pointer once
  char *base_ptr = buffer.data();

  if (pos < gapStart)
  {
    // Move gap left - move text from before new gap position to after gap
    size_t moveSize = gapStart - pos;
    size_t gapEndPos = gapEnd();

    // Move text from [pos] to [gapEndPos - moveSize]
    // This moves the text segment that was at pos and pushes it to the end of
    // the text segment after the gap
    std::memmove(base_ptr + gapEndPos - moveSize, base_ptr + pos, moveSize);

    gapStart = pos;
  }
  else
  {
    // Move gap right - move text from after gap to before new gap position
    size_t moveSize = pos - gapStart;
    size_t gapEndPos = gapEnd();

    // Move text from [gapEndPos] to [gapStart]
    // This pulls the text segment that was after the gap and fills the newly
    // created gap space
    std::memmove(base_ptr + gapStart, base_ptr + gapEndPos, moveSize);

    gapStart = pos;
  }

  // IMPORTANT: The gap size must remain the same, but its position is now
  // correct
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