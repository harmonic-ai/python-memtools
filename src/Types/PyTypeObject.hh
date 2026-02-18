#pragma once

#include "PyObject.hh"

struct PyDictObject;
struct PyTupleObject;

struct PyMemberDef {
  /* 00 */ MappedPtr<char> name;
  /* 08 */ int type;
  /* 10 */ ssize_t offset;
  /* 18 */ int flags;
  /* 20 */ MappedPtr<char> doc;
  /* 28 */

  std::string repr(const MemoryReader& r) const;
};

// See struct _typeobject in https://github.com/python/cpython/blob/3.10/Include/cpython/object.h
struct PyTypeObject : PyVarObject {
  /* 0000 */ MappedPtr<char> tp_name;
  /* 0008 */ int64_t tp_basicsize;
  /* 0010 */ int64_t tp_itemsize;
  /* 0018 */ MappedPtr<void> tp_dealloc;
  /* 0020 */ int64_t tp_vectorcall_offset;
  /* 0028 */ MappedPtr<void> tp_getattr;
  /* 0030 */ MappedPtr<void> tp_setattr;
  /* 0038 */ MappedPtr<void> tp_as_async; // PyAsyncMethods*
  /* 0040 */ MappedPtr<void> tp_repr;
  /* 0048 */ MappedPtr<void> tp_as_number; // PyNumberMethods*
  /* 0050 */ MappedPtr<void> tp_as_sequence; // PySequenceMethods*
  /* 0058 */ MappedPtr<void> tp_as_mapping; // PyMappingMethods*
  /* 0060 */ MappedPtr<void> tp_hash;
  /* 0068 */ MappedPtr<void> tp_call;
  /* 0070 */ MappedPtr<void> tp_str;
  /* 0078 */ MappedPtr<void> tp_getattro;
  /* 0080 */ MappedPtr<void> tp_setattro;
  /* 0088 */ MappedPtr<void> tp_as_buffer; // PyBufferProcs*
  /* 0090 */ unsigned long tp_flags;
  /* 0098 */ MappedPtr<char> tp_doc;
  /* 00A0 */ MappedPtr<void> tp_traverse;
  /* 00A8 */ MappedPtr<void> tp_clear;
  /* 00B0 */ MappedPtr<void> tp_richcompare;
  /* 00B8 */ int64_t tp_weaklistoffset;
  /* 00C0 */ MappedPtr<void> tp_iter;
  /* 00C8 */ MappedPtr<void> tp_iternext;
  /* 00D0 */ MappedPtr<void> tp_methods; // PyMethodDef*
  /* 00D8 */ MappedPtr<PyMemberDef> tp_members;
  /* 00E0 */ MappedPtr<void> tp_getset; // PyGetSetDef*
  /* 00E8 */ MappedPtr<PyTypeObject> tp_base;
  /* 00F0 */ MappedPtr<PyDictObject> tp_dict;
  /* 00F8 */ MappedPtr<void> tp_descr_get;
  /* 0100 */ MappedPtr<void> tp_descr_set;
  /* 0108 */ int64_t tp_dictoffset;
  /* 0110 */ MappedPtr<void> tp_init;
  /* 0118 */ MappedPtr<void> tp_alloc;
  /* 0120 */ MappedPtr<void> tp_new;
  /* 0128 */ MappedPtr<void> tp_free;
  /* 0130 */ MappedPtr<void> tp_is_gc;
  /* 0138 */ MappedPtr<PyTupleObject> tp_bases;
  /* 0140 */ MappedPtr<PyObject> tp_mro;
  /* 0148 */ MappedPtr<PyObject> tp_cache;
  /* 0150 */ MappedPtr<PyTupleObject> tp_subclasses;
  /* 0158 */ MappedPtr<PyObject> tp_weaklist;
  /* 0160 */ MappedPtr<void> tp_del;
  /* 0168 */ unsigned int tp_version_tag;
  /* 0170 */ MappedPtr<void> tp_finalize;
  /* 0178 */ MappedPtr<void> tp_vectorcall; // Not checked during invalid_reason

  const char* invalid_reason(const Environment& env) const;
  inline std::unordered_set<MappedPtr<void>> direct_referents(const Environment& env) const {
    return {
        this->tp_name, this->tp_dealloc, this->tp_getattr, this->tp_setattr, this->tp_as_async, this->tp_repr,
        this->tp_as_number, this->tp_as_sequence, this->tp_as_mapping, this->tp_hash, this->tp_call, this->tp_str,
        this->tp_getattro, this->tp_setattro, this->tp_as_buffer, this->tp_doc, this->tp_traverse, this->tp_clear,
        this->tp_richcompare, this->tp_iter, this->tp_iternext, this->tp_methods, this->tp_members, this->tp_getset,
        this->tp_base, this->tp_dict, this->tp_descr_get, this->tp_descr_set, this->tp_init, this->tp_alloc,
        this->tp_new, this->tp_free, this->tp_is_gc, this->tp_bases, this->tp_mro, this->tp_cache, this->tp_subclasses,
        this->tp_weaklist, this->tp_del, this->tp_finalize, this->tp_vectorcall};
  }
  std::string repr(Traversal& t) const;

  static bool type_name_is_valid(const std::string& name);
  std::string name(const MemoryReader& r) const;

  // Returns [(name, offset)]
  std::vector<std::pair<std::string, ssize_t>> slots(const MemoryReader& r) const;
};
