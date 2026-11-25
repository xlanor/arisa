#include <text_fixture.hpp>

#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/text/case_mapping.hpp>
#include <arisa/text/glyph_atlas.hpp>
#include <arisa/text/software_renderer.hpp>
#include <arisa/text/typesetter.hpp>
#include <arisa/text/utf8.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::text {

struct TruncationSample {
  std::string_view locale;
  std::string_view text;
  float width;
};

class TextFittingTest : public ::testing::Test {
 protected:
  static constexpr std::string_view ellipsis = "…";

  struct Ink {
    int left{-1};
    int right{-1};
  };

  static auto width(float points) -> LayoutWidth {
    return LayoutWidth::points(points).value();
  }

  static auto unit() -> provider::SurfaceScale {
    return provider::SurfaceScale::unit();
  }

  static auto typesetter() -> Typesetter {
    auto made = Typesetter::create(TextFixture::fonts());
    EXPECT_TRUE(made.has_value());
    return std::move(*made);
  }

  static auto fitting(std::optional<std::size_t> lines,
                      TruncationMode truncation = TruncationMode::tail,
                      float minimum_scale_factor = 1.0F,
                      bool allows_tightening = false,
                      std::optional<float> height = std::nullopt)
      -> TextFitting {
    TextFitting made;
    made.lines = lines;
    made.height = height;
    made.truncation = truncation;
    made.minimum_scale_factor = minimum_scale_factor;
    made.allows_tightening = allows_tightening;
    return made;
  }

  static auto natural(Typesetter& setter, std::string_view text,
                      const TextStyle& style) -> TextSize {
    return setter.measure(text, style, LayoutWidth::unbounded(), unit())
        .value_or(TextSize{});
  }

  static auto render(const TextLayout& layout) -> provider::Image {
    provider::headless::NullGpu gpu;
    auto atlas = GlyphAtlas::create(gpu);
    EXPECT_TRUE(atlas.has_value());
    auto image = SoftwareRenderer::render(
        layout, *atlas,
        provider::Colour{.red = 0.0F, .green = 0.0F, .blue = 0.0F},
        provider::Colour{.red = 1.0F, .green = 1.0F, .blue = 1.0F}, 0);
    EXPECT_TRUE(image.has_value());
    return std::move(*image);
  }

  static auto ink(const provider::Image& image, std::uint32_t top,
                  std::uint32_t bottom) -> Ink {
    Ink found;
    for (std::uint32_t y = top; y < std::min(bottom, image.height()); ++y) {
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        if (image.pixel(x, y).value()[0] < 128) {
          found.left = found.left < 0
                           ? static_cast<int>(x)
                           : std::min(found.left, static_cast<int>(x));
          found.right = std::max(found.right, static_cast<int>(x));
        }
      }
    }
    return found;
  }

  static auto combining(char32_t codepoint) -> bool {
    return codepoint == 0x0E31 ||
           (codepoint >= 0x0E34 && codepoint <= 0x0E3A) ||
           (codepoint >= 0x0E47 && codepoint <= 0x0E4E) ||
           (codepoint >= 0x064B && codepoint <= 0x065F);
  }

  static auto after(std::string_view text, std::size_t offset) -> char32_t {
    return offset < text.size() ? Utf8::decode(text, offset).codepoint : 0;
  }
};

TEST_F(TextFittingTest, TruncationModesKeepGraphemesWholeInEveryScript) {
  constexpr std::array samples{
      TruncationSample{"en", "The quick brown fox jumps over the lazy dog",
                       150.0F},
      TruncationSample{"ja",
                       "設定を変更しました"
                       "。「次へ」を押して"
                       "ください",
                       150.0F},
      TruncationSample{"ar",
                       "\u0627\u0644\u0625\u0639\u062F\u0627\u062F\u0627\u062A "
                       "\u0645\u062D\u0641\u0648\u0638\u0629 \u0627\u0644\u0625"
                       "\u0639\u062F\u0627\u062F\u0627\u062A \u0645\u062D\u0641"
                       "\u0648\u0638\u0629",
                       110.0F},
      TruncationSample{"th",
                       "\u0E01\u0E32\u0E23\u0E15\u0E31\u0E49\u0E07\u0E04\u0E48"
                       "\u0E32\u0E16\u0E39\u0E01\u0E1A\u0E31\u0E19\u0E17\u0E36"
                       "\u0E01\u0E41\u0E25\u0E49\u0E27"
                       "\u0E01\u0E32\u0E23\u0E15\u0E31\u0E49\u0E07\u0E04\u0E48"
                       "\u0E32\u0E16\u0E39\u0E01\u0E1A\u0E31\u0E19\u0E17\u0E36"
                       "\u0E01\u0E41\u0E25\u0E49\u0E27",
                       110.0F}};
  Typesetter setter = typesetter();
  for (const TruncationSample& sample : samples) {
    const TextStyle style = TextFixture::style(sample.locale);
    ASSERT_GT(natural(setter, sample.text, style).width, sample.width)
        << sample.locale;
    for (const TruncationMode mode :
         {TruncationMode::head, TruncationMode::middle, TruncationMode::tail}) {
      const auto fitted = setter.fitted(sample.text, style, width(sample.width),
                                        unit(), fitting(1, mode));
      ASSERT_TRUE(fitted.has_value());
      const std::string& shown = (*fitted)->text;
      const std::size_t at = shown.find(ellipsis);
      ASSERT_NE(at, std::string::npos) << sample.locale;
      EXPECT_TRUE(Utf8::valid(shown)) << sample.locale;
      if (mode == TruncationMode::head) {
        EXPECT_EQ(at, 0U) << sample.locale;
      } else if (mode == TruncationMode::tail) {
        EXPECT_EQ(at + ellipsis.size(), shown.size()) << sample.locale;
      } else {
        EXPECT_GT(at, 0U) << sample.locale;
        EXPECT_LT(at + ellipsis.size(), shown.size()) << sample.locale;
      }
      EXPECT_FALSE(combining(after(shown, at + ellipsis.size())))
          << sample.locale << " " << shown;
      EXPECT_FALSE(combining(after(shown, 0))) << sample.locale << " " << shown;
      const TextLayout laid =
          TextFixture::layout(shown, style, sample.width, 1.0F);
      EXPECT_EQ(laid.lines().size(), 1U) << sample.locale;
      for (const PositionedGlyph& glyph : laid.glyphs()) {
        EXPECT_NE(glyph.glyph.value(), 0U) << sample.locale;
      }
      const provider::Image image = render(laid);
      const Ink seen = ink(image, 0, image.height());
      ASSERT_GE(seen.left, 0) << sample.locale;
      EXPECT_LE(seen.right - seen.left, static_cast<int>(sample.width) + 2)
          << sample.locale << " " << static_cast<int>(mode);
      EXPECT_GT(seen.right - seen.left, static_cast<int>(sample.width * 0.6F))
          << sample.locale << " " << static_cast<int>(mode);
    }
  }
}

TEST_F(TextFittingTest, MultilineAlignmentPlacesTheShorterLine) {
  struct Case {
    std::string_view locale;
    std::string_view text;
    TextAlignment alignment;
    float expected;
  };
  constexpr std::array cases{
      Case{"en", "A much wider first line\nShort", TextAlignment::natural,
           0.0F},
      Case{"en", "A much wider first line\nShort", TextAlignment::center, 0.5F},
      Case{"en", "A much wider first line\nShort", TextAlignment::right, 1.0F},
      Case{"ar",
           "\u0627\u0644\u0625\u0639\u062F\u0627\u062F\u0627\u062A "
           "\u0645\u062D\u0641\u0648\u0638\u0629\n\u0645\u062D\u0641",
           TextAlignment::natural, 1.0F},
      Case{"th",
           "การตั้งค่า"
           "ถูกบันทึก\n"
           "แล้ว",
           TextAlignment::center, 0.5F},
      Case{"ja",
           "設定を変更しました\n"
           "次へ",
           TextAlignment::right, 1.0F}};
  for (const Case& each : cases) {
    TextStyle style = TextFixture::style(each.locale);
    style.alignment = each.alignment;
    const TextLayout laid = TextFixture::layout(each.text, style);
    ASSERT_EQ(laid.lines().size(), 2U) << each.locale;
    const provider::Image image = render(laid);
    const TextLine& second = laid.lines()[1];
    const Ink first = ink(image, 0, static_cast<std::uint32_t>(second.top));
    const Ink shorter =
        ink(image, static_cast<std::uint32_t>(second.top), image.height());
    ASSERT_GE(first.left, 0) << each.locale;
    ASSERT_GE(shorter.left, 0) << each.locale;
    const float room = static_cast<float>((first.right - first.left) -
                                          (shorter.right - shorter.left));
    ASSERT_GT(room, 20.0F) << each.locale;
    const float placed = static_cast<float>(shorter.left - first.left) / room;
    EXPECT_NEAR(placed, each.expected, 0.12F)
        << each.locale << " " << static_cast<int>(each.alignment);
  }
}

TEST_F(TextFittingTest, KerningAndTrackingSpreadEveryCluster) {
  Typesetter setter = typesetter();
  const TextStyle plain = TextFixture::style("en");
  TextStyle kerned = plain;
  kerned.kerning = 5.0F;
  TextStyle tracked = plain;
  tracked.tracking = 5.0F;
  const float base = natural(setter, "AAAA", plain).width;
  EXPECT_NEAR(natural(setter, "AAAA", kerned).width - base, 20.0F, 0.01F);
  EXPECT_NEAR(natural(setter, "AAAA", tracked).width - base, 20.0F, 0.01F);
  const provider::Image narrow = render(TextFixture::layout("AAAA", plain));
  const provider::Image wide = render(TextFixture::layout("AAAA", kerned));
  const Ink before = ink(narrow, 0, narrow.height());
  const Ink spread = ink(wide, 0, wide.height());
  EXPECT_NEAR((spread.right - spread.left) - (before.right - before.left), 15,
              2);
}

TEST_F(TextFittingTest, TabularDigitsShareOneAdvance) {
  Typesetter setter = typesetter();
  TextStyle style = TextFixture::style("en");
  EXPECT_LT(natural(setter, "111", style).width,
            natural(setter, "888", style).width - 1.0F);
  style.tabular_digits = true;
  EXPECT_NEAR(natural(setter, "111", style).width,
              natural(setter, "888", style).width, 0.01F);
}

TEST_F(TextFittingTest, LineSpacingAddsSpaceBetweenLinesOnly) {
  Typesetter setter = typesetter();
  TextStyle style = TextFixture::style("en");
  const float one = natural(setter, "Ab", style).height;
  const float three = natural(setter, "Ab\nAb\nAb", style).height;
  style.line_spacing = 10.0F;
  EXPECT_NEAR(natural(setter, "Ab", style).height, one, 0.01F);
  EXPECT_NEAR(natural(setter, "Ab\nAb\nAb", style).height, three + 20.0F,
              0.01F);
  const TextLayout laid = TextFixture::layout("Ab\nAb", style);
  ASSERT_EQ(laid.lines().size(), 2U);
  EXPECT_NEAR(
      laid.lines()[1].top - (laid.lines()[0].top + laid.lines()[0].height),
      10.0F, 0.01F);
}

TEST_F(TextFittingTest, BaselineOffsetGrowsTheBoxAndMovesTheInk) {
  Typesetter setter = typesetter();
  const TextStyle plain = TextFixture::style("en");
  TextStyle raised = plain;
  raised.baseline_offset = 8.0F;
  TextStyle lowered = plain;
  lowered.baseline_offset = -8.0F;
  const float height = natural(setter, "Ab", plain).height;
  EXPECT_NEAR(natural(setter, "Ab", raised).height, height + 8.0F, 0.01F);
  EXPECT_NEAR(natural(setter, "Ab", lowered).height, height + 8.0F, 0.01F);
  const auto top = [](const TextStyle& style) {
    const provider::Image image = render(TextFixture::layout("Ab", style));
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        if (image.pixel(x, y).value()[0] < 128) {
          return static_cast<int>(y);
        }
      }
    }
    return -1;
  };
  EXPECT_EQ(top(raised), top(plain));
  EXPECT_NEAR(top(lowered) - top(plain), 8, 1);
}

TEST_F(TextFittingTest, MinimumScaleShrinksBeforeTruncating) {
  Typesetter setter = typesetter();
  const TextStyle style = TextFixture::style("en");
  const std::string_view sentence =
      "The quick brown fox jumps over the lazy dog";
  const float full = natural(setter, sentence, style).width;
  const auto shrunk = setter.fitted(sentence, style, width(full * 0.6F), unit(),
                                    fitting(1, TruncationMode::tail, 0.5F));
  ASSERT_TRUE(shrunk.has_value());
  EXPECT_EQ((*shrunk)->text, sentence);
  EXPECT_NEAR((*shrunk)->style.size.value() / style.size.value(), 0.6F, 0.03F);
  EXPECT_LE(natural(setter, (*shrunk)->text, (*shrunk)->style).width,
            full * 0.6F + 0.01F);
  const auto floored =
      setter.fitted(sentence, style, width(full * 0.6F), unit(),
                    fitting(1, TruncationMode::tail, 0.8F));
  ASSERT_TRUE(floored.has_value());
  EXPECT_NEAR((*floored)->style.size.value() / style.size.value(), 0.8F,
              0.001F);
  EXPECT_NE((*floored)->text.find(ellipsis), std::string::npos);
  const auto wrapped =
      setter.fitted(sentence, style, width(full * 0.6F), unit(),
                    fitting(std::nullopt, TruncationMode::tail, 0.5F, false,
                            natural(setter, "A", style).height));
  ASSERT_TRUE(wrapped.has_value());
  EXPECT_LT((*wrapped)->style.size.value(), style.size.value() * 0.7F);
}

TEST_F(TextFittingTest, TighteningCompressesUpToTheMeasuredLimit) {
  Typesetter setter = typesetter();
  const TextStyle style = TextFixture::style("en");
  const std::string_view sentence =
      "The quick brown fox jumps over the lazy dog";
  const float full = natural(setter, sentence, style).width;
  const TextFitting tight = fitting(1, TruncationMode::tail, 1.0F, true);
  const auto close =
      setter.fitted(sentence, style, width(full * 0.97F), unit(), tight);
  ASSERT_TRUE(close.has_value());
  EXPECT_EQ((*close)->text, sentence);
  EXPECT_LT((*close)->style.tracking, 0.0F);
  const auto loose =
      setter.fitted(sentence, style, width(full * 0.97F), unit(), fitting(1));
  ASSERT_TRUE(loose.has_value());
  EXPECT_NE((*loose)->text.find(ellipsis), std::string::npos);
  const auto far =
      setter.fitted(sentence, style, width(full * 0.94F), unit(), tight);
  ASSERT_TRUE(far.has_value());
  EXPECT_NE((*far)->text.find(ellipsis), std::string::npos);
}

TEST_F(TextFittingTest, CaseMappingCoversLatinGreekAndCyrillic) {
  EXPECT_EQ(CaseMapping::uppercased("Hello, straße été"), "HELLO, STRASSE ÉTÉ");
  EXPECT_EQ(CaseMapping::uppercased("ωμέγα"), "ΩΜΈΓΑ");
  EXPECT_EQ(CaseMapping::uppercased("привет"), "ПРИВЕТ");
  EXPECT_EQ(CaseMapping::lowercased("HeLLo ŁÓDŹ"), "hello łódź");
  EXPECT_EQ(CaseMapping::uppercased("日本 123"), "日本 123");
}

}
