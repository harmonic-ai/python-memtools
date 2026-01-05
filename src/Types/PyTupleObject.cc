#include "PyTupleObject.hh"

std::vector<MappedPtr<PyObject>> PyTupleObject::get_items() const {
  return std::vector<MappedPtr<PyObject>>(this->items, this->items + this->ob_size);
}

const char* PyTupleObject::invalid_reason(const Environment& env) const {
  if (this->ob_size == 0) {
    return nullptr;
  }
  if (!env.r.exists_range(env.r.host_to_mapped(this), sizeof(PyTupleObject) + this->ob_size * sizeof(uint64_t))) {
    return "items_out_of_range";
  }
  try {
    for (ssize_t z = 0; z < this->ob_size; z++) {
      // Note that we call PyObject::invalid_reason here, not env.invalid_reason; this is because invalid_reason must
      // not be recursive (the caller is responsible for calling env.invalid_reason on any item before using it)
      if (const char* ir = env.r.get(this->items[z]).invalid_reason(env)) {
        return ir;
      }
    }
  } catch (const std::out_of_range&) {
    return "invalid_item_ptr";
  }
  return nullptr;
}

std::unordered_set<MappedPtr<void>> PyTupleObject::direct_referents(const Environment&) const {
  // It's assumed that invalid_reason returned nullptr before this is called, so it's safe to access the items directly
  std::unordered_set<MappedPtr<void>> ret;
  for (ssize_t z = 0; z < this->ob_size; z++) {
    ret.emplace(this->items[z]);
  }
  return ret;
}

std::string PyTupleObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<tuple !{}>", ir);
  }
  if (!t.recursion_allowed()) {
    return std::format("<tuple !recursion_depth>");
  }

  auto cycle_guard = t.cycle_guard(this);
  if (cycle_guard.is_recursive) {
    return std::format("<tuple !recursive_repr>");
  }

  auto indent = t.indent();
  std::vector<std::string> repr_strs;
  bool has_extra = false;
  for (ssize_t z = 0; z < this->ob_size; z++) {
    if ((t.max_entries >= 0) && (repr_strs.size() >= static_cast<size_t>(t.max_entries))) {
      has_extra = true;
      break;
    }
    repr_strs.emplace_back(t.repr(this->items[z]));
  }

  if (repr_strs.size() == 0) {
    return "()";

  } else if (repr_strs.size() == 1 && !has_extra) {
    return std::format("({},)", repr_strs[0]);

  } else { // 2 or more items
    std::string ret = "(\n";
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
    ret += ")";
    return ret;
  }
}
