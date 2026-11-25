#include <focus_fixture.hpp>

#include <arisa/components/attributed_string.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/system_format_style.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/time_data_source.hpp>
#include <arisa/components/timeline_schedule.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/composition/text_environment.hpp>
#include <arisa/controls/gauge.hpp>
#include <arisa/controls/gauge_style.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/text/line_height.hpp>
#include <arisa/text/line_pattern.hpp>
#include <arisa/text/typesetting_language.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;

class LiveTextTest : public FocusFixture {
 protected:
  using Date = components::Text::Date;

  static constexpr provider::SurfaceSize small{
      .width = provider::SurfaceWidth{240},
      .height = provider::SurfaceHeight{80}};
  static constexpr std::chrono::seconds epoch{1778307200};

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

  static auto now() -> Date { return Date{epoch}; }

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     Element content) -> Application {
    providers.clock().set_wall(now());
    fonts(providers);
    providers.surface().resize(small);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    return start(providers, components::VStack(std::move(content).id("live"))
                                .frame({.width = 240.0F, .height = 80.0F}));
  }

  static auto shown(Application& application) -> std::string {
    const auto node = node_named(application, "live");
    if (!node.has_value()) {
      return {};
    }
    const auto text = node->get().content_as<components::Text::Content>();
    return text.has_value() ? std::string{text->get().text()} : std::string{};
  }

  static auto frame_of(Application& application) -> composition::Rect {
    const auto node = node_named(application, "live");
    return node.has_value() ? node->get().frame() : composition::Rect{};
  }

  struct Tally {
    int presented{0};
  };

  static auto run(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, int count,
                  std::chrono::milliseconds step) -> Tally {
    Tally tally;
    for (int index = 0; index < count; ++index) {
      EXPECT_TRUE(providers.clock().advance(step).has_value());
      EXPECT_TRUE(application.frame().has_value());
      tally.presented += application.last_frame().presented ? 1 : 0;
    }
    return tally;
  }

  static auto render(provider::headless::HeadlessProviderFactory& providers,
                     Element content) -> std::optional<provider::Image> {
    fonts(providers);
    providers.surface().resize(small);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(small);
    Application application =
        start(providers, components::ZStack(std::move(content))
                             .frame({.width = 240.0F, .height = 80.0F})
                             .background(theme::Colour::hex(0xFFFFFF),
                                         theme::Shape::rectangle()));
    frames(application, 4);
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return image.has_value() ? std::optional{std::move(*image)} : std::nullopt;
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

  static auto segments(const provider::Image& image, std::uint32_t rgb)
      -> std::size_t {
    std::size_t best = 0;
    for (std::uint32_t y = 0; y < image.height(); ++y) {
      std::size_t runs = 0;
      bool inside = false;
      for (std::uint32_t x = 0; x < image.width(); ++x) {
        const bool on = close(image, x, y, rgb, 60);
        if (on && !inside) {
          ++runs;
        }
        inside = on;
      }
      best = std::max(best, runs);
    }
    return best;
  }

  struct Seen {
    composition::TextDecoration underline{};
    composition::TextDecoration strikethrough{};
    composition::WritingDirectionStrategy direction{};
    composition::TextAlignmentStrategy alignment{};
    std::optional<text::Locale> language{};
    composition::LineLimit limit{};
  };

  class Probe final : public composition::Component {
   public:
    explicit Probe(std::shared_ptr<Seen> seen) : seen_(std::move(seen)) {}

    auto operator==(const Probe&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      using composition::TextEnvironment;
      seen_->underline = context.environment(TextEnvironment::underline);
      seen_->strikethrough =
          context.environment(TextEnvironment::strikethrough);
      seen_->direction =
          context.environment(TextEnvironment::writing_direction);
      seen_->alignment =
          context.environment(TextEnvironment::alignment_strategy);
      seen_->language =
          context.environment(TextEnvironment::typesetting_language);
      seen_->limit = context.environment(TextEnvironment::line_limit);
      return components::Text("probe");
    }

   private:
    std::shared_ptr<Seen> seen_;
  };

  static auto pixels() -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> bytes;
    for (std::uint32_t at = 0; at < 16U * 16U; ++at) {
      for (const std::uint8_t channel : {std::uint8_t{0}, std::uint8_t{200},
                                         std::uint8_t{0}, std::uint8_t{255}}) {
        bytes.push_back(static_cast<std::byte>(channel));
      }
    }
    auto made = provider::Image::create(16, 16, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }
};

TEST_F(LiveTextTest, ATimerRepaintsOncePerSecondAndOnlyThen) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers,
      components::Text::timer_interval({.start = now(), .end = now() + 100s}));
  EXPECT_EQ(shown(application), "1:40");
  const Tally tally = run(providers, application, 160, 20ms);
  EXPECT_EQ(tally.presented, 3);
  EXPECT_EQ(shown(application), "1:37");
}

TEST_F(LiveTextTest, AMinuteTextStaysSettledBetweenMinutes) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers,
      components::Text{now() - 3h, components::Text::DateStyle::relative});
  EXPECT_EQ(shown(application), "3 hours, 0 minutes");
  run(providers, application, 5, 16ms);
  const Tally settled = run(providers, application, 50, 1000ms);
  EXPECT_EQ(settled.presented, 0);
  const Tally crossing = run(providers, application, 20, 1000ms);
  EXPECT_EQ(crossing.presented, 1);
  EXPECT_EQ(shown(application), "3 hours, 1 minute");
}

TEST_F(LiveTextTest, OffsetAndReferenceTextsTickWithTheClock) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers,
             components::Text{
                 components::TimeDataSource<Date>::current_date(),
                 components::SystemFormatStyle::DateReference{now() - 40s}});
  EXPECT_EQ(shown(application), "1 minute ago");
  run(providers, application, 60, 1000ms);
  EXPECT_EQ(shown(application), "2 minutes ago");
  provider::headless::HeadlessProviderFactory again;
  Application offset = launch(
      again,
      components::Text{now() - 45s, components::Text::DateStyle::offset});
  EXPECT_EQ(shown(offset), "+45 seconds");
  run(again, offset, 5, 1001ms);
  EXPECT_EQ(shown(offset), "+50 seconds");
}

TEST_F(LiveTextTest, APausedTimerFreezes) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers, components::Text::timer_interval(
                            {.start = now(), .end = now() + 100s}, now() + 2s));
  const Tally tally = run(providers, application, 6, 1000ms);
  EXPECT_LE(tally.presented, 3);
  EXPECT_EQ(shown(application), "1:38");
}

TEST_F(LiveTextTest, DashedUnderlinesDrawSeparateSegments) {
  provider::headless::HeadlessProviderFactory solid_providers;
  const auto solid = render(
      solid_providers, components::Text("MMMMMMMM")
                           .font(theme::Font::system(20.0F))
                           .underline(true, theme::Colour::hex(0xFF0000))
                           .foreground_style(theme::Colour::hex(0x000000)));
  provider::headless::HeadlessProviderFactory dashed_providers;
  const auto dashed = render(
      dashed_providers, components::Text("MMMMMMMM")
                            .font(theme::Font::system(20.0F))
                            .underline(true, text::LinePattern::dash,
                                       theme::Colour::hex(0xFF0000))
                            .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(solid.has_value());
  ASSERT_TRUE(dashed.has_value());
  EXPECT_EQ(segments(*solid, 0xFF0000), 1U);
  EXPECT_GE(segments(*dashed, 0xFF0000), 4U);
  EXPECT_LT(count(*dashed, 0xFF0000), count(*solid, 0xFF0000));
}

TEST_F(LiveTextTest, RunBackgroundsAndDecorationColoursPaint) {
  components::AttributedString text{"plain "};
  text.append("marked",
              components::AttributedString::Attributes{
                  .background = theme::Colour::hex(0xFFFF00),
                  .underline_style = composition::TextDecoration{
                      .active = true, .colour = theme::Colour::hex(0x0000FF)}});
  provider::headless::HeadlessProviderFactory providers;
  const auto image =
      render(providers, components::Text(std::move(text))
                            .font(theme::Font::system(20.0F))
                            .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(image.has_value());
  EXPECT_GT(count(*image, 0xFFFF00), 200U);
  EXPECT_GT(count(*image, 0x0000FF), 20U);
}

TEST_F(LiveTextTest, InlineImagesDrawInTheLine) {
  provider::headless::HeadlessProviderFactory providers;
  const auto image =
      render(providers, (components::Text("A ") +
                         components::Text(components::Image{pixels()}) +
                         components::Text(" B"))
                            .font(theme::Font::system(20.0F))
                            .foreground_style(theme::Colour::hex(0x000000)));
  ASSERT_TRUE(image.has_value());
  EXPECT_GT(count(*image, 0x00C800), 150U);
}

TEST_F(LiveTextTest, PerRunSizesAndLineHeightsGrowTheText) {
  provider::headless::HeadlessProviderFactory providers;
  components::AttributedString text{"Big"};
  text = text.setting<
      components::AttributedString::Scopes::SwiftUIAttributes::FontAttribute>(
      theme::Font::system(34.0F), 0, 3);
  text.append(" small");
  Application application = launch(
      providers,
      components::Text(std::move(text)).font(theme::Font::system(17.0F)));
  const composition::Rect frame = frame_of(application);
  EXPECT_GT(frame.height, 38.0F);
  provider::headless::HeadlessProviderFactory spaced;
  Application lines = launch(
      spaced,
      components::VStack(
          components::Text("One\nTwo\nThree").font(theme::Font::system(20.0F)))
          .line_height(text::LineHeight::exact(30.0F)));
  EXPECT_NEAR(frame_of(lines).height, 90.0F, 1.0F);
  provider::headless::HeadlessProviderFactory ranged;
  Application reserved = launch(
      ranged,
      components::VStack(
          components::Text("One").font(theme::Font::system(20.0F)))
          .line_limit(composition::LinesBetween{.minimum = 3, .maximum = 5}));
  EXPECT_GT(frame_of(reserved).height, 60.0F);
}

TEST_F(LiveTextTest, ElementModifiersWriteTheTextEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<Seen>();
  Application application = launch(
      providers,
      Element{Probe{seen}}
          .underline(true, text::LinePattern::dot, theme::Colour::hex(0xFF0000))
          .strikethrough(true, text::LinePattern::dash_dot)
          .writing_direction(
              composition::WritingDirectionStrategy::content_based)
          .multiline_text_alignment(
              composition::TextAlignmentStrategy::writing_direction_based)
          .typesetting_language(
              text::TypesettingLanguage::explicit_(*text::Locale::create("ja")))
          .line_limit(composition::LinesAtLeast{.minimum = 2}));
  EXPECT_TRUE(seen->underline.active);
  EXPECT_EQ(seen->underline.pattern, text::LinePattern::dot);
  EXPECT_EQ(seen->strikethrough.pattern, text::LinePattern::dash_dot);
  EXPECT_EQ(seen->direction,
            composition::WritingDirectionStrategy::content_based);
  EXPECT_EQ(seen->alignment,
            composition::TextAlignmentStrategy::writing_direction_based);
  EXPECT_EQ(seen->language, text::Locale::create("ja"));
  EXPECT_FALSE(seen->limit.lines.has_value());
  EXPECT_EQ(seen->limit.minimum, 2U);
}

TEST_F(LiveTextTest, ATimelineContextInvalidatesItsContent) {
  provider::headless::HeadlessProviderFactory providers;
  auto entries = std::make_shared<int>(0);
  auto held =
      std::make_shared<std::optional<components::TimelineView::Context>>();
  Application application = launch(
      providers,
      Element{components::TimelineView(
          components::TimelineSchedule::periodic(Date{}, std::chrono::hours{1}),
          [entries, held](const components::TimelineView::Context& timeline) {
            ++*entries;
            *held = timeline;
            return Element{components::Text("entry")};
          })});
  run(providers, application, 5, 16ms);
  const int settled = *entries;
  run(providers, application, 5, 16ms);
  EXPECT_EQ(*entries, settled);
  ASSERT_TRUE(held->has_value());
  (*held)->invalidate_timeline_content();
  run(providers, application, 2, 16ms);
  EXPECT_EQ(*entries, settled + 1);
}

TEST_F(LiveTextTest, LinearCapacityGaugesTakeATint) {
  provider::headless::HeadlessProviderFactory tinted_providers;
  const auto tinted =
      render(tinted_providers,
             Element{controls::Gauge(0.8, components::Text("Fuel"))}
                 .gauge_style(controls::GaugeStyle::linear_capacity(
                     graphics::ShapeStyle{theme::Colour::hex(0xFF0000)}))
                 .frame({.width = 200.0F}));
  provider::headless::HeadlessProviderFactory plain_providers;
  const auto plain = render(
      plain_providers, Element{controls::Gauge(0.8, components::Text("Fuel"))}
                           .gauge_style(controls::GaugeStyle::linear_capacity())
                           .frame({.width = 200.0F}));
  ASSERT_TRUE(tinted.has_value());
  ASSERT_TRUE(plain.has_value());
  EXPECT_GT(count(*tinted, 0xFF0000), 100U);
  EXPECT_LT(count(*plain, 0xFF0000), 10U);
}

}
