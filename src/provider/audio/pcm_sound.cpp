#include <arisa/provider/audio/pcm_sound.hpp>

#include <algorithm>
#include <cstring>
#include <utility>

namespace arisa::provider {

PcmSound::PcmSound(std::uint16_t channels, std::uint32_t sample_rate,
                   std::vector<std::int16_t> samples) noexcept
    : channels_(channels),
      sample_rate_(sample_rate),
      samples_(std::move(samples)) {}

auto PcmSound::parse_wav(std::span<const std::byte> bytes)
    -> std::expected<PcmSound, ProviderError> {
  static constexpr std::size_t riff_header = 12;
  static constexpr std::size_t chunk_header = 8;
  static constexpr std::size_t fmt_minimum = 16;
  static constexpr std::uint16_t pcm_format = 1;
  static constexpr std::uint16_t bits_per_sample = 16;
  if (!has_tag(bytes, 0, "RIFF") || !has_tag(bytes, 8, "WAVE")) {
    return malformed();
  }
  std::optional<std::uint16_t> channels;
  std::optional<std::uint32_t> sample_rate;
  std::optional<std::span<const std::byte>> data;
  std::size_t offset = riff_header;
  while (offset + chunk_header <= bytes.size()) {
    const std::optional<std::uint32_t> declared = read_u32(bytes, offset + 4);
    const std::size_t body = offset + chunk_header;
    if (!declared.has_value() || *declared > bytes.size() - body) {
      return malformed();
    }
    const std::span<const std::byte> chunk = bytes.subspan(body, *declared);
    if (has_tag(bytes, offset, "fmt ")) {
      if (chunk.size() < fmt_minimum || read_u16(chunk, 0) != pcm_format ||
          read_u16(chunk, 14) != bits_per_sample) {
        return malformed();
      }
      channels = read_u16(chunk, 2);
      sample_rate = read_u32(chunk, 4);
    } else if (has_tag(bytes, offset, "data")) {
      data = chunk;
    }
    offset = body + *declared + (*declared % 2);
  }
  if (!channels.has_value() || !sample_rate.has_value() || !data.has_value() ||
      *channels == 0 || *channels > max_channels || *sample_rate == 0) {
    return malformed();
  }
  const std::size_t frame_bytes = sizeof(std::int16_t) * *channels;
  const std::size_t frame_count = data->size() / frame_bytes;
  if (frame_count == 0) {
    return malformed();
  }
  std::vector<std::int16_t> samples(frame_count * *channels);
  std::memcpy(samples.data(), data->data(),
              samples.size() * sizeof(std::int16_t));
  return PcmSound{*channels, *sample_rate, std::move(samples)};
}

auto PcmSound::read_u16(std::span<const std::byte> bytes,
                        std::size_t offset) noexcept
    -> std::optional<std::uint16_t> {
  if (offset > bytes.size() || bytes.size() - offset < sizeof(std::uint16_t)) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(
      std::to_integer<std::uint16_t>(bytes[offset]) |
      (std::to_integer<std::uint16_t>(bytes[offset + 1]) << 8U));
}

auto PcmSound::read_u32(std::span<const std::byte> bytes,
                        std::size_t offset) noexcept
    -> std::optional<std::uint32_t> {
  const std::optional<std::uint16_t> low = read_u16(bytes, offset);
  const std::optional<std::uint16_t> high = read_u16(bytes, offset + 2);
  if (!low.has_value() || !high.has_value()) {
    return std::nullopt;
  }
  return static_cast<std::uint32_t>(*low) |
         (static_cast<std::uint32_t>(*high) << 16U);
}

auto PcmSound::has_tag(std::span<const std::byte> bytes, std::size_t offset,
                       std::string_view tag) noexcept -> bool {
  if (offset > bytes.size() || bytes.size() - offset < tag.size()) {
    return false;
  }
  return std::ranges::equal(bytes.subspan(offset, tag.size()), tag,
                            [](std::byte left, char right) {
                              return std::to_integer<char>(left) == right;
                            });
}

auto PcmSound::malformed() noexcept -> std::unexpected<ProviderError> {
  return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
}

}
