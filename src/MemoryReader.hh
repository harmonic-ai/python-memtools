#pragma once

#include <signal.h>
#include <stdint.h>
#include <sys/mman.h>

#include <filesystem>
#include <map>
#include <memory>
#include <phosg/Filesystem.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Common.hh"

class MemoryReader;

template <typename T = void>
struct MappedPtr {
  // "Opaque" type for pointers in the mapped process' address space. This isn't really opaque (you can still just use
  // .addr) but we use this to make it hard to accidentally confuse a mapped address for some other kind of uint64_t.

  uint64_t addr;

  MappedPtr() : addr(0) {}
  explicit MappedPtr(uint64_t addr) : addr(addr) {}
  MappedPtr(const MappedPtr& other) = default;
  MappedPtr(MappedPtr&& other) = default;
  MappedPtr& operator=(const MappedPtr& other) = default;
  MappedPtr& operator=(MappedPtr&& other) = default;
  ~MappedPtr() = default;
  bool operator==(const MappedPtr& other) const = default;
  bool operator!=(const MappedPtr& other) const = default;

  inline MappedPtr& operator=(nullptr_t) {
    this->addr = 0;
    return *this;
  }

  template <typename U>
    requires std::convertible_to<U*, T*>
  MappedPtr(const MappedPtr<U>& other) : addr(other.addr) {}

  template <typename U, typename V = T>
  MappedPtr<U> offset_t(ssize_t delta) const {
    return MappedPtr<U>{this->addr + delta * sizeof(T)};
  }

  MappedPtr<T> offset_bytes(ssize_t bytes) const {
    return MappedPtr<T>{this->addr + bytes};
  }

  template <typename U>
  size_t bytes_until(MappedPtr<U> end_ptr) const {
    return end_ptr.addr - this->addr;
  }

  std::strong_ordering operator<=>(const MappedPtr& other) const {
    return this->addr <=> other.addr;
  }

  bool is_null() const {
    return this->addr == 0;
  }

  template <typename U>
  MappedPtr<U> cast() const {
    return MappedPtr<U>{this->addr};
  }
};
static_assert(sizeof(MappedPtr<void>) == sizeof(void*), "MappedPtr layout is not just a single pointer-sized value");

namespace std {
template <typename T>
struct hash<MappedPtr<T>> {
  size_t operator()(const MappedPtr<T>& p) const noexcept {
    return std::hash<uint64_t>{}(p.addr);
  }
};

template <typename T>
struct formatter<MappedPtr<T>> {
  constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin();
  }
  auto format(const MappedPtr<T>& addr, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{:016X}", addr.addr);
  }
};
} // namespace std

class ProcessPauseGuard {
public:
  explicit ProcessPauseGuard(uint64_t pid);
  ~ProcessPauseGuard();

private:
  uint64_t pid;
};

struct MemoryMappedFile {
  struct View {
    MappedPtr<void> addr;
    uint64_t file_offset;
    void* data;
    size_t size;

    inline phosg::StringReader read() const {
      return phosg::StringReader(this->data, this->size);
    }
  };

  explicit MemoryMappedFile(int fd, uint64_t offset, size_t size, bool writable = false);
  explicit MemoryMappedFile(const std::string& filename, bool writable = false);
  MemoryMappedFile(const MemoryMappedFile&) = delete;
  MemoryMappedFile(MemoryMappedFile&& other);
  MemoryMappedFile& operator=(const MemoryMappedFile&) = delete;
  MemoryMappedFile& operator=(MemoryMappedFile&& other);
  ~MemoryMappedFile();

  inline View view(MappedPtr<void> addr, size_t offset, size_t size) const {
    if (offset + size > this->total_size) {
      throw std::runtime_error("Map view out of range");
    }
    return View{
        .addr = addr,
        .file_offset = this->map_offset + offset,
        .data = reinterpret_cast<uint8_t*>(this->all_data) + offset,
        .size = size,
    };
  }

  inline phosg::StringReader read() const {
    return phosg::StringReader(this->all_data, this->total_size);
  }

  std::string filename;
  uint64_t map_offset;
  void* all_data;
  size_t total_size;
};

class MemoryReader {
public:
  explicit MemoryReader(const std::string& data_path);
  MemoryReader(const MemoryReader&) = delete;
  MemoryReader(MemoryReader&&) = delete;
  MemoryReader& operator=(const MemoryReader&) = delete;
  MemoryReader& operator=(MemoryReader&&) = delete;
  ~MemoryReader() = default;

  bool exists(MappedPtr<void> addr) const;
  bool exists_range(MappedPtr<void> addr, size_t size) const;

  template <typename T>
  bool exists_array(MappedPtr<T> addr, size_t count) const {
    return this->exists_range(addr, count * sizeof(T));
  }

  template <typename T>
  const T& get(MappedPtr<T> addr) const {
    return this->read(addr, sizeof(T)).template get<T>();
  }
  template <typename T>
  const T* get_array(MappedPtr<T> addr, size_t count) const {
    return &this->read(addr, sizeof(T) * count).template get<T>();
  }

  inline std::string get_cstr(MappedPtr<char> addr) const {
    return this->read_to_end(addr).get_cstr();
  }

  phosg::StringReader read(MappedPtr<void> addr, size_t size) const;
  phosg::StringReader read_to_end(MappedPtr<void> addr) const;

  inline const void* readv(MappedPtr<void> addr, size_t size) const {
    return this->read(addr, size).getv(size);
  }

  std::pair<MappedPtr<void>, size_t> region_for_address(MappedPtr<void> addr) const;
  std::vector<std::pair<MappedPtr<void>, size_t>> all_regions() const;

  inline size_t bytes() const {
    return this->total_bytes;
  }
  inline size_t region_count() const {
    return this->regions_by_mapped.size();
  }

  template <typename T, typename FnT>
    requires(std::is_invocable_r_v<void, FnT, const T&, MappedPtr<T>, size_t>)
  void map_all_addresses(FnT&& fn, size_t stride, size_t num_threads = 0, size_t object_size = sizeof(T)) const {
    if (stride & (stride - 1)) {
      throw std::logic_error("Stride must be a power of 2");
    }
    constexpr size_t block_stride = 0x1000;
    if (stride > block_stride) {
      throw std::logic_error("Stride must not be greater than 0x1000");
    }

    if (num_threads == 0) {
      num_threads = std::thread::hardware_concurrency();
    }

    auto regions = this->all_regions();
    std::vector<phosg::StringReader> region_rs;
    std::vector<size_t> region_start_offsets;
    region_start_offsets.emplace_back(0);
    for (const auto& rgn : regions) {
      region_start_offsets.emplace_back(region_start_offsets.back() + rgn.second);
      region_rs.emplace_back(this->read(rgn.first, rgn.second));
    }

    std::atomic<uint64_t> current_offset(0);
    auto thread_fn = [&](size_t thread_index) -> void {
      size_t current_region = 0;
      uint64_t offset;
      while ((offset = current_offset.fetch_add(block_stride)) < region_start_offsets.back()) {
        while (offset >= region_start_offsets[current_region + 1]) {
          current_region++;
        }
        if (offset <= region_start_offsets[current_region + 1] - object_size) {
          uint64_t offset_within_region = offset - region_start_offsets[current_region];
          auto addr = regions[current_region].first.offset_bytes(offset_within_region).cast<T>();
          for (size_t z = 0; z < block_stride; z += stride) {
            if (offset + z > region_start_offsets[current_region + 1] - object_size) {
              break;
            }
            fn(region_rs[current_region].pget<T>(offset_within_region + z), addr.offset_bytes(z), thread_index);
          }
        }
      }
    };

    std::vector<std::thread> threads;
    while (threads.size() < num_threads) {
      size_t thread_index = threads.size();
      threads.emplace_back(thread_fn, thread_index);
    }

    size_t progress_current_region = 0;
    uint64_t progress_current_offset;
    while ((progress_current_offset = current_offset.load()) < region_start_offsets.back()) {
      while (progress_current_offset >= region_start_offsets[progress_current_region]) {
        progress_current_region++;
      }
      auto progress_current_addr = regions[progress_current_region].first.offset_bytes(
          progress_current_offset - region_start_offsets[progress_current_region]);
      auto checked_bytes_str = phosg::format_size(progress_current_offset);
      auto total_bytes_str = phosg::format_size(region_start_offsets.back());
      float progress = static_cast<float>(progress_current_offset) / static_cast<float>(region_start_offsets.back());
      phosg::fwrite_fmt(stderr, "... {} ({}/{} regions, {}/{}, {:g}%)" CLEAR_LINE_TO_END "\r",
          progress_current_addr, progress_current_region, regions.size(),
          checked_bytes_str, total_bytes_str, progress * 100.0f);
      usleep(100000);
    }

    for (auto& t : threads) {
      t.join();
    }
  }

  static std::vector<std::pair<MappedPtr<void>, size_t>> ranges_for_pid(uint64_t pid);
  static void dump(uint64_t pid, const std::string& directory, size_t max_threads);

  template <typename T>
  inline bool obj_valid(MappedPtr<T> addr, uint64_t alignment = 8) const {
    using SizeType = std::conditional_t<std::is_same_v<T, void>, uint8_t, T>;
    return ((addr.addr != 0) && !(addr.addr & (alignment - 1)) && this->exists_range(addr, sizeof(SizeType)));
  }
  template <typename T>
  inline bool obj_valid_or_null(MappedPtr<T> addr, uint64_t alignment = 8) const {
    using SizeType = std::conditional_t<std::is_same_v<T, void>, uint8_t, T>;
    return ((addr.addr == 0) || !((addr.addr & (alignment - 1)) && this->exists_range(addr, sizeof(SizeType))));
  }

  template <typename T>
  MappedPtr<T> host_to_mapped(const T* host_ptr) const {
    const auto& rgn = this->find_region_by_host_addr(host_ptr);
    size_t offset = reinterpret_cast<const uint8_t*>(host_ptr) - reinterpret_cast<const uint8_t*>(rgn.data);
    if (offset >= rgn.size) {
      throw std::out_of_range("Host address not within any block");
    }
    if (offset + sizeof(T) > rgn.size) {
      throw std::out_of_range("End of host structure out of range");
    }
    return rgn.addr.offset_bytes(offset).template cast<T>();
  }

protected:
  std::unordered_set<std::shared_ptr<MemoryMappedFile>> mapped_files;
  std::map<MappedPtr<void>, MemoryMappedFile::View> regions_by_mapped;
  std::map<const void*, MemoryMappedFile::View> regions_by_host;
  size_t total_bytes;

  const MemoryMappedFile::View& find_region_by_mapped_addr(MappedPtr<void> addr) const;
  const MemoryMappedFile::View& find_region_by_host_addr(const void* addr) const;
};
