#pragma once

#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/support/unique_handle.hpp>

#include <cstdio>
#include <string>
#include <string_view>

namespace arisa::nx {

class NxlinkLog final : public provider::DiagnosticsProvider {
 public:
  static constexpr std::string_view log_directory = "sdmc:/switch/arisa/logs";
#if defined(__SWITCH2__) && defined(ARISA_HORIZON_DEKO3D)
  static constexpr std::string_view backend_suffix = "-drako3d";
#else
  static constexpr std::string_view backend_suffix = "";
#endif

  static auto path(std::string_view app, std::string_view kind) -> std::string;

  explicit NxlinkLog(std::string_view app) noexcept;

  void emit(std::string_view message) const noexcept;

  void report(const provider::Diagnostic& diagnostic) noexcept override;

  auto connected() const noexcept -> bool { return link_.valid(); }

  auto recording() const noexcept -> bool { return file_.valid(); }

 private:
  struct SocketRelease {
    using handle_type = bool;
    static auto empty() noexcept -> handle_type { return false; }
    static auto is_valid(handle_type open) noexcept -> bool { return open; }
    static void release(handle_type& open) noexcept;
  };

  struct LinkRelease {
    using handle_type = int;
    static auto empty() noexcept -> handle_type { return -1; }
    static auto is_valid(handle_type socket) noexcept -> bool {
      return socket >= 0;
    }
    static void release(handle_type& socket) noexcept;
  };

  struct FileRelease {
    using handle_type = std::FILE*;
    static auto empty() noexcept -> handle_type { return nullptr; }
    static auto is_valid(handle_type file) noexcept -> bool {
      return file != nullptr;
    }
    static void release(handle_type& file) noexcept;
  };

  using Sockets = support::UniqueHandle<SocketRelease>;
  using Link = support::UniqueHandle<LinkRelease>;
  using File = support::UniqueHandle<FileRelease>;

  static auto open(std::string_view app) noexcept -> File;
  static void capture_errors(std::string_view app) noexcept;

  Sockets sockets_;
  Link link_;
  File file_;
};

}
