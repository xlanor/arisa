#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>

namespace arisa::runtime {

class GalleryDamageTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 protected:
  static constexpr provider::SurfaceSize size{
      .width = provider::SurfaceWidth{320},
      .height = provider::SurfaceHeight{180}};

  class Twin {
   public:
    explicit Twin(examples::GalleryPage page, bool glass = false,
                  provider::SurfaceSize surface = size) {
      partial_.gpu().swapchain(3, true);
      whole_.gpu().swapchain(1, false);
      partial_.gpu().rasterize(surface);
      whole_.gpu().rasterize(surface);
      partial_app_.emplace(start_gallery(partial_, page,
                                         theme::ColorScheme::light, surface,
                                         appearance(glass)));
      whole_app_.emplace(start_gallery(whole_, page, theme::ColorScheme::light,
                                       surface, appearance(glass)));
    }

    static auto appearance(bool glass) -> std::optional<AppearanceOptions> {
      if (!glass) {
        return std::nullopt;
      }
      AppearanceOptions options{
          .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
          .storage = std::nullopt};
      options.policy.defaults.theme = "arisa-theme-glass";
      return options;
    }

    void step(std::optional<provider::InputFrame> input = std::nullopt) {
      if (input.has_value()) {
        partial_.input().push(*input);
        whole_.input().push(std::move(*input));
      }
      ASSERT_TRUE(partial_.clock().advance(frame_step).has_value());
      ASSERT_TRUE(whole_.clock().advance(frame_step).has_value());
      ASSERT_TRUE(partial_app_->frame().has_value());
      ASSERT_TRUE(whole_app_->frame().has_value());
      ++frames_;
      if (partial_.gpu().last_damage().has_value()) {
        ++partial_frames_;
      }
      compare();
    }

    void stroke(provider::PixelPoint from, float distance) {
      constexpr int steps = 10;
      step(touching(from, provider::TouchPhase::began));
      for (int index = 1; index <= steps; ++index) {
        step(touching(
            provider::PixelPoint{
                .x = from.x,
                .y = from.y - (distance * static_cast<float>(index) /
                               static_cast<float>(steps))},
            provider::TouchPhase::moved));
      }
      step(touching(provider::PixelPoint{.x = from.x, .y = from.y - distance},
                    provider::TouchPhase::ended));
      for (int index = 0; index < 18; ++index) {
        step();
      }
    }

    void press(provider::Button button) {
      step(pad({button}));
      step(pad({}));
      for (int index = 0; index < 5; ++index) {
        step();
      }
    }

    auto mismatched() const noexcept -> int { return mismatched_; }

    auto partial_frames() const noexcept -> int { return partial_frames_; }

    auto first_difference() const -> const std::string& { return difference_; }

   private:
    void compare() {
      const auto seen = partial_.gpu().capture();
      const auto expected = whole_.gpu().capture();
      if (!seen.has_value() || !expected.has_value()) {
        return;
      }
      const auto left = seen->pixels();
      const auto right = expected->pixels();
      for (std::size_t at = 0; at < left.size() && at < right.size(); ++at) {
        if (left[at] != right[at]) {
          ++mismatched_;
          if (difference_.empty()) {
            const std::size_t pixel = at / 4;
            difference_ =
                std::format("frame {} pixel ({}, {})", frames_,
                            pixel % seen->width(), pixel / seen->width());
          }
          return;
        }
      }
    }

    provider::headless::HeadlessProviderFactory partial_;
    provider::headless::HeadlessProviderFactory whole_;
    std::optional<Application> partial_app_;
    std::optional<Application> whole_app_;
    int frames_{0};
    int partial_frames_{0};
    int mismatched_{0};
    std::string difference_;
  };
};

TEST_P(GalleryDamageTest, PartialRepaintMatchesAFullRepaintEveryFrame) {
  Twin twin{GetParam()};
  for (int index = 0; index < 10; ++index) {
    twin.step();
  }
  const provider::PixelPoint origin{
      .x = GetParam() == examples::GalleryPage::lists ? 237.0F : 190.0F,
      .y = 140.0F};
  twin.stroke(origin, 60.0F);
  twin.press(provider::Button::dpad_down);
  twin.press(provider::Button::dpad_right);
  twin.stroke(origin, -60.0F);
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.partial_frames(), 0);
}

TEST_F(GalleryDamageTest,
       GlassBesideTheDriftingWallpaperRepaintsOnlyWhatChanged) {
  constexpr provider::SurfaceSize wide{.width = provider::SurfaceWidth{480},
                                       .height = provider::SurfaceHeight{270}};
  Twin twin{examples::GalleryPage::materials, true, wide};
  for (int index = 0; index < 48; ++index) {
    twin.step();
  }
  EXPECT_EQ(twin.mismatched(), 0) << twin.first_difference();
  EXPECT_GT(twin.partial_frames(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryDamageTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}
