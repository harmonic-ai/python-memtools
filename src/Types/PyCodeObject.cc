#include "PyCodeObject.hh"
#include "PyStringObjects.hh"

const char* PyCodeObject::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->co_code, 1)) {
    return "invalid_co_code";
  }
  if (!env.r.obj_valid_or_null(this->co_consts, 1)) {
    return "invalid_co_consts";
  }
  if (!env.r.obj_valid_or_null(this->co_names, 1)) {
    return "invalid_co_names";
  }
  if (!env.r.obj_valid_or_null(this->co_varnames, 1)) {
    return "invalid_co_varnames";
  }
  if (!env.r.obj_valid_or_null(this->co_freevars, 1)) {
    return "invalid_co_freevars";
  }
  if (!env.r.obj_valid_or_null(this->co_cellvars, 1)) {
    return "invalid_co_cellvars";
  }
  if (!env.r.obj_valid_or_null(this->co_cell2arg, 1)) {
    return "invalid_co_cell2arg";
  }
  if (!env.r.obj_valid_or_null(this->co_filename, 1)) {
    return "invalid_co_filename";
  }
  if (!env.r.obj_valid_or_null(this->co_name, 1)) {
    return "invalid_co_name";
  }
  if (!env.r.obj_valid_or_null(this->co_linetable, 1)) {
    return "invalid_co_linetable";
  }
  if (!this->co_zombieframe.is_null() && !env.r.exists(this->co_zombieframe)) {
    return "invalid_co_zombieframe";
  }
  if (!env.r.obj_valid_or_null(this->co_weakreflist, 1)) {
    return "invalid_co_weakreflist";
  }
  if (!this->co_extra.is_null() && !env.r.exists(this->co_extra)) {
    return "invalid_co_extra";
  }
  if (!env.r.obj_valid_or_null(this->co_opcache_map, 1)) {
    return "invalid_co_opcache_map";
  }
  if (!this->co_opcache.is_null() && !env.r.exists(this->co_opcache)) {
    return "invalid_co_opcache";
  }
  return nullptr;
}

std::string PyCodeObject::repr(Traversal& t) const {
  if (const char* ir = t.check_valid(*this)) {
    return std::format("<code !{}>", ir);
  }
  if (!t.recursion_allowed()) {
    return "<code !recursion_depth>";
  }

  bool is_root = t.in_progress.empty();
  auto cycle_guard = t.cycle_guard(this);
  if (cycle_guard.is_recursive) {
    return std::format("<code !recursive_repr>");
  }
  auto indent = t.indent();
  std::vector<std::string> tokens;
  tokens.emplace_back("<code");
  tokens.emplace_back(std::format("name={}", t.repr(this->co_name)));
  tokens.emplace_back(std::format("start={}:{}", t.repr(this->co_filename), this->co_firstlineno));
  if (is_root) {
    bool prev_bytes_as_hex = t.bytes_as_hex;
    tokens.emplace_back(std::format("args_config=({} args, {} pos-only, {} kw-only)",
        this->co_argcount, this->co_posonlyargcount, this->co_kwonlyargcount));
    tokens.emplace_back(std::format("vars_config=({} locals, {} stack)", this->co_nlocals, this->co_stacksize));
    tokens.emplace_back(std::format("flags={:08X}", this->co_flags));
    t.bytes_as_hex = true;
    tokens.emplace_back(std::format("code={}", t.repr(this->co_code)));
    t.bytes_as_hex = prev_bytes_as_hex;
    tokens.emplace_back(std::format("consts={}", t.repr(this->co_consts)));
    tokens.emplace_back(std::format("names={}", t.repr(this->co_names)));
    tokens.emplace_back(std::format("varnames={}", t.repr(this->co_varnames)));
    tokens.emplace_back(std::format("freevars=@{}", this->co_freevars));
    tokens.emplace_back(std::format("cellvars=@{}", this->co_cellvars));
    tokens.emplace_back(std::format("cell2arg=@{}", this->co_cell2arg));
    t.bytes_as_hex = true;
    tokens.emplace_back(std::format("linetable={}", t.repr(this->co_linetable)));
    t.bytes_as_hex = prev_bytes_as_hex;
    tokens.emplace_back(std::format("zombieframe=@{}", this->co_zombieframe));
    tokens.emplace_back(std::format("weakreflist={}", t.repr(this->co_weakreflist)));
    tokens.emplace_back(std::format("extra=@{}", this->co_extra));
  }
  if (is_root) {
    std::string joiner = "\n";
    joiner.resize((t.recursion_depth * 2) + 1, ' ');
    return std::format("{}\n{}>", phosg::join(tokens, joiner), std::string((t.recursion_depth - 1) * 2, ' '));
  } else {
    return std::format("{}>", phosg::join(tokens, " "));
  }
}

size_t PyCodeObject::line_number_for_code_offset(const Environment& env, size_t code_offset) const {
  if (const char* ir = env.invalid_reason(this->co_linetable, env.get_type("bytes"))) {
    throw invalid_object(ir);
  }
  auto table_r = env.r.get<PyBytesObject>(this->co_linetable).read_contents();
  ssize_t line = this->co_firstlineno;
  ssize_t end = 0;
  while (!table_r.eof()) {
    uint8_t sdelta = table_r.get_u8();
    int8_t ldelta = table_r.get_s8();
    if (ldelta == 0) {
      end += sdelta;
      continue;
    }
    ssize_t start = end;
    end = start + sdelta;
    if (ldelta == -0x80) {
      continue;
    }
    line += ldelta;
    if (end == start) {
      continue;
    }
    if (static_cast<ssize_t>(code_offset) >= start && static_cast<ssize_t>(code_offset) < end) {
      return static_cast<size_t>(line);
    }
  }
  return 0;
}
