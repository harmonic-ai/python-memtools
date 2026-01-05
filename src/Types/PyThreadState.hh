#pragma once

#include "PyErr_StackItem.hh"
#include "PyFrameObject.hh"

struct CFrame {
  int use_tracing;
  MappedPtr<CFrame> previous;
};

// See struct _ts in https://github.com/python/cpython/blob/3.10/Include/cpython/pystate.h
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
