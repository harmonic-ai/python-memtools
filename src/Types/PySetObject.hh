#pragma once

#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/setobject.h
struct PySetObject : PyObject {
  struct Entry {
    MappedPtr<PyObject> key;
    uint64_t hash;
  };

  int64_t fill; // Number of active and dummy entries
  int64_t used; // Number of active entries
  int64_t mask; // Entry count = mask + 1 (which is a power of 2)
  MappedPtr<Entry> table; // Must not be null
  uint64_t hash; // Only used for frozenset objects
  int64_t finger; // Search finger for pop()
  // There are actually more entries here, but we don't include them because PySet_MINSIZE may differ:
  //   Entry smalltable[PySet_MINSIZE];
  //   PyObject* weakreflist;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  inline phosg::StringReader read_entries(const MemoryReader& r) const {
    return r.read(this->table, sizeof(Entry) * (this->mask + 1));
  }
  std::vector<MappedPtr<PyObject>> get_items(const MemoryReader& r) const;
};
