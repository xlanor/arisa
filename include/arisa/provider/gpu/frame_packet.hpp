#pragma once

#include <arisa/provider/clock/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::provider {

struct FrameReceipt {
  std::chrono::nanoseconds record{0};
  std::chrono::nanoseconds wait{0};
  std::optional<std::chrono::nanoseconds> displayed{};
  std::optional<Image> image{};
};

class FramePacket {
 public:
  using Clock = std::optional<std::reference_wrapper<const ClockProvider>>;

  class Submitter {
   public:
    Submitter() = default;
    Submitter(const Submitter&) = delete;
    auto operator=(const Submitter&) -> Submitter& = delete;
    Submitter(Submitter&&) = delete;
    auto operator=(Submitter&&) -> Submitter& = delete;
    virtual ~Submitter() = default;

    [[nodiscard]] virtual auto submit(const FramePacket& packet, Clock clock)
        -> std::expected<FrameReceipt, ProviderError> = 0;

    virtual void abandon(const FramePacket& packet) noexcept = 0;
  };

  struct Contents {
    FramePlan plan{};
    GpuSerial serial{0};
    std::uint64_t frame{0};
    std::uint32_t slot{0};
  };

  FramePacket(std::shared_ptr<Submitter> submitter, Contents contents) noexcept;

  FramePacket(FramePacket&& other) noexcept;
  auto operator=(FramePacket&& other) noexcept -> FramePacket&;
  FramePacket(const FramePacket&) = delete;
  auto operator=(const FramePacket&) -> FramePacket& = delete;
  ~FramePacket();

  auto plan() const noexcept -> const FramePlan& { return contents_.plan; }

  auto serial() const noexcept -> GpuSerial { return contents_.serial; }

  auto frame() const noexcept -> std::uint64_t { return contents_.frame; }

  auto slot() const noexcept -> std::uint32_t { return contents_.slot; }

  auto pending() const noexcept -> bool { return submitter_ != nullptr; }

  [[nodiscard]] auto submit(Clock clock = std::nullopt)
      -> std::expected<FrameReceipt, ProviderError>;

 private:
  void abandon() noexcept;

  std::shared_ptr<Submitter> submitter_;
  Contents contents_;
};

}
