#pragma once

#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/floatobject.h
struct PyFloatObject : PyObject {
  double ob_fval;

  inline const char* invalid_reason(const Environment& env) const {
    return nullptr;
  }

  std::string repr(Traversal& t) const;
};
