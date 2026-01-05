#include "PyFloatObject.hh"

std::string PyFloatObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<float !{}>", ir);
  }
  return std::format("{:g}", this->ob_fval);
}
