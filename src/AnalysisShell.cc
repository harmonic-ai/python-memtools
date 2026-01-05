#include <readline/history.h>
#include <readline/readline.h>

#include <atomic>
#include <mutex>
#include <phosg/Arguments.hh>
#include <set>

#include "AnalysisShell.hh"
#include "Types/PyAsyncObjects.hh"
#include "Types/PyGeneratorObjects.hh"
#include "Types/PyThreadState.hh"
#include "Types/PyTypeObject.hh"

struct ShellCommand {
  std::string name;
  std::string help_text;
  void (*run)(AnalysisShell&, phosg::Arguments&);

  static std::vector<const ShellCommand*> commands_by_order;
  static std::unordered_map<std::string, const ShellCommand*> commands_by_name;

  ShellCommand(std::string name, std::string help_text, void (*run)(AnalysisShell&, phosg::Arguments&))
      : name(std::move(name)), help_text(std::move(help_text)), run(run) {
    // These are expected to be constructed only statically, so it's OK to save raw pointers in these registries
    this->commands_by_name.emplace(this->name, this);
    this->commands_by_order.emplace_back(this);
  }

  static void dispatch(AnalysisShell& shell, const std::string& command) {
    phosg::Arguments args(command);
    const auto& command_name = args.get<std::string>(0, false);
    if (command_name.empty()) {
      return;
    }
    auto cmd_it = ShellCommand::commands_by_name.find(command_name);
    if (cmd_it == ShellCommand::commands_by_name.end()) {
      phosg::fwrite_fmt(stderr, "Invalid command: {}\n", command_name);
    } else {
      ShellCommand::commands_by_name.at(command_name)->run(shell, args);
    }
  }
};

std::vector<const ShellCommand*> ShellCommand::commands_by_order;
std::unordered_map<std::string, const ShellCommand*> ShellCommand::commands_by_name;

static void find_base_type_object(Environment& env, size_t max_threads) {
  std::mutex output_lock;
  std::vector<MappedPtr<PyTypeObject>> candidates;

  // Find a PyTypeObject that has ob_type == self and name == "type"
  env.r.map_all_addresses<PyTypeObject>([&](const PyTypeObject& ty, MappedPtr<PyTypeObject> addr, size_t) -> void {
    if ((ty.ob_type != addr) || ty.invalid_reason(env) || (ty.name(env.r) != "type")) {
      return;
    }
    std::lock_guard<std::mutex> g(output_lock);
    phosg::fwrite_fmt(stderr, CLEAR_LINE "Base type candidate found at {}\n", addr);
    candidates.emplace_back(addr);
  },
      8, max_threads);
  fputc('\n', stdout);
  if (candidates.size() == 1) {
    env.base_type_object = candidates[0];
    env.save_analysis();
  }
}

static void find_all_type_objects(Environment& env, size_t max_threads) {
  if (env.base_type_object.is_null()) {
    throw std::runtime_error("Base type object not found; run find-base-type first");
  }
  env.type_objects.clear();

  std::mutex output_lock;
  bool any_env_changes_made = false;

  // Find all PyTypeObjects with ob_type == type and invalid_reason == nullptr
  env.r.map_all_addresses<PyTypeObject>([&](const PyTypeObject& ty, MappedPtr<PyTypeObject> addr, size_t) -> void {
    if (ty.ob_type != env.base_type_object || ty.invalid_reason(env)) {
      return;
    }
    std::string type_name = ty.name(env.r);

    std::lock_guard<std::mutex> g(output_lock);
    auto emplace_ret = env.type_objects.emplace(type_name, addr);
    if (emplace_ret.second) {
      phosg::fwrite_fmt(stderr, CLEAR_LINE "Found <type {}> at {}" CLEAR_LINE_TO_END "\n", type_name, addr);
      any_env_changes_made = true;
    } else if (emplace_ret.first->second != addr) {
      env.type_objects.emplace(std::format("{}+{}", type_name, addr), addr);
      phosg::fwrite_fmt(stderr,
          CLEAR_LINE "Warning: found <type {}> at {}, but it already exists at {}" CLEAR_LINE_TO_END "\n",
          type_name, addr, emplace_ret.first->second);
    }
  },
      8, max_threads);
  fputc('\n', stdout);
  if (any_env_changes_made) {
    env.save_analysis();
  }
}

AnalysisShell::AnalysisShell(const std::string& data_path, size_t max_threads)
    : max_threads(max_threads), env(data_path) {
  if (this->max_threads == 0) {
    this->max_threads = std::thread::hardware_concurrency();
  }
}

void AnalysisShell::prepare() {
  if (this->env.base_type_object.is_null()) {
    phosg::fwrite_fmt(stderr, "Base type object not present in analysis data; looking for it\n");
    find_base_type_object(this->env, this->max_threads);
  }
  if (this->env.base_type_object.is_null()) {
    phosg::fwrite_fmt(stderr, "Failed to find exactly one base type object; cannot proceed with analysis\n");
  } else if (this->env.type_objects.empty()) {
    phosg::fwrite_fmt(stderr, "No type objects are present in analysis data; looking for them\n");
    find_all_type_objects(this->env, this->max_threads);
  }
}

void AnalysisShell::run() {
  this->prepare();

  std::string prompt = std::format("{}> ", this->env.data_path);
  while (!this->should_exit) {
    try {
      std::string command;
      {
        char* buf = readline(prompt.c_str());
        if (!buf) { // EOF (Ctrl+D)
          break;
        }
        if (buf[0] && (buf[0] != ' ')) {
          add_history(buf);
        }
        command = buf;
        free(buf);
      }

      if (command.empty()) {
        fputc('\n', stdout);
        break; // EOF
      }
      phosg::strip_whitespace(command);
      this->run_command(command);

    } catch (const std::exception& e) {
      phosg::fwrite_fmt(stderr, "Error: {}\n", e.what());
    }
  }
}

void AnalysisShell::run_command(const std::string& command) {
  ShellCommand::dispatch(*this, command);
}

ShellCommand c_help(
    "help", "\
  help\n\
    You\'re reading it now.\n",
    +[](AnalysisShell&, phosg::Arguments&) -> void {
      phosg::fwrite_fmt(stdout, "Commands:\n");
      for (const auto& def : ShellCommand::commands_by_order) {
        phosg::fwritex(stdout, def->help_text);
      }
    });

ShellCommand c_exit(
    "exit", "\
  exit\n\
    Ends this session.\n",
    +[](AnalysisShell& shell, phosg::Arguments&) -> void {
      shell.should_exit = true;
    });

ShellCommand c_regions(
    "regions", "\
  regions\n\
    Lists all memory regions in the current memory snapshot.\n",
    +[](AnalysisShell& shell, phosg::Arguments&) -> void {
      size_t total_size = 0;
      for (const auto& [start, size] : shell.env.r.all_regions()) {
        MappedPtr<void> end = start.offset_bytes(size);
        phosg::fwrite_fmt(stdout, "{}-{} ({})\n", start, end, phosg::format_size(size));
        total_size += size;
      }
      phosg::fwrite_fmt(stdout, "All regions: {}\n", phosg::format_size(total_size));
    });

ShellCommand c_show_analysis_data(
    "show-analysis-data", "\
  show-analysis-data\n\
    Shows the saved analysis data for this snapshot.\n",
    +[](AnalysisShell& shell, phosg::Arguments&) -> void {
      phosg::fwrite_fmt(stderr, "Base type object at {}\n", shell.env.base_type_object);
      std::vector<std::pair<std::string, MappedPtr<PyTypeObject>>> sorted_types;
      for (const auto& [name, addr] : shell.env.type_objects) {
        sorted_types.emplace_back(std::make_pair(name, addr));
      }
      sort(sorted_types.begin(), sorted_types.end());
      for (const auto& [name, addr] : sorted_types) {
        phosg::fwrite_fmt(stderr, "Type object {} at {}\n", name, addr);
      }
      phosg::fwrite_fmt(stderr, "{} non-base type objects overall\n", sorted_types.size());
    });

ShellCommand c_find(
    "find", "\
  find DATA [OPTIONS]\n\
    Searches for DATA in all readable memory. Options:\n\
      --ptr: Parse DATA as a 64-bit hexadecimal integer.\n\
      --bswap: Byteswap DATA before searching (only if --ptr is also given).\n\
      --align=ALIGN: Only find DATA at addresses aligned to ALIGN bytes\n\
          (default 8 if --ptr is given, or 1 otherwise).\n\
      --count: Don\'t print each occurrence, just count them.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      size_t alignment;
      std::string data;
      if (args.get<bool>("ptr")) {
        uint64_t ptr_value = std::stoull(args.get<std::string>(1, true), nullptr, 16);
        phosg::StringWriter w;
        if (args.get<bool>("bswap")) {
          w.put_u64r(ptr_value);
        } else {
          w.put_u64(ptr_value);
        }
        data = std::move(w.str());
        alignment = args.get<size_t>("align", 8);
      } else {
        data = phosg::parse_data_string(args.get<std::string>(1, true));
        alignment = args.get<size_t>("align", 1);
      }

      bool count_only = args.get<bool>("count");

      std::mutex console_lock;
      std::atomic<size_t> result_count = 0;

      if (data.size() == 8 && alignment == 8) {
        // Optimized common case: aligned 8-byte comparison instead of memcmp()
        uint64_t target_value = *reinterpret_cast<const uint64_t*>(data.data());
        shell.env.r.map_all_addresses<uint64_t>(
            [&](const uint64_t& value, MappedPtr<uint64_t> addr, size_t) -> void {
              if (value == target_value) {
                result_count++;
                if (!count_only) {
                  std::lock_guard<std::mutex> g(console_lock);
                  phosg::fwrite_fmt(stderr, CLEAR_LINE "Data found at {}\n", addr);
                }
              }
            },
            alignment, shell.max_threads);

      } else {
        shell.env.r.map_all_addresses<uint8_t>(
            [&](const uint8_t& mem_data, MappedPtr<void> addr, size_t) -> void {
              if (!memcmp(&mem_data, data.data(), data.size())) {
                result_count++;
                if (!count_only) {
                  std::lock_guard<std::mutex> g(console_lock);
                  phosg::fwrite_fmt(stderr, CLEAR_LINE "Data found at {}\n", addr);
                }
              }
            },
            alignment, shell.max_threads, data.size());
      }

      phosg::fwrite_fmt(stderr, CLEAR_LINE "{} results found\n", result_count.load());
    });

ShellCommand c_count_by_type(
    "count-by-type", "\
  count-by-type\n\
    Counts the number of existing objects for each known type.\n",
    +[](AnalysisShell& shell, phosg::Arguments&) -> void {
      if (shell.env.base_type_object.is_null()) {
        throw std::runtime_error("Base type object not present in analysis data");
      }

      // Invert type_objects for fast lookup
      std::unordered_map<MappedPtr<PyTypeObject>, std::string> name_for_type;
      for (const auto& [name, type] : shell.env.type_objects) {
        name_for_type.emplace(type, name);
      }

      std::vector<std::unordered_map<MappedPtr<PyTypeObject>, size_t>> count_for_type;
      count_for_type.resize(shell.max_threads);

      shell.env.r.map_all_addresses<PyTypeObject>(
          [&](const PyObject& obj, MappedPtr<PyObject> addr, size_t thread_index) -> void {
            if (name_for_type.count(obj.ob_type) && !shell.env.invalid_reason(addr)) {
              count_for_type[thread_index][obj.ob_type]++;
            }
          },
          8, shell.max_threads);
      fputc('\n', stdout);

      std::unordered_map<MappedPtr<PyTypeObject>, size_t> overall_count_for_type;
      for (size_t z = 0; z < count_for_type.size(); z++) {
        const auto& thread_count_for_type = count_for_type[z];
        phosg::fwrite_fmt(stderr, "Collecting {} results from thread {}\n", thread_count_for_type.size(), z);
        for (const auto& [type, count] : thread_count_for_type) {
          overall_count_for_type[type] += count;
        }
      }

      phosg::fwrite_fmt(stderr, "Found {} types\n", count_for_type.size());

      std::vector<std::tuple<size_t, std::string, MappedPtr<PyTypeObject>>> entries;
      entries.reserve(overall_count_for_type.size());
      for (const auto& [type_addr, count] : overall_count_for_type) {
        try {
          const std::string& type_name = name_for_type.at(type_addr);
          entries.emplace_back(std::make_tuple(count, type_name, type_addr));
        } catch (const std::out_of_range&) {
        }
      }

      phosg::fwrite_fmt(stderr, "Sorting {} entries\n", entries.size());
      sort(entries.begin(), entries.end());

      for (const auto& [count, name, type_addr] : entries) {
        phosg::fwrite_fmt(stderr, "({} objects) {} @ {}\n", count, name, type_addr);
      }
    });

ShellCommand c_find_all_objects(
    "find-all-objects", "\
  find-all-objects [OPTIONS]\n\
    Finds all objects of a given type. Options:\n\
      --type-addr=ADDRESS: Find objects whose type object is at this address.\n\
      --type-name=NAME: Find objects whose type has this name.\n\
      --count: Only count the number of objects; don\'t print them.\n\
    The formatting options to the repr command are also valid here.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      // TODO: It'd be nice to have something like --max-results=N here
      MappedPtr<PyTypeObject> type_addr{args.get<uint64_t>("type-addr", 0, phosg::Arguments::IntFormat::HEX)};
      if (type_addr.is_null()) {
        const std::string& type_name = args.get<std::string>("type-name", false);
        type_addr = shell.env.type_objects.at(type_name);
      }
      bool count_only = args.get<bool>("count");

      std::mutex output_lock;
      std::atomic<size_t> result_count = 0;
      shell.env.r.map_all_addresses<PyObject>([&](const PyObject& obj, MappedPtr<PyObject> addr, size_t) -> void {
        if ((obj.ob_type != type_addr) || shell.env.invalid_reason(addr)) {
          return;
        }

        if (count_only) {
          result_count++;
        } else {
          auto t = shell.env.traverse(&args);
          std::string repr = t.repr(addr);
          if (!t.is_valid) {
            return;
          }
          result_count++;

          std::lock_guard<std::mutex> g(output_lock);
          phosg::fwrite_fmt(stderr, CLEAR_LINE);
          phosg::fwrite_fmt(stdout, "{}\n", repr);
        }
      },
          8, shell.max_threads);
      phosg::fwrite_fmt(stderr, CLEAR_LINE "{} objects found\n", result_count.load());
    });

ShellCommand c_find_references(
    "find-references", "\
  find-references ADDRESS [OPTIONS]\n\
    Find references to the given object, from types that python-memtools\n\
    implements (importantly, this excludes many types defined in C extension\n\
    modules, even those that are part of the standard library).\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      auto target_addr = shell.parse_addr<void>(args.get<std::string>(1, true), args.get<bool>("bswap"));

      std::mutex output_lock;
      size_t result_count = 0;
      shell.env.r.map_all_addresses<PyObject>([&](const PyObject& obj, MappedPtr<PyObject> addr, size_t) -> void {
        // Check if the immediate object is value
        if (shell.env.invalid_reason(addr)) {
          return;
        }

        // Get all referents (this can still throw invalid_object if one of the downstream objects it needs is invalid)
        std::unordered_set<MappedPtr<void>> referents;
        try {
          referents = shell.env.direct_referents(addr);
        } catch (const invalid_object&) {
        }
        if (!referents.count(target_addr)) {
          return;
        }

        auto t = shell.env.traverse(&args);
        std::string repr = t.repr(addr);
        if (!t.is_valid) {
          return;
        }

        std::lock_guard<std::mutex> g(output_lock);
        phosg::fwrite_fmt(stderr, CLEAR_LINE);
        phosg::fwrite_fmt(stdout, "{}\n", repr);
        result_count++;
      },
          8, shell.max_threads);
      phosg::fwrite_fmt(stderr, CLEAR_LINE "{} objects found\n", result_count);
    });

ShellCommand c_find_module(
    "find-module", "\
  find-module NAME\n\
    Find all modules with the given name (as in the __name__ attribute). Note\n\
    that the `sys` module typically contains a dict of all other modules; to\n\
    find this, use `find-module sys`.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      auto module_name = args.get<std::string>(1);
      auto module_type = shell.env.get_type("module");
      auto dict_type = shell.env.get_type_if_exists("dict");

      std::mutex output_lock;
      std::atomic<size_t> result_count = 0;
      shell.env.r.map_all_addresses<PyObject>([&](const PyObject& obj, MappedPtr<PyObject> addr, size_t) -> void {
        if ((obj.ob_type != module_type) || shell.env.invalid_reason(addr)) {
          return;
        }

        auto dict_addr = shell.env.r.get(addr.offset_bytes(0x10).cast<MappedPtr<PyDictObject>>());
        const auto& dict_obj = shell.env.r.get(dict_addr);
        if (dict_obj.ob_type != dict_type) {
          return;
        }
        if (dict_obj.invalid_reason(shell.env)) {
          return;
        }

        try {
          MappedPtr<PyObject> name_addr = dict_obj.value_for_key<PyObject>(shell.env.r, "__name__");
          auto name = decode_string_types(shell.env.r, name_addr);
          if (name != module_name) {
            return;
          }
        } catch (const std::out_of_range&) {
          return;
        }

        auto t = shell.env.traverse(&args);
        std::string repr = t.repr(addr);
        if (!t.is_valid) {
          return;
        }
        result_count++;

        std::lock_guard<std::mutex> g(output_lock);
        phosg::fwrite_fmt(stderr, CLEAR_LINE);
        phosg::fwrite_fmt(stdout, "{}\n", repr);
      },
          8, shell.max_threads);
      phosg::fwrite_fmt(stderr, CLEAR_LINE "{} modules found\n", result_count.load());
    });

ShellCommand c_find_all_threads(
    "find-all-threads", "\
  find-all-threads\n\
    Finds all active thread states.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      std::mutex output_lock;
      shell.env.r.map_all_addresses<PyThreadState>(
          [&](const PyThreadState& obj, MappedPtr<PyThreadState> addr, size_t) -> void {
            if (obj.invalid_reason(shell.env)) {
              return;
            }

            auto t = shell.env.traverse(&args);
            std::string repr = obj.repr(t);
            if (!t.is_valid) {
              return;
            }

            std::lock_guard<std::mutex> g(output_lock);
            phosg::fwrite_fmt(stderr, CLEAR_LINE "{}\n", repr);
          },
          8, shell.max_threads);
    });

ShellCommand c_find_all_stacks(
    "find-all-stacks", "\
  find-all-stacks [OPTIONS]\n\
    Generates the graph of all running frames, then organizes them into\n\
    stacks. This shows what all threads were doing at snapshot time. Options:\n\
      --include-runnable: Include frames that were paused but later runnable.\n\
    The formatting options to the repr command are also valid here.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      bool include_runnable = args.get<bool>("include-runnable");

      MappedPtr<PyTypeObject> frame_type_addr;
      try {
        frame_type_addr = shell.env.type_objects.at("frame");
      } catch (const std::out_of_range&) {
        throw std::runtime_error("Frame type is missing from analysis data");
      }

      std::mutex output_lock;
      size_t num_non_runnable_frames = 0;
      std::unordered_map<MappedPtr<PyFrameObject>, MappedPtr<PyFrameObject>> back_for_frame;
      shell.env.r.map_all_addresses<PyFrameObject>(
          [&](const PyObject& obj, MappedPtr<PyFrameObject> addr, size_t) -> void {
            if (obj.ob_type != frame_type_addr || obj.invalid_reason(shell.env)) {
              return;
            }

            auto t = shell.env.traverse(&args);
            t.max_recursion_depth = 1;
            std::string repr = t.repr(addr);
            if (!t.is_valid) {
              return;
            }

            const auto& f_obj = shell.env.r.get<PyFrameObject>(addr);
            std::string state_name = f_obj.name_for_state(f_obj.f_state);

            std::lock_guard<std::mutex> g(output_lock);
            if (include_runnable ? !f_obj.is_runnable_or_running() : !f_obj.is_running()) {
              num_non_runnable_frames++;
            } else {
              back_for_frame.emplace(addr, f_obj.f_back);
            }
            phosg::fwrite_fmt(stderr,
                CLEAR_LINE "... {} {} from {} ({} runnable frames, {} non-runnable frames)\n",
                addr, state_name, f_obj.f_back, back_for_frame.size(), num_non_runnable_frames);
          },
          8, shell.max_threads);

      // Roots are all frames that are not the f_back of any other frame
      std::set<MappedPtr<PyFrameObject>> roots;
      for (const auto& it : back_for_frame) {
        roots.emplace(it.first);
      }
      for (const auto& it : back_for_frame) {
        roots.erase(it.second);
      }

      phosg::fwrite_fmt(stderr, CLEAR_LINE "\n");
      for (MappedPtr<PyFrameObject> addr : roots) {
        phosg::fwrite_fmt(stderr, "Traceback (most recent call FIRST):\n");
        auto t = shell.env.traverse(&args);
        t.frame_omit_back = true;
        t.is_short = true;
        t.recursion_depth = 1;
        while (!addr.is_null()) {
          std::string repr = t.repr(addr);
          for (ssize_t z = 0; z < t.recursion_depth * 2; z++) {
            fputc(' ', stderr);
          }
          phosg::fwrite_fmt(stderr, "{}\n", repr);
          try {
            addr = back_for_frame.at(addr);
          } catch (const std::out_of_range&) {
            for (ssize_t z = 0; z < t.recursion_depth * 2; z++) {
              fputc(' ', stderr);
            }
            phosg::fwrite_fmt(stderr,
                "<warning: frame points to f_back=@{} which is missing from the found frame list>\n", addr);
            addr = nullptr;
          }
        }
      }
    });

template <bool IsBytes>
void fn_aggregate_strings(AnalysisShell& shell, phosg::Arguments& args) {
  size_t print_smaller_than = args.get<uint64_t>("print-smaller-than", 0);
  size_t print_larger_than = args.get<uint64_t>("print-larger-than", 0);

  const char* type_name = IsBytes ? "bytes" : "str";
  auto type_addr = shell.env.get_type(type_name);

  static const std::vector<size_t> size_buckets = {
      0, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000,
      2000000, 5000000, 10000000, 20000000, 50000000, 100000000, 200000000, 500000000, 1000000000};
  std::vector<size_t> histogram_data;
  size_t total_size = 0;
  size_t total_objects = 0;

  std::mutex output_lock;
  shell.env.r.map_all_addresses<PyObject>([&](const PyObject& obj, MappedPtr<PyObject> addr, size_t) -> void {
    if ((obj.ob_type != type_addr) || obj.invalid_reason(shell.env)) {
      return;
    }

    size_t data_size;
    try {
      if constexpr (IsBytes) {
        data_size = shell.env.r.get(addr.cast<PyBytesObject>()).ob_size;
      } else {
        // TODO: This is slow; make a function that gets the size without decoding/copying the data
        data_size = decode_string_types(shell.env.r, addr).size();
      }
    } catch (const std::exception&) {
      return;
    }
    auto size_it = lower_bound(size_buckets.begin(), size_buckets.end(), data_size);
    size_t bucket_index = size_it - size_buckets.begin();

    std::lock_guard<std::mutex> g(output_lock);
    if (bucket_index >= histogram_data.size()) {
      histogram_data.resize(bucket_index + 1, 0);
    }
    histogram_data[bucket_index]++;
    total_objects++;
    total_size += data_size;
    if ((data_size >= print_larger_than) && (data_size < print_smaller_than)) {
      phosg::fwrite_fmt(stdout, CLEAR_LINE "{}\n", shell.env.traverse(&args).repr(addr));
    }
  },
      8, shell.max_threads);

  phosg::fwrite_fmt(stdout, "Found {} objects with {} data bytes overall ({})\n",
      total_objects, total_size, phosg::format_size(total_size));
  for (size_t z = 0; z < histogram_data.size(); z++) {
    std::string bucket_str = (z < size_buckets.size())
        ? std::format("{}", size_buckets[z])
        : std::format(">{}", size_buckets.back());
    phosg::fwrite_fmt(stdout, "Length <= {}: {} objects\n", bucket_str, histogram_data[z]);
  }
}

ShellCommand c_aggregate_strings(
    "aggregate-strings", "\
  aggregate-strings [OPTIONS]\n\
    Find all strings and generate a log-scaled histogram of their lengths.\n\
    Options:\n\
      --bytes: Aggregate over bytes objects instead of strings.\n\
      --print-smaller-than=N: Print all strings of fewer than N bytes.\n\
      --print-larger-than=N: Print all strings of N bytes or more.\n\
    The formatting options to the repr command are also valid here.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      if (args.get<bool>("bytes")) {
        fn_aggregate_strings<true>(shell, args);
      } else {
        fn_aggregate_strings<false>(shell, args);
      }
    });

ShellCommand c_async_task_graph(
    "async-task-graph", "\
  async-task-graph\n\
    Find all async tasks and futures, and show the graph of awaiters.\n\
    The formatting options to the repr command are also valid here.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      MappedPtr<PyTypeObject> task_type_addr, future_type_addr, gathering_future_type_addr;
      try {
        task_type_addr = shell.env.get_type("_asyncio.Task");
        future_type_addr = shell.env.get_type("_asyncio.Future");
        gathering_future_type_addr = shell.env.get_type("_GatheringFuture");
      } catch (const std::out_of_range&) {
        throw std::runtime_error("_asyncio.Task, _asyncio.Future, and _GatheringFuture must not be missing");
      }
      phosg::fwrite_fmt(stderr, "Looking for objects of types {} (Task), {} (Future), and {} (GatheringFuture)\n",
          task_type_addr, future_type_addr, gathering_future_type_addr);

      std::mutex output_lock;
      std::unordered_map<MappedPtr<PyObject>, std::unordered_set<MappedPtr<PyObject>>> await_targets_for_obj;
      shell.env.r.map_all_addresses<PyObject>([&](const PyObject& obj, MappedPtr<PyObject> addr, size_t) -> void {
        if ((obj.ob_type != task_type_addr) && (obj.ob_type != future_type_addr) && (obj.ob_type != gathering_future_type_addr)) {
          return;
        }
        if (shell.env.invalid_reason(addr)) {
          return;
        }

        auto t = shell.env.traverse(&args);
        t.is_short = true;
        std::string repr = t.repr(addr);
        if (!t.is_valid) {
          return;
        }

        if (obj.ob_type == task_type_addr) {
          const auto& obj = shell.env.r.get(addr.cast<PyAsyncTaskObject>());
          if (obj.invalid_reason(shell.env)) {
            return;
          }

          std::lock_guard<std::mutex> g(output_lock);
          phosg::fwrite_fmt(stderr, CLEAR_LINE "... {} task awaits {}\n", addr, obj.task_fut_waiter);
          await_targets_for_obj[addr].emplace(obj.task_fut_waiter);

        } else if (obj.ob_type == future_type_addr) {
          const auto& obj = shell.env.r.get(addr.cast<PyAsyncFutureObject>());
          if (obj.invalid_reason(shell.env)) {
            return;
          }

          std::lock_guard<std::mutex> g(output_lock);
          phosg::fwrite_fmt(stderr, CLEAR_LINE "... {} future\n", addr);
          await_targets_for_obj.emplace(addr, std::unordered_set<MappedPtr<PyObject>>());

        } else if (obj.ob_type == gathering_future_type_addr) {
          const auto& obj = shell.env.r.get(addr.cast<PyAsyncGatheringFutureObject>());
          if (obj.invalid_reason(shell.env)) {
            return;
          }

          std::lock_guard<std::mutex> g(output_lock);
          auto& targets_set = await_targets_for_obj[addr];
          try {
            for (auto child_addr : obj.children(shell.env)) {
              phosg::fwrite_fmt(stderr, CLEAR_LINE "... {} gather awaits {}\n", addr, child_addr);
              targets_set.emplace(child_addr);
            }
          } catch (const std::exception& e) {
            phosg::fwrite_fmt(stderr, CLEAR_LINE "... {} gather missing children ({})\n", addr, e.what());
          }
        }
      },
          8, shell.max_threads);

      // Roots are all task/future objects that are not the await target of any other task/future object
      std::set<MappedPtr<PyObject>> roots;
      for (const auto& it : await_targets_for_obj) {
        roots.emplace(it.first);
      }
      for (const auto& it : await_targets_for_obj) {
        for (const auto& target : it.second) {
          roots.erase(target);
        }
      }

      // This can't be auto because it's recursive; fortunately we don't need to hyper-optimize this function
      std::function<void(Traversal&, MappedPtr<PyObject>, std::unordered_set<MappedPtr<PyObject>>&)> print_entry =
          [&](Traversal& t, MappedPtr<PyObject> addr, std::unordered_set<MappedPtr<PyObject>>& seen) -> void {
        if (addr.is_null()) {
          return;
        }
        bool addr_seen = !seen.emplace(addr).second;

        std::string repr = addr_seen ? std::format("<!seen>@{}", addr) : t.repr(addr);
        for (ssize_t z = 0; z < t.recursion_depth * 2; z++) {
          fputc(' ', stderr);
        }
        phosg::fwrite_fmt(stderr, "{}\n", repr);

        if (!addr_seen) {
          std::unordered_set<MappedPtr<PyObject>>* next_addrs;
          try {
            next_addrs = &await_targets_for_obj.at(addr);
          } catch (const std::out_of_range&) {
            phosg::fwrite_fmt(stderr, "Warning: await target {} missing from graph\n", addr);
            return;
          }

          t.recursion_depth++;
          for (auto next_addr : *next_addrs) {
            print_entry(t, next_addr, seen);
          }
          t.recursion_depth--;
        }
      };

      for (auto addr : roots) {
        auto t = shell.env.traverse(&args);
        t.is_short = true;
        std::unordered_set<MappedPtr<PyObject>> seen;
        print_entry(t, addr, seen);
      }
    });

ShellCommand c_context(
    "context", "\
  context ADDRESS\n\
    Show the contents of memory near ADDRESS. Options:\n\
      --bswap: Byteswap ADDRESS before reading data.\n\
      --size: Show this many bytes before and after ADDRESS (default 0x100).\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      auto addr = shell.parse_addr<void>(args.get<std::string>(1, true), args.get<bool>("bswap"));
      size_t size = args.get<size_t>("size", 0x100);

      auto rgn = shell.env.r.region_for_address(addr);
      size_t bytes_before = std::min<size_t>(size, addr.addr - rgn.first.addr);
      size_t bytes_after = std::min<size_t>(size, addr.bytes_until(rgn.first.offset_bytes(rgn.second)));
      size_t bytes_to_read = bytes_before + bytes_after;
      auto read_start_addr = addr.offset_bytes(-bytes_before);
      const void* data = shell.env.r.read(read_start_addr, bytes_to_read).getv(bytes_to_read);
      phosg::print_data(stdout, data, bytes_to_read, read_start_addr.addr);
    });

ShellCommand c_repr(
    "repr", "\
  repr ADDRESS\n\
    Print the Python object at ADDRESS. If ADDRESS is preceded by one or more\n\
    asterisks, dereferences that many levels of pointers, and prints the\n\
    pointed-to object at the end of the pointer chain. Options:\n\
      --max-recursion-depth=N: Limit how deeply to print the found objects.\n\
      --max-entries=N: Limit how many items to print from each list/dict/etc.\n\
      --max-string-length=N: Limit, in bytes, how much data to print from each\n\
          str/bytes object (default 1KB).\n\
      --show-all-addresses: Show addresses for all objects, even ints/strs.\n\
      --frame-omit-back: Don\'t recur into f_back for frame objects.\n\
      --bytes-as-hex: Always format bytes objects as hex, even if they contain\n\
          only printable characters.\n\
      --short: Omit less-frequently-relevant fields on some objects.\n\
    All of these options are also valid for other commands that print object\n\
    representations.\n",
    +[](AnalysisShell& shell, phosg::Arguments& args) -> void {
      auto addr = shell.parse_addr<PyObject>(args.get<std::string>(1, true), args.get<bool>("bswap"));
      std::string repr = shell.env.traverse(&args).repr(addr);
      phosg::fwrite_fmt(stderr, "{}\n", repr);
    });
