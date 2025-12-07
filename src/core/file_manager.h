#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <memory>
#include <string>
#include <vector>

// Forward declarations
class GapBuffer;
class SyntaxConfigLoader;

class FileManager
{
public:
  struct FileLoadResult
  {
    bool success;
    bool isBinary;
    std::string detectedLanguage; // e.g., "cpp", "python", "markdown"
    std::string filename;
    std::string extension;
  };

  // Dependencies: Needs config loader to map extensions/filenames to languages
  FileManager(SyntaxConfigLoader *configLoader);
  ~FileManager();

  // Core Actions
  FileLoadResult loadFile(const std::string &path, GapBuffer &buffer);
  bool saveFile(const std::string &path, GapBuffer &buffer);

  // Helpers
  static std::string getExtension(const std::string &filename);
  static std::string findMagikaModels();

private:
  SyntaxConfigLoader *configLoader_;

  // Internal detection logic
  std::string detectLanguage(const std::string &filename,
                             const std::string &firstLine);
  bool isBinaryContent(const std::string &path);
};

#endif // FILE_MANAGER_H