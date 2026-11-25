#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::provider {

enum class ProviderErrorCode : std::uint8_t {
  unsupported_capability,
  invalid_argument,
  not_found,
  io_failure,
  resource_exhausted,
  platform_failure,
  cancelled,
};

enum class ProviderErrorDetail : std::uint8_t {
  none,
  no_frame,
  frame_closed,
  replay_frame,
  malformed_draw,
  zero_target,
  empty_region,
  non_finite_region,
  unknown_target,
  not_render_target,
  unknown_texture,
  foreign_geometry,
  released_texture,
  samples_target,
  device_lost,
  taken_over,
  not_taken_over,
  image_unavailable,
  missing_extension,
  missing_feature,
  voice_open,
  overlay_open,
  overlay_extent,
};

enum class NativeResultKind : std::uint8_t {
  platform,
  vulkan,
};

class ProviderError {
 public:
  constexpr explicit ProviderError(
      ProviderErrorCode code,
      std::optional<std::uint32_t> native_result = std::nullopt) noexcept
      : code_(code), native_result_(native_result) {}

  constexpr ProviderError(ProviderErrorCode code,
                          ProviderErrorDetail detail) noexcept
      : code_(code), detail_(detail) {}

  static constexpr auto unsupported_capability() noexcept -> ProviderError {
    return ProviderError{ProviderErrorCode::unsupported_capability};
  }

  static constexpr auto vulkan(ProviderErrorCode code,
                               std::int32_t result) noexcept -> ProviderError {
    ProviderError made{code, static_cast<std::uint32_t>(result)};
    made.native_kind_ = NativeResultKind::vulkan;
    return made;
  }

  constexpr auto code() const noexcept -> ProviderErrorCode { return code_; }

  constexpr auto detail() const noexcept -> ProviderErrorDetail {
    return detail_;
  }

  constexpr auto native_result() const noexcept
      -> std::optional<std::uint32_t> {
    return native_result_;
  }

  constexpr auto native_kind() const noexcept -> NativeResultKind {
    return native_kind_;
  }

  auto name() const noexcept -> std::string_view;

  auto description() const noexcept -> std::string_view;

  auto detail_description() const noexcept -> std::string_view;

  auto explanation() const -> std::string;

  friend constexpr auto operator==(const ProviderError&, const ProviderError&)
      -> bool = default;

 private:
  ProviderErrorCode code_;
  ProviderErrorDetail detail_{ProviderErrorDetail::none};
  std::optional<std::uint32_t> native_result_;
  NativeResultKind native_kind_{NativeResultKind::platform};
};

}
