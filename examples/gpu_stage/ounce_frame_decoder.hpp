#pragma once

#include <frame_decoder.hpp>
#include <h264_annex_b.hpp>

#include <switch.h>

extern "C" {
#include <switch2/video/decoder.h>
}

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace arisa::examples {

class OunceFrameDecoder final : public FrameDecoder {
  class Session;

 public:
  struct Config {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint32_t frame_rate{0};
    std::uint32_t image_count{0};
  };

  [[nodiscard]] static auto create(const H264AnnexB& stream, Config config)
      -> std::expected<std::unique_ptr<OunceFrameDecoder>, std::string>;

  explicit OunceFrameDecoder(std::shared_ptr<Session> session) noexcept;

  [[nodiscard]] auto submit(std::span<const std::byte> sample,
                            std::chrono::nanoseconds timestamp)
      -> std::expected<Submission, std::string> override;

  [[nodiscard]] auto next_frame()
      -> std::expected<std::optional<DecodedFrame>, std::string> override;

 private:
  static auto describe(Result result) -> std::string;

  static auto waiting(Result result) noexcept -> bool;

  static auto resident(const VideoDecoder& decoder,
                       const VideoFrame& frame) noexcept
      -> std::optional<DecodedFrame::Resident>;

  std::shared_ptr<Session> session_;
};

}
