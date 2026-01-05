#include "PySetObject.hh"

std::vector<MappedPtr<PyObject>> PySetObject::get_items(const MemoryReader& r) const {
  std::vector<MappedPtr<PyObject>> ret;
  auto entries_r = this->read_entries(r);
  while (!entries_r.eof()) {
    const auto& entry = entries_r.get<Entry>();
    if (!entry.key.is_null()) {
      ret.emplace_back(entry.key);
    }
  }
  return ret;
}

const char* PySetObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyObject::invalid_reason(env)) {
    return ir;
  }

  if (this->fill > this->mask + 1) {
    return "invalid_fill";
  }
  if (this->used > this->fill) {
    return "invalid_used";
  }

  if (!env.r.obj_valid(this->table)) {
    return "invalid_table";
  }

  auto entries_r = this->read_entries(env.r);
  while (!entries_r.eof()) {
    const auto& entry = entries_r.get<Entry>();
    if (!env.r.obj_valid_or_null(entry.key)) {
      return "invalid_entry";
    }
  }
  return nullptr;
}

std::unordered_set<MappedPtr<void>> PySetObject::direct_referents(const Environment& env) const {
  std::unordered_set<MappedPtr<void>> ret;
  for (const auto& it : this->get_items(env.r)) {
    ret.emplace(it);
  }
  return ret;
}

std::string PySetObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<set !{}>", ir);
  }
  if (!t.recursion_allowed()) {
    return "<set !recursion_depth>";
  }

  auto cycle_guard = t.cycle_guard(this);
  if (cycle_guard.is_recursive) {
    return "<set !recursive_repr>";
  }

  auto indent = t.indent();
  std::vector<std::string> repr_entries;
  bool has_extra = false;
  for (auto item_addr : this->get_items(t.env.r)) {
    if ((t.max_entries >= 0) && (repr_entries.size() >= static_cast<size_t>(t.max_entries))) {
      has_extra = true;
      break;
    }
    repr_entries.emplace_back(t.repr(item_addr));
  }

  std::string ret;
  if (repr_entries.size() == 0) {
    ret = "set()";

  } else if (repr_entries.size() == 1) {
    ret = std::format("{{{}}}", repr_entries[0]);

  } else { // 2 or more entries
    sort(repr_entries.begin(), repr_entries.end());
    ret = "{\n";
    for (const auto& e : repr_entries) {
      ret.append(t.recursion_depth * 2, ' ');
      ret += e;
      ret += ",\n";
    }
    if (has_extra) {
      ret.append(t.recursion_depth * 2, ' ');
      ret += "...\n";
    }
    ret.append((t.recursion_depth - 1) * 2, ' ');
    ret += "}";
  }
  return ret;
}
