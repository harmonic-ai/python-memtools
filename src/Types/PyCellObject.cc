#include "PyCellObject.hh"

const char* PyCellObject::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->ob_ref, 1)) {
    return "invalid_ob_ref";
  }
  return nullptr;
}

std::string PyCellObject::repr(Traversal& t) const {
  {
    const char* ir = t.check_valid(*this);
    if (!ir && !t.recursion_allowed()) {
      ir = "recursion_depth";
    }
    if (ir) {
      return std::format("<cell !{}>", ir);
    }
  }
  auto cycle_guard = t.cycle_guard(this);
  if (cycle_guard.is_recursive) {
    return "<cell !recursive_repr>";
  }
  auto indent = t.indent();
  return std::format("<cell ob_ref={}>", t.repr(this->ob_ref));
}
