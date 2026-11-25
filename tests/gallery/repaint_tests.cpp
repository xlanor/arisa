#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>

#include <gallery_fixture.hpp>
#include <launch_page.hpp>
#include <theming_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryRepaintTest : public GalleryFixture {
 protected:
  static constexpr provider::SurfaceSize size{
      .width = provider::SurfaceWidth{320},
      .height = provider::SurfaceHeight{180}};

  class Pair {
   public:
    Pair(examples::GalleryPage page, std::uint32_t images, bool glass) {
      partial_.gpu().swapchain(images, true);
      whole_.gpu().swapchain(1, false);
      partial_.gpu().rasterize(size);
      whole_.gpu().rasterize(size);
      partial_app_.emplace(start_gallery(
          partial_, page, theme::ColorScheme::light, size, appearance(glass)));
      whole_app_.emplace(start_gallery(whole_, page, theme::ColorScheme::light,
                                       size, appearance(glass)));
    }

    void step(int count = 1) {
      for (int index = 0; index < count; ++index) {
        ASSERT_TRUE(partial_.clock().advance(frame_step).has_value());
        ASSERT_TRUE(whole_.clock().advance(frame_step).has_value());
        ASSERT_TRUE(partial_app_->frame().has_value());
        ASSERT_TRUE(whole_app_->frame().has_value());
        partial_frames_ += partial_.gpu().last_damage().has_value() ? 1 : 0;
        if (++frames_ % compare_every == 0) {
          compare();
        }
      }
    }

    void push(provider::Button button) {
      partial_.input().push(pad({button}));
      whole_.input().push(pad({button}));
      step();
      partial_.input().push(pad({}));
      whole_.input().push(pad({}));
      step(6);
    }

    void activate(std::string_view key) {
      ASSERT_TRUE(partial_app_->root().activate(composition::ElementKey{key}));
      ASSERT_TRUE(whole_app_->root().activate(composition::ElementKey{key}));
    }

    auto application() -> Application& { return *partial_app_; }

    auto providers() -> provider::headless::HeadlessProviderFactory& {
      return partial_;
    }

    auto capture() -> std::optional<provider::Image> {
      auto taken = partial_.gpu().capture();
      if (!taken.has_value()) {
        return std::nullopt;
      }
      return std::move(*taken);
    }

    auto mismatched() const noexcept -> int { return mismatched_; }

    auto partial_frames() const noexcept -> int { return partial_frames_; }

   private:
    static auto appearance(bool glass) -> AppearanceOptions {
      AppearanceOptions options{
          .policy = examples::GalleryScreen::policy(std::nullopt),
          .storage = {}};
      if (glass) {
        options.policy.defaults.theme = "arisa-theme-glass";
      }
      return options;
    }

    void compare() {
      const auto seen = partial_.gpu().capture();
      const auto expected = whole_.gpu().capture();
      if (!seen.has_value() || !expected.has_value()) {
        return;
      }
      if (!std::ranges::equal(seen->pixels(), expected->pixels())) {
        ++mismatched_;
      }
    }

    provider::headless::HeadlessProviderFactory partial_;
    provider::headless::HeadlessProviderFactory whole_;
    std::optional<Application> partial_app_;
    std::optional<Application> whole_app_;
    static constexpr int compare_every = 4;

    int mismatched_{0};
    int partial_frames_{0};
    int frames_{0};
  };

  static auto lit(provider::headless::HeadlessProviderFactory& providers)
      -> std::size_t {
    const auto record = providers.gpu().last_shapes();
    std::vector<bool> seen(record.vertices.size(), false);
    std::size_t found = 0;
    for (const provider::DrawCommand& command : record.commands) {
      const auto indices = std::span{record.indices}.subspan(
          command.index_offset, command.index_count);
      for (const provider::Index index : indices) {
        const std::size_t at = index + command.vertex_offset;
        if (at >= record.vertices.size() || seen[at]) {
          continue;
        }
        seen[at] = true;
        const provider::ShapeVertex& vertex = record.vertices[at];
        const bool stroked = vertex.kind == provider::ShapeKind::path ||
                             vertex.kind == provider::ShapeKind::gradient ||
                             vertex.kind == provider::ShapeKind::glass_path;
        if (stroked && vertex.local[1] > 0.0F &&
            vertex.local[1] >= std::max(vertex.shape[2], command.trim[0]) &&
            vertex.local[1] <= std::min(vertex.shape[3], command.trim[1])) {
          ++found;
        }
      }
    }
    return found;
  }

  static auto sample(const provider::Image& image, Application& application,
                     std::string_view key) -> std::optional<theme::Colour> {
    const auto id = node_of(application, key);
    if (!id.has_value()) {
      return std::nullopt;
    }
    const composition::Node& node = application.root().tree().node(*id)->get();
    if (!node.presented().has_value()) {
      return std::nullopt;
    }
    const composition::Rect bounds = node.presented()->bounds;
    const float ratio = static_cast<float>(image.width()) /
                        application.root().tree().root().frame().width;
    const auto x = static_cast<std::uint32_t>(
        std::lround((bounds.x + (bounds.width * 0.5F)) * ratio));
    const auto y = static_cast<std::uint32_t>(
        std::lround((bounds.y + (bounds.height * 0.5F)) * ratio));
    const auto pixel = image.pixel(x, y);
    if (!pixel.has_value()) {
      return std::nullopt;
    }
    return theme::Colour{static_cast<float>((*pixel)[0]) / 255.0F,
                         static_cast<float>((*pixel)[1]) / 255.0F,
                         static_cast<float>((*pixel)[2]) / 255.0F, 1.0F};
  }

  static auto distance(theme::Colour left, theme::Colour right) -> float {
    return std::max({std::abs(left.red - right.red),
                     std::abs(left.green - right.green),
                     std::abs(left.blue - right.blue)});
  }

  static void focus(Pair& pair, std::string_view key) {
    pair.push(provider::Button::dpad_right);
    for (int index = 0; index < 12 && focused_key(pair.application()) != key;
         ++index) {
      pair.push(provider::Button::dpad_up);
    }
    for (int index = 0; index < 30 && focused_key(pair.application()) != key;
         ++index) {
      pair.push(provider::Button::dpad_down);
    }
    ASSERT_EQ(focused_key(pair.application()), key);
  }
};

class GalleryReplayTest
    : public GalleryRepaintTest,
      public ::testing::WithParamInterface<std::string_view> {};

TEST_P(GalleryReplayTest, ReplayWritesOnUnderPartialDamage) {
  Pair pair{examples::GalleryPage::launch, 3, false};
  pair.step(4);
  pair.activate(GetParam());
  std::array<std::size_t, 3> coverage{};
  for (int frame = 1; frame <= 120; ++frame) {
    pair.step();
    if (frame == 20) {
      coverage[0] = lit(pair.providers());
    } else if (frame == 60) {
      coverage[1] = lit(pair.providers());
    } else if (frame == 120) {
      coverage[2] = lit(pair.providers());
    }
  }
  EXPECT_LT(coverage[0], coverage[1]) << GetParam();
  EXPECT_LT(coverage[1], coverage[2]) << GetParam();
  EXPECT_EQ(pair.mismatched(), 0) << GetParam();
  EXPECT_GT(pair.partial_frames(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    Inks, GalleryReplayTest,
    ::testing::Values(examples::LaunchPage::glass_button,
                      examples::LaunchPage::fade_button,
                      examples::LaunchPage::gradient_button),
    [](const ::testing::TestParamInfo<std::string_view>& shown) {
      std::string name{shown.param};
      std::ranges::replace(name, '-', '_');
      return name;
    });

class GalleryThemeSwitchTest
    : public GalleryRepaintTest,
      public ::testing::WithParamInterface<std::pair<std::uint32_t, bool>> {};

TEST_P(GalleryThemeSwitchTest, PressingSwitchThemeRecoloursTheApp) {
  const auto [images, glass] = GetParam();
  Pair pair{examples::GalleryPage::theming, images, glass};
  pair.step(10);
  focus(pair, examples::ThemingPage::switch_button);
  pair.step(8);
  const auto before = pair.capture();
  ASSERT_TRUE(before.has_value());
  const auto start =
      sample(*before, pair.application(), examples::ThemingPage::switch_accent);
  ASSERT_TRUE(start.has_value());
  EXPECT_TRUE(shows(pair.application(), "Default"));
  pair.push(provider::Button::a);
  pair.step(4);
  EXPECT_TRUE(shows(pair.application(), "Warm"));
  const auto middle = pair.capture();
  ASSERT_TRUE(middle.has_value());
  const auto moving =
      sample(*middle, pair.application(), examples::ThemingPage::switch_accent);
  pair.step(42);
  const auto after = pair.capture();
  ASSERT_TRUE(after.has_value());
  const auto settled =
      sample(*after, pair.application(), examples::ThemingPage::switch_accent);
  ASSERT_TRUE(moving.has_value());
  ASSERT_TRUE(settled.has_value());
  const theme::Colour orange = theme::Colour::hex(0xFF9500);
  EXPECT_LT(distance(*settled, orange), 0.04F);
  EXPECT_GT(distance(*start, orange), 0.3F);
  EXPECT_GT(distance(*moving, *start), 0.02F);
  EXPECT_GT(distance(*moving, orange), 0.02F);
  EXPECT_EQ(pair.mismatched(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    Images, GalleryThemeSwitchTest,
    ::testing::Values(std::pair<std::uint32_t, bool>{2, false},
                      std::pair<std::uint32_t, bool>{3, false}),
    [](const ::testing::TestParamInfo<std::pair<std::uint32_t, bool>>& shown) {
      return std::string{shown.param.second ? "glass_" : "default_"} +
             std::to_string(shown.param.first);
    });

TEST_F(GalleryRepaintTest, BrokenThemeErrorSitsInItsOwnCard) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::theming);
  run(providers, application, 10);
  const auto error = node_of(application, examples::ThemingPage::broken_error);
  const auto broken = node_of(application, examples::ThemingPage::broken_card);
  const auto sampled = node_of(application, examples::ThemingPage::sample_card);
  ASSERT_TRUE(error.has_value());
  ASSERT_TRUE(broken.has_value());
  ASSERT_TRUE(sampled.has_value());
  EXPECT_TRUE(within(application, *error, examples::ThemingPage::broken_card));
  EXPECT_FALSE(within(application, *error, examples::ThemingPage::sample_card));
  const composition::Tree& tree = application.root().tree();
  const auto bounds = [&tree](composition::NodeId id) {
    const composition::Node& node = tree.node(id)->get();
    return node.presented().has_value() ? node.presented()->bounds
                                        : node.frame();
  };
  const composition::Rect own = bounds(*broken);
  const composition::Rect other = bounds(*sampled);
  const composition::Rect line = bounds(*error);
  EXPECT_GE(own.y, other.y + other.height);
  EXPECT_GE(line.y, own.y);
  EXPECT_LE(line.y + line.height, own.y + own.height);
  bool titled = false;
  walk(tree.node(*broken)->get(), [&titled](const composition::Node& node) {
    if (const auto text = node.content_as<components::Text::Content>()) {
      titled =
          titled || text->get().text() == examples::ThemingPage::broken_name;
    }
  });
  EXPECT_TRUE(titled);
}

}
