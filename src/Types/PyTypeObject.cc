#include "PyTypeObject.hh"
#include "PyDictObject.hh"
#include "PyTupleObject.hh"

bool PyTypeObject::type_name_is_valid(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  if (!isalpha(name[0]) && (name[0] != '_')) {
    return false;
  }
  for (char ch : name) {
    if (!isalnum(ch) && (ch != '_') && (ch != '.')) {
      return false;
    }
  }
  return true;
}

std::string PyTypeObject::name(const MemoryReader& r) const {
  std::string name = r.get_cstr(this->tp_name);
  return this->type_name_is_valid(name) ? name : "";
}

const char* PyTypeObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyVarObject::invalid_reason(env)) {
    return ir;
  }
  if (!env.r.obj_valid(this->tp_name, 1)) {
    return "invalid_tp_name";
  }
  if (!env.r.obj_valid_or_null(this->tp_dealloc, 1)) {
    return "invalid_tp_dealloc";
  }
  if (!env.r.obj_valid_or_null(this->tp_getattr, 1)) {
    return "invalid_tp_getattr";
  }
  if (!env.r.obj_valid_or_null(this->tp_setattr, 1)) {
    return "invalid_tp_setattr";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_async, 1)) {
    return "invalid_tp_as_async";
  }
  if (!env.r.obj_valid_or_null(this->tp_repr, 1)) {
    return "invalid_tp_repr";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_number, 1)) {
    return "invalid_tp_as_number";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_sequence, 1)) {
    return "invalid_tp_as_sequence";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_mapping, 1)) {
    return "invalid_tp_as_mapping";
  }
  if (!env.r.obj_valid_or_null(this->tp_hash, 1)) {
    return "invalid_tp_hash";
  }
  if (!env.r.obj_valid_or_null(this->tp_call, 1)) {
    return "invalid_tp_call";
  }
  if (!env.r.obj_valid_or_null(this->tp_str, 1)) {
    return "invalid_tp_str";
  }
  if (!env.r.obj_valid_or_null(this->tp_getattro, 1)) {
    return "invalid_tp_getattro";
  }
  if (!env.r.obj_valid_or_null(this->tp_setattro, 1)) {
    return "invalid_tp_setattro";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_buffer, 1)) {
    return "invalid_tp_as_buffer";
  }
  if (!env.r.obj_valid_or_null(this->tp_doc, 1)) {
    return "invalid_tp_doc";
  }
  if (!env.r.obj_valid_or_null(this->tp_traverse, 1)) {
    return "invalid_tp_traverse";
  }
  if (!env.r.obj_valid_or_null(this->tp_clear, 1)) {
    return "invalid_tp_clear";
  }
  if (!env.r.obj_valid_or_null(this->tp_richcompare, 1)) {
    return "invalid_tp_richcompare";
  }
  if (!env.r.obj_valid_or_null(this->tp_iter, 1)) {
    return "invalid_tp_iter";
  }
  if (!env.r.obj_valid_or_null(this->tp_iternext, 1)) {
    return "invalid_tp_iternext";
  }
  if (!env.r.obj_valid_or_null(this->tp_methods, 1)) {
    return "invalid_tp_methods";
  }
  if (!env.r.obj_valid_or_null(this->tp_members, 1)) {
    return "invalid_tp_members";
  }
  if (!env.r.obj_valid_or_null(this->tp_getset, 1)) {
    return "invalid_tp_getset";
  }
  if (!env.r.obj_valid_or_null(this->tp_base, 1)) {
    return "invalid_tp_base";
  }
  if (!env.r.obj_valid_or_null(this->tp_dict, 1)) {
    return "invalid_tp_dict";
  }
  if (!env.r.obj_valid_or_null(this->tp_descr_get, 1)) {
    return "invalid_tp_descr_get";
  }
  if (!env.r.obj_valid_or_null(this->tp_descr_set, 1)) {
    return "invalid_tp_descr_set";
  }
  if (!env.r.obj_valid_or_null(this->tp_init, 1)) {
    return "invalid_tp_init";
  }
  if (!env.r.obj_valid_or_null(this->tp_alloc, 1)) {
    return "invalid_tp_alloc";
  }
  if (!env.r.obj_valid_or_null(this->tp_new, 1)) {
    return "invalid_tp_new";
  }
  if (!env.r.obj_valid_or_null(this->tp_free, 1)) {
    return "invalid_tp_free";
  }
  if (!env.r.obj_valid_or_null(this->tp_is_gc, 1)) {
    return "invalid_tp_is_gc";
  }
  if (!env.r.obj_valid_or_null(this->tp_bases, 1)) {
    return "invalid_tp_bases";
  }
  if (!env.r.obj_valid_or_null(this->tp_mro, 1)) {
    return "invalid_tp_mro";
  }
  if (!env.r.obj_valid_or_null(this->tp_cache, 1)) {
    return "invalid_tp_cache";
  }
  if (!env.r.obj_valid_or_null(this->tp_subclasses, 1)) {
    return "invalid_tp_subclasses";
  }
  if (!env.r.obj_valid_or_null(this->tp_weaklist, 1)) {
    return "invalid_tp_weaklist";
  }
  if (!env.r.obj_valid_or_null(this->tp_del, 1)) {
    return "invalid_tp_del";
  }
  if (!env.r.obj_valid_or_null(this->tp_finalize, 1)) {
    return "invalid_tp_finalize";
  }
  try {
    if (this->name(env.r).empty()) {
      return "invalid_name";
    }
  } catch (const std::out_of_range&) {
    return "invalid_name_ptr";
  }
  return nullptr;
}

std::string PyTypeObject::repr(Traversal& t) const {
  return std::format("<type {}>", this->name(t.env.r));
}
