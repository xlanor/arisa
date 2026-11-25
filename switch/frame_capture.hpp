#pragma once

#include <arisa/provider/gpu/provider.hpp>
#include <nxlink_log.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace arisa::nx {

class FrameCapture {
 public:
  static constexpr std::string_view directory = "sdmc:/arisa/captures";

  explicit FrameCapture(const NxlinkLog& log) noexcept;

  auto poll(std::uint64_t held, std::uint64_t pressed,
            provider::GpuProvider& gpu) -> std::optional<std::string>;

 private:
  static auto requested(std::uint64_t held, std::uint64_t pressed) noexcept
      -> bool;

  auto save(provider::GpuProvider& gpu) const -> std::string;

  std::reference_wrapper<const NxlinkLog> log_;
};

}
