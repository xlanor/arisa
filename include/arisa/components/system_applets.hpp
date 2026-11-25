#pragma once

#include <arisa/provider/applets/provider.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>

namespace arisa::components {

class SystemApplets {
 public:
  explicit SystemApplets(provider::AppletProvider& applets) noexcept
      : applets_(applets) {}

  [[nodiscard]] auto select_account() const
      -> std::expected<std::optional<provider::UserAccount>,
                       provider::ProviderError> {
    return applets_.get().select_account();
  }

  [[nodiscard]] auto select_players(provider::PlayerSelection request) const
      -> std::expected<std::uint8_t, provider::ProviderError> {
    return applets_.get().select_players(request);
  }

  [[nodiscard]] auto show_error(const provider::ErrorReport& report) const
      -> std::expected<void, provider::ProviderError> {
    return applets_.get().show_error(report);
  }

  friend auto operator==(const SystemApplets& left, const SystemApplets& right)
      -> bool {
    return &left.applets_.get() == &right.applets_.get();
  }

 private:
  std::reference_wrapper<provider::AppletProvider> applets_;
};

}
