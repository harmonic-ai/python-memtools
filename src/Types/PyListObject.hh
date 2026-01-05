#pragma once

#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/cpython/listobject.h
struct PyListObject : PyVarObject {
  MappedPtr<MappedPtr<PyObject>> ob_item;
  uint64_t allocated; // Number of slots allocated (number in use is ob_size)

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<MappedPtr<PyObject>> get_items(const MemoryReader& r) const;
};
