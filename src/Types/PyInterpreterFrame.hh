#pragma once

#include "PyVersion.hh"

#if PYMEMTOOLS_PYTHON_VERSION == 314

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>

#include "Base.hh"
#include "PyCodeObject.hh"
#include "PyObject.hh"

struct PyFrameObject;

struct PyStackRef {
  uintptr_t bits;

  inline bool is_null() const {
    return this->bits == 1; // PyStackRef_NULL_BITS (Py_TAG_REFCNT)
  }

  inline bool is_tagged_int() const {
    return (this->bits & 3) == 3; // Py_INT_TAG
  }

  inline MappedPtr<PyObject> as_object() const {
    if (this->is_null() || this->is_tagged_int()) {
      return {};
    }
    return MappedPtr<PyObject>(this->bits & ~static_cast<uintptr_t>(1));
  }
};

struct PyInterpreterFrame {
  PyStackRef f_executable;
  MappedPtr<PyInterpreterFrame> previous;
  PyStackRef f_funcobj;
  MappedPtr<PyObject> f_globals;
  MappedPtr<PyObject> f_builtins;
  MappedPtr<PyObject> f_locals;
  MappedPtr<PyFrameObject> frame_obj;
  MappedPtr<uint16_t> instr_ptr;
  MappedPtr<PyStackRef> stackpointer;
  uint16_t return_offset;
  char owner;
  uint8_t visited;
  PyStackRef localsplus[1];

  const char* invalid_reason(const Environment& env) const;
  MappedPtr<PyCodeObject> executable_code(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  size_t code_offset(const Environment& env) const;
  std::string repr(Traversal& t) const;
};

#endif
