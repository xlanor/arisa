#include <arisa/provider/gpu/frame_source.hpp>

#include <utility>

namespace arisa::provider {

FrameSource::FrameSource(Open open,
                         std::weak_ptr<const OverlaySlots> slots) noexcept
    : open_(std::move(open)), slots_(std::move(slots)) {}

auto FrameSource::open() -> std::expected<GpuFrame, ProviderError> {
  if (closed()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument,
                                         ProviderErrorDetail::not_taken_over});
  }
  return open_();
}

auto FrameSource::closed() const -> bool {
  const std::shared_ptr<const OverlaySlots> held = slots_.lock();
  return held == nullptr || held->closed();
}

auto FrameSource::uses() const -> std::uint64_t {
  const std::shared_ptr<const OverlaySlots> held = slots_.lock();
  return held == nullptr ? 0 : held->uses();
}

auto FrameSource::await(std::uint64_t after,
                        std::chrono::nanoseconds limit) const -> std::uint64_t {
  const std::shared_ptr<const OverlaySlots> held = slots_.lock();
  if (held == nullptr) {
    return after;
  }
  return held->await(after, limit);
}

auto FrameSource::scenes() const -> std::uint64_t {
  const std::shared_ptr<const OverlaySlots> held = slots_.lock();
  return held == nullptr ? 0 : held->scenes();
}

auto FrameSource::await_scene(std::uint64_t after,
                              std::chrono::nanoseconds limit) const
    -> std::uint64_t {
  const std::shared_ptr<const OverlaySlots> held = slots_.lock();
  if (held == nullptr) {
    return after;
  }
  return held->await_scene(after, limit);
}
}
