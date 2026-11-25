#include <exit_trail.hpp>
#include <nxlink_log.hpp>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace arisa::nx {

void ExitTrail::begin(std::string_view app) noexcept {
  const std::string path = NxlinkLog::path(app, "-exit");
  if (path.size() >= path_.size()) {
    return;
  }
  std::ranges::copy(path, path_.begin());
  path_[path.size()] = '\0';
  const int descriptor =
      ::open(path_.data(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (descriptor >= 0) {
    ::close(descriptor);
  }
  std::atexit(handlers_finished);
  mark("started");
}

void ExitTrail::mark(std::string_view stage) noexcept {
  if (path_[0] == '\0') {
    return;
  }
  const int descriptor =
      ::open(path_.data(), O_WRONLY | O_CREAT | O_APPEND, 0666);
  if (descriptor < 0) {
    return;
  }
  static_cast<void>(::write(descriptor, stage.data(), stage.size()));
  static_cast<void>(::write(descriptor, "\n", 1));
  ::fsync(descriptor);
  ::close(descriptor);
}

void ExitTrail::handlers_finished() noexcept { mark("exit handlers done"); }

}

extern "C" void userAppExit(void) { arisa::nx::ExitTrail::mark("app exit"); }
