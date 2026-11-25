#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/glass_effect.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>

#include <gallery_fixture.hpp>
#include <glass_page.hpp>
#include <glass_preview.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::runtime {

class GalleryGlassPlaygroundTest : public GalleryFixture {
 protected:
  using Preview = examples::GlassPreview;
  using Page = examples::GlassPage;

  static constexpr int settle_frames = 120;

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    Application application = start_gallery(
        providers, examples::GalleryPage::glass, theme::ColorScheme::light,
        provider::headless::ScriptedSurface::default_size, std::nullopt, 1.0F);
    run(providers, application, settle_frames);
    return application;
  }

  static auto effect(Application& application, std::size_t index)
      -> std::optional<composition::GlassEffect> {
    const auto id = node_of(application, Preview::shape_views.at(index));
    if (!id.has_value()) {
      return std::nullopt;
    }
    return application.root().tree().node(*id)->get().modifiers().glass;
  }

  static auto label(Application& application, std::string_view text)
      -> std::optional<composition::Rect> {
    const auto panel = node_of(application, Page::panel_view);
    if (!panel.has_value()) {
      return std::nullopt;
    }
    std::optional<composition::Rect> found;
    walk(application.root().tree().node(*panel)->get(),
         [&](const composition::Node& node) {
           const auto written = node.content_as<components::Text::Content>();
           if (!found.has_value() && written.has_value() &&
               written->get().text() == text && node.presented().has_value()) {
             found = node.presented()->bounds;
           }
         });
    return found;
  }

  static auto centre_of(const composition::Rect& rect) -> provider::PixelPoint {
    return provider::PixelPoint{.x = rect.x + (rect.width * 0.5F),
                                .y = rect.y + (rect.height * 0.5F)};
  }

  struct Change {
    bool presented{false};
    std::uint64_t damaged{0};
  };

  static auto rest(provider::headless::HeadlessProviderFactory& providers,
                   Application& application) -> Change {
    Change change;
    for (int index = 0; index < 30; ++index) {
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      EXPECT_TRUE(application.frame().has_value());
      change.presented = change.presented || application.last_frame().presented;
      change.damaged += application.last_frame().damaged_pixels;
    }
    return change;
  }

  static auto choose(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, std::string_view text)
      -> Change {
    const auto at = label(application, text);
    EXPECT_TRUE(at.has_value()) << text;
    if (!at.has_value()) {
      return {};
    }
    tap(providers, application, centre_of(*at));
    return rest(providers, application);
  }

  static auto activate(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, std::string_view key)
      -> Change {
    EXPECT_TRUE(application.root().activate(composition::ElementKey{key}))
        << key;
    return rest(providers, application);
  }

  static void expect_idle(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) {
    for (int index = 0; index < 20; ++index) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application.frame().has_value());
      EXPECT_FALSE(application.last_frame().presented) << "frame " << index;
      EXPECT_EQ(application.last_frame().damaged_pixels, 0U);
      EXPECT_EQ(application.last_frame().blurs, 0U);
    }
  }

  static auto focus_shape(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) -> bool {
    for (int hop = 0; hop < 40; ++hop) {
      if (focused_key(application) == Preview::shape_views.front()) {
        return true;
      }
      const auto here = application.interaction().focused();
      const bool sideways =
          here.has_value() &&
          application.root()
              .tree()
              .node(*here)
              ->get()
              .content_as<controls::PickerStyle::Segmented::Content>()
              .has_value();
      press(
          providers, application,
          sideways ? provider::Button::dpad_up : provider::Button::dpad_right);
      run(providers, application, 2);
    }
    return focused_key(application) == Preview::shape_views.front();
  }
};

TEST_F(GalleryGlassPlaygroundTest, EachVariantChangesThePreview) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  ASSERT_TRUE(effect(application, 0).has_value());
  EXPECT_EQ(effect(application, 0)->glass->variant(),
            theme::Glass::Variant::regular);
  for (const auto& [text, variant] :
       {std::pair{std::string_view{"Clear"}, theme::Glass::Variant::clear},
        std::pair{std::string_view{"Identity"},
                  theme::Glass::Variant::identity},
        std::pair{std::string_view{"Regular"},
                  theme::Glass::Variant::regular}}) {
    const Change change = choose(providers, application, text);
    EXPECT_TRUE(change.presented) << text;
    EXPECT_GT(change.damaged, 0U) << text;
    ASSERT_TRUE(effect(application, 0).has_value()) << text;
    EXPECT_EQ(effect(application, 0)->glass->variant(), variant) << text;
    expect_idle(providers, application);
  }
}

TEST_F(GalleryGlassPlaygroundTest, TintAndShapeChangeThePreview) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const Change tinted =
      activate(providers, application, Page::swatch_views.at(6));
  EXPECT_TRUE(tinted.presented);
  ASSERT_TRUE(effect(application, 0).has_value());
  const std::optional<theme::ColourValue> tint =
      effect(application, 0)->glass->tint_colour();
  ASSERT_TRUE(tint.has_value());
  ASSERT_NE(tint->role(), nullptr);
  EXPECT_EQ(*tint->role(), theme::ColourRole::accent);
  EXPECT_NEAR(tint->opacity(), 0.3F, 0.001F);
  expect_idle(providers, application);

  const composition::Rect capsule =
      presented_of(application, Preview::shape_views.front());
  EXPECT_GT(capsule.width, capsule.height * 1.5F);
  const Change grown = choose(providers, application, "Large");
  EXPECT_TRUE(grown.presented);
  const composition::Rect large =
      presented_of(application, Preview::shape_views.front());
  EXPECT_GT(large.width, capsule.width);
  expect_idle(providers, application);
}

TEST_F(GalleryGlassPlaygroundTest, CircleShapeIsRound) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const auto row = node_of(application, Page::panel_view);
  ASSERT_TRUE(row.has_value());
  std::optional<composition::Rect> circle;
  walk(application.root().tree().node(*row)->get(),
       [&](const composition::Node& node) {
         const composition::Rect& frame = node.frame();
         if (!circle.has_value() && node.presented().has_value() &&
             std::abs(frame.width - 13.0F) < 0.5F &&
             std::abs(frame.height - 13.0F) < 0.5F) {
           circle = node.presented()->bounds;
         }
       });
  ASSERT_TRUE(circle.has_value());
  tap(providers, application, centre_of(*circle));
  const Change change = rest(providers, application);
  EXPECT_TRUE(change.presented);
  const composition::Rect shown =
      presented_of(application, Preview::shape_views.front());
  EXPECT_NEAR(shown.width, shown.height, 0.5F);
  expect_idle(providers, application);
}

TEST_F(GalleryGlassPlaygroundTest, ContainerWithThreeShapesBlendsInOneGroup) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_FALSE(node_of(application, Preview::shape_views.at(1)).has_value());
  EXPECT_TRUE(choose(providers, application, "On").presented);
  EXPECT_TRUE(choose(providers, application, "Two").presented);
  EXPECT_TRUE(node_of(application, Preview::shape_views.at(1)).has_value());
  EXPECT_FALSE(node_of(application, Preview::shape_views.at(2)).has_value());
  expect_idle(providers, application);
  EXPECT_TRUE(choose(providers, application, "Three").presented);
  for (const std::string_view key : Preview::shape_views) {
    const auto id = node_of(application, key);
    ASSERT_TRUE(id.has_value()) << key;
    bool contained = false;
    for (std::optional<composition::NodeId> at = id; at.has_value();) {
      const auto node = application.root().tree().node(*at);
      if (node->get()
              .content_as<components::GlassEffectContainer::Content>()
              .has_value()) {
        contained = true;
        break;
      }
      at = node->get().parent();
    }
    EXPECT_TRUE(contained) << key;
  }
  expect_idle(providers, application);
  const Change united = choose(providers, application, "Union");
  EXPECT_TRUE(united.presented);
  ASSERT_TRUE(effect(application, 2).has_value());
  EXPECT_TRUE(effect(application, 2)->unioned.has_value());
  expect_idle(providers, application);
}

TEST_F(GalleryGlassPlaygroundTest, DraggingRepaintsOnlyThePreview) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Rect stage =
      presented_of(application, Preview::stage_view);
  const composition::Rect before =
      presented_of(application, Preview::preview_view);
  const provider::PixelPoint from = centre_of(before);
  touch_down(providers, application, from);
  std::uint64_t most = 0;
  for (int index = 1; index <= 12; ++index) {
    const float progress = static_cast<float>(index) / 12.0F;
    touch_move(providers, application,
               provider::PixelPoint{.x = from.x + (90.0F * progress),
                                    .y = from.y + (40.0F * progress)});
    most = std::max(most, application.last_frame().damaged_pixels);
  }
  touch_up(providers, application,
           provider::PixelPoint{.x = from.x + 90.0F, .y = from.y + 40.0F});
  rest(providers, application);
  const composition::Rect after =
      presented_of(application, Preview::preview_view);
  EXPECT_NEAR(after.x - before.x, 90.0F, 1.0F);
  EXPECT_NEAR(after.y - before.y, 40.0F, 1.0F);
  EXPECT_GT(most, 0U);
  EXPECT_LE(static_cast<double>(most), static_cast<double>(stage.width) *
                                           static_cast<double>(stage.height));
  expect_idle(providers, application);
}

TEST_F(GalleryGlassPlaygroundTest, DpadNudgesThePreviewOnlyWhilePickedUp) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 10);
  ASSERT_TRUE(focus_shape(providers, application))
      << "focus stopped on " << focused_key(application);
  run(providers, application, 10);
  const composition::Rect before =
      presented_of(application, Preview::preview_view);
  press(providers, application, provider::Button::a);
  run(providers, application, 4);
  press(providers, application, provider::Button::dpad_right);
  press(providers, application, provider::Button::dpad_down);
  rest(providers, application);
  const composition::Rect moved =
      presented_of(application, Preview::preview_view);
  EXPECT_NEAR(moved.x - before.x, Preview::nudge, 0.5F);
  EXPECT_NEAR(moved.y - before.y, Preview::nudge, 0.5F);
  EXPECT_EQ(focused_key(application), Preview::shape_views.front());
  expect_idle(providers, application);
  press(providers, application, provider::Button::a);
  run(providers, application, 4);
  press(providers, application, provider::Button::dpad_left);
  rest(providers, application);
  const composition::Rect placed =
      presented_of(application, Preview::preview_view);
  EXPECT_NEAR(placed.x, moved.x, 0.5F);
  EXPECT_NE(focused_key(application), Preview::shape_views.front());
}

}
