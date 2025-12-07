#include "history_manager.h"
#include "buffer.h"                          // Adjust path as needed
#include "src/features/syntax_highlighter.h" // Adjust path as needed
#include <algorithm>
#include <iostream>

HistoryManager::HistoryManager() {}

void HistoryManager::beginDeltaGroup(int initialLineCount,
                                     size_t initialBufferSize)
{
  currentDeltaGroup_ = DeltaGroup();
  currentDeltaGroup_.initialLineCount = initialLineCount;
  currentDeltaGroup_.initialBufferSize = initialBufferSize;
  currentDeltaGroup_.timestamp = std::chrono::steady_clock::now();
}

void HistoryManager::addDelta(const EditDelta &delta)
{
  currentDeltaGroup_.addDelta(delta);
}

bool HistoryManager::isCurrentGroupEmpty() const
{
  return currentDeltaGroup_.isEmpty();
}

std::chrono::steady_clock::time_point
HistoryManager::getCurrentGroupTimestamp() const
{
  return currentDeltaGroup_.timestamp;
}

void HistoryManager::commitDeltaGroup()
{
  if (currentDeltaGroup_.isEmpty())
  {
    return;
  }

  undoStack_.push(currentDeltaGroup_);

  // Clear redo stack on new edit
  while (!redoStack_.empty())
  {
    redoStack_.pop();
  }

  // Limit stack size
  while (undoStack_.size() > MAX_UNDO_LEVELS)
  {
    // Remove oldest (bottom of stack)
    std::stack<DeltaGroup> temp;
    bool first = true;
    while (!undoStack_.empty())
    {
      if (first)
      {
        first = false;
        undoStack_.pop(); // Discard oldest
      }
      else
      {
        temp.push(undoStack_.top());
        undoStack_.pop();
      }
    }
    while (!temp.empty())
    {
      undoStack_.push(temp.top());
      temp.pop();
    }
  }

  // Reset current group
  currentDeltaGroup_ = DeltaGroup();
}

void HistoryManager::markModified() { isModified_ = true; }

void HistoryManager::markSaved() { isModified_ = false; }

void HistoryManager::clear()
{
  while (!undoStack_.empty())
    undoStack_.pop();
  while (!redoStack_.empty())
    redoStack_.pop();
  currentDeltaGroup_ = DeltaGroup();
  isModified_ = false;
}

bool HistoryManager::undo(GapBuffer &buffer, int &cursorLine, int &cursorCol,
                          int &viewportTop, int &viewportLeft,
                          SyntaxHighlighter *highlighter)
{

  // If there is an active group forming (e.g., user typing), commit it first
  if (!currentDeltaGroup_.isEmpty())
  {
    commitDeltaGroup();
  }

  if (undoStack_.empty())
  {
    return false;
  }

  isUndoRedoing_ = true;

  // Get the delta group to undo
  DeltaGroup group = undoStack_.top();
  undoStack_.pop();

  // Track affected line range for incremental highlighting
  int minAffectedLine = buffer.getLineCount();

  // Apply deltas in REVERSE order
  for (auto it = group.deltas.rbegin(); it != group.deltas.rend(); ++it)
  {
    minAffectedLine =
        std::min(minAffectedLine, std::min(it->startLine, it->preCursorLine));
    applyDeltaReverse(buffer, *it, cursorLine, cursorCol, viewportTop,
                      viewportLeft, highlighter);
  }

  // Save to redo stack
  redoStack_.push(group);

  // Trigger Syntax Update
  if (highlighter)
  {
    highlighter->invalidateLineRange(minAffectedLine,
                                     buffer.getLineCount() - 1);
    highlighter->parseViewportOnly(buffer, viewportTop);
    highlighter->scheduleBackgroundParse(buffer);
  }

  isModified_ = true;
  isUndoRedoing_ = false;
  return true;
}

bool HistoryManager::redo(GapBuffer &buffer, int &cursorLine, int &cursorCol,
                          int &viewportTop, int &viewportLeft,
                          SyntaxHighlighter *highlighter)
{

  if (redoStack_.empty())
  {
    return false;
  }

  isUndoRedoing_ = true;

  // Get the delta group to redo
  DeltaGroup group = redoStack_.top();
  redoStack_.pop();

  int minAffectedLine = buffer.getLineCount();

  // Apply deltas in FORWARD order
  for (const auto &delta : group.deltas)
  {
    minAffectedLine = std::min(minAffectedLine,
                               std::min(delta.startLine, delta.preCursorLine));
    applyDeltaForward(buffer, delta, cursorLine, cursorCol, viewportTop,
                      viewportLeft, highlighter);
  }

  // Save to undo stack
  undoStack_.push(group);

  if (highlighter)
  {
    highlighter->invalidateLineRange(minAffectedLine,
                                     buffer.getLineCount() - 1);
    highlighter->parseViewportOnly(buffer, viewportTop);
    highlighter->scheduleBackgroundParse(buffer);
  }

  isModified_ = true;
  isUndoRedoing_ = false;
  return true;
}

// ====================================================================================
// Private Logic Implementation (Moved from Editor.cpp)
// ====================================================================================

void HistoryManager::validateCursorAndViewport(const GapBuffer &buffer,
                                               int &cursorLine, int &cursorCol,
                                               int &viewportTop,
                                               int &viewportLeft,
                                               int viewportHeight)
{
  // Basic clamping used during undo/redo replay
  if (cursorLine < 0)
    cursorLine = 0;
  if (cursorLine >= buffer.getLineCount())
    cursorLine = buffer.getLineCount() - 1;

  std::string line = buffer.getLine(cursorLine);
  if (cursorCol < 0)
    cursorCol = 0;
  if (cursorCol > (int)line.length())
    cursorCol = (int)line.length();

  // Minimal viewport adjustment (full logic usually remains in UI/Editor,
  // but we need basic sanity here to match recorded state)
  // Note: The recorded delta state usually dictates the viewport, so this is
  // just a safety fallback.
}

void HistoryManager::applyDeltaForward(GapBuffer &buffer,
                                       const EditDelta &delta, int &cursorLine,
                                       int &cursorCol, int &viewportTop,
                                       int &viewportLeft,
                                       SyntaxHighlighter *highlighter)
{

  // Restore cursor to PRE-edit position
  cursorLine = delta.preCursorLine;
  cursorCol = delta.preCursorCol;
  viewportTop = delta.preViewportTop;
  viewportLeft = delta.preViewportLeft;

  switch (delta.operation)
  {
  case EditDelta::INSERT_CHAR:
  case EditDelta::INSERT_TEXT:
  {
    std::string line = buffer.getLine(cursorLine);
    line.insert(cursorCol, delta.insertedContent);
    buffer.replaceLine(cursorLine, line);
    cursorCol += delta.insertedContent.length();
    break;
  }
  case EditDelta::DELETE_CHAR:
  case EditDelta::DELETE_TEXT:
  {
    if (delta.startLine == delta.endLine)
    {
      std::string line = buffer.getLine(delta.startLine);
      line.erase(delta.startCol, delta.deletedContent.length());
      buffer.replaceLine(delta.startLine, line);
    }
    else
    {
      std::string firstLine = buffer.getLine(delta.startLine);
      std::string lastLine = buffer.getLine(delta.endLine);
      std::string newLine =
          firstLine.substr(0, delta.startCol) + lastLine.substr(delta.endCol);
      buffer.replaceLine(delta.startLine, newLine);
      for (int i = delta.endLine; i > delta.startLine; i--)
      {
        buffer.deleteLine(i);
      }
    }
    break;
  }
  case EditDelta::SPLIT_LINE:
  {
    std::string line = buffer.getLine(cursorLine);
    std::string leftPart = line.substr(0, cursorCol);
    std::string rightPart = line.substr(cursorCol);
    buffer.replaceLine(cursorLine, leftPart);
    buffer.insertLine(cursorLine + 1, rightPart);
    cursorLine++;
    cursorCol = 0;
    break;
  }
  case EditDelta::JOIN_LINES:
  {
    if (delta.startLine + 1 < buffer.getLineCount())
    {
      std::string firstLine = buffer.getLine(delta.startLine);
      std::string secondLine = buffer.getLine(delta.startLine + 1);
      buffer.replaceLine(delta.startLine, firstLine + secondLine);
      buffer.deleteLine(delta.startLine + 1);
    }
    break;
  }
  case EditDelta::REPLACE_LINE:
  {
    if (!delta.insertedContent.empty())
    {
      buffer.replaceLine(delta.startLine, delta.insertedContent);
    }
    break;
  }
  }

  // Restore POST-edit cursor position
  cursorLine = delta.postCursorLine;
  cursorCol = delta.postCursorCol;
  viewportTop = delta.postViewportTop;
  viewportLeft = delta.postViewportLeft;

  buffer.invalidateLineIndex();

  // Notify Syntax Highlighter
  notifyTreeSitterEdit(delta, false, buffer, highlighter);
}

void HistoryManager::applyDeltaReverse(GapBuffer &buffer,
                                       const EditDelta &delta, int &cursorLine,
                                       int &cursorCol, int &viewportTop,
                                       int &viewportLeft,
                                       SyntaxHighlighter *highlighter)
{

  // Restore cursor to POST-edit position
  cursorLine = delta.postCursorLine;
  cursorCol = delta.postCursorCol;
  viewportTop = delta.postViewportTop;
  viewportLeft = delta.postViewportLeft;

  switch (delta.operation)
  {
  case EditDelta::INSERT_CHAR:
  case EditDelta::INSERT_TEXT:
  {
    std::string line = buffer.getLine(delta.startLine);
    if (delta.startCol + delta.insertedContent.length() <= line.length())
    {
      line.erase(delta.startCol, delta.insertedContent.length());
      buffer.replaceLine(delta.startLine, line);
    }
    break;
  }
  case EditDelta::DELETE_CHAR:
  case EditDelta::DELETE_TEXT:
  {
    if (delta.startLine == delta.endLine)
    {
      std::string line = buffer.getLine(delta.startLine);
      line.insert(delta.startCol, delta.deletedContent);
      buffer.replaceLine(delta.startLine, line);
    }
    else
    {
      // Multi-line restoration
      std::string currentLine = buffer.getLine(delta.startLine);
      std::string beforeInsert = currentLine.substr(0, delta.startCol);
      std::string afterInsert = currentLine.substr(delta.startCol);

      std::vector<std::string> linesToRestore;
      size_t pos = 0;
      size_t nextNewline;
      while ((nextNewline = delta.deletedContent.find('\n', pos)) !=
             std::string::npos)
      {
        linesToRestore.push_back(
            delta.deletedContent.substr(pos, nextNewline - pos));
        pos = nextNewline + 1;
      }
      if (pos < delta.deletedContent.length())
      {
        linesToRestore.push_back(delta.deletedContent.substr(pos));
      }

      if (!linesToRestore.empty())
      {
        buffer.replaceLine(delta.startLine, beforeInsert + linesToRestore[0]);
        for (size_t i = 1; i < linesToRestore.size(); ++i)
        {
          buffer.insertLine(delta.startLine + i, linesToRestore[i]);
        }
        int lastLineIdx = delta.startLine + linesToRestore.size() - 1;
        std::string lastLine = buffer.getLine(lastLineIdx);
        buffer.replaceLine(lastLineIdx, lastLine + afterInsert);
      }
    }
    break;
  }
  case EditDelta::SPLIT_LINE:
  {
    if (!delta.lineBeforeSplit.empty())
    {
      if (delta.startLine + 1 < buffer.getLineCount())
      {
        buffer.replaceLine(delta.startLine, delta.lineBeforeSplit);
        buffer.deleteLine(delta.startLine + 1);
      }
    }
    break;
  }
  case EditDelta::JOIN_LINES:
  {
    if (!delta.firstLineBeforeJoin.empty() &&
        !delta.secondLineBeforeJoin.empty())
    {
      buffer.replaceLine(delta.startLine, delta.firstLineBeforeJoin);
      buffer.insertLine(delta.startLine + 1, delta.secondLineBeforeJoin);
    }
    break;
  }
  case EditDelta::REPLACE_LINE:
  {
    if (!delta.deletedContent.empty())
    {
      buffer.replaceLine(delta.startLine, delta.deletedContent);
    }
    break;
  }
  }

  // Restore PRE-edit cursor position
  cursorLine = delta.preCursorLine;
  cursorCol = delta.preCursorCol;
  viewportTop = delta.preViewportTop;
  viewportLeft = delta.preViewportLeft;

  buffer.invalidateLineIndex();

  // Notify Syntax Highlighter
  notifyTreeSitterEdit(delta, true, buffer, highlighter);
}

void HistoryManager::notifyTreeSitterEdit(const EditDelta &delta,
                                          bool isReverse, GapBuffer &buffer,
                                          SyntaxHighlighter *highlighter)
{
  if (!highlighter)
    return;

  size_t start_byte = buffer.lineColToPos(delta.startLine, delta.startCol);

  if (isReverse)
  {
    // Undoing: Reverse the logic
    switch (delta.operation)
    {
    case EditDelta::INSERT_CHAR:
    case EditDelta::INSERT_TEXT:
      highlighter->notifyEdit(start_byte, 0, delta.insertedContent.length(),
                              delta.startLine, delta.startCol, delta.startLine,
                              delta.startCol, delta.postCursorLine,
                              delta.postCursorCol);
      break;
    case EditDelta::DELETE_CHAR:
    case EditDelta::DELETE_TEXT:
      highlighter->notifyEdit(start_byte, delta.deletedContent.length(), 0,
                              delta.startLine, delta.startCol, delta.endLine,
                              delta.endCol, delta.startLine, delta.startCol);
      break;
    case EditDelta::SPLIT_LINE:
      highlighter->notifyEdit(start_byte, 0, 1, delta.startLine, delta.startCol,
                              delta.startLine, delta.startCol,
                              delta.startLine + 1, 0);
      break;
    case EditDelta::JOIN_LINES:
      highlighter->notifyEdit(start_byte, 1, 0, delta.startLine, delta.startCol,
                              delta.startLine + 1, 0, delta.startLine,
                              delta.startCol);
      break;
    default:
      break;
    }
  }
  else
  {
    // Redoing: Standard logic
    switch (delta.operation)
    {
    case EditDelta::INSERT_CHAR:
    case EditDelta::INSERT_TEXT:
      highlighter->notifyEdit(start_byte, delta.insertedContent.length(), 0,
                              delta.startLine, delta.startCol,
                              delta.postCursorLine, delta.postCursorCol,
                              delta.startLine, delta.startCol);
      break;
    case EditDelta::DELETE_CHAR:
    case EditDelta::DELETE_TEXT:
      highlighter->notifyEdit(start_byte, 0, delta.deletedContent.length(),
                              delta.startLine, delta.startCol, delta.startLine,
                              delta.startCol, delta.endLine, delta.endCol);
      break;
    case EditDelta::SPLIT_LINE:
      highlighter->notifyEdit(start_byte, 1, 0, delta.startLine, delta.startCol,
                              delta.startLine + 1, 0, delta.startLine,
                              delta.startCol);
      break;
    case EditDelta::JOIN_LINES:
      highlighter->notifyEdit(start_byte, 0, 1, delta.startLine, delta.startCol,
                              delta.startLine, delta.startCol,
                              delta.startLine + 1, 0);
      break;
    default:
      break;
    }
  }
}

size_t HistoryManager::getUndoMemoryUsage() const
{
  size_t total = 0;
  std::stack<DeltaGroup> temp = undoStack_;
  while (!temp.empty())
  {
    total += temp.top().getMemorySize();
    temp.pop();
  }
  return total;
}

size_t HistoryManager::getRedoMemoryUsage() const
{
  size_t total = 0;
  std::stack<DeltaGroup> temp = redoStack_;
  while (!temp.empty())
  {
    total += temp.top().getMemorySize();
    temp.pop();
  }
  return total;
}