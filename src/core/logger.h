#pragma once
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

enum class LogLevel
{
  DEBUG,
  INFO,
  WARN,
  ERROR
};

class Logger
{
public:
  static void init(const std::string &filename,
                   LogLevel minLevel = LogLevel::DEBUG)
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open())
    {
      file_.open(filename, std::ios::app);
    }
    minLevel_ = minLevel;
    log(LogLevel::INFO, "=== Logger initialized: " + filename + " ===");
  }

  static void shutdown()
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
    {
      log(LogLevel::INFO, "=== Logger shutting down ===");
      file_.close();
    }
  }

  static void log(LogLevel level, const std::string &message)
  {
    if (level < minLevel_)
      return;

    std::ostringstream oss;
    oss << timestamp() << " [" << levelToString(level) << "] " << message
        << "\n";

    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open())
    {
      file_ << oss.str();
      file_.flush();
    }
#ifdef DEBUG_CONSOLE
    // Optional: also print to console (useful during dev)
    LOG_DEBUG( oss.str();
#endif
  }

private:
  static std::string timestamp()
  {
    auto now = std::chrono::system_clock::now();
    auto in_time = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
#ifdef _WIN32
    localtime_s(&buf, &in_time);
#else
    localtime_r(&in_time, &buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
  }

  static std::string levelToString(LogLevel level)
  {
    switch (level)
    {
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARN:
      return "WARN";
    case LogLevel::ERROR:
      return "ERROR";
    default:
      return "LOG";
    }
  }

  static inline std::ofstream file_;
  static inline std::mutex mutex_;
  static inline LogLevel minLevel_ = LogLevel::DEBUG;
};
