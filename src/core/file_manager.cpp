#include "file_manager.h"
#include "src/core/buffer.h"
#include "src/features/syntax_highlighter.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <magika/magika.hpp>
#include <numeric>

namespace fs = std::filesystem;

FileManager::FileManager(SyntaxConfigLoader *configLoader)
    : configLoader_(configLoader)
{
}

FileManager::~FileManager() {}

std::string FileManager::getExtension(const std::string &filename)
{
  if (filename.empty())
    return "";
  size_t dot = filename.find_last_of(".");
  if (dot == std::string::npos)
    return "";

  std::string ext = filename.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext;
}

std::string FileManager::findMagikaModels()
{
  // Try environment variable first
  if (const char *env_path = std::getenv("MAGIKA_MODEL_PATH"))
  {
    if (fs::exists(env_path))
      return env_path;
  }

  // Try common install locations
  std::vector<std::string> search_paths = {
      "/usr/local/share/magika/models/standard_v3_3",
      "/usr/share/magika/models/standard_v3_3",
      "../../assets/models/standard_v3_3", "./models/standard_v3_3",
      "../models/standard_v3_3"};

  for (const auto &path : search_paths)
  {
    if (fs::exists(path + "/model.onnx"))
      return path;
  }

  // Return empty or throw, handled in isBinaryContent
  return "";
}

bool FileManager::isBinaryContent(const std::string &path)
{
  try
  {
    std::string model_path = findMagikaModels();
    if (model_path.empty())
      return false; // Fail safe to text if model missing

    static magika::Magika detector(model_path);
    auto result = detector.identify_path(path);

    // Allow text, code, and empty files (inode)
    return !(result.group == "text" || result.group == "code" ||
             result.group == "inode");
  }
  catch (...)
  {
    return false; // Fail safe
  }
}

std::string FileManager::detectLanguage(const std::string &filename,
                                        const std::string &firstLine)
{
  if (!configLoader_)
    return "text";

  std::string language_name = "text";

  // 1. Extension
  std::string extension = getExtension(filename);
  if (!extension.empty())
  {
    language_name = configLoader_->getLanguageFromExtension(extension);
    if (!language_name.empty() && language_name != "text")
      return language_name;
  }

  // 2. Filename (Makefile, Dockerfile)
  std::string from_filename = configLoader_->getLanguageFromFilename(filename);
  if (!from_filename.empty() && from_filename != "text")
    return from_filename;

  // 3. Shebang
  language_name = configLoader_->getLanguageFromShebang(firstLine);
  if (!language_name.empty() && language_name != "text")
    return language_name;

  return "text"; // Fallback
}

FileManager::FileLoadResult FileManager::loadFile(const std::string &path,
                                                  GapBuffer &buffer)
{
  FileLoadResult result;
  result.filename = path;
  result.extension = getExtension(path);
  result.success = false;
  result.isBinary = false;
  result.detectedLanguage = "text";

  // 1. Check Binary
  if (isBinaryContent(path))
  {
    result.isBinary = true;
    result.success = true; // Technically loaded, just can't display content
    return result;
  }

  // 2. Load Content
  if (!buffer.loadFromFile(path))
  {
    buffer.clear();
    buffer.insertLine(0, "");
    return result; // success = false
  }

  // 3. Detect Language
  std::string firstLine = (buffer.getLineCount() > 0) ? buffer.getLine(0) : "";
  result.detectedLanguage = detectLanguage(path, firstLine);
  result.success = true;

  return result;
}

bool FileManager::saveFile(const std::string &path, GapBuffer &buffer)
{
  return buffer.saveToFile(path);
}