#include "PyGeneratorObjects.hh"

const char* PyGenObject::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->gi_frame, 1)) {
    return "invalid_gi_frame";
  }
  if (!env.r.obj_valid_or_null(this->gi_code, 1)) {
    return "invalid_gi_code";
  }
  if (!env.r.obj_valid_or_null(this->gi_weakreflist, 1)) {
    return "invalid_gi_weakreflist";
  }
  if (!env.r.obj_valid_or_null(this->gi_name, 1)) {
    return "invalid_gi_name";
  }
  if (!env.r.obj_valid_or_null(this->gi_qualname, 1)) {
    return "invalid_gi_qualname";
  }
  return this->gi_exc_state.invalid_reason(env);
}

std::unordered_set<MappedPtr<void>> PyGenObject::direct_referents(const Environment& env) const {
  auto ret = this->gi_exc_state.direct_referents(env);
  ret.emplace(this->gi_frame);
  ret.emplace(this->gi_code);
  ret.emplace(this->gi_weakreflist);
  ret.emplace(this->gi_name);
  ret.emplace(this->gi_qualname);
  return ret;
}

std::vector<std::string> PyGenObject::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens;
  if (!this->gi_name.is_null()) {
    tokens.emplace_back(std::format("name={}", t.repr(this->gi_name)));
  }
  if (!this->gi_qualname.is_null()) {
    tokens.emplace_back(std::format("qualname={}", t.repr(this->gi_qualname)));
  }
  if (!this->gi_exc_state.exc_value.is_null()) {
    tokens.emplace_back(std::format("exc_value={}", t.repr(this->gi_exc_state.exc_value)));
  }
  if (!this->gi_frame.is_null()) {
    tokens.emplace_back(std::format("frame={}", t.repr(this->gi_frame)));
  }
  if (!this->gi_code.is_null()) {
    tokens.emplace_back(std::format("code={}", t.repr(this->gi_code)));
  }
  if (!this->gi_weakreflist.is_null()) {
    tokens.emplace_back(std::format("weakreflist={}", t.repr(this->gi_weakreflist)));
  }
  return tokens;
}

std::string PyGenObject::repr(Traversal& t) const {
  return t.token_repr<PyGenObject>(*this, "generator");
}

const char* PyCoroObject::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->cr_origin, 1)) {
    return "invalid_cr_origin";
  }
  return this->PyGenObject::invalid_reason(env);
}

std::unordered_set<MappedPtr<void>> PyCoroObject::direct_referents(const Environment& env) const {
  auto ret = this->PyGenObject::direct_referents(env);
  ret.emplace(this->cr_origin);
  return ret;
}

std::vector<std::string> PyCoroObject::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens = this->PyGenObject::repr_tokens(t);
  if (!this->cr_origin.is_null()) {
    tokens.emplace_back(std::format("origin={}", t.repr(this->cr_origin)));
  }
  return tokens;
}

std::string PyCoroObject::repr(Traversal& t) const {
  if (t.is_short) {
    auto name = t.repr(this->gi_qualname);
    if (!this->gi_frame.is_null()) {
      const auto& frame = t.env.r.get(this->gi_frame);
      if (const char* ir = frame.invalid_reason(t.env)) {
        return std::format("<coroutine !invalid_frame:{}>", ir);
      }
      auto state = frame.name_for_state(frame.f_state);
      auto where = frame.where(t);
      return std::format("<coroutine {} {} @ {}>", name, state, where);
    } else {
      return std::format("<coroutine {} (no frame)>", name);
    }
  } else {
    return t.token_repr(*this, "coroutine");
  }
}

const char* PyAsyncGenObject::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->ag_finalizer, 1)) {
    return "invalid_ag_finalizer";
  }
  return this->PyGenObject::invalid_reason(env);
}

std::unordered_set<MappedPtr<void>> PyAsyncGenObject::direct_referents(const Environment& env) const {
  auto ret = this->PyGenObject::direct_referents(env);
  ret.emplace(this->ag_finalizer);
  return ret;
}

std::vector<std::string> PyAsyncGenObject::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens = this->PyGenObject::repr_tokens(t);
  if (!this->ag_finalizer.is_null()) {
    tokens.emplace_back(std::format("finalizer=", t.repr(this->ag_finalizer)));
  }
  return tokens;
}

std::string PyAsyncGenObject::repr(Traversal& t) const {
  return t.token_repr(*this, "asyncgen");
}
