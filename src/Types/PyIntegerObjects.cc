#include "PyIntegerObjects.hh"

const char* PyLongObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyVarObject::invalid_reason(env)) {
    return ir;
  }
  auto data_addr = env.r.host_to_mapped(this).offset_bytes(sizeof(*this));
  if (!env.r.exists_range(data_addr, this->ob_size * 4)) {
    return "invalid_digits";
  }
  return nullptr;
}

std::string PyLongObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<int !{}>", ir);
  }

  int64_t num_digits = this->ob_size;
  bool is_negative = (num_digits < 0);
  if (is_negative) {
    num_digits = -num_digits;
  }

  if (num_digits == 0) {
    return "0";
  }

  phosg::StringReader digits_r;
  auto data_addr = t.env.r.host_to_mapped(this).offset_bytes(sizeof(*this));
  digits_r = t.env.r.read(data_addr, num_digits * 4);

  switch (num_digits) {
    case 1: {
      int32_t value = digits_r.get_u32l() & 0x3FFFFFFF;
      return std::format("{}", is_negative ? (-value) : value);
    }
    case 2: {
      int64_t value = digits_r.get_u32l() & 0x3FFFFFFF;
      value |= static_cast<int64_t>(digits_r.get_u32l() & 0x3FFFFFFF) << 30;
      return std::format("{}", is_negative ? (-value) : value);
    }
    case 3: {
      int64_t value = digits_r.get_u32l() & 0x3FFFFFFF;
      value |= static_cast<int64_t>(digits_r.get_u32l() & 0x3FFFFFFF) << 30;
      uint32_t high = digits_r.get_u32l();
      if ((high & 0xFFFFFFF8) == 0) {
        value |= static_cast<int64_t>(high) << 60;
        return std::format("{}", is_negative ? (-value) : value);
      } else if (((high & 0xFFFFFFF0) == 0) && !is_negative) {
        uint64_t uvalue = value | static_cast<uint64_t>(high) << 60;
        return std::format("{}", uvalue);
      }
      [[fallthrough]];
    }
    default:
      // TODO: It'd be nice to be able to format these as natural numbers, but for now we choose to be lazy, since
      // these are far less common than the above cases
      std::string ret = "<int ";
      ret += (is_negative ? "-" : "+");
      while (!digits_r.eof()) {
        ret += std::format(" {:08X}", digits_r.get_u32l());
      }
      ret += ">";
      return ret;
  }
}

const char* PyBoolObject::invalid_reason(const Environment& env) const {
  if (this->ob_size > 1) {
    return "invalid_size";
  }
  return this->PyLongObject::invalid_reason(env);
}

std::string PyBoolObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<bool !{}>", ir);
  } else {
    return this->ob_size ? "True" : "False";
  }
}
