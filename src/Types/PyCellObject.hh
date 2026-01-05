#pragma once

#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/cellobject.h
struct PyCellObject : PyObject {
  MappedPtr<PyObject> ob_ref;

  const char* invalid_reason(const Environment& env) const;
  inline std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const {
    return {this->ob_ref};
  }
  std::string repr(Traversal& t) const;
};