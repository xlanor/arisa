#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/pointer_reading.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <switch.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

namespace arisa::provider::horizon::ounce {

class OuncePointer {
 public:
  static constexpr std::size_t history = 16;
  static constexpr std::uint32_t retry_interval = 30;
  static constexpr std::uint32_t tracked_flag = 1U << 1U;
  static constexpr std::size_t max_devices = 16;
  static constexpr std::uint32_t joy_cons =
      HidNmouseType_JoyLeft | HidNmouseType_JoyRight;
  static constexpr std::uint32_t sources = HidNmouseType_JoyLeft |
                                           HidNmouseType_JoyRight |
                                           HidNmouseType_StandardMouse;

  explicit OuncePointer(SurfaceSize surface);

  auto sample(SurfaceSize surface) -> PointerReading;

  auto failure() const noexcept -> std::optional<ProviderError> {
    return failure_;
  }

 private:
  struct alignas(0x1000) TransferMemory {
    std::array<std::byte, HID_NMOUSE_TRANSFER_MEMORY_SIZE> bytes;
  };

  class Session {
   public:
    [[nodiscard]] static auto create(SurfaceSize surface)
        -> std::expected<Session, ProviderError>;

    Session(const Session&) = delete;
    auto operator=(const Session&) -> Session& = delete;
    Session(Session&& other) noexcept = default;
    auto operator=(Session&& other) noexcept -> Session&;
    ~Session();

    void resize(SurfaceSize surface);

   private:
    Session(std::unique_ptr<TransferMemory> memory, SurfaceSize surface)
        : memory_(std::move(memory)), surface_(surface) {}

    void finish() noexcept;

    std::unique_ptr<TransferMemory> memory_;
    SurfaceSize surface_;
  };

  auto acquire() -> bool;

  static auto presser() -> std::optional<std::size_t>;

  std::optional<Session> session_;
  std::optional<ProviderError> failure_;
  std::optional<HidNmouseHandle> handle_;
  std::optional<PointerState> state_;
  std::uint64_t sampled_{0};
  std::optional<std::size_t> claimed_;
  std::uint32_t retry_{0};
};

}
