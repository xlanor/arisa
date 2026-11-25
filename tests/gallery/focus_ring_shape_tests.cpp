#include <arisa/controls/picker_style.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/metrics.hpp>

#include <gallery_fixture.hpp>
#include <glass_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class GalleryFocusRingShapeTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<std::string_view> {
 protected:
  static constexpr float scale = 1.25F;

  struct Ring {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
    float radius{0.0F};
  };

  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    examples::GalleryPage page) -> Application {
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.theme = "arisa-theme-glass";
    options.policy.defaults.ui_scale = appearance::UiScales{
        .handheld = scale, .docked = scale, .desktop = scale};
    return start_gallery(providers, page, theme::ColorScheme::light,
                         provider::headless::ScriptedSurface::default_size,
                         std::move(options));
  }

  static auto bounds_of(Application& application, std::string_view key)
      -> std::optional<composition::Rect> {
    const auto id = node_of(application, key);
    if (!id.has_value()) {
      return std::nullopt;
    }
    const auto node = application.root().tree().node(*id);
    if (!node.has_value() || !node->get().presented().has_value()) {
      return std::nullopt;
    }
    return node->get().presented()->bounds;
  }

  static auto focus_on(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, std::string_view key) -> bool {
    for (int hop = 0; hop < 60; ++hop) {
      if (focused_key(application) == key) {
        return true;
      }
      const auto target = bounds_of(application, key);
      const auto here = application.interaction().focused();
      if (!target.has_value() || !here.has_value()) {
        press(providers, application, provider::Button::dpad_right);
        run(providers, application, 2);
        continue;
      }
      const auto node = application.root().tree().node(*here);
      const composition::Rect at = node->get().presented().has_value()
                                       ? node->get().presented()->bounds
                                       : node->get().frame();
      const float dx =
          (target->x + (target->width * 0.5F)) - (at.x + (at.width * 0.5F));
      const float dy =
          (target->y + (target->height * 0.5F)) - (at.y + (at.height * 0.5F));
      const bool sideways =
          node->get()
              .content_as<controls::PickerStyle::Segmented::Content>()
              .has_value();
      provider::Button step = provider::Button::dpad_right;
      if (std::abs(dy) > std::max(target->height * 0.5F, 4.0F)) {
        step =
            dy > 0.0F ? provider::Button::dpad_down : provider::Button::dpad_up;
      } else if (sideways) {
        step = provider::Button::dpad_up;
      } else {
        step = dx > 0.0F ? provider::Button::dpad_right
                         : provider::Button::dpad_left;
      }
      press(providers, application, step);
      run(providers, application, 2);
    }
    return focused_key(application) == key;
  }

  static auto ring_around(
      provider::headless::HeadlessProviderFactory& providers,
      const composition::Rect& around) -> std::optional<Ring> {
    const auto record = providers.gpu().last_shapes();
    std::optional<Ring> best;
    for (std::size_t base = 0; base + 3 < record.vertices.size(); base += 4) {
      const provider::ShapeVertex& corner = record.vertices[base];
      if (corner.kind != provider::ShapeKind::stroke ||
          (corner.colour >> 24U) < 160U) {
        continue;
      }
      const float width = corner.half_size[0] * 2.0F;
      const float height = corner.half_size[1] * 2.0F;
      const provider::ShapeVertex& opposite = record.vertices[base + 2];
      const float cx = (corner.position[0] + opposite.position[0]) * 0.5F;
      const float cy = (corner.position[1] + opposite.position[1]) * 0.5F;
      const bool centred =
          std::abs(cx - (around.x + (around.width * 0.5F))) <
              theme::metrics::focus_ring.outset * scale &&
          std::abs(cy - (around.y + (around.height * 0.5F))) < 2.0F;
      if (!centred || width < around.width + 1.0F ||
          height < around.height + 1.0F || width > around.width + 16.0F ||
          height > around.height + 16.0F) {
        continue;
      }
      if (!best.has_value() || width < best->width) {
        best = Ring{.x = cx - (width * 0.5F),
                    .y = cy - (height * 0.5F),
                    .width = width,
                    .height = height,
                    .radius = corner.shape[provider::ShapeVertex::radius]};
      }
    }
    return best;
  }

  static auto expect_capsule(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, std::string_view key) -> std::optional<Ring> {
    const auto shown = bounds_of(application, key);
    EXPECT_TRUE(shown.has_value()) << key;
    if (!shown.has_value()) {
      return std::nullopt;
    }
    const auto ring = ring_around(
        providers, composition::Rect{.x = shown->x * scale,
                                     .y = shown->y * scale,
                                     .width = shown->width * scale,
                                     .height = shown->height * scale});
    EXPECT_TRUE(ring.has_value()) << key;
    if (!ring.has_value()) {
      return std::nullopt;
    }
    EXPECT_NEAR(ring->radius, std::min(ring->width, ring->height) * 0.5F, 0.75F)
        << key << " ring " << ring->width << "x" << ring->height << " radius "
        << ring->radius;
    return ring;
  }
};

TEST_P(GalleryFocusRingShapeTest, GlassControlRingFollowsTheCapsule) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, examples::GalleryPage::glass);
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 10);
  ASSERT_TRUE(focus_on(providers, application, GetParam()))
      << "focus stopped on " << focused_key(application);
  run(providers, application, 30);
  expect_capsule(providers, application, GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    Controls, GalleryFocusRingShapeTest,
    ::testing::Values(examples::GlassPreview::shape_views.front(),
                      examples::GlassPage::regular_view,
                      examples::GlassPage::clear_view,
                      examples::GlassPage::interactive_view));

class GalleryHeaderRingTest : public GalleryFocusRingShapeTest {};

TEST_F(GalleryHeaderRingTest, HeaderItemRingFollowsTheCapsule) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, examples::GalleryPage::typography);
  run(providers, application, 20);
  ASSERT_TRUE(
      focus_on(providers, application, examples::GalleryScreen::scheme_button))
      << "focus stopped on " << focused_key(application);
  run(providers, application, 30);
  const auto ring = expect_capsule(providers, application,
                                   examples::GalleryScreen::scheme_button);
  const auto group =
      bounds_of(application, structure::Scaffold::trailing_group_key);
  ASSERT_TRUE(ring.has_value());
  ASSERT_TRUE(group.has_value());
  constexpr float edge = 0.5F;
  EXPECT_GE(ring->x, (group->x * scale) - edge);
  EXPECT_GE(ring->y, (group->y * scale) - edge);
  EXPECT_LE(ring->x + ring->width, ((group->x + group->width) * scale) + edge);
  EXPECT_LE(ring->y + ring->height,
            ((group->y + group->height) * scale) + edge);
}

}
