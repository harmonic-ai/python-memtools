#include "PyDictObject.hh"

#include <algorithm>

const char* PyDictKeyEntry::invalid_reason(const MemoryReader& r, bool is_split) const {
  if (!r.obj_valid(this->me_key)) {
    return "invalid_key";
  }
  if (!is_split && (!r.obj_valid(this->me_value))) {
    return "invalid_value";
  }
  return nullptr;
}

const char* PyDictKeysObject::invalid_reason(const Environment& env) const {
  return nullptr;
}

std::string PyDictKeysObject::repr(Traversal& t) const {
  return std::format("<dict.keys size={} usable={} nentries={}>", this->dk_size, this->dk_usable, this->dk_nentries);
}

const char* PyDictObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyObject::invalid_reason(env)) {
    return ir;
  }

  if (!env.r.obj_valid(this->ma_keys)) {
    return "invalid_ma_keys";
  }
  const auto& keys = env.r.get(this->ma_keys);
  if (const char* ir = keys.invalid_reason(env)) {
    return ir;
  }

  size_t bytes_per_table_value = keys.bytes_per_table_value();
  MappedPtr<void> table_addr = this->ma_keys.offset_bytes(sizeof(keys));
  if (!env.r.exists_range(table_addr, bytes_per_table_value * keys.dk_size)) {
    return "invalid_ma_keys_table";
  }
  auto entries_addr = table_addr.offset_bytes(bytes_per_table_value * keys.dk_size).cast<PyDictKeyEntry>();
  size_t num_entries = keys.dk_usable + keys.dk_nentries;
  if (!env.r.exists_array(entries_addr, num_entries)) {
    return "invalid_ma_keys_entries";
  }

  if (!this->ma_values.is_null()) {
    if (!env.r.obj_valid(this->ma_values)) {
      return "invalid_ma_values";
    }
    if (!env.r.exists_array(this->ma_values, num_entries)) {
      return "invalid_ma_values_range";
    }
  }

  for (const auto& [key, value] : this->get_items(env.r)) {
    if (!env.r.obj_valid(key) || !env.r.obj_valid(value)) {
      return "invalid_entry";
    }
    if (const char* ir = env.r.get(key).invalid_reason(env)) {
      return ir;
    }
    if (const char* ir = env.r.get(value).invalid_reason(env)) {
      return ir;
    }
  }
  return nullptr;
}

phosg::StringReader PyDictObject::read_table(const MemoryReader& r) const {
  const auto& keys = r.get(this->ma_keys);
  MappedPtr<void> table_addr = this->ma_keys.offset_bytes(sizeof(keys));
  size_t bytes_per_table_value = keys.bytes_per_table_value();
  return r.read(table_addr, bytes_per_table_value * keys.dk_size);
}

std::vector<int64_t> PyDictObject::get_table(const MemoryReader& r) const {
  const auto& keys = r.get(this->ma_keys);
  size_t bytes_per_table_value = keys.bytes_per_table_value();

  std::vector<int64_t> table;
  auto table_r = this->read_table(r);
  while (!table_r.eof()) {
    if (bytes_per_table_value == 1) {
      table.emplace_back(table_r.get_s8());
    } else if (bytes_per_table_value == 2) {
      table.emplace_back(table_r.get_s16l());
    } else if (bytes_per_table_value == 4) {
      table.emplace_back(table_r.get_s32l());
    } else {
      table.emplace_back(table_r.get_s64l());
    }
  }
  return table;
}

phosg::StringReader PyDictObject::read_values(const MemoryReader& r) const {
  if (!this->ma_values.is_null()) {
    const auto& keys = r.get(this->ma_keys);
    return r.read(this->ma_values, sizeof(uint64_t) * (keys.dk_usable + keys.dk_nentries));
  } else {
    return phosg::StringReader();
  }
}

phosg::StringReader PyDictObject::read_entries(const MemoryReader& r) const {
  const auto& keys = r.get(this->ma_keys);
  MappedPtr<void> table_addr = this->ma_keys.offset_bytes(sizeof(keys));
  size_t bytes_per_table_value = keys.bytes_per_table_value();
  auto entries_addr = table_addr.offset_bytes(bytes_per_table_value * keys.dk_size).cast<PyDictKeyEntry>();
  return r.read(entries_addr, sizeof(PyDictKeyEntry) * (keys.dk_usable + keys.dk_nentries));
}

std::vector<std::pair<MappedPtr<PyObject>, MappedPtr<PyObject>>> PyDictObject::get_items(const MemoryReader& r) const {
  phosg::StringReader values_r = this->read_values(r);
  phosg::StringReader entries_r = this->read_entries(r);

  std::vector<std::pair<MappedPtr<PyObject>, MappedPtr<PyObject>>> ret;
  for (int64_t table_v : this->get_table(r)) {
    if (table_v >= 0) {
      const auto& entry = entries_r.pget<PyDictKeyEntry>(table_v * sizeof(PyDictKeyEntry));
      MappedPtr<PyObject> value_addr = (values_r.size() > 0)
          ? MappedPtr<PyObject>(values_r.pget_u64l(sizeof(uint64_t) * table_v))
          : entry.me_value;
      ret.emplace_back(std::make_pair(entry.me_key, value_addr));
    }
  }
  return ret;
}

std::unordered_set<MappedPtr<void>> PyDictObject::direct_referents(const Environment& env) const {
  std::unordered_set<MappedPtr<void>> ret{this->ma_keys, this->ma_values};
  for (const auto& it : this->get_items(env.r)) {
    ret.emplace(it.first);
    ret.emplace(it.second);
  }
  return ret;
}

std::string PyDictObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<dict !{}>", ir);
  }

  const auto& keys = t.env.r.get(this->ma_keys);
  if (const char* ir = t.check_valid(keys)) {
    return std::format("<dict keys:!{}>", ir);
  }

  size_t bytes_per_table_value = keys.bytes_per_table_value();
  MappedPtr<void> table_addr = this->ma_keys.offset_bytes(sizeof(keys));
  auto entries_addr = table_addr.offset_bytes(bytes_per_table_value * keys.dk_size).cast<PyDictKeyEntry>();

  std::vector<int64_t> table;
  try {
    auto table_r = t.env.r.read(table_addr, bytes_per_table_value * keys.dk_size);
    while (!table_r.eof()) {
      if (bytes_per_table_value == 1) {
        table.emplace_back(table_r.get_s8());
      } else if (bytes_per_table_value == 2) {
        table.emplace_back(table_r.get_s16l());
      } else if (bytes_per_table_value == 4) {
        table.emplace_back(table_r.get_s32l());
      } else {
        table.emplace_back(table_r.get_s64l());
      }
    }
  } catch (const std::out_of_range&) {
    return "<dict keys:!table_unreadable>";
  }

  phosg::StringReader values_r;
  if (!this->ma_values.is_null()) {
    try {
      values_r = t.env.r.read(this->ma_values, sizeof(uint64_t) * (keys.dk_usable + keys.dk_nentries));
    } catch (const std::out_of_range&) {
      return "<dict keys:!values_unreadable>";
    }
  }

  auto cycle_guard = t.cycle_guard(this);
  if (cycle_guard.is_recursive) {
    return "<dict !recursive_repr>";
  }

  if (!t.recursion_allowed()) {
    return std::format("<dict !recursion_depth len={}>", this->ma_used);
  }

  auto indent = t.indent();
  std::vector<std::pair<std::string, std::string>> repr_entries;
  bool has_extra = false;
  try {
    auto entries_r = t.env.r.read(entries_addr, sizeof(PyDictKeyEntry) * (keys.dk_usable + keys.dk_nentries));
    for (int64_t table_v : table) {
      if (table_v < 0) {
        continue;
      }

      if ((t.max_entries >= 0) && (repr_entries.size() >= static_cast<size_t>(t.max_entries))) {
        has_extra = true;
        break;
      }

      try {
        const auto& entry = entries_r.pget<PyDictKeyEntry>(table_v * sizeof(PyDictKeyEntry));
        MappedPtr<PyObject> value = (values_r.size() > 0)
            ? MappedPtr<PyObject>(values_r.pget_u64l(sizeof(uint64_t) * table_v))
            : entry.me_value;
        std::string key_repr = t.repr(entry.me_key);
        std::string value_repr = t.repr(value);
        repr_entries.emplace_back(make_pair(std::move(key_repr), std::move(value_repr)));
      } catch (const std::out_of_range&) {
        repr_entries.emplace_back(std::make_pair("<!key_entry_unreadable>", "<!key_entry_unreadable>"));
        continue;
      }
    }
  } catch (const std::out_of_range&) {
    return "<dict keys:!entries_unreadable>";
  }

  std::string ret;
  if (repr_entries.size() == 0) {
    ret = "{}";

  } else if (repr_entries.size() == 1 && !has_extra) {
    ret = std::format("{{{}: {}}}", repr_entries[0].first, repr_entries[0].second);

  } else { // 2 or more entries
    sort(repr_entries.begin(), repr_entries.end());
    ret = "{\n";
    for (const auto& e : repr_entries) {
      ret.append(t.recursion_depth * 2, ' ');
      ret += e.first;
      ret += ": ";
      ret += e.second;
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
