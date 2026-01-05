#pragma once

#include "PyObject.hh"
#include "PyStringObjects.hh"
#include "PyTupleObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/cpython/code.h
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

  const char* invalid_reason(const Environment& env) const;

  inline std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const {
    return {this->co_code, this->co_consts, this->co_names, this->co_varnames, this->co_freevars, this->co_cellvars,
        this->co_cell2arg, this->co_filename, this->co_name, this->co_linetable, this->co_zombieframe,
        this->co_weakreflist, this->co_extra, this->co_opcache_map, this->co_opcache};
  }

  std::string repr(Traversal& t) const;

  size_t line_number_for_code_offset(const Environment& env, size_t code_offset) const;
};
