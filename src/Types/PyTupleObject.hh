#pragma once

#include "PyObject.hh"

// See struct PyTupleObject in https://github.com/python/cpython/blob/3.10/Include/cpython/tupleobject.h
struct PyTupleObject : PyVarObject {
  MappedPtr<PyObject> items[0];

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<MappedPtr<PyObject>> get_items() const;
};
