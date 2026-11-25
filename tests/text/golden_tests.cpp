#include <text_fixture.hpp>

#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/text/glyph_atlas.hpp>
#include <arisa/text/software_renderer.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::text {

struct GoldenSample {
  std::string_view name;
  std::string_view locale;
  std::string_view text;
  float scale;
};

class TextGolden : public ::testing::TestWithParam<GoldenSample> {
 protected:
  static constexpr int level_threshold = 16;
  static constexpr double maximum_off_fraction = 0.0005;
  static constexpr double maximum_mean_difference = 0.1;

  struct Gray {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> pixels;
  };

  static auto path(std::string_view name) -> std::filesystem::path {
    return std::filesystem::path{ARISA_TEXT_GOLDENS} /
           (std::string{name} + ".pgm");
  }

  static auto updating() -> bool {
    const char* value = std::getenv("ARISA_UPDATE_GOLDENS");
    return value != nullptr && std::string_view{value} == "1";
  }

  static auto gray(const provider::Image& image) -> Gray {
    Gray result{.width = image.width(), .height = image.height(), .pixels = {}};
    result.pixels.reserve(static_cast<std::size_t>(image.width()) *
                          image.height());
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        result.pixels.push_back(image.pixel(x, y).value()[0]);
      }
    }
    return result;
  }

  static void write(const std::filesystem::path& target, const Gray& image) {
    std::filesystem::create_directories(target.parent_path());
    std::ofstream file{target, std::ios::binary};
    file << "P5\n" << image.width << ' ' << image.height << "\n255\n";
    file.write(
        static_cast<const char*>(static_cast<const void*>(image.pixels.data())),
        static_cast<std::streamsize>(image.pixels.size()));
  }

  static auto displaced(const Gray& rendered, const Gray& stored,
                        std::uint32_t x, std::uint32_t y) -> bool {
    const int value = rendered.pixels[(std::size_t{y} * rendered.width) + x];
    for (std::uint32_t row = y == 0 ? 0 : y - 1;
         row <= std::min(y + 1, stored.height - 1); ++row) {
      for (std::uint32_t column = x == 0 ? 0 : x - 1;
           column <= std::min(x + 1, stored.width - 1); ++column) {
        const int other =
            stored.pixels[(std::size_t{row} * stored.width) + column];
        if (std::abs(value - other) <= level_threshold) {
          return true;
        }
      }
    }
    return false;
  }

  static auto read(const std::filesystem::path& source) -> std::optional<Gray> {
    std::ifstream file{source, std::ios::binary};
    if (!file) {
      return std::nullopt;
    }
    std::string magic;
    Gray image;
    int maximum = 0;
    file >> magic >> image.width >> image.height >> maximum;
    file.get();
    if (magic != "P5" || maximum != 255) {
      return std::nullopt;
    }
    image.pixels.resize(static_cast<std::size_t>(image.width) * image.height);
    file.read(static_cast<char*>(static_cast<void*>(image.pixels.data())),
              static_cast<std::streamsize>(image.pixels.size()));
    if (!file) {
      return std::nullopt;
    }
    return image;
  }
};

TEST_P(TextGolden, MatchesTheStoredRender) {
  const GoldenSample& sample = GetParam();
  provider::headless::NullGpu gpu;
  auto atlas = GlyphAtlas::create(gpu);
  ASSERT_TRUE(atlas.has_value());
  const TextLayout layout =
      TextFixture::layout(sample.text, TextFixture::style(sample.locale),
                          std::nullopt, sample.scale);
  for (const GlyphRun& run : layout.runs()) {
    for (const PositionedGlyph& glyph : layout.glyphs(run)) {
      EXPECT_NE(glyph.glyph.value(), 0U) << sample.name;
    }
  }
  auto image = SoftwareRenderer::render(
      layout, *atlas,
      provider::Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F},
      provider::Colour{.red = 1.0F, .green = 1.0F, .blue = 1.0F}, 4);
  ASSERT_TRUE(image.has_value());
  const Gray rendered = gray(*image);
  EXPECT_TRUE(std::ranges::any_of(
      rendered.pixels, [](std::uint8_t value) { return value < 128; }));
  if (updating()) {
    write(path(sample.name), rendered);
    return;
  }
  const std::optional<Gray> stored = read(path(sample.name));
  ASSERT_TRUE(stored.has_value())
      << path(sample.name) << " is missing; run make update-goldens";
  ASSERT_EQ(stored->width, rendered.width) << sample.name;
  ASSERT_EQ(stored->height, rendered.height) << sample.name;
  std::size_t differing = 0;
  double total = 0.0;
  for (std::uint32_t y = 0; y < rendered.height; ++y) {
    for (std::uint32_t x = 0; x < rendered.width; ++x) {
      const std::size_t index = (std::size_t{y} * rendered.width) + x;
      const int difference =
          std::abs(int{rendered.pixels[index]} - int{stored->pixels[index]});
      total += difference;
      if (difference > level_threshold && !displaced(rendered, *stored, x, y)) {
        ++differing;
      }
    }
  }
  const auto count = static_cast<double>(rendered.pixels.size());
  EXPECT_LE(static_cast<double>(differing) / count, maximum_off_fraction)
      << sample.name;
  EXPECT_LE(total / count, maximum_mean_difference) << sample.name;
}

INSTANTIATE_TEST_SUITE_P(
    Samples, TextGolden,
    ::testing::Values(GoldenSample{"en", "en",
                                   "Display & Brightness → Wave office 1234",
                                   1.0F},
                      GoldenSample{"ja", "ja",
                                   "設定を変更しました"
                                   "。「次へ」を押して"
                                   "ください。",
                                   1.0F},
                      GoldenSample{"ja_docked", "ja",
                                   "設定を変更しました"
                                   "。",
                                   1.5F},
                      GoldenSample{"zh_hans", "zh-Hans",
                                   "设置已保存。请按「"
                                   "确定」继续。",
                                   1.0F},
                      GoldenSample{"zh_hant", "zh-Hant",
                                   "設定已儲存。請按「"
                                   "確定」繼續。",
                                   1.0F},
                      GoldenSample{"ko", "ko",
                                   "설정이 저장되었습니"
                                   "다. 계속하려면 확인을 "
                                   "누르세요.",
                                   1.0F},
                      GoldenSample{"ar", "ar",
                                   "الإعدادات "
                                   "محفوظة",
                                   1.0F},
                      GoldenSample{"he", "he",
                                   "ההגדרות "
                                   "נשמרו",
                                   1.0F},
                      GoldenSample{"th", "th",
                                   "การตั้งค่"
                                   "าถูกบันทึ"
                                   "กแล้ว",
                                   1.0F},
                      GoldenSample{"hi", "hi",
                                   "सेटिंग्स "
                                   "सहेजी गईं",
                                   1.0F},
                      GoldenSample{"bidi", "en",
                                   "Hello שלום world "
                                   "مرحبا 123",
                                   1.0F}),
    [](const ::testing::TestParamInfo<GoldenSample>& sample) {
      return std::string{sample.param.name};
    });

}
