// src/features/syntax_highlighter.h
#pragma once

#include "src/core/gap_buffer.h"
#include <regex>
#include <string>
#include <vector>

struct ColorSpan
{
  int start;
  int end;
  int colorPair;
  int attribute;
  int priority;
};

struct SyntaxRule
{
  std::regex pattern;
  int colorPair;
  int attribute;
  int priority;
};

class SyntaxHighlighter
{
public:
  SyntaxHighlighter();

  // Core functionality
  void setLanguage(const std::string &extension);
  std::vector<ColorSpan> getHighlightSpans(const std::string &line, int lineNum,
                                           const GapBuffer &buffer) const;

private:
  // Language rule loaders
  void loadGenericTextRules();
  void loadCppRules();
  void loadPythonRules();
  void loadMarkdownRules(); // NEW: Markdown syntax rules

  // Multi-line construct detection
  bool isInMultiLineComment(int lineIndex,
                            const std::vector<std::string> &lines) const;
  bool isInTripleQuote(int lineIndex,
                       const std::vector<std::string> &lines) const;
  bool
  isInMarkdownCodeBlock(int lineIndex, // NEW: Markdown code block detection
                        const std::vector<std::string> &lines) const;
  void handleMultiLineStarts(const std::string &line,
                             std::vector<ColorSpan> &spans) const;

  // Data members
  std::vector<SyntaxRule> rules;
  std::string currentLanguage;
};