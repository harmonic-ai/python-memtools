#include "PyStringObjects.hh"
#include "Base.hh"

std::string escape_string_data(const void* data, size_t size, bool is_str, size_t excess_bytes) {
  std::string ret;
  if (!is_str) {
    ret.push_back('b');
  }
  ret.push_back('\'');
  phosg::StringReader r(data, size);
  while (!r.eof()) {
    uint8_t ch = r.get_u8();
    if (ch == '\t') {
      ret += "\\t";
    } else if (ch == '\r') {
      ret += "\\r";
    } else if (ch == '\n') {
      ret += "\\n";
    } else if (ch == '\'') {
      ret += "\\\'";
    } else if (ch < 0x20 || (!is_str && (ch > 0x7E))) {

      ret += std::format("\\x{:02X}", ch);
    } else {
      ret.push_back(ch);
    }
  }
  ret.push_back('\'');
  if (excess_bytes) {
    ret += std::format("...<0x{:X} more bytes>", excess_bytes);
  }
  return ret;
}

static DecodedString decode_ucs(phosg::StringReader r, uint8_t ucs_type, size_t max_len) {
  if (r.size() & (ucs_type - 1)) {
    throw std::runtime_error("Invalid UCS string length");
  }

  DecodedString ret;
  while (!r.eof() && (!max_len || (ret.data.size() < max_len))) {
    uint32_t ch;
    if (ucs_type == 1) {
      ch = r.get_u8();
    } else if (ucs_type == 2) {
      ch = r.get_u16l();
    } else if (ucs_type == 4) {
      ch = r.get_u32l();
    } else {
      throw std::logic_error("Invalid UCS encoding type");
    }

    if (ch < 0x80) {
      ret.data.push_back(ch);
    } else if (ch < 0x800) {
      ret.data.push_back(0xC0 | ((ch >> 6) & 0x1F));
      ret.data.push_back(0x80 | (ch & 0x3F));
    } else if (ch < 0x10000) {
      ret.data.push_back(0xE0 | ((ch >> 12) & 0x0F));
      ret.data.push_back(0x80 | ((ch >> 6) & 0x3F));
      ret.data.push_back(0x80 | (ch & 0x3F));
    } else if (ch < 0x110000) {
      ret.data.push_back(0xF0 | ((ch >> 18) & 0x07));
      ret.data.push_back(0x80 | ((ch >> 12) & 0x3F));
      ret.data.push_back(0x80 | ((ch >> 6) & 0x3F));
      ret.data.push_back(0x80 | (ch & 0x3F));
    } else {
      throw std::out_of_range("Invalid UCS character");
    }
  }
  ret.excess_bytes = r.remaining();
  return ret;
}

DecodedString decode_string_types(const MemoryReader& r, MappedPtr<PyObject> addr, size_t max_len) {
  const auto& obj = r.get(addr.cast<PyASCIIStringObject>());
  if (obj.length == 0) {
    return DecodedString{"", 0};
  }

  if (obj.is_compact() && obj.is_ascii()) {
    try {
      auto data_r = r.read(addr.offset_bytes(sizeof(obj)).cast<char>(), obj.length);
      if (max_len && (data_r.size() > max_len)) {
        return DecodedString{data_r.read(max_len), data_r.size() - max_len};
      } else {
        return DecodedString{data_r.all(), 0};
      }
    } catch (const std::out_of_range&) {
      throw invalid_object("invalid_ascii_str_data");
    }

  } else {
    uint8_t char_kind = obj.char_kind();
    bool is_compact = obj.is_compact();
    MappedPtr<void> data_addr;
    if (is_compact) {
      data_addr = addr.offset_bytes(sizeof(PyCompactStringObject));
    } else {
      data_addr = r.get(addr.cast<PyGeneralStringObject>()).data;
    }

    try {
      return decode_ucs(r.read(data_addr, obj.length * char_kind), char_kind, max_len);
    } catch (const std::out_of_range&) {
      throw invalid_object("invalid_unicode_str_data");
    }
  }
}

static std::string repr_string_types(Traversal& t, MappedPtr<PyObject> addr) {
  try {
    auto ret = decode_string_types(t.env.r, addr, t.max_string_length);
    return escape_string_data(ret.data.data(), ret.data.size(), true, ret.excess_bytes);
  } catch (const invalid_object& e) {
    return std::format("<str !{}>", e.reason);
  }
}

const char* PyBytesObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyVarObject::invalid_reason(env)) {
    return ir;
  }
  auto data_addr = env.r.host_to_mapped(this).offset_bytes(sizeof(*this));
  if (!env.r.exists_range(data_addr, this->ob_size)) {
    return "invalid_data";
  }
  return nullptr;
}

phosg::StringReader PyBytesObject::read_contents() const {
  if (this->ob_size == 0) {
    return phosg::StringReader();
  }
  return phosg::StringReader(this->data, this->ob_size);
}

std::string PyBytesObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<bytes !{}>", ir);
  }
  try {
    auto r = this->read_contents();
    std::string ret;
    if (t.bytes_as_hex) {
      if (t.max_string_length && (r.size() > t.max_string_length)) {
        ret = std::format(
            "bytes.fromhex(\'{}\'...<0x{:X} more bytes>)",
            phosg::format_data_string(r.getv(r.size()), t.max_string_length, nullptr, phosg::FormatDataFlags::HEX_ONLY),
            r.size() - t.max_string_length);
      } else {
        ret = std::format(
            "bytes.fromhex(\'{}\')",
            phosg::format_data_string(r.getv(r.size()), r.size(), nullptr, phosg::FormatDataFlags::HEX_ONLY));
      }
    } else {
      size_t excess_bytes = 0;
      if (t.max_string_length && (r.size() > t.max_string_length)) {
        excess_bytes = r.size() - t.max_string_length;
        r.truncate(t.max_string_length);
      }
      ret = escape_string_data(r.getv(r.size()), r.size(), false, excess_bytes);
    }
    return ret;
  } catch (const std::out_of_range&) {
    return std::format("<bytes !unreadable_data>");
  }
}

const char* PyASCIIStringObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyObject::invalid_reason(env)) {
    return ir;
  }

  auto this_addr = env.r.host_to_mapped(this);
  if (this->is_compact() && this->is_ascii()) {
    if (!env.r.exists_range(this_addr.offset_bytes(sizeof(*this)), this->length)) {
      return "invalid_ascii_str_data";
    }
    return nullptr;
  } else {
    uint8_t char_kind = this->char_kind();
    if ((char_kind != 1) && (char_kind != 2) && (char_kind != 4)) {
      return "invalid_char_kind";
    }
    if (this->is_compact()) {
      if (!env.r.obj_valid(this_addr.cast<PyCompactStringObject>())) {
        return "address_out_of_range";
      }
      const auto& compact_str = env.r.get(this_addr.cast<PyCompactStringObject>());
      if (!env.r.obj_valid_or_null(compact_str.utf8, 1)) {
        return "invalid_utf8";
      }
      if (!env.r.exists_range(this_addr.offset_bytes(sizeof(compact_str)), compact_str.length * char_kind)) {
        return "invalid_compact_str_data";
      }
      return nullptr;
    } else {
      if (!env.r.obj_valid(this_addr.cast<PyGeneralStringObject>())) {
        return "address_out_of_range";
      }
      const auto& general_str = env.r.get(this_addr.cast<PyGeneralStringObject>());
      // TODO: Is it valid for a general string to have length 0? (Can data be null in that case, and is that OK?)
      if (!env.r.exists_range(general_str.data, general_str.length * general_str.char_kind())) {
        return "invalid_general_str_data";
      }
      return nullptr;
    }
  }
}

std::string PyASCIIStringObject::repr(Traversal& t) const {
  return repr_string_types(t, t.env.r.host_to_mapped(this));
}
