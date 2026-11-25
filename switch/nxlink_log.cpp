#include <nxlink_log.hpp>

#include <switch.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace arisa::nx {

NxlinkLog::NxlinkLog(std::string_view app) noexcept : file_(open(app)) {
  capture_errors(app);
  if (R_SUCCEEDED(socketInitializeDefault())) {
    sockets_ = Sockets{true};
    link_ = Link{nxlinkStdio()};
  }
}

auto NxlinkLog::path(std::string_view app, std::string_view kind)
    -> std::string {
  return std::format("{}/{}{}{}.log", log_directory, app, backend_suffix, kind);
}

auto NxlinkLog::open(std::string_view app) noexcept -> File {
  mkdir("sdmc:/switch", 0777);
  mkdir("sdmc:/switch/arisa", 0777);
  mkdir(std::string{log_directory}.c_str(), 0777);
  return File{std::fopen(path(app, "").c_str(), "w")};
}

void NxlinkLog::capture_errors(std::string_view app) noexcept {
  if (std::freopen(path(app, "-stderr").c_str(), "w", stderr) != nullptr) {
    std::setvbuf(stderr, nullptr, _IONBF, 0);
  }
}

void NxlinkLog::emit(std::string_view message) const noexcept {
  if (link_.valid()) {
    std::println("[arisa] {}", message);
    std::fflush(stdout);
  }
  if (file_.valid()) {
    std::println(file_.get(), "[arisa] {}", message);
    std::fflush(file_.get());
  }
}

void NxlinkLog::report(const provider::Diagnostic& diagnostic) noexcept {
  emit(std::format("{}: {}", diagnostic.category, diagnostic.message));
}

void NxlinkLog::SocketRelease::release(handle_type&) noexcept { socketExit(); }

void NxlinkLog::LinkRelease::release(handle_type& socket) noexcept {
  close(socket);
}

void NxlinkLog::FileRelease::release(handle_type& file) noexcept {
  std::fclose(file);
}

}
