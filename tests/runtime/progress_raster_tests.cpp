#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace arisa::runtime {

class ProgressRasterTest : public ::testing::Test {
 protected:
  static constexpr std::string_view spinner_key = "raster-spinner";
  static constexpr std::string_view sweep_key = "raster-sweep";

  class Indicators final : public composition::Component {
   public:
    auto operator==(const Indicators&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::VStack(
          components::VStackOptions{.spacing = 24.0F},
          composition::Element{controls::ProgressView()}.id(
              std::string{spinner_key}),
          composition::Element{controls::ProgressView()}
              .progress_view_style(controls::LinearProgressViewStyle{})
              .frame({.width = 200.0F})
              .id(std::string{sweep_key}));
    }
  };

  static auto rect(Application& application, std::string_view key)
      -> composition::Rect {
    const auto id = application.root().tree().find(
        composition::ElementKey{std::string{key}});
    EXPECT_TRUE(id.has_value());
    if (!id.has_value()) {
      return {};
    }
    return application.root().tree().node(*id)->get().frame();
  }

  static auto ink(const provider::Image& image,
                  const std::array<std::uint8_t, 4>& ground, float left,
                  float top, float right, float bottom) -> int {
    int strongest = 0;
    for (auto y = static_cast<std::uint32_t>(std::max(top, 0.0F));
         y < static_cast<std::uint32_t>(bottom); ++y) {
      for (auto x = static_cast<std::uint32_t>(std::max(left, 0.0F));
           x < static_cast<std::uint32_t>(right); ++x) {
        const auto seen = image.pixel(x, y);
        if (!seen.has_value()) {
          continue;
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
          strongest =
              std::max(strongest, std::abs(static_cast<int>((*seen)[channel]) -
                                           static_cast<int>(ground[channel])));
        }
      }
    }
    return strongest;
  }
};

TEST_F(ProgressRasterTest, SpinnerSpokesSpreadAroundTheCircleAndTheSweepShows) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{320},
                                   .height = provider::SurfaceHeight{240}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  auto created = Application::create(providers, Indicators{});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int step = 0; step < 20; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  const composition::Rect spinner = rect(application, spinner_key);
  const composition::Rect sweep = rect(application, sweep_key);
  ASSERT_GT(spinner.width, 20.0F);
  ASSERT_GT(sweep.width, 100.0F);
  int swept = 0;
  for (int sample = 0; sample < 8; ++sample) {
    for (int step = 0; step < 6; ++step) {
      ASSERT_TRUE(
          providers.clock().advance(std::chrono::milliseconds{16}).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
    const auto image = providers.gpu().capture();
    ASSERT_TRUE(image.has_value());
    const auto ground = image->pixel(2, 2);
    ASSERT_TRUE(ground.has_value());
    const float cx = spinner.x + (spinner.width * 0.5F);
    const float cy = spinner.y + (spinner.height * 0.5F);
    const float band = spinner.width * 0.3F;
    const float reach = spinner.width * 0.25F;
    EXPECT_GT(ink(*image, *ground, cx - band, spinner.y, cx + band,
                  spinner.y + reach),
              24);
    EXPECT_GT(
        ink(*image, *ground, cx - band, spinner.y + spinner.height - reach,
            cx + band, spinner.y + spinner.height),
        24);
    EXPECT_GT(ink(*image, *ground, spinner.x, cy - band, spinner.x + reach,
                  cy + band),
              24);
    EXPECT_GT(ink(*image, *ground, spinner.x + spinner.width - reach, cy - band,
                  spinner.x + spinner.width, cy + band),
              24);
    EXPECT_LT(ink(*image, *ground, cx - 2.0F, cy - 2.0F, cx + 2.0F, cy + 2.0F),
              24);
    const auto track = image->pixel(
        static_cast<std::uint32_t>(sweep.x + 1.0F),
        static_cast<std::uint32_t>(sweep.y + (sweep.height * 0.5F)));
    ASSERT_TRUE(track.has_value());
    if (ink(*image, *track, sweep.x, sweep.y, sweep.x + sweep.width,
            sweep.y + sweep.height) > 40) {
      ++swept;
    }
  }
  EXPECT_GT(swept, 3);
}

}
