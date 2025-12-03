#pragma once

#include <cstdlib>
#include <optional>
#include <string>

enum class ImageProtocol
{
  Kitty,   // Best quality
  ITerm2,  // Good quality
  Sixel,   // Wide compatibility
  Unicode, // Fallback
  None     // No image support
};

class TerminalDetector
{
public:
  static ImageProtocol detect_best_protocol()
  {
    // Check environment variables for terminal identification
    const char *term_program = std::getenv("TERM_PROGRAM");
    const char *term = std::getenv("TERM");
    const char *wt_session = std::getenv("WT_SESSION");

    // Priority 1: Kitty Graphics Protocol
    if (term_program &&
        std::string(term_program).find("kitty") != std::string::npos)
    {
      return ImageProtocol::Kitty;
    }

    // Check for WezTerm (supports both Kitty and iTerm2)
    if (term_program && std::string(term_program) == "WezTerm")
    {
      return ImageProtocol::Kitty; // Prefer Kitty on WezTerm
    }

    // Check for Ghostty
    if (term_program && std::string(term_program) == "ghostty")
    {
      return ImageProtocol::Kitty;
    }

    // Priority 2: iTerm2 Inline Images
    if (term_program && std::string(term_program) == "iTerm.app")
    {
      return ImageProtocol::ITerm2;
    }

    // Priority 3: SIXEL
    // Windows Terminal
    if (wt_session)
    {
      return ImageProtocol::Sixel;
    }

    // Check TERM for sixel-capable terminals
    if (term)
    {
      std::string term_str(term);
      if (term_str.find("xterm") != std::string::npos ||
          term_str.find("konsole") != std::string::npos ||
          term_str.find("foot") != std::string::npos)
      {
        return ImageProtocol::Sixel;
      }
    }

    // Priority 4: Unicode fallback
    return ImageProtocol::Unicode;
  }

  static std::string protocol_name(ImageProtocol protocol)
  {
    switch (protocol)
    {
    case ImageProtocol::Kitty:
      return "Kitty Graphics";
    case ImageProtocol::ITerm2:
      return "iTerm2 Inline";
    case ImageProtocol::Sixel:
      return "SIXEL";
    case ImageProtocol::Unicode:
      return "Unicode Art";
    case ImageProtocol::None:
      return "None";
    }
    return "Unknown";
  }

  static bool is_color_capable()
  {
    const char *colorterm = std::getenv("COLORTERM");
    return colorterm && (std::string(colorterm) == "truecolor" ||
                         std::string(colorterm) == "24bit");
  }
};
