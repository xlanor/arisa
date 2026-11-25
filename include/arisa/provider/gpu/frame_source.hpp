#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/gpu_frame.hpp>
#include <arisa/provider/gpu/overlay_slots.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>

namespace arisa::provider {

class FrameSource {
 public:
  using Open =
      std::move_only_function<std::expected<GpuFrame, ProviderError>()>;

  FrameSource(Open open, std::weak_ptr<const OverlaySlots> slots) noexcept;

  FrameSource(FrameSource&&) noexcept = default;
  auto operator=(FrameSource&&) noexcept -> FrameSource& = default;
  FrameSource(const FrameSource&) = delete;
  auto operator=(const FrameSource&) -> FrameSource& = delete;
  ~FrameSource() = default;

  [[nodiscard]] auto open() -> std::expected<GpuFrame, ProviderError>;

  auto closed() const -> bool;

  auto uses() const -> std::uint64_t;

  auto await(std::uint64_t after, std::chrono::nanoseconds limit) const
      -> std::uint64_t;

  auto scenes() const -> std::uint64_t;

  auto await_scene(std::uint64_t after, std::chrono::nanoseconds limit) const
      -> std::uint64_t;

 private:
  Open open_;
  std::weak_ptr<const OverlaySlots> slots_;
};

}
