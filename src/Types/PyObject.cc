#include "PyObject.hh"
#include "PyTypeObject.hh"

const char* PyLinkedObject::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid(this->_ob_next)) {
    return "invalid_ob_next";
  }
  if (!env.r.obj_valid(this->_ob_prev)) {
    return "invalid_ob_prev";
  }
  return nullptr;
}

const char* PyObject::invalid_reason(const Environment& env) const {
  if (!this->refcount_is_valid()) {
    return "invalid_refcount";
  }
  if (!env.r.obj_valid(this->ob_type)) {
    return "invalid_type";
  }
  return nullptr;
}
