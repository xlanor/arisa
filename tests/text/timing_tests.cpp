#include <text_fixture.hpp>

#include <arisa/text/typesetter.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <print>
#include <string>

namespace arisa::text {

TEST(TextTiming, ReportsMixedParagraphCosts) {
  const std::string paragraph =
      "Settings saved. 設定を変更しました"
      "。「次へ」を押してくださ"
      "い。 مرحبا بالع"
      "الم and שלום עול"
      "ם, then 설정이 저장되었습니"
      "다. สวัสดีครับ "
      "क्षत्रिय हिन"
      "्दी. The quick brown fox jumps over the lazy dog 1234 "
      "设置已保存。 Display & Brightness, office "
      "affinity 設定已儲存。";
  std::size_t characters = 0;
  for (const char byte : paragraph) {
    if ((static_cast<unsigned char>(byte) & 0xC0U) != 0x80U) {
      ++characters;
    }
  }
  EXPECT_GE(characters, 200U);
  const TextStyle style = TextFixture::style("ja", 17.0F);
  const auto width = LayoutWidth::points(320.0F).value();
  const auto scale = provider::SurfaceScale::unit();
  using Clock = std::chrono::steady_clock;
  constexpr int rounds = 50;
  const auto shaped_start = Clock::now();
  std::size_t lines = 0;
  for (int round = 0; round < rounds; ++round) {
    auto layout = TextLayout::create(TextFixture::fonts(), paragraph, style,
                                     width, scale);
    ASSERT_TRUE(layout.has_value());
    lines = layout->lines().size();
  }
  const auto shaped_time = Clock::now() - shaped_start;
  auto typesetter = Typesetter::create(TextFixture::fonts());
  ASSERT_TRUE(typesetter.has_value());
  ASSERT_TRUE(typesetter->measure(paragraph, style, width, scale).has_value());
  const auto measure_start = Clock::now();
  for (int round = 0; round < rounds; ++round) {
    const auto varying =
        LayoutWidth::points(200.0F + static_cast<float>(round)).value();
    ASSERT_TRUE(
        typesetter->measure(paragraph, style, varying, scale).has_value());
  }
  const auto measure_time = Clock::now() - measure_start;
  const auto cached_start = Clock::now();
  for (int round = 0; round < rounds; ++round) {
    ASSERT_TRUE(typesetter->layout(paragraph, style, width, scale).has_value());
  }
  const auto cached_time = Clock::now() - cached_start;
  const auto microseconds = [](Clock::duration duration) {
    return std::chrono::duration<double, std::micro>(duration).count() / rounds;
  };
  std::println(
      "{} characters, {} lines: shape+layout {:.1f} us, measure on cached "
      "shaping {:.1f} us, cached layout {:.2f} us",
      characters, lines, microseconds(shaped_time), microseconds(measure_time),
      microseconds(cached_time));
  EXPECT_GT(lines, 1U);
}

}
