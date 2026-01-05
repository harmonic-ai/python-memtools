#pragma once

#include "PyObject.hh"
#include "PyStringObjects.hh"

struct PyDictKeyEntry {
  uint64_t me_hash;
  MappedPtr<PyObject> me_key;
  MappedPtr<PyObject> me_value; // Only used for non-split tables

  const char* invalid_reason(const MemoryReader& r, bool is_split) const;
};

struct PyDictKeysObject {
  // Note that this is not a PyObject - there's no ob_type!
  uint64_t dk_refcnt;
  uint64_t dk_size;
  MappedPtr<void> dk_lookup;
  uint64_t dk_usable;
  uint64_t dk_nentries;
  // IndexType dk_indices[dk_size];
  // PyDictKeyEntry dk_entries[dk_usable + dk_nentries];

  inline int64_t bytes_per_table_value() const {
    if (this->dk_size < 0x100) {
      return 1;
    } else if (this->dk_size < 0x10000) {
      return 2;
    } else if (this->dk_size < 0x100000000) {
      return 4;
    } else {
      return 8;
    }
  }

  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};

// See https://github.com/python/cpython/blob/3.10/Include/cpython/dictobject.h
struct PyDictObject : PyObject {
  int64_t ma_used;
  uint64_t ma_version_tag;
  MappedPtr<PyDictKeysObject> ma_keys;
  MappedPtr<MappedPtr<PyObject>> ma_values; // May be null if table is combined (values stored with keys in ma_keys)

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  phosg::StringReader read_table(const MemoryReader& r) const;
  std::vector<int64_t> get_table(const MemoryReader& r) const;
  phosg::StringReader read_values(const MemoryReader& r) const;
  phosg::StringReader read_entries(const MemoryReader& r) const;
  std::vector<std::pair<MappedPtr<PyObject>, MappedPtr<PyObject>>> get_items(const MemoryReader& r) const;

  template <typename T>
  MappedPtr<T> value_for_key(const MemoryReader& r, const std::string& key) const {
    // TODO: This is slow. We should only call get_items when we actually need all the items.
    for (auto it : this->get_items(r)) {
      try {
        std::string key_str = decode_string_types(r, it.first);
        if (key_str == key) {
          return it.second.cast<T>();
        }
      } catch (const invalid_object&) {
      }
    }
    throw std::out_of_range("Key not found");
  }
};
