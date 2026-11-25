#include <arisa/provider/standard/standard_clock.hpp>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace arisa::provider::standard {

auto StandardClock::now() const noexcept -> Instant {
  return Instant{std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch())};
}

auto StandardClock::wall_now() const noexcept
    -> std::chrono::system_clock::time_point {
  return std::chrono::system_clock::now();
}

auto StandardClock::local_offset() const
    -> std::expected<std::chrono::seconds, ProviderError> {
  const std::time_t now = std::time(nullptr);
  std::tm local{};
  std::tm universal{};
  if (localtime_r(&now, &local) == nullptr ||
      gmtime_r(&now, &universal) == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  const auto stamp = [](const std::tm& fields) {
    const std::chrono::sys_days day{
        std::chrono::year{fields.tm_year + 1900} /
        std::chrono::month{static_cast<unsigned>(fields.tm_mon + 1)} /
        std::chrono::day{static_cast<unsigned>(fields.tm_mday)}};
    return day + std::chrono::hours{fields.tm_hour} +
           std::chrono::minutes{fields.tm_min} +
           std::chrono::seconds{fields.tm_sec};
  };
  return std::chrono::duration_cast<std::chrono::seconds>(stamp(local) -
                                                          stamp(universal));
}

auto StandardClock::zone_name() const
    -> std::expected<std::string, ProviderError> {
  if (const char* variable = std::getenv("TZ");
      variable != nullptr && *variable != '\0' && *variable != ':') {
    return std::string{variable};
  }
  std::error_code failure;
  const std::filesystem::path target =
      std::filesystem::read_symlink("/etc/localtime", failure);
  if (failure) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  const std::string link = target.generic_string();
  constexpr std::string_view marker = "zoneinfo/";
  const std::size_t found = link.find(marker);
  if (found == std::string::npos) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return link.substr(found + marker.size());
}

}
