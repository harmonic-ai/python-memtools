#include "PyListObject.hh"

const char* PyListObject::invalid_reason(const Environment& env) const {
  if (static_cast<uint64_t>(this->ob_size) > this->allocated) {
    return "invalid_size";
  }

  if (!this->ob_item.is_null()) {
    if (!this->allocated) {
      return "invalid_alloc_count";
    }
    const auto* items = env.r.get_array(this->ob_item, this->ob_size);
    for (ssize_t z = 0; z < this->ob_size; z++) {
      const auto& item = env.r.get(items[z]);
      if (const char* ir = item.invalid_reason(env)) {
        return ir;
      }
    }
  } else if (this->allocated) {
    return "invalid_item_list";
  }
  return nullptr;
}

std::unordered_set<MappedPtr<void>> PyListObject::direct_referents(const Environment& env) const {
  std::unordered_set<MappedPtr<void>> ret;
  ret.reserve(this->ob_size);
  const auto* items = env.r.get_array(this->ob_item, this->ob_size);
  for (ssize_t z = 0; z < this->ob_size; z++) {
    ret.emplace(items[z]);
  }
  return ret;
}

std::vector<MappedPtr<PyObject>> PyListObject::get_items(const MemoryReader& r) const {
  const auto* items = r.get_array(this->ob_item, this->ob_size);
  return std::vector<MappedPtr<PyObject>>(items, items + this->ob_size);
}

std::string PyListObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<list !{}>", ir);
  }
  if (!t.recursion_allowed()) {
    return "<list !recursion_depth>";
  }

  if (this->ob_size == 0) {
    return "[]";
  }

  auto cycle_guard = t.cycle_guard(this);
  if (cycle_guard.is_recursive) {
    return "<list !recursive_repr>";
  }

  std::vector<std::string> repr_strs;
  bool has_extra = false;
  const auto* items = t.env.r.get_array(this->ob_item, this->ob_size);
  for (ssize_t z = 0; z < this->ob_size; z++) {
    if ((t.max_entries >= 0) && (repr_strs.size() >= static_cast<size_t>(t.max_entries))) {
      has_extra = true;
      break;
    }
    repr_strs.emplace_back(t.repr(items[z]));
  }

  auto indent = t.indent();
  if (repr_strs.size() == 0) {
    return "[]";

  } else if ((repr_strs.size() == 1) && !has_extra) {
    return std::format("[{}]", repr_strs[0]);

  } else { // 2 or more items
    std::string ret = "[\n";
    for (const auto& repr_str : repr_strs) {
      ret.append(t.recursion_depth * 2, ' ');
      ret += repr_str;
      ret += ",\n";
    }
    if (has_extra) {
      ret.append(t.recursion_depth * 2, ' ');
      ret += "...\n";
    }
    ret.append((t.recursion_depth - 1) * 2, ' ');
    ret += "]";
    return ret;
  }
}