#include "Base.hh"
#include "PyAsyncObjects.hh"
#include "PyCellObject.hh"
#include "PyCodeObject.hh"
#include "PyDictObject.hh"
#include "PyFloatObject.hh"
#include "PyFrameObject.hh"
#include "PyGeneratorObjects.hh"
#include "PyIntegerObjects.hh"
#include "PyListObject.hh"
#include "PyObject.hh"
#include "PySetObject.hh"
#include "PyStringObjects.hh"
#include "PyTupleObject.hh"
#include "PyTypeObject.hh"

Environment::Environment(const std::string& data_path)
    : data_path(data_path),
      analysis_filename(std::format(
          "{}{:c}analysis-data.json", data_path, std::filesystem::is_directory(this->data_path) ? '/' : ':')),
      r(data_path) {
  phosg::JSON json;
  try {
    json = phosg::JSON::parse(phosg::load_file(this->analysis_filename));
  } catch (const phosg::cannot_open_file&) {
    return;
  }
  try {
    this->base_type_object.addr = json.get_int("base_type_object", this->base_type_object.addr);
  } catch (const std::out_of_range&) {
  }
  try {
    for (const auto& it : json.get_dict("type_objects")) {
      this->type_objects.emplace(it.first, it.second->as_int());
    }
  } catch (const std::out_of_range&) {
  }
}

void Environment::save_analysis() const {
  auto type_objects_json = phosg::JSON::dict();
  for (const auto& [name, addr] : this->type_objects) {
    type_objects_json.emplace(name, addr.addr);
  }
  auto json = phosg::JSON::dict({
      {"base_type_object", this->base_type_object.addr},
      {"type_objects", type_objects_json},
  });
  phosg::save_file(this->analysis_filename, json.serialize());
}

const char* Environment::invalid_reason(MappedPtr<PyObject> addr, MappedPtr<PyTypeObject> expected_type) const {
  if (addr.is_null()) {
    return "null_obj_ptr";
  }

  try {
    const auto& obj = this->r.get(addr);
    if (const char* ir = obj.invalid_reason(*this)) {
      return ir;
    }

    const auto& type_obj = this->r.get(obj.ob_type);
    if (type_obj.invalid_reason(*this)) {
      return "invalid_type_obj";
    }
    if (!expected_type.is_null() && (obj.ob_type != expected_type)) {
      return "incorrect_type";
    }

    if (obj.ob_type == this->base_type_object) {
      return this->r.get(addr.cast<PyTypeObject>()).invalid_reason(*this);

    } else if (obj.ob_type == this->get_type_if_exists("int")) {
      return this->r.get(addr.cast<PyLongObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("bool")) {
      return this->r.get(addr.cast<PyBoolObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("float")) {
      return this->r.get(addr.cast<PyFloatObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("bytes")) {
      return this->r.get(addr.cast<PyBytesObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("str")) {
      return this->r.get(addr.cast<PyASCIIStringObject>()).invalid_reason(*this);

    } else if (obj.ob_type == this->get_type_if_exists("tuple")) {
      return this->r.get(addr.cast<PyTupleObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("list")) {
      return this->r.get(addr.cast<PyListObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("set")) {
      return this->r.get(addr.cast<PySetObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("dict")) {
      return this->r.get(addr.cast<PyDictObject>()).invalid_reason(*this);

    } else if (obj.ob_type == this->get_type_if_exists("code")) {
      return this->r.get(addr.cast<PyCodeObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("cell")) {
      return this->r.get(addr.cast<PyCellObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("frame")) {
      return this->r.get(addr.cast<PyFrameObject>()).invalid_reason(*this);

    } else if (obj.ob_type == this->get_type_if_exists("generator")) {
      return this->r.get(addr.cast<PyGenObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("coroutine")) {
      return this->r.get(addr.cast<PyCoroObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("asyncgen")) { // TODO: This might be wrong
      return this->r.get(addr.cast<PyAsyncGenObject>()).invalid_reason(*this);

    } else if (obj.ob_type == this->get_type_if_exists("_asyncio.Future")) {
      return this->r.get(addr.cast<PyAsyncFutureObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("_asyncio.Task")) {
      return this->r.get(addr.cast<PyAsyncTaskObject>()).invalid_reason(*this);
    } else if (obj.ob_type == this->get_type_if_exists("_GatheringFuture")) {
      return this->r.get(addr.cast<PyAsyncGatheringFutureObject>()).invalid_reason(*this);

    } else {
      auto type_name = type_obj.name(this->r);
      if (type_name == "NoneType") {
        return "None";

      } else {
        try {
          auto dict_addr = this->r.get(addr.offset_bytes(0x10).cast<MappedPtr<PyDictObject>>());
          const auto& dict_obj = this->r.get(dict_addr);
          if (dict_obj.ob_type != this->get_type_if_exists("dict")) {
            return "dict_attr_not_dict";
          }
          return dict_obj.invalid_reason(*this);

        } catch (const std::out_of_range&) {
          return "dict_out_of_range";
        }
      }
    }
  } catch (const std::out_of_range&) {
    return "invalid_addr";
  }
}

std::unordered_set<MappedPtr<void>> Environment::direct_referents(MappedPtr<PyObject> addr) const {
  if (addr.is_null()) {
    throw invalid_object("null_obj_ptr");
  }

  try {
    const auto& obj = this->r.get(addr);
    if (const char* ir = obj.invalid_reason(*this)) {
      throw invalid_object(ir);
    }

    if (obj.ob_type == this->base_type_object) {
      return this->r.get(addr.cast<PyTypeObject>()).direct_referents(*this);

    } else if (obj.ob_type == this->get_type_if_exists("int")) {
      return this->r.get(addr.cast<PyLongObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("bool")) {
      return this->r.get(addr.cast<PyBoolObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("float")) {
      return this->r.get(addr.cast<PyFloatObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("bytes")) {
      return this->r.get(addr.cast<PyBytesObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("str")) {
      return this->r.get(addr.cast<PyASCIIStringObject>()).direct_referents(*this);

    } else if (obj.ob_type == this->get_type_if_exists("tuple")) {
      return this->r.get(addr.cast<PyTupleObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("list")) {
      return this->r.get(addr.cast<PyListObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("set")) {
      return this->r.get(addr.cast<PySetObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("dict")) {
      return this->r.get(addr.cast<PyDictObject>()).direct_referents(*this);

    } else if (obj.ob_type == this->get_type_if_exists("code")) {
      return this->r.get(addr.cast<PyCodeObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("cell")) {
      return this->r.get(addr.cast<PyCellObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("frame")) {
      return this->r.get(addr.cast<PyFrameObject>()).direct_referents(*this);

    } else if (obj.ob_type == this->get_type_if_exists("generator")) {
      return this->r.get(addr.cast<PyGenObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("coroutine")) {
      return this->r.get(addr.cast<PyCoroObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("asyncgen")) { // TODO: This might be wrong
      return this->r.get(addr.cast<PyAsyncGenObject>()).direct_referents(*this);

    } else if (obj.ob_type == this->get_type_if_exists("_asyncio.Future")) {
      return this->r.get(addr.cast<PyAsyncFutureObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("_asyncio.Task")) {
      return this->r.get(addr.cast<PyAsyncTaskObject>()).direct_referents(*this);
    } else if (obj.ob_type == this->get_type_if_exists("_GatheringFuture")) {
      return this->r.get(addr.cast<PyAsyncGatheringFutureObject>()).direct_referents(*this);

    } else {
      const auto& type_obj = this->r.get(obj.ob_type);
      if (type_obj.invalid_reason(*this)) {
        throw invalid_object("invalid_type_obj");
      }

      auto type_name = type_obj.name(this->r);
      if (type_name == "NoneType") {
        return {};

      } else {
        try {
          auto dict_addr = this->r.get(addr.offset_bytes(0x10).cast<MappedPtr<PyDictObject>>());
          const auto& dict_obj = this->r.get(dict_addr);
          if (dict_obj.ob_type != this->get_type_if_exists("dict")) {
            throw invalid_object("dict_attr_not_dict");
          }
          if (const char* ir = dict_obj.invalid_reason(*this)) {
            throw invalid_object(ir);
          }
          return dict_obj.direct_referents(*this);

        } catch (const std::out_of_range&) {
          throw invalid_object("dict_out_of_range");
        }
      }
    }
  } catch (const std::out_of_range&) {
    throw invalid_object("invalid_addr");
  }
}

Traversal Environment::traverse(phosg::Arguments* args) const {
  return Traversal(*this, args);
}

Traversal::Traversal(const Environment& env, phosg::Arguments* args) : env(env) {
  if (args) {
    this->max_recursion_depth = args->get<size_t>("max-recursion-depth", this->max_recursion_depth);
    this->max_entries = args->get<size_t>("max-entries", this->max_entries);
    this->max_string_length = args->get<size_t>("max-string-length", this->max_string_length);
    this->frame_omit_back = args->get<bool>("frame-omit-back");
    this->frame_omit_locals = args->get<bool>("frame-omit-locals");
    this->bytes_as_hex = args->get<bool>("bytes-as-hex");
    this->show_all_addresses = args->get<bool>("show-all-addresses");
    this->is_short = args->get<bool>("short");
  }
}

std::string Traversal::repr(MappedPtr<PyObject> addr) {
  if (addr.is_null()) {
    return "NULL";
  }

  bool show_address = true;
  std::string ret;
  try {
    const auto& obj = this->env.r.get<PyObject>(addr);
    if (const char* ir = this->check_valid(obj)) {
      ret = std::format("<!{}>", ir);
    }

    const auto& type_obj = this->env.r.get<PyTypeObject>(obj.ob_type);
    if (const char* ir = this->check_valid(type_obj)) {
      this->is_valid = false;
      ret = std::format("<<!{}>@{}>", ir, obj.ob_type);
    }

    auto check_valid_and_repr = [&]<typename T>() -> std::string {
      const auto& obj = this->env.r.get(addr.cast<T>());
      if (const char* ir = obj.invalid_reason(this->env)) {
        return std::format("<{} !{}>", type_obj.name(this->env.r), ir);
      }
      return obj.repr(*this);
    };

    if (obj.ob_type == this->env.base_type_object) {
      ret = check_valid_and_repr.template operator()<PyTypeObject>();

    } else if (obj.ob_type == this->env.get_type_if_exists("int")) {
      ret = check_valid_and_repr.template operator()<PyLongObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("bool")) {
      ret = check_valid_and_repr.template operator()<PyBoolObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("float")) {
      ret = check_valid_and_repr.template operator()<PyFloatObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("bytes")) {
      ret = check_valid_and_repr.template operator()<PyBytesObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("str")) {
      ret = check_valid_and_repr.template operator()<PyASCIIStringObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();

    } else if (obj.ob_type == this->env.get_type_if_exists("tuple")) {
      ret = check_valid_and_repr.template operator()<PyTupleObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("list")) {
      ret = check_valid_and_repr.template operator()<PyListObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("set")) {
      ret = check_valid_and_repr.template operator()<PySetObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();
    } else if (obj.ob_type == this->env.get_type_if_exists("dict")) {
      ret = check_valid_and_repr.template operator()<PyDictObject>();
      show_address = this->show_all_addresses || this->in_progress.empty();

    } else if (obj.ob_type == this->env.get_type_if_exists("code")) {
      ret = check_valid_and_repr.template operator()<PyCodeObject>();
    } else if (obj.ob_type == this->env.get_type_if_exists("cell")) {
      ret = check_valid_and_repr.template operator()<PyCellObject>();
    } else if (obj.ob_type == this->env.get_type_if_exists("frame")) {
      ret = check_valid_and_repr.template operator()<PyFrameObject>();

    } else if (obj.ob_type == this->env.get_type_if_exists("generator")) {
      ret = check_valid_and_repr.template operator()<PyGenObject>();
    } else if (obj.ob_type == this->env.get_type_if_exists("coroutine")) {
      ret = check_valid_and_repr.template operator()<PyCoroObject>();
    } else if (obj.ob_type == this->env.get_type_if_exists("asyncgen")) { // TODO: This might be wrong
      ret = check_valid_and_repr.template operator()<PyAsyncGenObject>();

    } else if (obj.ob_type == this->env.get_type_if_exists("_asyncio.Future")) {
      ret = check_valid_and_repr.template operator()<PyAsyncFutureObject>();
    } else if (obj.ob_type == this->env.get_type_if_exists("_asyncio.Task")) {
      ret = check_valid_and_repr.template operator()<PyAsyncTaskObject>();
    } else if (obj.ob_type == this->env.get_type_if_exists("_GatheringFuture")) {
      ret = check_valid_and_repr.template operator()<PyAsyncGatheringFutureObject>();

    } else {
      auto type_name = type_obj.name(this->env.r);
      if (type_name == "NoneType") {
        ret = "None";
        show_address = this->show_all_addresses || this->in_progress.empty();

      } else {
        try {
          // Only try to expand user type dicts if this is the root object
          if (!this->in_progress.empty()) {
            throw std::out_of_range("Not root object");
          }
          auto dict_addr = this->env.r.get(addr.offset_bytes(0x10).cast<MappedPtr<PyDictObject>>());
          const auto& dict_obj = this->env.r.get<PyDictObject>(dict_addr);
          if (dict_obj.ob_type != this->env.get_type_if_exists("dict")) {
            throw std::out_of_range("__dict__ object is not a dict");
          }
          auto cycle_guard = this->cycle_guard(&this->env.r.get(addr));
          std::string dict_repr = this->repr(dict_addr);
          ret = std::format("<{} {}>", type_name, dict_repr);

        } catch (const std::out_of_range&) {
          ret = std::format("<{}>", type_name);
        }
      }
    }
  } catch (const std::out_of_range&) {
    this->is_valid = false;
    ret = "<!invalid_addr>";
  }

  return show_address ? std::format("{}@{}", ret, addr) : ret;
}
