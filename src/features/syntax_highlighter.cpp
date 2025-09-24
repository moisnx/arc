// src/features/syntax_highlighter.cpp
#include "syntax_highlighter.h"
#include "src/ui/theme_manager.h"
#include <algorithm>
#ifdef _WIN32
#include <curses.h> // pdcurses on Windows
#else
#include <ncurses.h> // ncurses on Unix-like systems
#endif
#include <iostream>
#include <unordered_set>

// Constructor
SyntaxHighlighter::SyntaxHighlighter()
{
  // No state tracking needed
}

void SyntaxHighlighter::loadGenericTextRules()
{
  rules.clear();
  rules.push_back({std::regex(R"(#.*)"), COMMENT, 0, 100});
  rules.push_back({std::regex(R"(//.*)"), COMMENT, 0, 100});
  rules.push_back({std::regex(R"(\/\*.*?\*\/)"), COMMENT, 0, 90});
  rules.push_back(
      {std::regex(R"("[^"\\]*(?:\\.[^"\\]*)*")"), STRING_LITERAL, 0, 80});
  rules.push_back(
      {std::regex(R"(\b\d+(\.\d*)?([eE][+-]?\d+)?\b)"), NUMBER, 0, 70});
}

void SyntaxHighlighter::loadCppRules()
{
  rules.clear();

  std::unordered_set<std::string> cppKeywords = {
      "alignas",       "alignof",     "and",
      "and_eq",        "asm",         "auto",
      "bitand",        "bitor",       "bool",
      "break",         "case",        "catch",
      "char",          "char8_t",     "char16_t",
      "char32_t",      "class",       "compl",
      "concept",       "const",       "consteval",
      "constexpr",     "constinit",   "const_cast",
      "continue",      "co_await",    "co_return",
      "co_yield",      "decltype",    "default",
      "delete",        "do",          "double",
      "dynamic_cast",  "else",        "enum",
      "explicit",      "export",      "extern",
      "false",         "float",       "for",
      "friend",        "goto",        "if",
      "inline",        "int",         "long",
      "mutable",       "namespace",   "new",
      "noexcept",      "not",         "not_eq",
      "nullptr",       "operator",    "or",
      "or_eq",         "private",     "protected",
      "public",        "register",    "reinterpret_cast",
      "requires",      "return",      "short",
      "signed",        "sizeof",      "static",
      "static_assert", "static_cast", "struct",
      "switch",        "template",    "this",
      "thread_local",  "throw",       "true",
      "try",           "typedef",     "typeid",
      "typename",      "union",       "unsigned",
      "using",         "virtual",     "void",
      "volatile",      "wchar_t",     "while",
      "xor",           "xor_eq",      "final",
      "override"};

  std::unordered_set<std::string> stdTypes = {"std",
                                              "string",
                                              "vector",
                                              "map",
                                              "unordered_map",
                                              "set",
                                              "unordered_set",
                                              "list",
                                              "deque",
                                              "array",
                                              "tuple",
                                              "pair",
                                              "shared_ptr",
                                              "unique_ptr",
                                              "weak_ptr",
                                              "function",
                                              "thread",
                                              "mutex",
                                              "condition_variable",
                                              "atomic",
                                              "future",
                                              "promise",
                                              "optional",
                                              "variant",
                                              "any",
                                              "size_t",
                                              "ptrdiff_t",
                                              "nullptr_t",
                                              "int8_t",
                                              "int16_t",
                                              "int32_t",
                                              "int64_t",
                                              "uint8_t",
                                              "uint16_t",
                                              "uint32_t",
                                              "uint64_t",
                                              "intptr_t",
                                              "uintptr_t"};

  // Preprocessor directives
  rules.push_back({std::regex(R"(^\s*#\s*include\s*[<"][^>"]*[>"])"),
                   PREPROCESSOR_INCLUDE, A_BOLD, 95});
  rules.push_back(
      {std::regex(R"(^\s*#\s*define\s+\w+)"), PREPROCESSOR_DEFINE, A_BOLD, 94});
  rules.push_back({std::regex(R"(^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b)"),
                   PREPROCESSOR, A_BOLD, 93});
  rules.push_back({std::regex(R"(^\s*#\s*\w+)"), PREPROCESSOR, A_BOLD, 92});

  // Comments
  rules.push_back({std::regex(R"(//.*)"), COMMENT, 0, 100});

  // Keywords
  std::string keywordPattern = "\\b(";
  for (const auto &keyword : cppKeywords)
  {
    keywordPattern += keyword + "|";
  }
  keywordPattern.pop_back();
  keywordPattern += ")\\b";
  rules.push_back({std::regex(keywordPattern), KEYWORD, A_BOLD, 90});

  // Types
  std::string typePattern = "\\b(";
  for (const auto &type : stdTypes)
  {
    typePattern += type + "|";
  }
  typePattern.pop_back();
  typePattern += ")\\b";
  rules.push_back({std::regex(typePattern), CPP_TYPE, A_BOLD, 85});

  // Function calls
  rules.push_back(
      {std::regex(R"(\b[a-zA-Z_][a-zA-Z0-9_]*(?=\s*\())"), FUNCTION, 0, 80});

  // Class names
  rules.push_back({std::regex(R"(\b[A-Z][a-zA-Z0-9_]*\b(?!\s*\())"), CLASS_NAME,
                   A_BOLD, 75});

  // Strings
  rules.push_back({std::regex(R"(R"([^(]*)\((?:[^)]|\)[^"]*)*\)\1")"),
                   STRING_LITERAL, 0, 70});
  rules.push_back(
      {std::regex(R"("[^"\\]*(?:\\.[^"\\]*)*")"), STRING_LITERAL, 0, 70});
  rules.push_back(
      {std::regex(R"('[^'\\]*(?:\\.[^'\\]*)*')"), STRING_LITERAL, 0, 70});

  // Numbers
  rules.push_back(
      {std::regex(R"(\b0[xX][0-9a-fA-F]+[uUlL]*\b)"), NUMBER, 0, 60});
  rules.push_back({std::regex(R"(\b0[bB][01]+[uUlL]*\b)"), NUMBER, 0, 60});
  rules.push_back({std::regex(R"(\b0[0-7]+[uUlL]*\b)"), NUMBER, 0, 60});
  rules.push_back(
      {std::regex(R"(\b\d+(\.\d*)?([eE][+-]?\d+)?[fFlL]*\b)"), NUMBER, 0, 60});
  rules.push_back(
      {std::regex(R"(\b\.\d+([eE][+-]?\d+)?[fFlL]*\b)"), NUMBER, 0, 60});

  // Operators
  rules.push_back(
      {std::regex(
           R"(\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||::|\.\*|->|\*=|/=|%=|\+=|-=|<<=|>>=|&=|\|=|\^=)"),
       OPERATOR, 0, 10});
  rules.push_back({std::regex(R"([+\-*/%=<>!&|^~?:])"), OPERATOR, 0, 10});
}

void SyntaxHighlighter::loadPythonRules()
{
  rules.clear();

  std::unordered_set<std::string> pythonKeywords = {
      "False",  "None",   "True",    "and",      "as",       "assert", "async",
      "await",  "break",  "class",   "continue", "def",      "del",    "elif",
      "else",   "except", "finally", "for",      "from",     "global", "if",
      "import", "in",     "is",      "lambda",   "nonlocal", "not",    "or",
      "pass",   "raise",  "return",  "try",      "while",    "with",   "yield"};

  std::unordered_set<std::string> pythonBuiltins = {
      "abs",         "all",        "any",        "ascii",      "bin",
      "bool",        "bytearray",  "bytes",      "callable",   "chr",
      "classmethod", "compile",    "complex",    "delattr",    "dict",
      "dir",         "divmod",     "enumerate",  "eval",       "exec",
      "filter",      "float",      "format",     "frozenset",  "getattr",
      "globals",     "hasattr",    "hash",       "help",       "hex",
      "id",          "input",      "int",        "isinstance", "issubclass",
      "iter",        "len",        "list",       "locals",     "map",
      "max",         "memoryview", "min",        "next",       "object",
      "oct",         "open",       "ord",        "pow",        "print",
      "property",    "range",      "repr",       "reversed",   "round",
      "set",         "setattr",    "slice",      "sorted",     "staticmethod",
      "str",         "sum",        "super",      "tuple",      "type",
      "vars",        "zip",        "__import__", "self",       "cls"};

  // Comments
  rules.push_back({std::regex(R"(#.*)"), COMMENT, 0, 100});

  // Decorators
  rules.push_back(
      {std::regex(R"(@[a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z_][a-zA-Z0-9_]*)*)"),
       PYTHON_DECORATOR, A_BOLD, 95});

  // Keywords
  std::string keywordPattern = "\\b(";
  for (const auto &keyword : pythonKeywords)
  {
    keywordPattern += keyword + "|";
  }
  keywordPattern.pop_back();
  keywordPattern += ")\\b";
  rules.push_back({std::regex(keywordPattern), PYTHON_KEYWORD, A_BOLD, 90});

  // Function definitions (only the name)
  rules.push_back({std::regex(R"(\bdef\s+([a-zA-Z_][a-zA-Z0-9_]*))"),
                   PYTHON_FUNCTION_DEF, A_BOLD, 85});

  // Class definitions (only the name)
  rules.push_back({std::regex(R"(\bclass\s+([A-Za-z_][a-zA-Z0-9_]*))"),
                   PYTHON_CLASS_DEF, A_BOLD, 85});

  // Builtins
  std::string builtinPattern = "\\b(";
  for (const auto &builtin : pythonBuiltins)
  {
    builtinPattern += builtin + "|";
  }
  builtinPattern.pop_back();
  builtinPattern += ")\\b";
  rules.push_back({std::regex(builtinPattern), PYTHON_BUILTIN, A_BOLD, 80});

  // Strings (including triple quotes on single line)
  rules.push_back({std::regex("\"\"\".*?\"\"\""), STRING_LITERAL, 0, 78});
  rules.push_back({std::regex("'''.*?'''"), STRING_LITERAL, 0, 78});
  rules.push_back({std::regex(R"("([^"\\]|\\.)*")"), STRING_LITERAL, 0, 75});
  rules.push_back({std::regex(R"('([^'\\]|\\.)*')"), STRING_LITERAL, 0, 75});

  // Function calls
  rules.push_back(
      {std::regex(R"([a-zA-Z_][a-zA-Z0-9_]*(?=\s*\())"), FUNCTION, 0, 70});

  // Numbers
  rules.push_back({std::regex(R"(\b0x[0-9a-fA-F]+\b)"), NUMBER, 0, 60});
  rules.push_back({std::regex(R"(\b0b[01]+\b)"), NUMBER, 0, 60});
  rules.push_back({std::regex(R"(\b0o[0-7]+\b)"), NUMBER, 0, 60});
  rules.push_back({std::regex(R"(\b\d+\.?\d*\b)"), NUMBER, 0, 60});

  // Operators
  rules.push_back({std::regex(R"(\*\*|\+=|-=|\*=|/=|%=|==|!=|<=|>=|<<|>>)"),
                   OPERATOR, 0, 10});
  rules.push_back({std::regex(R"([+\-*/%=<>!&|^~])"), OPERATOR, 0, 10});
}

void SyntaxHighlighter::loadMarkdownRules()
{
  rules.clear();

  // Headings - highest priority for line-start patterns
  rules.push_back(
      {std::regex(R"(^#{1,6}\s+.*)"), MARKDOWN_HEADING, A_BOLD, 100});

  // Code blocks (fenced) - high priority
  rules.push_back({std::regex(R"(^```.*$)"), MARKDOWN_CODE_BLOCK, A_BOLD, 95});
  rules.push_back({std::regex(R"(^~~~.*$)"), MARKDOWN_CODE_BLOCK, A_BOLD, 95});

  // Blockquotes
  rules.push_back({std::regex(R"(^>\s*.*)"), MARKDOWN_BLOCKQUOTE, 0, 90});

  // Lists (unordered and ordered)
  rules.push_back({std::regex(R"(^\s*[-*+]\s+)"), MARKDOWN_LIST, A_BOLD, 85});
  rules.push_back({std::regex(R"(^\s*\d+\.\s+)"), MARKDOWN_LIST, A_BOLD, 85});

  // Tables (pipe characters)
  rules.push_back({std::regex(R"(\|)"), MARKDOWN_TABLE, 0, 80});
  rules.push_back({std::regex(R"(^\s*\|.*\|\s*$)"), MARKDOWN_TABLE, 0, 80});

  // Links with URLs - [text](url)
  rules.push_back(
      {std::regex(R"(\[([^\]]*)\]\(([^)]*)\))"), MARKDOWN_LINK, 0, 75});

  // Bold text - **text** or __text__
  rules.push_back(
      {std::regex(R"(\*\*([^*]+)\*\*)"), MARKDOWN_BOLD, A_BOLD, 70});
  rules.push_back({std::regex(R"(__([^_]+)__)"), MARKDOWN_BOLD, A_BOLD, 70});

  // Italic text - *text* or _text_ (simplified patterns)
  rules.push_back(
      {std::regex(R"(\*([^*\s][^*]*[^*\s])\*)"), MARKDOWN_ITALIC, A_DIM, 65});
  rules.push_back({std::regex(R"(\*(\S)\*)"), MARKDOWN_ITALIC, A_DIM,
                   65}); // Single char italic
  rules.push_back(
      {std::regex(R"(_([^_\s][^_]*[^_\s])_)"), MARKDOWN_ITALIC, A_DIM, 65});
  rules.push_back({std::regex(R"(_(\S)_)"), MARKDOWN_ITALIC, A_DIM,
                   65}); // Single char italic

  // Strikethrough - ~~text~~
  rules.push_back(
      {std::regex(R"(~~([^~]+)~~)"), MARKDOWN_STRIKETHROUGH, 0, 60});

  // Inline code - `code`
  rules.push_back({std::regex(R"(`([^`]+)`)"), MARKDOWN_CODE, 0, 55});

  // Horizontal rules
  rules.push_back({std::regex(R"(^---+\s*$)"), MARKDOWN_LIST, A_BOLD, 50});
  rules.push_back({std::regex(R"(^\*\*\*+\s*$)"), MARKDOWN_LIST, A_BOLD, 50});

  // URLs (standalone)
  rules.push_back(
      {std::regex(R"(https?://[^\s)]+)"), MARKDOWN_URL, A_UNDERLINE, 45});

  // Email addresses
  rules.push_back(
      {std::regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})"),
       MARKDOWN_URL, A_UNDERLINE, 40});
}

void SyntaxHighlighter::setLanguage(const std::string &extension)
{
  currentLanguage = extension;

  if (extension == "cpp" || extension == "h" || extension == "hpp" ||
      extension == "c" || extension == "cc" || extension == "cxx" ||
      extension == "c++" || extension == "hh" || extension == "hxx")
  {
    loadCppRules();
  }
  else if (extension == "py" || extension == "pyw")
  {
    loadPythonRules();
  }
  else if (extension == "md" || extension == "markdown" ||
           extension == "mdown" || extension == "mkd" || extension == "mkdn" ||
           extension == "txt")
  {

    std::cout << "Loading markdown rules" << std::endl; // Debug
    loadMarkdownRules();
    currentLanguage = "markdown";
    std::cout << "Loaded " << rules.size() << " markdown rules"
              << std::endl; // Debug
  }
  else
  {
    loadGenericTextRules();
    currentLanguage = "text";
  }
}
// Check if we're in a markdown code block
bool SyntaxHighlighter::isInMarkdownCodeBlock(
    int lineIndex, const std::vector<std::string> &lines) const
{
  if (lineIndex < 0 || lineIndex >= static_cast<int>(lines.size()))
  {
    return false;
  }

  int codeBlockCount = 0;

  // Scan backwards from the line before current line
  for (int i = lineIndex - 1; i >= 0; --i)
  {
    const std::string &line = lines[i];

    // Check for fenced code blocks (``` or ~~~)
    if (line.length() >= 3)
    {
      std::string trimmed = line;
      // Remove leading whitespace
      size_t start = trimmed.find_first_not_of(" \t");
      if (start != std::string::npos)
      {
        trimmed = trimmed.substr(start);

        if (trimmed.substr(0, 3) == "```" || trimmed.substr(0, 3) == "~~~")
        {
          codeBlockCount++;
        }
      }
    }
  }

  // If odd number of code block markers found, we're inside one
  return (codeBlockCount % 2 == 1);
}

std::vector<ColorSpan>
SyntaxHighlighter::getHighlightSpans(const std::string &lineContent,
                                     int lineIndex,
                                     const GapBuffer &buffer) const
{
  std::vector<ColorSpan> spans;
  if (lineContent.empty())
  {
    return spans;
  }

  std::vector<std::string> allLines;
  for (int i = 0; i < buffer.getLineCount(); ++i)
  {
    allLines.push_back(buffer.getLine(i));
  }

  // Handle markdown code blocks specially
  if (currentLanguage == "markdown")
  {
    bool inCodeBlock = isInMarkdownCodeBlock(lineIndex, allLines);

    if (inCodeBlock)
    {
      // Check if this line ends the code block
      std::string trimmed = lineContent;
      size_t start = trimmed.find_first_not_of(" \t");
      if (start != std::string::npos)
      {
        trimmed = trimmed.substr(start);

        if (trimmed.substr(0, 3) == "```" || trimmed.substr(0, 3) == "~~~")
        {
          // This line ends the code block, highlight it as code block marker
          spans.push_back({0, static_cast<int>(lineContent.length()),
                           MARKDOWN_CODE_BLOCK, A_BOLD, 100});
          return spans;
        }
      }

      // We're inside a code block, highlight the entire line as code
      spans.push_back({0, static_cast<int>(lineContent.length()),
                       MARKDOWN_CODE_BLOCK, 0, 100});
      return spans;
    }
  }

  bool inMultiLineComment = isInMultiLineComment(lineIndex, allLines);
  bool inTripleQuote = isInTripleQuote(lineIndex, allLines);

  // Handle multi-line constructs first (C++/Python)
  if (inMultiLineComment && (currentLanguage == "cpp" ||
                             currentLanguage == "c" || currentLanguage == "h"))
  {
    size_t endPos = lineContent.find("*/");
    if (endPos != std::string::npos)
    {
      spans.push_back({0, static_cast<int>(endPos + 2), COMMENT, 0, 100});

      if (endPos + 2 < lineContent.length())
      {
        std::string remaining = lineContent.substr(endPos + 2);
        auto remainingSpans = getHighlightSpans(remaining, lineIndex, buffer);
        for (auto span : remainingSpans)
        {
          span.start += endPos + 2;
          span.end += endPos + 2;
          spans.push_back(span);
        }
      }
      return spans;
    }
    else
    {
      spans.push_back(
          {0, static_cast<int>(lineContent.length()), COMMENT, 0, 100});
      return spans;
    }
  }

  if (inTripleQuote && currentLanguage == "py")
  {
    size_t end1 = lineContent.find("\"\"\"");
    size_t end2 = lineContent.find("'''");
    size_t endPos = std::min(end1, end2);

    if (end1 == std::string::npos)
      endPos = end2;
    if (end2 == std::string::npos)
      endPos = end1;

    if (endPos != std::string::npos)
    {
      spans.push_back(
          {0, static_cast<int>(endPos + 3), STRING_LITERAL, 0, 100});

      if (endPos + 3 < lineContent.length())
      {
        std::string remaining = lineContent.substr(endPos + 3);
        auto remainingSpans = getHighlightSpans(remaining, lineIndex, buffer);
        for (auto span : remainingSpans)
        {
          span.start += endPos + 3;
          span.end += endPos + 3;
          spans.push_back(span);
        }
      }
      return spans;
    }
    else
    {
      spans.push_back(
          {0, static_cast<int>(lineContent.length()), STRING_LITERAL, 0, 100});
      return spans;
    }
  }

  // Handle new multi-line construct starts
  handleMultiLineStarts(lineContent, spans);

  // Apply regular syntax rules
  for (const auto &rule : rules)
  {
    auto begin = std::sregex_iterator(lineContent.begin(), lineContent.end(),
                                      rule.pattern);
    auto end = std::sregex_iterator();

    for (auto i = begin; i != end; ++i)
    {
      std::smatch match = *i;
      int start, end;

      // Handle special cases for capture groups
      if ((rule.colorPair == PYTHON_FUNCTION_DEF ||
           rule.colorPair == PYTHON_CLASS_DEF) &&
          match.size() > 1)
      {
        start = match.position(1);
        end = match.position(1) + match.length(1);
      }
      else if (rule.colorPair == MARKDOWN_LINK && match.size() > 2)
      {
        // For markdown links, highlight the link text part [text]
        start = match.position(1);
        end = match.position(1) + match.length(1);

        // Also add the URL part with different color
        int url_start = match.position(2);
        int url_end = match.position(2) + match.length(2);
        spans.push_back(
            {url_start, url_end, MARKDOWN_URL, A_UNDERLINE, rule.priority - 1});
      }
      else if ((rule.colorPair == MARKDOWN_BOLD ||
                rule.colorPair == MARKDOWN_ITALIC ||
                rule.colorPair == MARKDOWN_STRIKETHROUGH ||
                rule.colorPair == MARKDOWN_CODE) &&
               match.size() > 1)
      {
        // For markdown formatting, highlight just the content, not the markers
        start = match.position(1);
        end = match.position(1) + match.length(1);
      }
      else
      {
        start = match.position();
        end = match.position() + match.length();
      }

      bool conflicts = false;
      for (const auto &existing : spans)
      {
        if (!(end <= existing.start || start >= existing.end))
        {
          conflicts = true;
          break;
        }
      }

      if (!conflicts)
      {
        spans.push_back(
            {start, end, rule.colorPair, rule.attribute, rule.priority});
      }
    }
  }
  // Sort by position, then priority
  std::sort(spans.begin(), spans.end(),
            [](const ColorSpan &a, const ColorSpan &b)
            {
              if (a.start != b.start)
                return a.start < b.start;
              if (a.priority != b.priority)
                return a.priority > b.priority;
              return a.end < b.end;
            });

  // Remove overlapping spans
  std::vector<ColorSpan> result;
  for (const auto &span : spans)
  {
    bool add = true;
    for (auto it = result.begin(); it != result.end();)
    {
      if (!(span.end <= it->start || span.start >= it->end))
      {
        if (span.priority > it->priority)
        {
          it = result.erase(it);
        }
        else
        {
          add = false;
          break;
        }
      }
      else
      {
        ++it;
      }
    }
    if (add)
    {
      result.push_back(span);
    }
  }

  return result;
}
// Simple check if line is inside C++ multi-line comment
bool SyntaxHighlighter::isInMultiLineComment(
    int lineIndex, const std::vector<std::string> &lines) const
{
  if (lineIndex <= 0 || lineIndex >= static_cast<int>(lines.size()))
  {
    return false;
  }

  int commentDepth = 0;

  for (int i = lineIndex - 1; i >= 0; --i)
  {
    const std::string &line = lines[i];

    if (line.length() < 2)
    {
      continue; // Skip lines too short to contain /* or */
    }

    // Count */ and /* from right to left
    for (int pos = line.length() - 2; pos >= 0; --pos)
    {
      if (pos + 1 < static_cast<int>(line.length()) &&
          line.substr(pos, 2) == "*/")
      {
        commentDepth++;
      }
      else if (pos + 1 < static_cast<int>(line.length()) &&
               line.substr(pos, 2) == "/*")
      {
        commentDepth--;
        if (commentDepth < 0)
        {
          return true; // We're inside a comment
        }
      }
    }

    if (commentDepth > 0)
    {
      return false; // Comment was closed
    }
  }

  return false;
}

// Simple check if line is inside Python triple quote
bool SyntaxHighlighter::isInTripleQuote(
    int lineIndex, const std::vector<std::string> &lines) const
{
  if (lineIndex <= 0 || lineIndex >= static_cast<int>(lines.size()))
  {
    return false;
  }

  int tripleCount = 0;

  // Scan backwards from the line before current line
  for (int i = lineIndex - 1; i >= 0; --i)
  {
    const std::string &line = lines[i];

    if (line.length() < 3)
    {
      continue; // Skip lines too short to contain triple quotes
    }

    // Count triple quotes on this line, from left to right
    for (size_t pos = 0; pos <= line.length() - 3; ++pos)
    {
      if ((pos + 2 < line.length()) &&
          (line.substr(pos, 3) == "\"\"\"" || line.substr(pos, 3) == "'''"))
      {
        tripleCount++;
        pos += 2; // Skip ahead 2 more positions (we'll increment 1 more in
                  // the loop)
      }
    }
  }

  // If odd number of triple quotes found, we're inside one
  return (tripleCount % 2 == 1);
}

// Handle start of multi-line constructs on current line
void SyntaxHighlighter::handleMultiLineStarts(
    const std::string &line, std::vector<ColorSpan> &spans) const
{
  if (currentLanguage == "cpp" || currentLanguage == "c" ||
      currentLanguage == "h")
  {
    size_t pos = 0;
    while (pos < line.length())
    {
      size_t start = line.find("/*", pos);
      if (start == std::string::npos)
        break;

      size_t end = line.find("*/", start + 2);
      if (end != std::string::npos)
      {
        // Complete comment on same line
        spans.push_back({static_cast<int>(start), static_cast<int>(end + 2),
                         COMMENT, 0, 100});
        pos = end + 2;
      }
      else
      {
        // Multi-line comment starts
        spans.push_back({static_cast<int>(start),
                         static_cast<int>(line.length()), COMMENT, 0, 100});
        break;
      }
    }
  }
  else if (currentLanguage == "py")
  {
    size_t pos = 0;
    while (pos < line.length())
    {
      size_t start1 = line.find("\"\"\"", pos);
      size_t start2 = line.find("'''", pos);

      size_t start = std::min(start1, start2);
      if (start1 == std::string::npos)
        start = start2;
      if (start2 == std::string::npos)
        start = start1;

      if (start == std::string::npos)
        break;

      std::string quote = line.substr(start, 3);
      size_t end = line.find(quote, start + 3);

      if (end != std::string::npos)
      {
        // Complete triple quote on same line
        spans.push_back({static_cast<int>(start), static_cast<int>(end + 3),
                         STRING_LITERAL, 0, 100});
        pos = end + 3;
      }
      else
      {
        // Multi-line triple quote starts
        spans.push_back({static_cast<int>(start),
                         static_cast<int>(line.length()), STRING_LITERAL, 0,
                         100});
        break;
      }
    }
  }
}