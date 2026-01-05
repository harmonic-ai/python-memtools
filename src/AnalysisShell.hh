#include <stdint.h>

#include <memory>
#include <phosg/Encoding.hh>
#include <string>

#include "Common.hh"
#include "MemoryReader.hh"
#include "Types/Base.hh"

class AnalysisShell {
public:
  AnalysisShell() = delete;
  explicit AnalysisShell(const std::string& data_path, size_t max_threads);
  AnalysisShell(const AnalysisShell&) = delete;
  AnalysisShell(AnalysisShell&&) = delete;
  AnalysisShell& operator=(const AnalysisShell&) = delete;
  AnalysisShell& operator=(AnalysisShell&&) = delete;
  ~AnalysisShell() = default;

  void prepare();

  void run();

  template <typename T>
  MappedPtr<T> parse_addr(std::string addr_str, bool bswap) {
    uint64_t num_dereferences = 0;
    while (addr_str.starts_with("*")) {
      num_dereferences++;
      addr_str = addr_str.substr(1);
    }
    MappedPtr<T> addr{stoull(addr_str, nullptr, 16)};
    if (bswap) {
      addr.addr = phosg::bswap64(addr.addr);
    }
    for (; num_dereferences > 0; num_dereferences--) {
      addr.addr = this->env.r.read(addr, sizeof(uint64_t)).get_u64l();
    }
    return addr;
  }

  void run_command(const std::string& command);

  bool should_exit = false;
  size_t max_threads;
  Environment env;
};
