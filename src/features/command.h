// src/features/command.h
#ifndef COMMAND_H
#define COMMAND_H

#include <functional>
#include <string>
#include <vector>

class Editor;

enum class CommandType
{
  EDITOR, // Built-in editor operations (safe)
  BUILD,  // Predefined build/compile commands (sandboxed)
  SHELL,  // Direct shell access (opt-in, dangerous)
  CUSTOM  // User-defined commands from config
};

enum class CommandSafety
{
  SAFE,      // No shell access, pure editor operations
  SANDBOXED, // Limited shell access with validation
  DANGEROUS  // Full shell access (requires confirmation)
};

struct Command
{
  std::string id;
  std::string display_name;
  std::string description;
  CommandType type;
  CommandSafety safety_level;

  // For shell commands
  std::string shell_template;
  std::vector<std::string> required_vars;

  // For editor commands (function pointer)
  std::function<void(Editor &)> editor_handler;

  // Optional features
  std::string working_directory;
  bool show_output;
  bool require_confirmation;
  std::string keybinding;

  // Constructor for editor commands
  Command(const std::string &id_, const std::string &display_,
          const std::string &desc_, std::function<void(Editor &)> handler)
      : id(id_), display_name(display_), description(desc_),
        type(CommandType::EDITOR), safety_level(CommandSafety::SAFE),
        editor_handler(handler), show_output(false), require_confirmation(false)
  {
  }

  // Constructor for shell commands
  Command(const std::string &id_, const std::string &display_,
          const std::string &desc_, const std::string &shell_template_,
          CommandSafety safety = CommandSafety::SANDBOXED)
      : id(id_), display_name(display_), description(desc_),
        type(CommandType::BUILD), safety_level(safety),
        shell_template(shell_template_), show_output(true),
        require_confirmation(false)
  {
  }

  // Default constructor
  Command()
      : type(CommandType::EDITOR), safety_level(CommandSafety::SAFE),
        show_output(false), require_confirmation(false)
  {
  }
};

#endif // COMMAND_H