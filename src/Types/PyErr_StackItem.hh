#pragma once

#include "PyObject.hh"

// See struct _err_stackitem in https://github.com/python/cpython/blob/3.10/Include/cpython/pystate.h
struct PyErr_StackItem {
  MappedPtr<PyObject> exc_type;
  MappedPtr<PyObject> exc_value;
  MappedPtr<PyObject> exc_traceback;
  MappedPtr<PyErr_StackItem> exc_prev;

  inline const char* invalid_reason(const Environment& env) const {
    if (!env.r.obj_valid_or_null(this->exc_type, 1)) {
      return "invalid_exc_type";
    }
    if (!env.r.obj_valid_or_null(this->exc_value, 1)) {
      return "invalid_exc_value";
    }
    if (!env.r.obj_valid_or_null(this->exc_traceback, 1)) {
      return "invalid_exc_traceback";
    }
    if (!env.r.obj_valid_or_null(this->exc_prev, 1)) {
      return "invalid_exc_prev";
    }
    return nullptr;
  }

  inline std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const {
    return {this->exc_type, this->exc_value, this->exc_traceback};
  }
};
