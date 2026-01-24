#pragma once

#include "PyCodeObject.hh"
#include "PyErr_StackItem.hh"
#include "PyFrameObject.hh"
#include "PyInterpreterFrame.hh"
#include "PyObject.hh"

#if PYMEMTOOLS_PYTHON_VERSION == 314
// See https://github.com/python/cpython/blob/3.14/Include/internal/pycore_interpframe_structs.h
struct PyGenObject : PyObject {
  MappedPtr<PyObject> gi_weakreflist;
  MappedPtr<PyObject> gi_name;
  MappedPtr<PyObject> gi_qualname;
  PyErr_StackItem gi_exc_state;
  MappedPtr<PyObject> gi_origin_or_finalizer;
  char gi_hooks_inited;
  char gi_closed;
  char gi_running_async;
  int8_t gi_frame_state;
  PyInterpreterFrame gi_iframe;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};

struct PyCoroObject : PyGenObject {
  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};

struct PyAsyncGenObject : PyGenObject {
  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};
#else
// See https://github.com/python/cpython/blob/3.10/Include/genobject.h
struct PyGenObject : PyObject {
  MappedPtr<PyFrameObject> gi_frame;
  MappedPtr<PyCodeObject> gi_code;
  MappedPtr<PyObject> gi_weakreflist;
  MappedPtr<PyObject> gi_name;
  MappedPtr<PyObject> gi_qualname;
  PyErr_StackItem gi_exc_state;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};

// See https://github.com/python/cpython/blob/3.10/Include/genobject.h
struct PyCoroObject : PyGenObject {
  MappedPtr<PyObject> cr_origin;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};

// See https://github.com/python/cpython/blob/3.10/Include/genobject.h
struct PyAsyncGenObject : PyGenObject {
  MappedPtr<PyObject> ag_finalizer;
  int ag_hooks_inited;
  int ag_closed;
  int ag_running_async;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};
#endif
