#pragma once

#include "Base.hh"
#include "PyCodeObject.hh"
#include "PyDictObject.hh"
#include "PyObject.hh"
#include "PyTupleObject.hh"

enum PyFrameState : int8_t {
  FRAME_CREATED = -2,
  FRAME_SUSPENDED = -1,
  FRAME_EXECUTING = 0,
  FRAME_RETURNED = 1,
  FRAME_UNWINDING = 2,
  FRAME_RAISED = 3,
  FRAME_CLEARED = 4,
};

struct PyTryBlock {
  int b_type;
  int b_handler;
  int b_level;
};

union Py_CODEUNIT {
  uint16_t cache;
  struct {
    uint8_t code;
    uint8_t arg;
  } op;
  uint16_t value_and_backoff;
};

// See struct _frame in https://github.com/python/cpython/blob/3.10/Include/cpython/frameobject.h
struct PyFrameObject : PyVarObject {
  MappedPtr<PyFrameObject> f_back;
  MappedPtr<PyCodeObject> f_code;
  MappedPtr<PyDictObject> f_builtins;
  MappedPtr<PyDictObject> f_globals;
  MappedPtr<PyObject> f_locals; // Usually PyDictObject, but technically can be any mapping
  MappedPtr<MappedPtr<PyObject>> f_valuestack;
  MappedPtr<PyObject> f_trace;
  int f_stackdepth;
  char f_trace_lines;
  char f_trace_opcodes;
  MappedPtr<PyObject> f_gen;
  int f_lasti;
  int f_lineno;
  int f_iblock;
  PyFrameState f_state;
  PyTryBlock f_blockstack[20];
  MappedPtr<PyObject> f_localsplus[0];

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;

  static std::string name_for_state(PyFrameState st);
  std::string where(Traversal& t) const;

  std::unordered_map<MappedPtr<PyObject>, MappedPtr<PyObject>> locals(const Environment& env) const;

  inline bool is_runnable_or_running() const {
    return static_cast<int8_t>(this->f_state) <= static_cast<int8_t>(PyFrameState::FRAME_EXECUTING);
  }
  inline bool is_running() const {
    return this->f_state == PyFrameState::FRAME_EXECUTING;
  }
};