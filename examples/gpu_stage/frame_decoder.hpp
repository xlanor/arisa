#pragma once

#include <decoded_frame.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>

namespace arisa::examples {

class FrameDecoder {
 public:
  enum class Submission : std::uint8_t {
    accepted,
    busy,
  };

  FrameDecoder() = default;
  FrameDecoder(const FrameDecoder&) = delete;
  auto operator=(const FrameDecoder&) -> FrameDecoder& = delete;
  FrameDecoder(FrameDecoder&&) = delete;
  auto operator=(FrameDecoder&&) -> FrameDecoder& = delete;
  virtual ~FrameDecoder() = default;

  [[nodiscard]] virtual auto submit(std::span<const std::byte> sample,
                                    std::chrono::nanoseconds timestamp)
      -> std::expected<Submission, std::string> = 0;

  [[nodiscard]] virtual auto next_frame()
      -> std::expected<std::optional<DecodedFrame>, std::string> = 0;
};

}
