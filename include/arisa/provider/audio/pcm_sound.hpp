#pragma once

#include <arisa/provider/error.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace arisa::provider {

class PcmSound {
 public:
  static constexpr std::uint16_t max_channels = 2;

  [[nodiscard]] static auto parse_wav(std::span<const std::byte> bytes)
      -> std::expected<PcmSound, ProviderError>;

  auto channels() const noexcept -> std::uint16_t { return channels_; }

  auto sample_rate() const noexcept -> std::uint32_t { return sample_rate_; }

  auto samples() const noexcept -> std::span<const std::int16_t> {
    return samples_;
  }

  auto frames() const noexcept -> std::size_t {
    return samples_.size() / channels_;
  }

 private:
  PcmSound(std::uint16_t channels, std::uint32_t sample_rate,
           std::vector<std::int16_t> samples) noexcept;

  static auto read_u16(std::span<const std::byte> bytes,
                       std::size_t offset) noexcept
      -> std::optional<std::uint16_t>;
  static auto read_u32(std::span<const std::byte> bytes,
                       std::size_t offset) noexcept
      -> std::optional<std::uint32_t>;
  static auto has_tag(std::span<const std::byte> bytes, std::size_t offset,
                      std::string_view tag) noexcept -> bool;
  static auto malformed() noexcept -> std::unexpected<ProviderError>;

  std::uint16_t channels_;
  std::uint32_t sample_rate_;
  std::vector<std::int16_t> samples_;
};

}
