#include "PyDictObject.hh"

#include <algorithm>
#include <cstddef>

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
  return std::format("<dict.keys size={} usable={} nentries={}>",
      this->table_size(), this->dk_usable, this->dk_nentries);
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
  if (!env.r.exists_range(table_addr, bytes_per_table_value * keys.table_size())) {
    return "invalid_ma_keys_table";
  }
  auto entries_addr = table_addr.offset_bytes(bytes_per_table_value * keys.table_size()).cast<void>();
  size_t num_entries = keys.entry_count();
  if (!env.r.exists_range(entries_addr.cast<void>(), num_entries * keys.entry_size())) {
    return "invalid_ma_keys_entries";
  }

  if (!this->ma_values.is_null()) {
#if PYMEMTOOLS_PYTHON_VERSION == 314
    if (!env.r.obj_valid(this->ma_values)) {
      return "invalid_ma_values";
    }
    const auto& values = env.r.get(this->ma_values);
    auto values_addr = this->ma_values.offset_bytes(offsetof(PyDictValues, values)).cast<PyObject>();
    if (!env.r.exists_array(values_addr, values.capacity)) {
      return "invalid_ma_values_range";
    }
#else
    if (!env.r.obj_valid(this->ma_values)) {
      return "invalid_ma_values";
    }
    if (!env.r.exists_array(this->ma_values, num_entries)) {
      return "invalid_ma_values_range";
    }
#endif
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
  return r.read(table_addr, bytes_per_table_value * keys.table_size());
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
#if PYMEMTOOLS_PYTHON_VERSION == 314
    const auto& values = r.get(this->ma_values);
    if (values.capacity == 0) {
      return phosg::StringReader();
    }
    auto values_addr = this->ma_values.offset_bytes(offsetof(PyDictValues, values)).cast<PyObject>();
    return r.read(values_addr, sizeof(uint64_t) * values.capacity);
#else
    const auto& keys = r.get(this->ma_keys);
    return r.read(this->ma_values, sizeof(uint64_t) * keys.entry_count());
#endif
  } else {
    return phosg::StringReader();
  }
}

phosg::StringReader PyDictObject::read_entries(const MemoryReader& r) const {
  const auto& keys = r.get(this->ma_keys);
  MappedPtr<void> table_addr = this->ma_keys.offset_bytes(sizeof(keys));
  size_t bytes_per_table_value = keys.bytes_per_table_value();
  auto entries_addr = table_addr.offset_bytes(bytes_per_table_value * keys.table_size()).cast<void>();
  return r.read(entries_addr, keys.entry_size() * keys.entry_count());
}

std::vector<std::pair<MappedPtr<PyObject>, MappedPtr<PyObject>>> PyDictObject::get_items(const MemoryReader& r) const {
  phosg::StringReader values_r = this->read_values(r);
  phosg::StringReader entries_r = this->read_entries(r);
  size_t value_count = values_r.size() / sizeof(uint64_t);
  const auto& keys = r.get(this->ma_keys);
  const size_t entry_size = keys.entry_size();

  std::vector<std::pair<MappedPtr<PyObject>, MappedPtr<PyObject>>> ret;
  for (int64_t table_v : this->get_table(r)) {
    if (table_v >= 0) {
      MappedPtr<PyObject> key_addr;
      MappedPtr<PyObject> value_addr;
#if PYMEMTOOLS_PYTHON_VERSION == 314
      const bool use_unicode_entries = keys.uses_unicode_entries();
      if (use_unicode_entries) {
        const auto& entry = entries_r.pget<PyDictUnicodeEntry>(table_v * entry_size);
        key_addr = entry.me_key;
        value_addr = entry.me_value;
      } else {
        const auto& entry = entries_r.pget<PyDictKeyEntry>(table_v * entry_size);
        key_addr = entry.me_key;
        value_addr = entry.me_value;
      }
#else
      const auto& entry = entries_r.pget<PyDictKeyEntry>(table_v * entry_size);
      key_addr = entry.me_key;
      value_addr = entry.me_value;
#endif
      if (values_r.size() > 0) {
        if (static_cast<size_t>(table_v) >= value_count) {
          continue;
        }
        value_addr = MappedPtr<PyObject>(values_r.pget_u64l(sizeof(uint64_t) * table_v));
      }
      ret.emplace_back(std::make_pair(key_addr, value_addr));
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
  auto entries_addr = table_addr.offset_bytes(bytes_per_table_value * keys.table_size()).cast<void>();

  std::vector<int64_t> table;
  try {
    auto table_r = t.env.r.read(table_addr, bytes_per_table_value * keys.table_size());
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
#if PYMEMTOOLS_PYTHON_VERSION == 314
      const auto& values = t.env.r.get(this->ma_values);
      auto values_addr = this->ma_values.offset_bytes(offsetof(PyDictValues, values)).cast<PyObject>();
      values_r = t.env.r.read(values_addr, sizeof(uint64_t) * values.capacity);
#else
      values_r = t.env.r.read(this->ma_values, sizeof(uint64_t) * keys.entry_count());
#endif
    } catch (const std::out_of_range&) {
      return "<dict keys:!values_unreadable>";
    }
  }

  size_t value_count = values_r.size() / sizeof(uint64_t);
#if PYMEMTOOLS_PYTHON_VERSION == 314
  const bool use_unicode_entries = keys.uses_unicode_entries();
#endif
  const size_t entry_size = keys.entry_size();
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
    auto entries_r = t.env.r.read(entries_addr, entry_size * keys.entry_count());
    for (int64_t table_v : table) {
      if (table_v < 0) {
        continue;
      }

      if ((t.max_entries >= 0) && (repr_entries.size() >= static_cast<size_t>(t.max_entries))) {
        has_extra = true;
        break;
      }

      try {
        MappedPtr<PyObject> key;
        MappedPtr<PyObject> value;
#if PYMEMTOOLS_PYTHON_VERSION == 314
        if (use_unicode_entries) {
          const auto& entry = entries_r.pget<PyDictUnicodeEntry>(table_v * entry_size);
          key = entry.me_key;
          value = entry.me_value;
        } else {
          const auto& entry = entries_r.pget<PyDictKeyEntry>(table_v * entry_size);
          key = entry.me_key;
          value = entry.me_value;
        }
#else
        const auto& entry = entries_r.pget<PyDictKeyEntry>(table_v * entry_size);
        key = entry.me_key;
        value = entry.me_value;
#endif
        if (values_r.size() > 0) {
          if (static_cast<size_t>(table_v) >= value_count) {
            repr_entries.emplace_back(std::make_pair("<!value_oob>", "<!value_oob>"));
            continue;
          }
          value = MappedPtr<PyObject>(values_r.pget_u64l(sizeof(uint64_t) * table_v));
        }
        std::string key_repr = t.repr(key);
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
