#pragma once

#include "PyVersion.hh"
#include "PyObject.hh"
#include "PyStringObjects.hh"
#include "PyTupleObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/cpython/code.h
#if PYMEMTOOLS_PYTHON_VERSION == 314
struct PyCodeObject : PyVarObject {
  MappedPtr<PyObject> co_consts; // List (constants used)
  MappedPtr<PyObject> co_names; // List of strings (names used)
  MappedPtr<PyObject> co_exceptiontable; // Bytes (exception handling table)
  int32_t co_flags; // CO_* flags
  int32_t co_argcount;
  int32_t co_posonlyargcount;
  int32_t co_kwonlyargcount;
  int32_t co_stacksize;
  int32_t co_firstlineno;
  int32_t co_nlocalsplus;
  int32_t co_framesize;
  int32_t co_nlocals;
  int32_t co_ncellvars;
  int32_t co_nfreevars;
  uint32_t co_version;
  MappedPtr<PyObject> co_localsplusnames; // Tuple mapping offsets to names
  MappedPtr<PyObject> co_localspluskinds; // Bytes mapping local kinds
  MappedPtr<PyObject> co_filename; // String (where it was loaded from)
  MappedPtr<PyObject> co_name; // String (name, for reference)
  MappedPtr<PyObject> co_qualname; // String (qualname, for reference)
  MappedPtr<PyBytesObject> co_linetable; // See Objects/lnotab_notes.txt for details (or line_number_for_code_offset)
  MappedPtr<PyObject> co_weakreflist; // Supports weakrefs to code objects
  MappedPtr<void> co_executors;
  MappedPtr<void> _co_cached;
  uint64_t _co_instrumentation_version;
  MappedPtr<void> _co_monitoring;
  int64_t _co_unique_id;
  int32_t _co_firsttraceable;
  MappedPtr<void> co_extra;
  uint8_t co_code_adaptive[1];
#else
struct PyCodeObject : PyObject {
  int co_argcount;
  int co_posonlyargcount;
  int co_kwonlyargcount;
  int co_nlocals;
  int co_stacksize;
  int co_flags; // CO_* flags
  int co_firstlineno;
  MappedPtr<PyObject> co_code; // Compiled opcodes
  MappedPtr<PyObject> co_consts; // List (constants used)
  MappedPtr<PyObject> co_names; // List of strings (names used)
  MappedPtr<PyTupleObject> co_varnames; // Tuple of strings (local variable names)
  MappedPtr<PyTupleObject> co_freevars; // Tuple of strings (free variable names)
  MappedPtr<PyTupleObject> co_cellvars; // Tuple of strings (cell variable names)
  MappedPtr<int64_t> co_cell2arg; // Maps cell vars which are arguments
  MappedPtr<PyObject> co_filename; // String (where it was loaded from)
  MappedPtr<PyObject> co_name; // String (name, for reference)
  MappedPtr<PyBytesObject> co_linetable; // See Objects/lnotab_notes.txt for details (or line_number_for_code_offset)
  MappedPtr<void> co_zombieframe; // For optimization only (see frameobject.c)
  MappedPtr<PyObject> co_weakreflist; // Supports weakrefs to code objects
  MappedPtr<void> co_extra;
  MappedPtr<uint8_t> co_opcache_map;
  MappedPtr<void> co_opcache; // PyOpcache* (which we don't implement)
  int co_opcache_flag;
  uint8_t co_opcache_size;
#endif

  const char* invalid_reason(const Environment& env) const;

  inline std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const {
#if PYMEMTOOLS_PYTHON_VERSION == 314
    return {this->co_consts, this->co_names, this->co_exceptiontable, this->co_localsplusnames, this->co_localspluskinds,
        this->co_filename, this->co_name, this->co_qualname, this->co_linetable, this->co_weakreflist,
        this->co_executors, this->_co_cached, this->_co_monitoring, this->co_extra};
#else
    return {this->co_code, this->co_consts, this->co_names, this->co_varnames, this->co_freevars, this->co_cellvars,
        this->co_cell2arg, this->co_filename, this->co_name, this->co_linetable, this->co_zombieframe,
        this->co_weakreflist, this->co_extra, this->co_opcache_map, this->co_opcache};
#endif
  }

  std::string repr(Traversal& t) const;

  size_t line_number_for_code_offset(const Environment& env, size_t code_offset) const;
};
