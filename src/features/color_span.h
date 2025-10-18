#pragma once

/**
 * ColorSpan - Represents a syntax-highlighted span of text
 *
 * Used by both SyntaxHighlighter and InjectionManager to avoid circular
 * dependencies
 */
struct ColorSpan
{
  int start;     // Start column
  int end;       // End column
  int colorPair; // ncurses color pair ID
  int attribute; // ncurses attributes (A_BOLD, etc.)
  int priority;  // Higher priority spans override lower ones
};