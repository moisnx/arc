#ifndef BINARY_DETECTOR_H
#define BINARY_DETECTOR_H

#include "src/core/buffer.h"
#include <algorithm>
#include <fstream>
#include <string>

class BinaryDetector
{
public:
  // Check if file appears to be binary by scanning for null bytes
  static bool isBinaryFile(const std::string &filepath,
                           size_t bytesToCheck = 8192)
  {

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
      return false;
    }

    // Read first N bytes
    std::vector<char> GapBuffer(bytesToCheck);
    file.read(GapBuffer.data(), bytesToCheck);
    size_t bytesRead = file.gcount();
    file.close();

    if (bytesRead == 0)
    {
      return false; // Empty file = text
    }

    // Check for null bytes (strong indicator of binary)
    if (std::find(GapBuffer.begin(), GapBuffer.begin() + bytesRead, '\0') !=
        GapBuffer.begin() + bytesRead)
    {
      return true;
    }

    // Additional heuristic: check for high ratio of non-printable characters
    int nonPrintable = 0;
    for (size_t i = 0; i < bytesRead; i++)
    {
      unsigned char c = GapBuffer[i];
      // Allow common control chars: \n, \r, \t
      if (c < 32 && c != '\n' && c != '\r' && c != '\t')
      {
        nonPrintable++;
      }
      // High bytes (128-255) are suspicious if too frequent
      if (c >= 128)
      {
        nonPrintable++;
      }
    }

    // If >30% non-printable, likely binary
    double nonPrintableRatio = static_cast<double>(nonPrintable) / bytesRead;
    return nonPrintableRatio > 0.30;
  }

  // Check common binary file extensions
  static bool hasBinaryExtension(const std::string &filename)
  {
    static const std::vector<std::string> binaryExts = {
        ".exe", ".dll",  ".so",    ".dylib", ".a",    ".lib", ".o",
        ".obj", ".bin",  ".dat",   ".pak",   ".zip",  ".tar", ".gz",
        ".bz2", ".7z",   ".jpg",   ".jpeg",  ".png",  ".gif", ".bmp",
        ".ico", ".webp", ".mp3",   ".mp4",   ".avi",  ".mkv", ".mov",
        ".flv", ".wav",  ".pdf",   ".doc",   ".docx", ".xls", ".xlsx",
        ".ppt", ".pptx", ".class", ".pyc",   ".pyo",  ".swp"};

    size_t dotPos = filename.find_last_of(".");
    if (dotPos == std::string::npos)
    {
      return false;
    }

    std::string ext = filename.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return std::find(binaryExts.begin(), binaryExts.end(), ext) !=
           binaryExts.end();
  }

  // Comprehensive check: extension + content analysis
  static bool shouldTreatAsBinary(const std::string &filepath)
  {
    // Fast check: known binary extension
    if (hasBinaryExtension(filepath))
    {
      return true;
    }

    // Slower check: scan file content
    return isBinaryFile(filepath);
  }

  // Get human-readable file size
  static std::string getFileSize(const std::string &filepath)
  {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
      return "Unknown";
    }

    std::streamsize size = file.tellg();
    file.close();

    const char *units[] = {"B", "KB", "MB", "GB"};
    int unitIndex = 0;
    double displaySize = static_cast<double>(size);

    while (displaySize >= 1024.0 && unitIndex < 3)
    {
      displaySize /= 1024.0;
      unitIndex++;
    }

    char GapBuffer[32];
    snprintf(GapBuffer, sizeof(GapBuffer), "%.1f %s", displaySize,
             units[unitIndex]);
    return std::string(GapBuffer);
  }

  // Detect file type by magic bytes (first few bytes signature)
  static std::string detectFileType(const std::string &filepath)
  {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
      return "Unknown";
    }

    unsigned char magic[8] = {0};
    file.read(reinterpret_cast<char *>(magic), 8);
    file.close();

    // Check common file signatures
    if (magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' &&
        magic[3] == 'F')
    {
      return "ELF Executable";
    }
    if (magic[0] == 'M' && magic[1] == 'Z')
    {
      return "PE Executable (.exe)";
    }
    if (magic[0] == 0xFF && magic[1] == 0xD8 && magic[2] == 0xFF)
    {
      return "JPEG Image";
    }
    if (magic[0] == 0x89 && magic[1] == 'P' && magic[2] == 'N' &&
        magic[3] == 'G')
    {
      return "PNG Image";
    }
    if (magic[0] == 'G' && magic[1] == 'I' && magic[2] == 'F')
    {
      return "GIF Image";
    }
    if (magic[0] == 'B' && magic[1] == 'M')
    {
      return "BMP Image";
    }
    if (magic[0] == 0x25 && magic[1] == 0x50 && magic[2] == 0x44 &&
        magic[3] == 0x46)
    {
      return "PDF Document";
    }
    if (magic[0] == 'P' && magic[1] == 'K')
    {
      return "ZIP Archive";
    }
    if (magic[0] == 0x1F && magic[1] == 0x8B)
    {
      return "GZIP Archive";
    }

    return "Binary Data";
  }
};

#endif // BINARY_DETECTOR_H