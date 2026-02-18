#include "PyFrameObject.hh"

#include <algorithm>

const char* PyFrameObject::invalid_reason(const Environment& env) const {
  if (this->f_state < PyFrameState::FRAME_CREATED || this->f_state > PyFrameState::FRAME_CLEARED) {
    return "invalid_f_state";
  }
  if (!env.r.obj_valid_or_null(this->f_back, 8)) {
    return "invalid_f_back";
  }
  if (!env.r.obj_valid_or_null(this->f_code, 8)) {
    return "invalid_f_code";
  }
  if (!env.r.obj_valid_or_null(this->f_builtins, 8)) {
    return "invalid_f_builtins";
  }
  if (!env.r.obj_valid_or_null(this->f_globals, 8)) {
    return "invalid_f_globals";
  }
  if (!env.r.obj_valid_or_null(this->f_locals, 8)) {
    return "invalid_f_locals";
  }
  if (!env.r.obj_valid_or_null(this->f_valuestack, 1)) {
    return "invalid_f_valuestack";
  }
  if (!env.r.obj_valid_or_null(this->f_trace, 1)) {
    return "invalid_f_trace";
  }
  if (!env.r.obj_valid_or_null(this->f_gen, 1)) {
    return "invalid_f_gen";
  }
  if (!this->f_code.is_null()) {
    if (const char* ir = env.invalid_reason(this->f_code, env.get_type("code"))) {
      return ir;
    }
    const auto& code = env.r.get(this->f_code);
    if (const char* ir = env.invalid_reason(code.co_varnames, env.get_type("tuple"))) {
      return ir;
    }

    const auto& varnames = env.r.get(code.co_varnames);

    if (!env.r.exists_range(env.r.host_to_mapped(this), sizeof(*this) + varnames.ob_size * sizeof(MappedPtr<PyObject>))) {
      return "invalid_f_localsplus_range";
    }

    for (ssize_t z = 0; z < varnames.ob_size; z++) {
      if (const char* ir = env.invalid_reason(varnames.items[z], env.get_type("str"))) {
        return ir;
      }
      if (!this->f_localsplus[z].is_null()) {
        if (const char* ir = env.r.get(this->f_localsplus[z]).invalid_reason(env)) {
          return ir;
        }
      }
    }
  }
  return nullptr;
}

std::unordered_set<MappedPtr<void>> PyFrameObject::direct_referents(const Environment& env) const {
  std::unordered_set<MappedPtr<void>> ret{
      this->f_back, this->f_code, this->f_builtins, this->f_globals, this->f_locals, this->f_trace, this->f_gen};
  for (const auto& [name_addr, value_addr] : this->locals(env)) {
    ret.emplace(name_addr);
    ret.emplace(value_addr);
  }
  return ret;
}

std::string PyFrameObject::name_for_state(PyFrameState st) {
  switch (st) {
    case PyFrameState::FRAME_CREATED:
      return "created";
    case PyFrameState::FRAME_SUSPENDED:
      return "suspended";
      break;
    case PyFrameState::FRAME_EXECUTING:
      return "executing";
      break;
    case PyFrameState::FRAME_RETURNED:
      return "returned";
      break;
    case PyFrameState::FRAME_UNWINDING:
      return "unwinding";
      break;
    case PyFrameState::FRAME_RAISED:
      return "raised";
      break;
    case PyFrameState::FRAME_CLEARED:
      return "cleared";
      break;
    default:
      return std::format("state:{:02X}", static_cast<int8_t>(st));
  }
}

std::string PyFrameObject::where(Traversal& t) const {
  try {
    const auto& code_obj = t.env.r.get(this->f_code);
    if (const char* ir = t.check_valid(code_obj)) {
      throw invalid_object(ir);
    }
    std::string filename_repr = t.repr(code_obj.co_filename);
    try {
      auto line = code_obj.line_number_for_code_offset(t.env, this->f_lasti * sizeof(Py_CODEUNIT));
      return std::format("{}:{}", filename_repr, line);
    } catch (const std::exception& e) {
      return std::format("{}:!({})", filename_repr, e.what());
    }
  } catch (const std::exception& e) {
    return std::format("!({})", e.what());
  }
}

std::unordered_map<MappedPtr<PyObject>, MappedPtr<PyObject>> PyFrameObject::locals(const Environment& env) const {
  if (const char* ir = env.invalid_reason(this->f_code, env.get_type("code"))) {
    throw invalid_object(ir);
  }

  const auto& code_obj = env.r.get(this->f_code);
  if (const char* ir = env.invalid_reason(code_obj.co_varnames, env.get_type("tuple"))) {
    throw invalid_object(ir);
  }

  const auto& varnames = env.r.get(code_obj.co_varnames);

  std::unordered_map<MappedPtr<PyObject>, MappedPtr<PyObject>> ret;
  for (ssize_t z = 0; z < varnames.ob_size; z++) {
    ret.emplace(varnames.items[z], this->f_localsplus[z]);
  }
  return ret;
}

std::vector<std::string> PyFrameObject::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens;
  tokens.emplace_back(this->name_for_state(this->f_state));
  tokens.emplace_back(std::format("where={}", this->where(t)));
  if (!t.is_short) {
    if (t.frame_omit_back) {
      tokens.emplace_back(std::format("f_back=@{}", this->f_back));
    } else {
      tokens.emplace_back(std::format("f_back={}", t.repr(this->f_back)));
    }
    tokens.emplace_back(std::format("f_code={}", t.repr(this->f_code)));
    tokens.emplace_back(std::format("f_builtins=@{}", this->f_builtins));
    tokens.emplace_back(std::format("f_globals=@{}", this->f_globals));
    tokens.emplace_back(std::format("f_locals={}", t.repr(this->f_locals)));
    tokens.emplace_back(std::format("f_valuestack=@{}", this->f_valuestack));
    tokens.emplace_back(std::format("f_trace={}", t.repr(this->f_trace)));
    tokens.emplace_back(std::format("f_stackdepth={}", this->f_stackdepth));
    tokens.emplace_back(std::format("f_trace_lines=0x{:02X}", this->f_trace_lines));
    tokens.emplace_back(std::format("f_trace_opcodes=0x{:02X}", this->f_trace_opcodes));
    tokens.emplace_back(std::format("f_gen={}", t.repr(this->f_gen)));
    tokens.emplace_back(std::format("f_lasti={} (offset={})", this->f_lasti, this->f_lasti * sizeof(Py_CODEUNIT)));
    tokens.emplace_back(std::format("f_lineno={}", this->f_lineno));
    tokens.emplace_back(std::format("f_iblock={}", this->f_iblock));

    if (!t.frame_omit_locals) {
      try {
        std::vector<std::string> locals_entries;
        locals_entries.emplace_back("locals:");
        auto indent = t.indent();
        for (auto [name_addr, value_addr] : this->locals(t.env)) {
          locals_entries.emplace_back(std::format("  {} = {}", t.repr(name_addr), t.repr(value_addr)));
        }
        std::sort(locals_entries.begin() + 1, locals_entries.end());
        tokens.insert(
            tokens.end(),
            std::make_move_iterator(locals_entries.begin()),
            std::make_move_iterator(locals_entries.end()));
      } catch (const std::exception& e) {
        tokens.emplace_back(std::format("locals=!({})", e.what()));
      }
    }
  }

  return tokens;
}

std::string PyFrameObject::repr(Traversal& t) const {
  return t.token_repr<PyFrameObject>(*this, "frame");
}
