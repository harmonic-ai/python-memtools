#include "PyAsyncObjects.hh"
#include "Base.hh"
#include "PyListObject.hh"

const char* PyAsyncFutureObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyObject::invalid_reason(env)) {
    return ir;
  }
  if (!env.r.obj_valid_or_null(this->fut_loop, 1)) {
    return "invalid_fut_loop";
  }
  if (!env.r.obj_valid_or_null(this->fut_callback0, 1)) {
    return "invalid_fut_callback0";
  }
  if (!env.r.obj_valid_or_null(this->fut_context0, 1)) {
    return "invalid_fut_context0";
  }
  if (!env.r.obj_valid_or_null(this->fut_callbacks, 1)) {
    return "invalid_fut_callbacks";
  }
  if (!env.r.obj_valid_or_null(this->fut_exception, 1)) {
    return "invalid_fut_exception";
  }
  if (!env.r.obj_valid_or_null(this->fut_exception_tb, 1)) {
    return "invalid_fut_exception_tb";
  }
  if (!env.r.obj_valid_or_null(this->fut_result, 1)) {
    return "invalid_fut_result";
  }
  if (!env.r.obj_valid_or_null(this->fut_source_tb, 1)) {
    return "invalid_fut_source_tb";
  }
  if (!env.r.obj_valid_or_null(this->fut_cancel_msg, 1)) {
    return "invalid_fut_cancel_msg";
  }
  if (!env.r.obj_valid_or_null(this->dict, 1)) {
    return "invalid_dict";
  }
  if (!env.r.obj_valid_or_null(this->fut_weakreflist, 1)) {
    return "invalid_fut_weakreflist";
  }
  if ((this->fut_state != PyFutureState::STATE_PENDING) &&
      (this->fut_state != PyFutureState::STATE_CANCELLED) &&
      (this->fut_state != PyFutureState::STATE_FINISHED)) {
    return "invalid_state";
  }
  return this->fut_cancelled_exc.invalid_reason(env);
}

std::unordered_set<MappedPtr<void>> PyAsyncFutureObject::direct_referents(const Environment& env) const {
  auto ret = this->fut_cancelled_exc.direct_referents(env);
  ret.emplace(this->fut_loop);
  ret.emplace(this->fut_callback0);
  ret.emplace(this->fut_context0);
  ret.emplace(this->fut_callbacks);
  ret.emplace(this->fut_exception);
  ret.emplace(this->fut_exception_tb);
  ret.emplace(this->fut_result);
  ret.emplace(this->fut_source_tb);
  ret.emplace(this->fut_cancel_msg);
  ret.emplace(this->dict);
  ret.emplace(this->fut_weakreflist);
  return ret;
}

std::vector<std::string> PyAsyncFutureObject::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens;
  if (this->fut_state == PyFutureState::STATE_PENDING) {
    tokens.emplace_back("pending");
  } else if (this->fut_state == PyFutureState::STATE_CANCELLED) {
    tokens.emplace_back("cancelled");
  } else if (this->fut_state == PyFutureState::STATE_FINISHED) {
    tokens.emplace_back("finished");
  } else {
    tokens.emplace_back(std::format("!state:{}", static_cast<uint64_t>(this->fut_state)));
  }
  if (!this->fut_loop.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("loop={}", t.repr(this->fut_loop)));
  }
  if (!this->fut_callback0.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("callback0={}", t.repr(this->fut_callback0)));
  }
  if (!this->fut_context0.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("context0={}", t.repr(this->fut_context0)));
  }
  if (!this->fut_callbacks.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("callbacks={}", t.repr(this->fut_callbacks)));
  }
  if (!this->fut_exception.is_null()) {
    tokens.emplace_back(std::format("exception={}", t.repr(this->fut_exception)));
  }
  if (!this->fut_exception_tb.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("exception_tb={}", t.repr(this->fut_exception_tb)));
  }
  if (!this->fut_result.is_null()) {
    tokens.emplace_back(std::format("result={}", t.repr(this->fut_result)));
  }
  if (!this->fut_source_tb.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("source_tb={}", t.repr(this->fut_source_tb)));
  }
  if (!this->fut_cancel_msg.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("cancel_msg={}", t.repr(this->fut_cancel_msg)));
  }
  if (!this->dict.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("dict={}", t.repr(this->dict)));
  }
  if (!this->fut_weakreflist.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("weakreflist={}", t.repr(this->fut_weakreflist)));
  }
  if (!this->fut_cancelled_exc.exc_value.is_null() && !t.is_short) {
    tokens.emplace_back(std::format("cancelled_exc={}", t.repr(this->fut_cancelled_exc.exc_value)));
  }
  return tokens;
}

std::string PyAsyncFutureObject::repr(Traversal& t) const {
  return t.token_repr(*this, "async future");
}

std::vector<MappedPtr<PyObject>> PyAsyncGatheringFutureObject::children(const Environment& env) const {
  const auto& dict = env.r.get(this->dict);
  if (const char* ir = dict.invalid_reason(env)) {
    throw invalid_object(ir);
  }

  const auto& children = env.r.get(dict.value_for_key<PyListObject>(env.r, "_children"));
  if (const char* ir = children.invalid_reason(env)) {
    throw invalid_object(ir);
  }

  return children.get_items(env.r);
}

std::vector<std::string> PyAsyncGatheringFutureObject::repr_tokens(Traversal& t) const {
  auto tokens = this->PyAsyncFutureObject::repr_tokens(t);
  if (!t.is_short) {
    try {
      auto children = this->children(t.env);
      for (size_t z = 0; z < children.size(); z++) {
        tokens.emplace_back(std::format("children[{}]={}", z, t.repr(children[z])));
      }
    } catch (const std::exception& e) {
      tokens.emplace_back(std::format("children=!({})", e.what()));
    }
  }
  return tokens;
}

std::unordered_set<MappedPtr<void>> PyAsyncGatheringFutureObject::direct_referents(const Environment& env) const {
  auto ret = this->PyAsyncFutureObject::direct_referents(env);
  for (auto child : this->children(env)) {
    ret.emplace(child);
  }
  return ret;
}

std::string PyAsyncGatheringFutureObject::repr(Traversal& t) const {
  return t.token_repr(*this, "async _GatheringFuture");
}

const char* PyAsyncTaskObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyAsyncFutureObject::invalid_reason(env)) {
    return ir;
  }
  if (!env.r.obj_valid_or_null(this->task_fut_waiter, 1)) {
    return "invalid_task_fut_waiter";
  }
  if (!env.r.obj_valid(this->task_coro, 1)) {
    return "invalid_task_coro";
  }
  if (!env.r.obj_valid_or_null(this->task_name, 1)) {
    return "invalid_task_name";
  }
  if (!env.r.obj_valid_or_null(this->task_context, 1)) {
    return "invalid_task_context";
  }
  return nullptr;
}

std::unordered_set<MappedPtr<void>> PyAsyncTaskObject::direct_referents(const Environment& env) const {
  auto ret = this->PyAsyncFutureObject::direct_referents(env);
  ret.emplace(this->task_fut_waiter);
  ret.emplace(this->task_coro);
  ret.emplace(this->task_name);
  ret.emplace(this->task_context);
  return ret;
}

std::vector<std::string> PyAsyncTaskObject::repr_tokens(Traversal& t) const {
  std::vector<std::string> tokens = this->PyAsyncFutureObject::repr_tokens(t);
  if (this->task_must_cancel) {
    tokens.emplace_back(std::format("cancels={}", this->task_must_cancel));
  }
  tokens.emplace_back(std::format("coro={}", t.repr(this->task_coro)));
  if (!t.is_short) {
    tokens.emplace_back(std::format("waiter={}", t.repr(this->task_fut_waiter)));
    tokens.emplace_back(std::format("name={}", t.repr(this->task_name)));
    tokens.emplace_back(std::format("context={}", t.repr(this->task_context)));
  }
  return tokens;
}

std::string PyAsyncTaskObject::repr(Traversal& t) const {
  return t.token_repr(*this, "async task");
}
