#pragma once

#include <arisa/provider/error.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace arisa::provider {

struct UserAccount {
  std::string identifier;
  std::string nickname;

  friend auto operator==(const UserAccount&, const UserAccount&)
      -> bool = default;
};

struct PlayerSelection {
  std::uint8_t minimum{1};
  std::uint8_t maximum{4};
  bool single_player{false};

  friend constexpr auto operator==(const PlayerSelection&,
                                   const PlayerSelection&) -> bool = default;
};

struct ErrorReport {
  std::uint32_t code{0};
  std::string message;
  std::string details;

  friend auto operator==(const ErrorReport&, const ErrorReport&)
      -> bool = default;
};

class AppletProvider {
 public:
  virtual ~AppletProvider() = default;

  [[nodiscard]] virtual auto select_account()
      -> std::expected<std::optional<UserAccount>, ProviderError> = 0;

  [[nodiscard]] virtual auto select_players(PlayerSelection request)
      -> std::expected<std::uint8_t, ProviderError> = 0;

  [[nodiscard]] virtual auto show_error(const ErrorReport& report)
      -> std::expected<void, ProviderError> = 0;
};

}
