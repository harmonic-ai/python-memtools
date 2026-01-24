#pragma once

#include "PyVersion.hh"
#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/longintrepr.h
#if PYMEMTOOLS_PYTHON_VERSION == 314
struct PyLongObject : PyObject {
  uint64_t lv_tag;
  uint32_t ob_digit[0];

  inline uint64_t digit_count() const {
    return this->lv_tag >> 3;
  }
  inline uint8_t sign_tag() const {
    return this->lv_tag & 3;
  }

  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};
#else
struct PyLongObject : PyVarObject {
  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};
#endif

struct PyBoolObject : PyLongObject {
  const char* invalid_reason(const Environment& env) const;
  std::string repr(Traversal& t) const;
};
