#pragma once

#include "Base.hh"

struct PyTypeObject;

struct PyLinkedObject {
  MappedPtr<PyObject> _ob_next;
  MappedPtr<PyObject> _ob_prev;

  const char* invalid_reason(const Environment& env) const;
};

// See https://github.com/python/cpython/blob/3.10/Include/object.h
struct PyObject {
  // Delete the copy and move constructors to prevent bugs - we should only access PyObjects (and any subclasses
  // thereof) through references returned by MemoryReader
  PyObject(const PyObject&) = delete;
  PyObject(PyObject&&) = delete;
  PyObject& operator=(const PyObject&) = delete;
  PyObject& operator=(PyObject&&) = delete;

  uint64_t ob_refcnt;
  MappedPtr<PyTypeObject> ob_type;

  const char* invalid_reason(const Environment& env) const;

  inline std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const {
    return {};
  }

  inline bool refcount_is_valid() const {
    return (
        (this->ob_refcnt & 0x4000000000000000) || // Immortable bit is set, or...
        ((this->ob_refcnt > 0) && (this->ob_refcnt < 0x10000000)) // Refcount is in a reasonable range
    );
  }
};

struct PyVarObject : PyObject {
  int64_t ob_size;
};
