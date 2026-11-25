#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/support/unique_handle.hpp>

#include <switch.h>

#include <concepts>
#include <expected>

namespace arisa::provider::horizon {

template <class Service>
concept HorizonService = requires {
  { Service::open() } noexcept -> std::same_as<Result>;
  { Service::close() } noexcept;
};

template <HorizonService Service>
class ServiceSession {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<ServiceSession, ProviderError> {
    const Result opened = Service::open();
    if (R_FAILED(opened)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::platform_failure, opened});
    }
    return ServiceSession{};
  }

 private:
  struct Release {
    using handle_type = bool;

    static auto empty() noexcept -> handle_type { return false; }

    static auto is_valid(handle_type open) noexcept -> bool { return open; }

    static void release(handle_type&) noexcept { Service::close(); }
  };

  ServiceSession() noexcept : open_(true) {}

  support::UniqueHandle<Release> open_;
};

}
