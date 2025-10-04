// src/features/markdown_state.h

#pragma once

enum class MarkdownState
{
  DEFAULT = 0,          // Not inside any block
  IN_FENCED_CODE_BLOCK, // Inside ```...```
  IN_HTML_COMMENT,      // Inside IN_BLOCKQUOTE,           // Inside a > block
  // Add other multi-line states here (e.g., IN_MULTI_LINE_LIST)
  IN_BLOCKQUOTE
};