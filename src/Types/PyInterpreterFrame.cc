#include "PyInterpreterFrame.hh"

#if PYMEMTOOLS_PYTHON_VERSION == 314

const char* PyInterpreterFrame::invalid_reason(const Environment& env) const {
  auto executable_ptr = this->f_executable.as_object();
  if (executable_ptr.is_null() || !env.r.obj_valid(executable_ptr)) {
    return "invalid_executable";
  }

  if (!env.r.obj_valid_or_null(this->f_globals, 1)) {
    return "invalid_globals";
  }
  if (!env.r.obj_valid_or_null(this->f_builtins, 1)) {
    return "invalid_builtins";
  }
  if (!env.r.obj_valid_or_null(this->f_locals, 1)) {
    return "invalid_locals";
  }
  if (!env.r.obj_valid_or_null(this->frame_obj.cast<void>(), 8)) {
    return "invalid_frame_obj";
  }
  if (!this->previous.is_null() && !env.r.exists_range(this->previous, sizeof(PyInterpreterFrame))) {
    return "invalid_previous";
  }
  return nullptr;
}

MappedPtr<PyCodeObject> PyInterpreterFrame::executable_code(const Environment& env) const {
  auto executable_ptr = this->f_executable.as_object();
  if (executable_ptr.is_null()) {
    return {};
  }
  auto code_type = env.get_type_if_exists("code");
  if (code_type.is_null()) {
    return {};
  }
  const auto& obj = env.r.get(executable_ptr);
  if (obj.ob_type != code_type) {
    return {};
  }
  return executable_ptr.cast<PyCodeObject>();
}

std::unordered_set<MappedPtr<void>> PyInterpreterFrame::direct_referents(const Environment& env) const {
  std::unordered_set<MappedPtr<void>> ret;
  auto executable_ptr = this->f_executable.as_object();
  if (!executable_ptr.is_null()) {
    ret.emplace(executable_ptr);
  }
  ret.emplace(this->f_globals);
  ret.emplace(this->f_builtins);
  ret.emplace(this->f_locals);
  ret.emplace(this->frame_obj);
  auto code_addr = this->executable_code(env);
  if (!code_addr.is_null()) {
    ret.emplace(code_addr);
  }
  return ret;
}

size_t PyInterpreterFrame::code_offset(const Environment& env) const {
  auto code_addr = this->executable_code(env);
  if (code_addr.is_null()) {
    return 0;
  }
  if (this->instr_ptr.is_null()) {
    return 0;
  }
  const auto& code = env.r.get(code_addr);
  auto code_offset = reinterpret_cast<const char*>(&code.co_code_adaptive) - reinterpret_cast<const char*>(&code);
  auto base = code_addr.offset_bytes(static_cast<size_t>(code_offset));
  if (this->instr_ptr.addr < base.addr) {
    return 0;
  }
  return this->instr_ptr.addr - base.addr;
}

std::string PyInterpreterFrame::repr(Traversal& t) const {
  if (const char* ir = this->invalid_reason(t.env)) {
    return std::format("<iframe !{}>", ir);
  }
  auto code_addr = this->executable_code(t.env);
  if (code_addr.is_null()) {
    return "<iframe !no_code>";
  }
  const auto& code = t.env.r.get(code_addr);
  std::string where;
  try {
    auto offset = this->code_offset(t.env);
    auto line = code.line_number_for_code_offset(t.env, offset);
    where = std::format("{}:{}", t.repr(code.co_filename), line);
  } catch (const std::exception& e) {
    where = std::format("!({})", e.what());
  }
  return std::format("<iframe {} {}>", t.repr(code.co_name), where);
}

#endif
