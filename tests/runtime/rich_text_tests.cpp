#include <focus_fixture.hpp>

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/text/font_entry.hpp>
#include <arisa/text/font_face.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class RichTextTest : public FocusFixture {
 protected:
  static constexpr provider::SurfaceSize small{
      .width = provider::SurfaceWidth{240},
      .height = provider::SurfaceHeight{80}};
  static constexpr std::string_view notes =
      "Read the [changelog](https://example.com/changelog) or the "
      "[guide](https://example.com/guide).";

  static auto read(const std::filesystem::path& path)
      -> std::vector<std::byte> {
    std::ifstream file{path, std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{file},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    return bytes;
  }

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    for (const std::string_view file :
         {std::string_view{"arisa/fonts/InterVariable.ttf"},
          std::string_view{"arisa/fonts/InterItalic-Regular.ttf"},
          std::string_view{"arisa/fonts/JetBrainsMono-Regular.ttf"}}) {
      const std::vector<std::byte> bytes =
          read(std::filesystem::path{ARISA_SOURCE_ASSETS} / file);
      ASSERT_FALSE(bytes.empty()) << file;
      providers.assets().add(*provider::AssetPath::create(file), bytes);
    }
  }

  static auto script(std::string_view file) -> text::FontEntry {
    auto face =
        text::FontFace::create(std::make_shared<const provider::AssetBlob>(
            read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/fonts" /
                 file)));
    EXPECT_TRUE(face.has_value()) << file;
    return text::FontEntry{
        .role = text::FontRole::script,
        .face = std::make_shared<const text::FontFace>(std::move(*face))};
  }

  static auto render(composition::Element content,
                     std::vector<text::FontEntry> extra = {})
      -> std::optional<provider::Image> {
    provider::headless::HeadlessProviderFactory providers;
    fonts(providers);
    providers.surface().resize(small);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(small);
    auto created =
        Application::create(providers,
                            components::ZStack(std::move(content))
                                .frame({.width = 240.0F, .height = 80.0F})
                                .background(theme::Colour::hex(0xFFFFFF),
                                            theme::Shape::rectangle()),
                            ApplicationOptions{.executors = std::nullopt,
                                               .fonts = std::move(extra),
                                               .atlas = {},
                                               .interaction = {}});
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return std::nullopt;
    }
    Application application = std::move(*created);
    frames(application, 5);
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return std::nullopt;
    }
    return std::move(*image);
  }

  static auto close(const provider::Image& image, std::uint32_t x,
                    std::uint32_t y, std::uint32_t rgb, int slack) -> bool {
    const auto seen = image.pixel(x, y);
    if (!seen.has_value()) {
      return false;
    }
    const std::array<int, 3> wanted{static_cast<int>((rgb >> 16U) & 0xffU),
                                    static_cast<int>((rgb >> 8U) & 0xffU),
                                    static_cast<int>(rgb & 0xffU)};
    for (std::size_t channel = 0; channel < 3; ++channel) {
      if (std::abs((*seen)[channel] - wanted[channel]) > slack) {
        return false;
      }
    }
    return true;
  }

  static auto rows(const provider::Image& image, std::uint32_t rgb)
      -> std::vector<std::uint32_t> {
    std::vector<std::uint32_t> found;
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      std::uint32_t count = 0;
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        count += close(image, x, y, rgb, 40) ? 1U : 0U;
      }
      if (count >= 30) {
        found.push_back(y);
      }
    }
    return found;
  }

  static auto count(const provider::Image& image, std::uint32_t rgb)
      -> std::uint32_t {
    std::uint32_t found = 0;
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        found += close(image, x, y, rgb, 40) ? 1U : 0U;
      }
    }
    return found;
  }

  static auto linked(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    fonts(providers);
    return start(providers, components::VStack(
                                button("above", 120.0F, 30.0F),
                                components::Text::markdown(notes).id("notes"))
                                .frame({.width = 600.0F}));
  }
};

TEST_F(RichTextTest, UnderlineAndStrikethroughRasterAsSolidRows) {
  const auto underlined =
      render(components::Text("Hxgy Hxgy")
                 .font(theme::Font::system(20.0F))
                 .underline(true, theme::Colour::hex(0xFF0000))
                 .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(underlined.has_value());
  const std::vector<std::uint32_t> lined = rows(*underlined, 0xFF0000);
  ASSERT_FALSE(lined.empty());
  EXPECT_LE(lined.size(), 3U);
  const auto struck =
      render(components::Text("Hxgy Hxgy")
                 .font(theme::Font::system(20.0F))
                 .strikethrough(true, theme::Colour::hex(0x0000FF))
                 .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(struck.has_value());
  const std::vector<std::uint32_t> crossed = rows(*struck, 0x0000FF);
  ASSERT_FALSE(crossed.empty());
  EXPECT_LT(crossed.back(), lined.front());
  EXPECT_GE(lined.front() - crossed.front(), 6U);
}

TEST_F(RichTextTest, UnderlineSitsWhereSwiftUIPutsIt) {
  const auto image =
      render(components::Text("HHHH")
                 .font(theme::Font::system(20.0F))
                 .underline(true, theme::Colour::hex(0xFF0000))
                 .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(image.has_value());
  const std::vector<std::uint32_t> lined = rows(*image, 0xFF0000);
  ASSERT_FALSE(lined.empty());
  std::uint32_t baseline = 0;
  for (std::uint32_t y = 0; y < image->height(); ++y) {
    for (std::uint32_t x = 0; x < image->width(); ++x) {
      if (close(*image, x, y, 0x000000, 60)) {
        baseline = std::max(baseline, y + 1);
      }
    }
  }
  ASSERT_GT(baseline, 0U);
  const auto gap = static_cast<int>(lined.front()) - static_cast<int>(baseline);
  EXPECT_GE(gap, 1);
  EXPECT_LE(gap, 4);
}

TEST_F(RichTextTest, RunsInkTheirOwnColours) {
  components::AttributedString text{"plain "};
  text.append("red", components::AttributedString::Attributes{
                         .foreground = theme::Colour::hex(0xFF0000)});
  text.append(" plain");
  const auto image =
      render(components::Text(std::move(text))
                 .font(theme::Font::system(24.0F))
                 .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(image.has_value());
  EXPECT_GT(count(*image, 0xFF0000), 20U);
  EXPECT_GT(count(*image, 0x000000), 20U);
}

TEST_F(RichTextTest, ConcatenatedTextKeepsEachSegmentsStyle) {
  const auto image = render(
      (components::Text("Was ") +
       components::Text("$59").foreground_color(theme::Colour::hex(0x0000FF)) +
       components::Text(" now"))
          .font(theme::Font::system(24.0F))
          .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(image.has_value());
  EXPECT_GT(count(*image, 0x0000FF), 20U);
  EXPECT_GT(count(*image, 0x000000), 20U);
}

TEST_F(RichTextTest, StyledRunsRasterAcrossScripts) {
  components::AttributedString text;
  text.append("Hi \u0645\u0631\u062D\u0628\u0627",
              components::AttributedString::Attributes{
                  .weight = theme::Font::Weight::bold,
                  .foreground = theme::Colour::hex(0xFF0000),
                  .underline = true});
  const auto image = render(components::Text(std::move(text))
                                .font(theme::Font::system(24.0F))
                                .foreground_style(theme::Colour::hex(0x000000)),
                            {script("NotoSansArabic-Subset.ttf")});
  ASSERT_TRUE(image.has_value());
  const std::vector<std::uint32_t> lined = rows(*image, 0xFF0000);
  ASSERT_FALSE(lined.empty());
  std::uint32_t left = image->width();
  std::uint32_t right = 0;
  for (std::uint32_t x = 0; x < image->width(); ++x) {
    if (close(*image, x, lined.front(), 0xFF0000, 40)) {
      left = std::min(left, x);
      right = std::max(right, x);
    }
  }
  EXPECT_GT(right - left, 70U);
  std::uint32_t arabic = 0;
  for (std::uint32_t y = 0; y < lined.front(); ++y) {
    for (std::uint32_t x = left + ((right - left) / 2); x <= right; ++x) {
      arabic += close(*image, x, y, 0xFF0000, 60) ? 1U : 0U;
    }
  }
  EXPECT_GT(arabic, 20U);
  EXPECT_EQ(count(*image, 0x000000), 0U);
}

TEST_F(RichTextTest, LinksTakeTheFocusAndOpenOnA) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = linked(providers);
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::dpad_down);
  ASSERT_EQ(focused_key(application), "notes");
  const auto node = node_named(application, "notes");
  ASSERT_TRUE(node.has_value());
  ASSERT_EQ(node->get().link_frames().size(), 2U);
  EXPECT_LT(node->get().link_frames()[0].frame.x,
            node->get().link_frames()[1].frame.x);
  press(providers, application, provider::Button::a);
  ASSERT_EQ(providers.browser().opened().size(), 1U);
  EXPECT_EQ(providers.browser().opened().back().value(),
            "https://example.com/changelog");
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(focused_key(application), "notes");
  EXPECT_EQ(node->get().selected_link(), 1U);
  press(providers, application, provider::Button::a);
  ASSERT_EQ(providers.browser().opened().size(), 2U);
  EXPECT_EQ(providers.browser().opened().back().value(),
            "https://example.com/guide");
  press(providers, application, provider::Button::dpad_left);
  EXPECT_EQ(node->get().selected_link(), 0U);
  press(providers, application, provider::Button::dpad_up);
  EXPECT_EQ(focused_key(application), "above");
}

TEST_F(RichTextTest, ATapOpensTheLinkUnderTheFinger) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = linked(providers);
  const auto node = node_named(application, "notes");
  ASSERT_TRUE(node.has_value());
  ASSERT_EQ(node->get().link_frames().size(), 2U);
  const composition::Rect frame = node->get().frame();
  const composition::Rect second = node->get().link_frames()[1].frame;
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + second.x + (second.width * 0.5F),
                           .y = frame.y + second.y + (second.height * 0.5F)});
  ASSERT_EQ(providers.browser().opened().size(), 1U);
  EXPECT_EQ(providers.browser().opened().back().value(),
            "https://example.com/guide");
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + 2.0F,
                           .y = frame.y + (frame.height * 0.5F)});
  EXPECT_EQ(providers.browser().opened().size(), 1U);
}

}
