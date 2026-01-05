#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <mutex>
#include <phosg/Arguments.hh>
#include <phosg/Filesystem.hh>
#include <phosg/JSON.hh>
#include <phosg/Process.hh>
#include <phosg/Strings.hh>
#include <phosg/Tools.hh>
#include <set>
#include <string>

#include "AnalysisShell.hh"
#include "Common.hh"
#include "MemoryReader.hh"

void chown_tree(const std::string& path, uid_t uid, gid_t gid) {
  if (lchown(path.c_str(), uid, gid) != 0) {
    throw std::runtime_error(std::format("Failed to change ownership on {}", path));
  }
  if (std::filesystem::is_directory(path)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
      const std::string current_path = entry.path().string();
      if (lchown(current_path.c_str(), uid, gid) != 0) {
        throw std::runtime_error(std::format("Failed to change ownership on {}", current_path));
      }
    }
  }
}

void print_usage() {
  phosg::fwrite_fmt(stderr, "\
To create a memory snapshot:\n\
  sudo python-memtools --dump --pid=PID --path=PATH [--skip-chown]\n\
The process will be temporarily paused (via SIGSTOP and SIGCONT) while the\n\
snapshot is taken. By default, python-memtools will change the owner of the\n\
resulting files to the user specified by $SUDO_USER (if set); to prevent this,\n\
use --skip-chown.\n\
\n\
To analyze a memory snapshot:\n\
  python-memtools --path=PATH [--command=COMMAND]\n\
If COMMAND is given, runs that command and exits. Otherwise, opens a shell in\n\
which you can analyze the snapshot. Run `help` in this shell to see the\n\
available commands.\n");
}

int main(int argc, char** argv) {
  phosg::Arguments args(argv, argc);

  const std::string& data_path = args.get<std::string>("path", false);
  if (data_path.empty()) {
    phosg::fwrite_fmt(stderr, "Usage error: --path is required.\n\n");
    print_usage();
    return 1;
  }

  size_t max_threads = args.get<uint64_t>("max-threads", 0);

  if (args.get<bool>("dump")) {
    uint64_t pid = args.get<uint64_t>("pid", 0);
    if ((pid == 0) || data_path.empty()) {
      print_usage();
      throw std::runtime_error("Both --pid and --path are required for --dump");
    }
    MemoryReader::dump(pid, data_path, max_threads);
    if (!args.get<bool>("skip-chown")) {
      const char* sudo_user = getenv("SUDO_USER");
      if (sudo_user) {
        const struct passwd* user = getpwnam(sudo_user);
        if (user == nullptr) {
          phosg::log_warning_f(
              "$SUDO_USER is {}, but that user appears not to exist; not changing file ownership", sudo_user);
        } else {
          phosg::log_info_f(
              "Changing owner on {} and all contents to {} ({}:{})", data_path, sudo_user, user->pw_uid, user->pw_gid);
          chown_tree(data_path, user->pw_uid, user->pw_gid);
        }
      }
    }
    return 0;
  }

  AnalysisShell shell(data_path, max_threads);
  {
    std::string size_str = phosg::format_size(shell.env.r.bytes());
    phosg::fwrite_fmt(stderr, "Loaded {} in {} regions\n", size_str, shell.env.r.region_count());
  }

  const auto& command = args.get<std::string>("command");
  if (!command.empty()) {
    shell.prepare();
    shell.run_command(command);
  } else {
    shell.run();
  }

  return 0;
}
