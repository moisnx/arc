#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <chrono>
#include <ostream>
#include <string>

struct BenchmarkResult
{
  std::chrono::milliseconds init_time;
  std::chrono::milliseconds theme_time;
  std::chrono::milliseconds editor_creation_time;
  std::chrono::milliseconds file_load_time;
  std::chrono::milliseconds syntax_highlight_time;
  std::chrono::milliseconds first_render_time;
  std::chrono::milliseconds total_time;

  void print(std::ostream &os) const;
};

class Benchmark
{
public:
  static int runStartup(const std::string &filename,
                        bool enable_syntax_highlighting);
  static int runQuickStartup();

private:
  static BenchmarkResult runStartupInteractive(const std::string &filename,
                                               bool enable_syntax_highlighting);
};

#endif // BENCHMARK_H