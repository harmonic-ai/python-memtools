#pragma once

#include "PyObject.hh"

// See https://github.com/python/cpython/blob/3.10/Include/cpython/bytesobject.h
struct PyBytesObject : PyVarObject {
  uint64_t ob_shash;
  uint8_t data[0];

  const char* invalid_reason(const Environment& env) const;
  // direct_referents inherited from PyVarObject
  std::string repr(Traversal& t) const;

  phosg::StringReader read_contents() const;
};

// See https://github.com/python/cpython/blob/3.10/Include/cpython/unicodeobject.h for this and the following structs
struct PyASCIIStringObject : PyObject {
  uint64_t length; // Number of code points, not number of bytes!
  uint64_t hash;
  uint8_t flags; // Bits: SACKKKII (S = static, A = ASCII, C = compact, K = kind, I = intern state)
  MappedPtr<wchar_t> wstr;

  const char* invalid_reason(const Environment& env) const;
  // direct_referents inherited from PyObject
  std::string repr(Traversal& t) const;

  inline bool is_static() const {
    return (this->flags >> 7) & 1;
  }
  inline bool is_ascii() const {
    return (this->flags >> 6) & 1;
  }
  inline bool is_compact() const {
    return (this->flags >> 5) & 1;
  }
  inline uint8_t char_kind() const {
    // 1 = UCS1 (1-byte chars, code points 00-FF)
    // 2 = UCS2 (2-byte chars, code points 0000-FFFF)
    // 4 = UCS4 (4-byte chars, any code point)
    return (this->flags >> 2) & 7;
  }
  inline uint8_t intern_state() const {
    // 0=not interned, 1=interned, 2=interned+immortal, 3=interned+immortal+static
    return this->flags & 3;
  }
};

struct PyCompactStringObject : PyASCIIStringObject {
  uint64_t utf8_length; // Bytes in UTF-8 representation
  MappedPtr<char> utf8; // May be null if there's no UTF-8 representation
  uint64_t wstr_length;
};

struct PyGeneralStringObject : PyCompactStringObject {
  MappedPtr<void> data; // void*, Py_UCS1*, Py_UCS2*, or Py_UCS4*
};

// Returns a copy of the UTF-8 data associated with a string, regardless of what format is actually stored in memory.
// For bytes objects, use PyBytesObject::read_contents instead.
struct DecodedString {
  std::string data;
  size_t excess_bytes; // Always 0 unless max_len > 0 and the string is longer than max_len
};
DecodedString decode_string_types(const MemoryReader& r, MappedPtr<PyObject> addr, size_t max_len = 0);

std::string escape_string_data(const void* data, size_t size, bool is_str, size_t excess_bytes = 0);
