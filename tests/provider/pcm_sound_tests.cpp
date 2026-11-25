#include <arisa/provider/audio/pcm_sound.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace arisa::provider {

class PcmSoundTest : public ::testing::Test {
 protected:
  static void append_tag(std::vector<std::byte>& bytes, std::string_view tag) {
    for (const char character : tag) {
      bytes.push_back(static_cast<std::byte>(character));
    }
  }

  static void append_u16(std::vector<std::byte>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::byte>(value & 0xFFU));
    bytes.push_back(static_cast<std::byte>(value >> 8U));
  }

  static void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value & 0xFFFFU));
    append_u16(bytes, static_cast<std::uint16_t>(value >> 16U));
  }

  static auto wav(std::uint16_t format, std::uint16_t channels,
                  std::uint16_t bits, std::span<const std::int16_t> samples,
                  bool with_extra_chunk = false) -> std::vector<std::byte> {
    std::vector<std::byte> bytes;
    append_tag(bytes, "RIFF");
    append_u32(bytes, 0);
    append_tag(bytes, "WAVE");
    if (with_extra_chunk) {
      append_tag(bytes, "LIST");
      append_u32(bytes, 3);
      append_tag(bytes, "abc");
      bytes.push_back(std::byte{0});
    }
    append_tag(bytes, "fmt ");
    append_u32(bytes, 16);
    append_u16(bytes, format);
    append_u16(bytes, channels);
    append_u32(bytes, sample_rate);
    append_u32(bytes, sample_rate * channels * 2U);
    append_u16(bytes, static_cast<std::uint16_t>(channels * 2U));
    append_u16(bytes, bits);
    append_tag(bytes, "data");
    append_u32(bytes, static_cast<std::uint32_t>(samples.size() * 2U));
    for (const std::int16_t sample : samples) {
      append_u16(bytes, static_cast<std::uint16_t>(sample));
    }
    return bytes;
  }

  static constexpr std::uint32_t sample_rate = 48000;
};

TEST_F(PcmSoundTest, ParsesMonoPcm16) {
  const std::vector<std::int16_t> samples{0, 1000, -1000, 32767, -32768};
  const auto sound = PcmSound::parse_wav(wav(1, 1, 16, samples));
  ASSERT_TRUE(sound.has_value());
  EXPECT_EQ(sound->channels(), 1);
  EXPECT_EQ(sound->sample_rate(), sample_rate);
  EXPECT_EQ(sound->frames(), samples.size());
  EXPECT_TRUE(std::ranges::equal(sound->samples(), samples));
}

TEST_F(PcmSoundTest, ParsesStereoAndSkipsUnknownChunks) {
  const std::vector<std::int16_t> samples{1, 2, 3, 4};
  const auto sound = PcmSound::parse_wav(wav(1, 2, 16, samples, true));
  ASSERT_TRUE(sound.has_value());
  EXPECT_EQ(sound->channels(), 2);
  EXPECT_EQ(sound->frames(), 2U);
}

TEST_F(PcmSoundTest, RejectsUnsupportedFormats) {
  const std::vector<std::int16_t> samples{1, 2};
  EXPECT_FALSE(PcmSound::parse_wav(wav(3, 1, 16, samples)).has_value());
  EXPECT_FALSE(PcmSound::parse_wav(wav(1, 1, 8, samples)).has_value());
  EXPECT_FALSE(PcmSound::parse_wav(wav(1, 3, 16, samples)).has_value());
  EXPECT_FALSE(PcmSound::parse_wav(wav(1, 1, 16, {})).has_value());
}

TEST_F(PcmSoundTest, RejectsTruncatedAndForeignData) {
  const std::vector<std::int16_t> samples{1, 2, 3};
  std::vector<std::byte> truncated = wav(1, 1, 16, samples);
  truncated.resize(truncated.size() - 4);
  const auto result = PcmSound::parse_wav(truncated);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code(), ProviderErrorCode::invalid_argument);
  const std::vector<std::byte> text(16, std::byte{'x'});
  EXPECT_FALSE(PcmSound::parse_wav(text).has_value());
  EXPECT_FALSE(PcmSound::parse_wav({}).has_value());
}

}
