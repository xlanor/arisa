#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <vector>

namespace arisa::provider {

class ImageDecodeTest : public ::testing::Test {
 protected:
  static constexpr std::array<std::uint8_t, 76> square{
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00,
      0x0D, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
      0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D, 0x24,
      0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63,
      0xF8, 0xCF, 0xC0, 0xF0, 0x1F, 0x0C, 0x81, 0x34, 0x08, 0x34, 0x00,
      0x00, 0x49, 0x49, 0x09, 0x78, 0x9C, 0x51, 0x17, 0x92, 0x00, 0x00,
      0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

  static auto bytes() -> std::span<const std::byte> {
    return std::as_bytes(std::span{square});
  }
};

TEST_F(ImageDecodeTest, DecodesAPngIntoRgbaPixels) {
  const auto decoded = Image::decode(bytes());
  ASSERT_TRUE(decoded.has_value());
  EXPECT_EQ(decoded->width(), 2U);
  EXPECT_EQ(decoded->height(), 2U);
  EXPECT_EQ(decoded->pixel(0, 0),
            (std::array<std::uint8_t, Image::channels>{255, 0, 0, 255}));
  EXPECT_EQ(decoded->pixel(1, 0),
            (std::array<std::uint8_t, Image::channels>{0, 255, 0, 255}));
  EXPECT_EQ(decoded->pixel(0, 1),
            (std::array<std::uint8_t, Image::channels>{0, 0, 255, 255}));
  EXPECT_EQ(decoded->pixel(1, 1),
            (std::array<std::uint8_t, Image::channels>{255, 255, 255, 128}));
}

TEST_F(ImageDecodeTest, RejectsBytesThatAreNotAnImage) {
  constexpr std::array<std::uint8_t, 8> junk{1, 2, 3, 4, 5, 6, 7, 8};
  const auto decoded = Image::decode(std::as_bytes(std::span{junk}));
  ASSERT_FALSE(decoded.has_value());
  EXPECT_EQ(decoded.error().code(), ProviderErrorCode::invalid_argument);
  EXPECT_FALSE(Image::decode({}).has_value());
}

TEST_F(ImageDecodeTest, RejectsATruncatedPng) {
  EXPECT_FALSE(Image::decode(bytes().first(40)).has_value());
}

TEST_F(ImageDecodeTest, FuzzedImagesDecodeOrFailCleanly) {
  const std::filesystem::path regressions =
      std::filesystem::path{ARISA_TEST_FIXTURES}.parent_path().parent_path() /
      "fuzz" / "regressions" / "image";
  int replayed = 0;
  for (const auto& entry : std::filesystem::directory_iterator{regressions}) {
    std::ifstream file{entry.path(), std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{file},
                                std::istreambuf_iterator<char>{}};
    const auto decoded = Image::decode(std::as_bytes(std::span{raw}));
    if (decoded.has_value()) {
      EXPECT_GT(decoded->width(), 0U) << entry.path();
      EXPECT_GT(decoded->height(), 0U) << entry.path();
    }
    ++replayed;
  }
  EXPECT_GE(replayed, 1);
}

}
