#pragma once

#include "PyDictObject.hh"
#include "PyErr_StackItem.hh"
#include "PyObject.hh"

enum PyFutureState : uint8_t {
  STATE_PENDING,
  STATE_CANCELLED,
  STATE_FINISHED,
};

// See struct FutureObj in https://github.com/python/cpython/blob/3.10/Modules/_asynciomodule.c
struct PyAsyncFutureObject : PyObject {
  MappedPtr<PyObject> fut_loop;
  MappedPtr<PyObject> fut_callback0;
  MappedPtr<PyObject> fut_context0;
  MappedPtr<PyObject> fut_callbacks;
  MappedPtr<PyObject> fut_exception;
  MappedPtr<PyObject> fut_exception_tb;
  MappedPtr<PyObject> fut_result;
  MappedPtr<PyObject> fut_source_tb;
  MappedPtr<PyObject> fut_cancel_msg;
  PyFutureState fut_state;
  int fut_log_tb;
  int fut_blocking;
  MappedPtr<PyDictObject> dict;
  MappedPtr<PyObject> fut_weakreflist;
  PyErr_StackItem fut_cancelled_exc;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};

struct PyAsyncGatheringFutureObject : PyAsyncFutureObject {
  // invalid_reason inherited from PyAsyncFutureObject
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;

  std::vector<MappedPtr<PyObject>> children(const Environment& env) const;
};

// See struct TaskObj in https://github.com/python/cpython/blob/3.10/Modules/_asynciomodule.c
struct PyAsyncTaskObject : PyAsyncFutureObject {
  MappedPtr<PyObject> task_fut_waiter;
  MappedPtr<PyObject> task_coro;
  MappedPtr<PyObject> task_name;
  MappedPtr<PyObject> task_context;
  int task_must_cancel;
  int task_log_destroy_pending;

  const char* invalid_reason(const Environment& env) const;
  std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const;
  std::string repr(Traversal& t) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
};
