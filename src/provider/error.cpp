#include <arisa/provider/error.hpp>

#include <format>

namespace arisa::provider {

auto ProviderError::name() const noexcept -> std::string_view {
  switch (code_) {
    case ProviderErrorCode::unsupported_capability:
      return "unsupported_capability";
    case ProviderErrorCode::invalid_argument:
      return "invalid_argument";
    case ProviderErrorCode::not_found:
      return "not_found";
    case ProviderErrorCode::io_failure:
      return "io_failure";
    case ProviderErrorCode::resource_exhausted:
      return "resource_exhausted";
    case ProviderErrorCode::platform_failure:
      return "platform_failure";
    case ProviderErrorCode::cancelled:
      return "cancelled";
  }
  return "unknown";
}

auto ProviderError::description() const noexcept -> std::string_view {
  switch (code_) {
    case ProviderErrorCode::unsupported_capability:
      return "unsupported capability";
    case ProviderErrorCode::invalid_argument:
      return "invalid argument";
    case ProviderErrorCode::not_found:
      return "not found";
    case ProviderErrorCode::io_failure:
      return "i/o failure";
    case ProviderErrorCode::resource_exhausted:
      return "resource exhausted";
    case ProviderErrorCode::platform_failure:
      return "platform failure";
    case ProviderErrorCode::cancelled:
      return "cancelled";
  }
  return "unknown provider error";
}

auto ProviderError::detail_description() const noexcept -> std::string_view {
  switch (detail_) {
    case ProviderErrorDetail::none:
      return "no further detail";
    case ProviderErrorDetail::no_frame:
      return "the frame has no backend implementation";
    case ProviderErrorDetail::frame_closed:
      return "the frame is sealed or its device is gone";
    case ProviderErrorDetail::replay_frame:
      return "a replay frame takes no new draws";
    case ProviderErrorDetail::malformed_draw:
      return "a command reaches past its geometry";
    case ProviderErrorDetail::zero_target:
      return "the render pass has no target";
    case ProviderErrorDetail::empty_region:
      return "the render pass region is empty";
    case ProviderErrorDetail::non_finite_region:
      return "the render pass region is not finite";
    case ProviderErrorDetail::unknown_target:
      return "the render pass target is not a live texture";
    case ProviderErrorDetail::not_render_target:
      return "the render pass target is not a render target";
    case ProviderErrorDetail::unknown_texture:
      return "a command samples a texture that is not live";
    case ProviderErrorDetail::foreign_geometry:
      return "the geometry was not handed out by this frame";
    case ProviderErrorDetail::released_texture:
      return "a command samples a texture its owner released before this "
             "frame";
    case ProviderErrorDetail::samples_target:
      return "a command samples the texture its render pass draws into";
    case ProviderErrorDetail::device_lost:
      return "the GPU device was lost; no further frames are drawn";
    case ProviderErrorDetail::taken_over:
      return "an app holds the GPU takeover; arisa does not present until it "
             "is handed back";
    case ProviderErrorDetail::not_taken_over:
      return "the takeover was handed back or never held";
    case ProviderErrorDetail::image_unavailable:
      return "no swapchain image could be acquired";
    case ProviderErrorDetail::missing_extension:
      return "a requested Vulkan device extension is not supported";
    case ProviderErrorDetail::missing_feature:
      return "a requested Vulkan device feature is not supported";
    case ProviderErrorDetail::voice_open:
      return "an app audio voice is already open";
    case ProviderErrorDetail::overlay_open:
      return "the takeover already records an overlay";
    case ProviderErrorDetail::overlay_extent:
      return "the target is not the size the overlay was painted at";
  }
  return "unknown detail";
}

auto ProviderError::explanation() const -> std::string {
  std::string text = std::format("{} ({})", name(), description());
  if (detail_ != ProviderErrorDetail::none) {
    text += std::format(": {}", detail_description());
  }
  if (native_result_.has_value()) {
    if (native_kind_ == NativeResultKind::vulkan) {
      text += std::format(", VkResult {}",
                          static_cast<std::int32_t>(*native_result_));
    } else {
      text += std::format(", result {:#x}", *native_result_);
    }
  }
  return text;
}

}
