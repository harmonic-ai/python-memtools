#pragma once

#include <phosg/Arguments.hh>
#include <phosg/JSON.hh>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../MemoryReader.hh"

struct PyObject;
struct PyTypeObject;
struct Traversal;

class invalid_object : public std::runtime_error {
public:
  invalid_object(const char* reason) : runtime_error("Invalid object"), reason(reason) {}
  ~invalid_object() = default;

  const char* reason;
};

struct Environment {
  std::string data_path;
  std::string analysis_filename;

  const MemoryReader r;
  MappedPtr<PyTypeObject> base_type_object;
  std::unordered_map<std::string, MappedPtr<PyTypeObject>> type_objects;

  Environment() = delete;
  explicit Environment(const std::string& data_path);

  void save_analysis() const;

  inline MappedPtr<PyTypeObject> get_type_if_exists(const char* name) const {
    try {
      return this->type_objects.at(name);
    } catch (const std::out_of_range&) {
      return MappedPtr<PyTypeObject>();
    }
  }

  inline MappedPtr<PyTypeObject> get_type(const char* name) const {
    try {
      return this->type_objects.at(name);
    } catch (const std::out_of_range&) {
      throw std::runtime_error(std::format("{} type must not be missing; run find-all-types first", name));
    }
  }

  const char* invalid_reason(
      MappedPtr<PyObject> addr, MappedPtr<PyTypeObject> expected_type = MappedPtr<PyTypeObject>{0}) const;
  std::unordered_set<MappedPtr<void>> direct_referents(MappedPtr<PyObject> addr) const;

  Traversal traverse(phosg::Arguments* args = nullptr) const; // Can't be inlined because Traversal is incomplete here
};

struct Traversal {
  const Environment& env;
  std::unordered_set<const void*> in_progress; // These are host pointers, not mapped pointers
  ssize_t recursion_depth = 0;
  ssize_t max_recursion_depth = -1; // -1 means no limit. 0 is valid here; it just means no recursion allowed at all
  ssize_t max_entries = -1;
  size_t max_string_length = 0x400; // 1KB
  bool frame_omit_back = false;
  bool frame_omit_locals = false;
  bool bytes_as_hex = false;
  bool show_all_addresses = false;
  bool is_valid = true;
  bool is_short = false;

  Traversal(const Environment& env, phosg::Arguments* args = nullptr);

  inline bool recursion_allowed() const {
    return (this->max_recursion_depth < 0) || (this->recursion_depth < this->max_recursion_depth);
  }

  template <typename T>
  const char* check_valid(const T& obj) {
    const char* reason;
    if constexpr (requires { obj.invalid_reason(this->env); }) {
      reason = obj.invalid_reason(this->env);
    } else {
      reason = obj.invalid_reason(this->env.r);
    }
    if (reason) {
      this->is_valid = false;
    }
    return reason;
  }

  std::string repr(MappedPtr<PyObject> addr);

  struct CycleGuard {
    Traversal& t;
    const void* addr;
    bool is_recursive;
    inline CycleGuard(Traversal& t, const void* addr) : t(t), addr(addr) {
      this->is_recursive = !this->t.in_progress.emplace(this->addr).second;
    }
    inline ~CycleGuard() {
      if (!this->is_recursive) {
        this->t.in_progress.erase(this->addr);
      }
    }
  };
  inline CycleGuard cycle_guard(const void* addr) {
    return CycleGuard(*this, addr);
  }

  struct IndentGuard {
    Traversal& t;
    inline IndentGuard(Traversal& t) : t(t) {
      this->t.recursion_depth++;
    }
    inline ~IndentGuard() {
      this->t.recursion_depth--;
    }
  };
  inline IndentGuard indent() {
    return IndentGuard(*this);
  }

  template <typename T>
  std::string token_repr(const T& obj, const char* type_name) {
    {
      const char* ir = this->check_valid(obj);
      if (!ir && !this->recursion_allowed()) {
        ir = "recursion_depth";
      }
      if (ir) {
        return std::format("<{} !{}>", type_name, ir);
      }
    }

    auto cycle_guard = this->cycle_guard(&obj);
    if (cycle_guard.is_recursive) {
      return std::format("<{} !recursive_repr>", type_name);
    }
    auto indent = this->indent();
    auto tokens = obj.repr_tokens(*this);
    if (this->is_short) {
      std::string ret = std::format("<{}", type_name);
      for (const auto& token : tokens) {
        ret.append(1, ' ');
        ret.append(token);
      }
      ret += ">";
      return ret;
    } else {
      std::string ret = std::format("<{}\n", type_name);
      for (const auto& token : tokens) {
        ret.append(this->recursion_depth * 2, ' ');
        ret.append(token);
        ret.append(1, '\n');
      }
      ret.append((this->recursion_depth - 1) * 2, ' ');
      ret += ">";
      return ret;
    }
  }
};
