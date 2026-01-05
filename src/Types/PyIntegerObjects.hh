#pragma once

#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/longintrepr.h
struct PyLongObject : PyVarObject {
  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};

struct PyBoolObject : PyLongObject {
  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};
