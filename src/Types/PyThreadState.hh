#pragma once

#include "PyVersion.hh"
#include "PyErr_StackItem.hh"
#include "PyFrameObject.hh"

struct CFrame {
  int use_tracing;
  MappedPtr<CFrame> previous;
};

// See struct _ts in https://github.com/python/cpython/blob/3.10/Include/cpython/pystate.h
#if PYMEMTOOLS_PYTHON_VERSION == 314
struct PyThreadState { // Note: not a PyObject!
  MappedPtr<PyThreadState> prev;
  MappedPtr<PyThreadState> next;
  MappedPtr<void> interp; // PyInterpreterState*
  uintptr_t eval_breaker;
  uint32_t _status;
  int holds_gil;
  int _whence;
  int state;
  int py_recursion_remaining;
  int py_recursion_limit;
  int recursion_headroom;
  int tracing;
  int what_event;
  MappedPtr<void> current_frame; // _PyInterpreterFrame*
  MappedPtr<void> c_profilefunc; // Py_tracefunc
  MappedPtr<void> c_tracefunc; // Py_tracefunc
  MappedPtr<PyObject> c_profileobj;
  MappedPtr<PyObject> c_traceobj;
  MappedPtr<PyObject> current_exception;
  MappedPtr<PyErr_StackItem> exc_info;
  MappedPtr<PyObject> dict;
  int gilstate_counter;
  MappedPtr<PyObject> async_exc;
  unsigned long thread_id;
  unsigned long native_thread_id;
  MappedPtr<PyObject> delete_later;
  uintptr_t critical_section;
  int coroutine_origin_tracking_depth;
  MappedPtr<PyObject> async_gen_firstiter;
  MappedPtr<PyObject> async_gen_finalizer;
  MappedPtr<PyObject> context;
  uint64_t context_ver;
  uint64_t id;
  MappedPtr<void> datastack_chunk;
  MappedPtr<void> datastack_top;
  MappedPtr<void> datastack_limit;
  PyErr_StackItem exc_state;
  MappedPtr<PyObject> current_executor;

  const char* invalid_reason(const Environment& r) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
  std::string repr(Traversal& t) const;
};
#else
struct PyThreadState { // Note: not a PyObject!
  MappedPtr<PyThreadState> prev;
  MappedPtr<PyThreadState> next;
  MappedPtr<void> interp; // PyInterpreterState*
  MappedPtr<PyFrameObject> frame; // May be null
  int recursion_depth;
  int recursion_headroom;
  int stackcheck_counter;
  int tracing;
  MappedPtr<CFrame> cframe; // CFrame*
  MappedPtr<void> c_profilefunc; // Py_tracefunc; probably nullable)
  MappedPtr<void> c_tracefunc; // Py_tracefunc; probably nullable)
  MappedPtr<PyObject> c_profileobj; // Probably nullable
  MappedPtr<PyObject> c_traceobj; // Probably nullable
  MappedPtr<PyObject> curexc_type; // Probably nullable
  MappedPtr<PyObject> curexc_value; // Probably nullable
  MappedPtr<PyObject> curexc_traceback; // Probably nullable
  PyErr_StackItem exc_state;
  MappedPtr<PyErr_StackItem> exc_info;
  MappedPtr<PyObject> dict;
  int gilstate_counter;
  MappedPtr<PyObject> async_exc;
  unsigned long thread_id;
  int trash_delete_nesting;
  MappedPtr<PyObject> trash_delete_later;
  MappedPtr<void (*)(void*)> on_delete;
  MappedPtr<void> on_delete_data; // Argument for on_delete
  int coroutine_origin_tracking_depth;
  MappedPtr<PyObject> async_gen_firstiter;
  MappedPtr<PyObject> async_gen_finalizer;
  MappedPtr<PyObject> context;
  uint64_t context_ver;
  uint64_t id;
  CFrame root_cframe;

  const char* invalid_reason(const Environment& r) const;

  std::vector<std::string> repr_tokens(Traversal& t) const;
  std::string repr(Traversal& t) const;
};
#endif
