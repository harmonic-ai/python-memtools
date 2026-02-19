#include "PyTypeObject.hh"
#include "PyDictObject.hh"
#include "PyStringObjects.hh" // for excape_string_data
#include "PyTupleObject.hh"

std::string PyMemberDef::repr(const MemoryReader& r) const {
  try {
    std::string doc;
    if (this->doc.is_null()) {
      doc = "NULL";
    } else {
      // TODO: It'd be nice to do this without copying the data out of r before calling escape_string_data.
      std::string raw_doc = r.get_cstr(this->doc);
      doc = escape_string_data(raw_doc.data(), raw_doc.size(), true);
    }
    return std::format("<PyMemberDef {} type={} offset=0x{:X} flags=0x{:X} doc={}>",
        r.get_cstr(this->name), this->type, this->offset, this->flags, doc);

  } catch (const std::exception& e) {
    return std::format("<PyMemberDef !invalid:>", e.what());
  }
}

bool PyTypeObject::type_name_is_valid(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  if (!isalpha(name[0]) && (name[0] != '_')) {
    return false;
  }
  for (char ch : name) {
    if (!isalnum(ch) && (ch != '_') && (ch != '.')) {
      return false;
    }
  }
  return true;
}

std::string PyTypeObject::name(const MemoryReader& r) const {
  std::string name = r.get_cstr(this->tp_name);
  return this->type_name_is_valid(name) ? name : "";
}

std::vector<std::pair<std::string, ssize_t>> PyTypeObject::slots(const MemoryReader& r) const {
  if (this->tp_members.is_null()) {
    return {};
  }

  std::vector<std::pair<std::string, ssize_t>> ret;
  auto def_ptr = this->tp_members;
  for (;;) {
    const auto& def = r.get(def_ptr);
    if (def.name.is_null()) {
      return ret;
    }
    ret.emplace_back(std::make_pair(r.get_cstr(def.name), def.offset));
    def_ptr = def_ptr.offset(1);
  }
}

const char* PyTypeObject::invalid_reason(const Environment& env) const {
  if (const char* ir = this->PyVarObject::invalid_reason(env)) {
    return ir;
  }
  if (!env.r.obj_valid(this->tp_name, 1)) {
    return "invalid_tp_name";
  }
  if (!env.r.obj_valid_or_null(this->tp_dealloc, 1)) {
    return "invalid_tp_dealloc";
  }
  if (!env.r.obj_valid_or_null(this->tp_getattr, 1)) {
    return "invalid_tp_getattr";
  }
  if (!env.r.obj_valid_or_null(this->tp_setattr, 1)) {
    return "invalid_tp_setattr";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_async, 1)) {
    return "invalid_tp_as_async";
  }
  if (!env.r.obj_valid_or_null(this->tp_repr, 1)) {
    return "invalid_tp_repr";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_number, 1)) {
    return "invalid_tp_as_number";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_sequence, 1)) {
    return "invalid_tp_as_sequence";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_mapping, 1)) {
    return "invalid_tp_as_mapping";
  }
  if (!env.r.obj_valid_or_null(this->tp_hash, 1)) {
    return "invalid_tp_hash";
  }
  if (!env.r.obj_valid_or_null(this->tp_call, 1)) {
    return "invalid_tp_call";
  }
  if (!env.r.obj_valid_or_null(this->tp_str, 1)) {
    return "invalid_tp_str";
  }
  if (!env.r.obj_valid_or_null(this->tp_getattro, 1)) {
    return "invalid_tp_getattro";
  }
  if (!env.r.obj_valid_or_null(this->tp_setattro, 1)) {
    return "invalid_tp_setattro";
  }
  if (!env.r.obj_valid_or_null(this->tp_as_buffer, 1)) {
    return "invalid_tp_as_buffer";
  }
  if (!env.r.obj_valid_or_null(this->tp_doc, 1)) {
    return "invalid_tp_doc";
  }
  if (!env.r.obj_valid_or_null(this->tp_traverse, 1)) {
    return "invalid_tp_traverse";
  }
  if (!env.r.obj_valid_or_null(this->tp_clear, 1)) {
    return "invalid_tp_clear";
  }
  if (!env.r.obj_valid_or_null(this->tp_richcompare, 1)) {
    return "invalid_tp_richcompare";
  }
  if (!env.r.obj_valid_or_null(this->tp_iter, 1)) {
    return "invalid_tp_iter";
  }
  if (!env.r.obj_valid_or_null(this->tp_iternext, 1)) {
    return "invalid_tp_iternext";
  }
  if (!env.r.obj_valid_or_null(this->tp_methods, 1)) {
    return "invalid_tp_methods";
  }
  if (!env.r.obj_valid_or_null(this->tp_members, 1)) {
    return "invalid_tp_members";
  }
  if (!env.r.obj_valid_or_null(this->tp_getset, 1)) {
    return "invalid_tp_getset";
  }
  if (!env.r.obj_valid_or_null(this->tp_base, 1)) {
    return "invalid_tp_base";
  }
  if (!env.r.obj_valid_or_null(this->tp_dict, 1)) {
    return "invalid_tp_dict";
  }
  if (!env.r.obj_valid_or_null(this->tp_descr_get, 1)) {
    return "invalid_tp_descr_get";
  }
  if (!env.r.obj_valid_or_null(this->tp_descr_set, 1)) {
    return "invalid_tp_descr_set";
  }
  if (!env.r.obj_valid_or_null(this->tp_init, 1)) {
    return "invalid_tp_init";
  }
  if (!env.r.obj_valid_or_null(this->tp_alloc, 1)) {
    return "invalid_tp_alloc";
  }
  if (!env.r.obj_valid_or_null(this->tp_new, 1)) {
    return "invalid_tp_new";
  }
  if (!env.r.obj_valid_or_null(this->tp_free, 1)) {
    return "invalid_tp_free";
  }
  if (!env.r.obj_valid_or_null(this->tp_is_gc, 1)) {
    return "invalid_tp_is_gc";
  }
  if (!env.r.obj_valid_or_null(this->tp_bases, 1)) {
    return "invalid_tp_bases";
  }
  if (!env.r.obj_valid_or_null(this->tp_mro, 1)) {
    return "invalid_tp_mro";
  }
  if (!env.r.obj_valid_or_null(this->tp_cache, 1)) {
    return "invalid_tp_cache";
  }
  if (!env.r.obj_valid_or_null(this->tp_subclasses, 1)) {
    return "invalid_tp_subclasses";
  }
  if (!env.r.obj_valid_or_null(this->tp_weaklist, 1)) {
    return "invalid_tp_weaklist";
  }
  if (!env.r.obj_valid_or_null(this->tp_del, 1)) {
    return "invalid_tp_del";
  }
  if (!env.r.obj_valid_or_null(this->tp_finalize, 1)) {
    return "invalid_tp_finalize";
  }
  try {
    if (this->name(env.r).empty()) {
      return "invalid_name";
    }
  } catch (const std::out_of_range&) {
    return "invalid_name_ptr";
  }
  return nullptr;
}

std::string PyTypeObject::repr(Traversal& t) const {
  if (t.in_progress.empty()) {
    std::string indent_str(t.recursion_depth * 2, ' ');

    std::deque<std::string> lines;
    auto cycle_guard = t.cycle_guard(this);
    if (cycle_guard.is_recursive) {
      throw std::logic_error("Recursive repr when in_progress was previously empty");
    }
    if (!t.recursion_allowed()) {
      return std::format("<type {} !recursion_depth>", this->name(t.env.r));
    }

    auto indent = t.indent();
    lines.emplace_back(std::format("<type {}", this->name(t.env.r)));
    lines.emplace_back(std::format("{}  tp_basicsize={}", indent_str, this->tp_basicsize));
    lines.emplace_back(std::format("{}  tp_itemsize={}", indent_str, this->tp_itemsize));
    lines.emplace_back(std::format("{}  tp_dealloc={}", indent_str, this->tp_dealloc));
    lines.emplace_back(std::format("{}  tp_vectorcall_offset={}", indent_str, this->tp_vectorcall_offset));
    lines.emplace_back(std::format("{}  tp_getattr={}", indent_str, this->tp_getattr));
    lines.emplace_back(std::format("{}  tp_setattr={}", indent_str, this->tp_setattr));
    lines.emplace_back(std::format("{}  tp_as_async={}", indent_str, this->tp_as_async)); // TODO: Parse as PyAsyncMethods*
    lines.emplace_back(std::format("{}  tp_repr={}", indent_str, this->tp_repr));
    lines.emplace_back(std::format("{}  tp_as_number={}", indent_str, this->tp_as_number)); // TODO: Parse as PyNumberMethods*
    lines.emplace_back(std::format("{}  tp_as_sequence={}", indent_str, this->tp_as_sequence)); // TODO: Parse as PySequenceMethods*
    lines.emplace_back(std::format("{}  tp_as_mapping={}", indent_str, this->tp_as_mapping)); // TODO: Parse as PyMappingMethods*
    lines.emplace_back(std::format("{}  tp_hash={}", indent_str, this->tp_hash));
    lines.emplace_back(std::format("{}  tp_call={}", indent_str, this->tp_call));
    lines.emplace_back(std::format("{}  tp_str={}", indent_str, this->tp_str));
    lines.emplace_back(std::format("{}  tp_getattro={}", indent_str, this->tp_getattro));
    lines.emplace_back(std::format("{}  tp_setattro={}", indent_str, this->tp_setattro));
    lines.emplace_back(std::format("{}  tp_as_buffer={}", indent_str, this->tp_as_buffer)); // TODO: Parse as PyBufferProcs*
    lines.emplace_back(std::format("{}  tp_flags={}", indent_str, this->tp_flags));
    lines.emplace_back(std::format("{}  tp_doc={}", indent_str, this->tp_doc));
    lines.emplace_back(std::format("{}  tp_traverse={}", indent_str, this->tp_traverse));
    lines.emplace_back(std::format("{}  tp_clear={}", indent_str, this->tp_clear));
    lines.emplace_back(std::format("{}  tp_richcompare={}", indent_str, this->tp_richcompare));
    lines.emplace_back(std::format("{}  tp_weaklistoffset={}", indent_str, this->tp_weaklistoffset));
    lines.emplace_back(std::format("{}  tp_iter={}", indent_str, this->tp_iter));
    lines.emplace_back(std::format("{}  tp_iternext={}", indent_str, this->tp_iternext));
    lines.emplace_back(std::format("{}  tp_methods={}", indent_str, this->tp_methods)); // TODO: Parse as PyMethodDef*
    if (!this->tp_members.is_null()) {
      std::string tp_members_repr;
      try {
        tp_members_repr = std::format("{} [\n", this->tp_members);
        auto def_ptr = this->tp_members;
        for (;;) {
          const auto& def = t.env.r.get(def_ptr);
          if (def.name.is_null()) {
            break;
          }
          tp_members_repr += std::format("{}    {}\n", indent_str, def.repr(t.env.r));
          def_ptr = def_ptr.offset(1);
        }
        tp_members_repr += std::format("{}  ]", indent_str);
      } catch (const std::exception& e) {
        tp_members_repr = std::format("{} !invalid:{}", this->tp_members, e.what());
      }
      lines.emplace_back(std::format("{}  tp_members={}", indent_str, tp_members_repr));
    } else {
      lines.emplace_back(std::format("{}  tp_members=NULL", indent_str));
    }
    lines.emplace_back(std::format("{}  tp_getset={}", indent_str, this->tp_getset)); // TODO: Parse as PyGetSetDef*
    lines.emplace_back(std::format("{}  tp_base={}", indent_str, t.repr(this->tp_base)));
    size_t prev_max_recursion_depth = t.max_recursion_depth;
    t.max_recursion_depth = 2;
    lines.emplace_back(std::format("{}  tp_dict={}", indent_str, t.repr(this->tp_dict)));
    t.max_recursion_depth = prev_max_recursion_depth;
    lines.emplace_back(std::format("{}  tp_descr_get={}", indent_str, this->tp_descr_get));
    lines.emplace_back(std::format("{}  tp_descr_set={}", indent_str, this->tp_descr_set));
    lines.emplace_back(std::format("{}  tp_dictoffset={}", indent_str, this->tp_dictoffset));
    lines.emplace_back(std::format("{}  tp_init={}", indent_str, this->tp_init));
    lines.emplace_back(std::format("{}  tp_alloc={}", indent_str, this->tp_alloc));
    lines.emplace_back(std::format("{}  tp_new={}", indent_str, this->tp_new));
    lines.emplace_back(std::format("{}  tp_free={}", indent_str, this->tp_free));
    lines.emplace_back(std::format("{}  tp_is_gc={}", indent_str, this->tp_is_gc));
    lines.emplace_back(std::format("{}  tp_bases={}", indent_str, t.repr(this->tp_bases)));
    lines.emplace_back(std::format("{}  tp_mro={}", indent_str, t.repr(this->tp_mro)));
    lines.emplace_back(std::format("{}  tp_cache={}", indent_str, t.repr(this->tp_cache)));
    lines.emplace_back(std::format("{}  tp_subclasses={}", indent_str, t.repr(this->tp_subclasses)));
    lines.emplace_back(std::format("{}  tp_weaklist={}", indent_str, t.repr(this->tp_weaklist)));
    lines.emplace_back(std::format("{}  tp_del={}", indent_str, this->tp_del));
    lines.emplace_back(std::format("{}  tp_version_tag={}", indent_str, this->tp_version_tag));
    lines.emplace_back(std::format("{}  tp_finalize={}", indent_str, this->tp_finalize));
    lines.emplace_back(std::format("{}  tp_vectorcall={}", indent_str, this->tp_vectorcall));
    lines.emplace_back(indent_str + ">");
    return phosg::join(lines, "\n");

  } else {
    return std::format("<type {}>", this->name(t.env.r));
  }
}
