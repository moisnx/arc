#include "src/ui/icon_provider.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

IconProvider::IconProvider(IconStyle style) : current_style_(style)
{
  // Auto-detect if requested
  if (current_style_ == IconStyle::AUTO)
  {
    current_style_ =
        detectUnicodeSupport() ? IconStyle::NERD_FONTS : IconStyle::ASCII;
  }

  // Initialize Nerd Fonts mappings
  if (current_style_ == IconStyle::NERD_FONTS)
  {
    initializeNerdFontMap();
  }
}

bool IconProvider::detectUnicodeSupport() const
{
  // Check LANG environment variable for UTF-8
  const char *lang = std::getenv("LANG");
  if (lang &&
      (strstr(lang, "UTF-8") || strstr(lang, "utf8") || strstr(lang, "UTF8")))
  {
    return true;
  }

  // Check for modern terminal emulators
  const char *term = std::getenv("TERM");
  if (term)
  {
    std::string term_str(term);
    if (term_str.find("xterm") != std::string::npos ||
        term_str.find("kitty") != std::string::npos ||
        term_str.find("alacritty") != std::string::npos ||
        term_str.find("wezterm") != std::string::npos ||
        term_str.find("tmux") != std::string::npos ||
        term_str.find("screen") != std::string::npos)
    {
      return true;
    }
  }

  // Check TERM_PROGRAM for macOS terminals
  const char *term_program = std::getenv("TERM_PROGRAM");
  if (term_program)
  {
    return true; // Most macOS terminals support Unicode
  }

  return false;
}

void IconProvider::initializeNerdFontMap()
{
  // NOTE: These are Nerd Font codepoints. Make sure this file is saved as
  // UTF-8! If icons don't show, your editor might have converted them to
  // another encoding.

  // Programming languages
  nerd_font_map_[".c"] = "\ue61e";   //
  nerd_font_map_[".cpp"] = "\ue61d"; //
  nerd_font_map_[".cc"] = "\ue61d";
  nerd_font_map_[".cxx"] = "\ue61d";
  nerd_font_map_[".h"] = "\ue61e";
  nerd_font_map_[".hpp"] = "\ue61d";
  nerd_font_map_[".py"] = "\ue606";  //
  nerd_font_map_[".js"] = "\ue74e";  //
  nerd_font_map_[".ts"] = "\ue628";  //
  nerd_font_map_[".jsx"] = "\ue7ba"; //
  nerd_font_map_[".tsx"] = "\ue7ba";
  nerd_font_map_[".rs"] = "\ue7a8";    //
  nerd_font_map_[".go"] = "\ue627";    //
  nerd_font_map_[".java"] = "\ue738";  //
  nerd_font_map_[".rb"] = "\ue791";    //
  nerd_font_map_[".php"] = "\ue73d";   //
  nerd_font_map_[".swift"] = "\ue755"; //
  nerd_font_map_[".kt"] = "\ue634";    //
  nerd_font_map_[".scala"] = "\ue737"; //
  nerd_font_map_[".lua"] = "\ue620";   //
  nerd_font_map_[".vim"] = "\ue62b";   //
  nerd_font_map_[".sh"] = "\uf489";    //
  nerd_font_map_[".bash"] = "\uf489";
  nerd_font_map_[".zsh"] = "\uf489";
  nerd_font_map_[".fish"] = "\uf489";

  // Web technologies
  nerd_font_map_[".html"] = "\ue60e"; //
  nerd_font_map_[".css"] = "\ue749";  //
  nerd_font_map_[".scss"] = "\ue603"; //
  nerd_font_map_[".sass"] = "\ue603";
  nerd_font_map_[".json"] = "\ue60b"; //
  nerd_font_map_[".xml"] = "\ue619";  //
  nerd_font_map_[".yaml"] = "\uf481"; //
  nerd_font_map_[".yml"] = "\uf481";
  nerd_font_map_[".toml"] = "\ue615"; //

  // Documents
  nerd_font_map_[".md"] = "\ue609";  //
  nerd_font_map_[".txt"] = "\uf15c"; //
  nerd_font_map_[".pdf"] = "\uf1c1"; //
  nerd_font_map_[".doc"] = "\uf1c2"; //
  nerd_font_map_[".docx"] = "\uf1c2";

  // Images
  nerd_font_map_[".png"] = "\uf1c5"; //
  nerd_font_map_[".jpg"] = "\uf1c5";
  nerd_font_map_[".jpeg"] = "\uf1c5";
  nerd_font_map_[".gif"] = "\uf1c5";
  nerd_font_map_[".svg"] = "\uf1c5";
  nerd_font_map_[".ico"] = "\uf1c5";
  nerd_font_map_[".bmp"] = "\uf1c5";

  // Archives
  nerd_font_map_[".zip"] = "\uf410"; //
  nerd_font_map_[".tar"] = "\uf410";
  nerd_font_map_[".gz"] = "\uf410";
  nerd_font_map_[".bz2"] = "\uf410";
  nerd_font_map_[".xz"] = "\uf410";
  nerd_font_map_[".7z"] = "\uf410";
  nerd_font_map_[".rar"] = "\uf410";

  // Media
  nerd_font_map_[".mp3"] = "\uf001"; //
  nerd_font_map_[".mp4"] = "\uf03d"; //
  nerd_font_map_[".avi"] = "\uf03d";
  nerd_font_map_[".mkv"] = "\uf03d";
  nerd_font_map_[".wav"] = "\uf001";
  nerd_font_map_[".flac"] = "\uf001";

  // Git
  nerd_font_map_[".git"] = "\ue702"; //
  nerd_font_map_[".gitignore"] = "\ue702";
  nerd_font_map_[".gitmodules"] = "\ue702";

  // Config files
  nerd_font_map_[".conf"] = "\ue615"; //
  nerd_font_map_[".config"] = "\ue615";
  nerd_font_map_[".ini"] = "\ue615";
  nerd_font_map_[".env"] = "\uf462"; //

  // Build files
  nerd_font_map_["makefile"] = "\ue779"; //
  nerd_font_map_["Makefile"] = "\ue779";
  nerd_font_map_["CMakeLists.txt"] = "\ue615";
  nerd_font_map_[".cmake"] = "\ue615";
  nerd_font_map_["package.json"] = "\ue71e"; //
  nerd_font_map_["Cargo.toml"] = "\ue7a8";   //
  nerd_font_map_["Cargo.lock"] = "\ue7a8";

  // Special files
  nerd_font_map_["README"] = "\ue609"; //
  nerd_font_map_["README.md"] = "\ue609";
  nerd_font_map_["LICENSE"] = "\uf48a";    //
  nerd_font_map_["Dockerfile"] = "\uf308"; //
  nerd_font_map_[".dockerignore"] = "\uf308";
}

std::string IconProvider::getFileExtension(const std::string &filename) const
{
  size_t dot_pos = filename.rfind('.');
  if (dot_pos != std::string::npos && dot_pos > 0)
  {
    return filename.substr(dot_pos);
  }
  return "";
}

std::string IconProvider::getDirectoryIcon() const
{
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf07c" : "+"; //
}

std::string IconProvider::getParentIcon() const
{
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf0a9" : "^"; //
}

std::string IconProvider::getExecutableIcon() const
{
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf489" : "*"; //
}

std::string IconProvider::getSymlinkIcon() const
{
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf0c1" : "@"; //
}

std::string IconProvider::getHiddenIcon() const
{
  return (current_style_ == IconStyle::NERD_FONTS) ? "\uf070" : "."; //
}

std::string IconProvider::getFileIcon(const std::string &filename) const
{
  if (current_style_ == IconStyle::ASCII)
  {
    return " ";
  }

  // Check for exact filename matches first (case-sensitive for special files)
  auto it = nerd_font_map_.find(filename);
  if (it != nerd_font_map_.end())
  {
    return it->second;
  }

  // Check for case-insensitive exact matches
  std::string lower_filename = filename;
  std::transform(lower_filename.begin(), lower_filename.end(),
                 lower_filename.begin(), ::tolower);

  it = nerd_font_map_.find(lower_filename);
  if (it != nerd_font_map_.end())
  {
    return it->second;
  }

  // Check by extension
  std::string ext = getFileExtension(filename);
  if (!ext.empty())
  {
    // Try lowercase extension
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
                   ::tolower);

    it = nerd_font_map_.find(lower_ext);
    if (it != nerd_font_map_.end())
    {
      return it->second;
    }
  }

  // Default file icon
  return "\uf15b"; //
}