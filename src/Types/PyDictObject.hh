#pragma once

#include "PyVersion.hh"
#include "PyObject.hh"
#include "PyStringObjects.hh"

struct PyDictKeyEntry {
  uint64_t me_hash;
  MappedPtr<PyObject> me_key;
  MappedPtr<PyObject> me_value; // Only used for non-split tables

  const char* invalid_reason(const MemoryReader& r, bool is_split) const;
};

#if PYMEMTOOLS_PYTHON_VERSION == 314
struct PyDictUnicodeEntry {
  MappedPtr<PyObject> me_key;
  MappedPtr<PyObject> me_value; // Only used for non-split tables
};
#endif

struct PyDictKeysObject {
  // Note that this is not a PyObject - there's no ob_type!
  uint64_t dk_refcnt;
#if PYMEMTOOLS_PYTHON_VERSION == 314
  uint8_t dk_log2_size;
  uint8_t dk_log2_index_bytes;
  uint8_t dk_kind;
  uint8_t dk_padding;
  uint32_t dk_version;
  int64_t dk_usable;
  int64_t dk_nentries;
#else
  uint64_t dk_size;
  MappedPtr<void> dk_lookup;
  uint64_t dk_usable;
  uint64_t dk_nentries;
#endif
  // IndexType dk_indices[dk_size];
  // PyDictKeyEntry dk_entries[dk_usable + dk_nentries];

  inline size_t table_size() const {
#if PYMEMTOOLS_PYTHON_VERSION == 314
    return 1ULL << this->dk_log2_size;
#else
    return this->dk_size;
#endif
  }

  inline int64_t bytes_per_table_value() const {
#if PYMEMTOOLS_PYTHON_VERSION == 314
    size_t table = this->table_size();
    if (table == 0) {
      return 0;
    }
    size_t total_bytes = 1ULL << this->dk_log2_index_bytes;
    return total_bytes / table;
#else
    if (this->dk_size < 0x100) {
      return 1;
    } else if (this->dk_size < 0x10000) {
      return 2;
    } else if (this->dk_size < 0x100000000) {
      return 4;
    } else {
      return 8;
    }
#endif
  }

  inline size_t entry_count() const {
    return this->dk_usable + this->dk_nentries;
  }
#if PYMEMTOOLS_PYTHON_VERSION == 314
  inline bool uses_unicode_entries() const {
    return this->dk_kind != 0;
  }
  inline size_t entry_size() const {
    return this->uses_unicode_entries() ? sizeof(PyDictUnicodeEntry) : sizeof(PyDictKeyEntry);
  }
#else
  inline size_t entry_size() const {
    return sizeof(PyDictKeyEntry);
  }
#endif

  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};

#if PYMEMTOOLS_PYTHON_VERSION == 314
struct PyDictValues {
  uint8_t capacity;
  uint8_t size;
  uint8_t embedded;
  uint8_t valid;
  MappedPtr<PyObject> values[1];
};
#endif

// See https://github.com/python/cpython/blob/3.10/Include/cpython/dictobject.h
struct PyDictObject : PyObject {
  int64_t ma_used;
#if PYMEMTOOLS_PYTHON_VERSION == 314
  uint64_t _ma_watcher_tag;
  MappedPtr<PyDictKeysObject> ma_keys;
  MappedPtr<PyDictValues> ma_values; // May be null if table is combined (values stored with keys in ma_keys)
#else
  uint64_t ma_version_tag;
  MappedPtr<PyDictKeysObject> ma_keys;
  MappedPtr<MappedPtr<PyObject>> ma_values; // May be null if table is combined (values stored with keys in ma_keys)
#endif

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
