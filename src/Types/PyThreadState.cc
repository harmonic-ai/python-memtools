#include "PyThreadState.hh"

#if PYMEMTOOLS_PYTHON_VERSION == 314
const char* PyThreadState::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->prev, 8)) {
    return "invalid_prev";
  }
  if (!env.r.obj_valid_or_null(this->next, 8)) {
    return "invalid_next";
  }
  if (!env.r.obj_valid(this->interp, 8)) {
    return "invalid_interp";
  }
  if (!env.r.obj_valid_or_null(this->current_frame, 8)) {
    return "invalid_current_frame";
  }
  if (!env.r.obj_valid_or_null(this->c_profilefunc, 1)) {
    return "invalid_c_profilefunc";
  }
  if (!env.r.obj_valid_or_null(this->c_tracefunc, 1)) {
    return "invalid_c_tracefunc";
  }
  if (!this->c_profileobj.is_null() && env.invalid_reason(this->c_profileobj)) {
    return "invalid_c_profileobj";
  }
  if (!this->c_traceobj.is_null() && env.invalid_reason(this->c_traceobj)) {
    return "invalid_c_traceobj";
  }
  if (!this->current_exception.is_null() && env.invalid_reason(this->current_exception)) {
    return "invalid_current_exception";
  }
  if (!env.r.obj_valid_or_null(this->exc_info, 8)) {
    return "invalid_exc_info";
  }
  if (!this->dict.is_null() && env.invalid_reason(this->dict, env.get_type_if_exists("dict"))) {
    return "invalid_dict";
  }
  if (!this->async_exc.is_null() && env.invalid_reason(this->async_exc)) {
    return "invalid_async_exc";
  }
  if (!this->delete_later.is_null() && env.invalid_reason(this->delete_later)) {
    return "invalid_delete_later";
  }
  if (!this->async_gen_firstiter.is_null() && env.invalid_reason(this->async_gen_firstiter)) {
    return "invalid_async_gen_firstiter";
  }
  if (!this->async_gen_finalizer.is_null() && env.invalid_reason(this->async_gen_finalizer)) {
    return "invalid_async_gen_finalizer";
  }
  if (!this->context.is_null() && env.invalid_reason(this->context)) {
    return "invalid_context";
  }
  return nullptr;
}

std::vector<std::string> PyThreadState::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens;
  tokens.emplace_back(std::format("prev=@{}", this->prev));
  tokens.emplace_back(std::format("next=@{}", this->next));
  tokens.emplace_back(std::format("interp=@{}", this->interp));
  tokens.emplace_back(std::format("current_frame=@{}", this->current_frame));
  tokens.emplace_back(std::format("thread_id={}", this->thread_id));
  tokens.emplace_back(std::format("id={:X}", this->id));
  if (!this->current_exception.is_null()) {
    tokens.emplace_back(std::format("current_exception={}", t.repr(this->current_exception)));
  }
  if (!this->async_exc.is_null()) {
    tokens.emplace_back(std::format("async_exc={}", t.repr(this->async_exc)));
  }
  tokens.emplace_back(std::format("dict={}", t.repr(this->dict)));
  tokens.emplace_back(std::format("context={}", t.repr(this->context)));
  return tokens;
}

std::string PyThreadState::repr(Traversal& t) const {
  return t.token_repr<PyThreadState>(*this, "thread state");
}
#else
const char* PyThreadState::invalid_reason(const Environment& env) const {
  if (!env.r.obj_valid_or_null(this->prev, 8)) {
    return "invalid_prev";
  }
  if (!env.r.obj_valid_or_null(this->next, 8)) {
    return "invalid_next";
  }
  if (!env.r.obj_valid(this->interp, 8)) {
    return "invalid_interp";
  }
  if (!this->frame.is_null() && env.invalid_reason(this->frame, env.get_type_if_exists("frame"))) {
    return "invalid_frame";
  }
  if (!env.r.obj_valid_or_null(this->cframe, 8)) {
    return "invalid_cframe";
  }
  if (!env.r.obj_valid_or_null(this->c_profilefunc, 1)) {
    return "invalid_c_profilefunc";
  }
  if (!env.r.obj_valid_or_null(this->c_tracefunc, 1)) {
    return "invalid_c_tracefunc";
  }
  if (!this->c_profileobj.is_null() && env.invalid_reason(this->c_profileobj)) {
    return "invalid_c_profileobj";
  }
  if (!this->c_traceobj.is_null() && env.invalid_reason(this->c_traceobj)) {
    return "invalid_c_traceobj";
  }
  if (!this->curexc_type.is_null() && env.invalid_reason(this->curexc_type)) {
    return "invalid_curexc_type";
  }
  if (!this->curexc_value.is_null() && env.invalid_reason(this->curexc_value)) {
    return "invalid_curexc_value";
  }
  if (!this->curexc_traceback.is_null() && env.invalid_reason(this->curexc_traceback)) {
    return "invalid_curexc_traceback";
  }
  if (this->exc_state.invalid_reason(env)) {
    return "invalid_exc_state";
  }
  if (!env.r.obj_valid_or_null(this->exc_info, 8)) {
    return "invalid_exc_info";
  }
  if (!this->dict.is_null() && env.invalid_reason(this->dict, env.get_type_if_exists("dict"))) {
    return "invalid_dict";
  }
  if (!this->async_exc.is_null() && env.invalid_reason(this->async_exc)) {
    return "invalid_async_exc";
  }
  if (!this->trash_delete_later.is_null() && env.invalid_reason(this->trash_delete_later)) {
    return "invalid_trash_delete_later";
  }
  if (!env.r.obj_valid_or_null(this->on_delete, 1)) {
    return "invalid_on_delete";
  }
  if (!env.r.obj_valid_or_null(this->on_delete_data, 1)) {
    return "invalid_on_delete_data";
  }
  if (!this->async_gen_firstiter.is_null() && env.invalid_reason(this->async_gen_firstiter)) {
    return "invalid_async_gen_firstiter";
  }
  if (!this->async_gen_finalizer.is_null() && env.invalid_reason(this->async_gen_finalizer)) {
    return "invalid_async_gen_finalizer";
  }
  if (!this->context.is_null() && env.invalid_reason(this->context)) {
    return "invalid_context";
  }
  return nullptr;
}

std::vector<std::string> PyThreadState::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens;
  tokens.emplace_back(std::format("prev=@{}", this->prev));
  tokens.emplace_back(std::format("next=@{}", this->next));
  tokens.emplace_back(std::format("interp=@{}", this->interp));
  tokens.emplace_back(std::format("frame={}", t.repr(this->frame)));
  tokens.emplace_back(std::format("recursion_depth={}", this->recursion_depth));
  if (!this->c_profileobj.is_null()) {
    tokens.emplace_back(std::format("c_profileobj={}", t.repr(this->c_profileobj)));
  }
  if (!this->c_traceobj.is_null()) {
    tokens.emplace_back(std::format("c_traceobj={}", t.repr(this->c_traceobj)));
  }
  if (!this->curexc_type.is_null() && !this->curexc_value.is_null() && !this->curexc_traceback.is_null()) {
    tokens.emplace_back(std::format("curexc=(type={} value={} traceback={})", t.repr(this->curexc_type), t.repr(this->curexc_value), t.repr(this->curexc_traceback)));
  }
  if (!this->async_exc.is_null()) {
    tokens.emplace_back(std::format("async_exc={}", t.repr(this->async_exc)));
  }
  tokens.emplace_back(std::format("dict={}", t.repr(this->dict)));
  tokens.emplace_back(std::format("thread_id={}", this->thread_id));
  tokens.emplace_back(std::format("context={}", t.repr(this->context)));
  tokens.emplace_back(std::format("id={:X}", this->id));
  return tokens;
}

std::string PyThreadState::repr(Traversal& t) const {
  return t.token_repr<PyThreadState>(*this, "thread state");
}
#endif
