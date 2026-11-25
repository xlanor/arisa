#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace arisa::runtime {

using controls::ProgressViewStyle;

class GalleryProgressTest : public GalleryFixture {
 protected:
  static auto indicator(Application& application,
                        ProgressViewStyle::Indeterminate::Motion motion)
      -> std::optional<composition::NodeId> {
    std::optional<composition::NodeId> found;
    walk(application.root().tree().root(),
         [&found, motion](const composition::Node& node) {
           const auto content =
               node.content_as<ProgressViewStyle::Indeterminate::Content>();
           if (!found.has_value() && content.has_value() &&
               content->get().motion() == motion) {
             found = node.id();
           }
         });
    return found;
  }

  static auto labelled(Application& application, std::string_view label)
      -> std::optional<composition::NodeId> {
    std::optional<composition::NodeId> found;
    walk(application.root().tree().root(),
         [&found, label](const composition::Node& node) {
           if (found.has_value() ||
               !node.content_as<components::Button::Content>().has_value()) {
             return;
           }
           walk(node, [&found, &node, label](const composition::Node& inner) {
             const auto text = inner.content_as<components::Text::Content>();
             if (!found.has_value() && text.has_value() &&
                 text->get().text() == label) {
               found = node.id();
             }
           });
         });
    return found;
  }

  static auto snapshot(Application& application, composition::NodeId id)
      -> std::vector<composition::PresentedFrame> {
    const auto node = application.root().tree().node(id);
    EXPECT_TRUE(node.has_value());
    return node.has_value() ? presented(node->get())
                            : std::vector<composition::PresentedFrame>{};
  }

  static void reveal(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, composition::NodeId id) {
    for (int attempt = 0; attempt < 16; ++attempt) {
      const auto node = application.root().tree().node(id);
      ASSERT_TRUE(node.has_value());
      if (visible(node->get(), 1280.0F, 720.0F)) {
        const composition::Rect& shown = node->get().presented()->bounds;
        const composition::Rect area = *node->get().presented()->clip;
        if (shown.y + shown.height < area.y + area.height - 40.0F) {
          return;
        }
      }
      scroll_by(providers, application,
                provider::PixelPoint{.x = 760.0F, .y = 620.0F}, 160.0F);
    }
  }

  static void expect_segment_sweeps(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, composition::NodeId id) {
    const auto track = application.root().tree().node(id);
    ASSERT_TRUE(track.has_value());
    std::optional<composition::NodeId> segment;
    walk(track->get(), [&segment](const composition::Node& node) {
      if (!segment.has_value() && node.modifiers().effects.offset_x != 0.0F) {
        segment = node.id();
      }
    });
    ASSERT_TRUE(segment.has_value());
    int inside = 0;
    std::vector<float> positions;
    for (int sample = 0; sample < 12; ++sample) {
      run(providers, application, 5);
      const auto shown = application.root().tree().node(*segment);
      const auto bar = application.root().tree().node(id);
      ASSERT_TRUE(shown.has_value() && bar.has_value());
      ASSERT_TRUE(shown->get().presented().has_value());
      ASSERT_TRUE(bar->get().presented().has_value());
      const composition::Rect& piece = shown->get().presented()->bounds;
      const composition::Rect& rail = bar->get().presented()->bounds;
      positions.push_back(piece.x);
      if (piece.width > 1.0F && piece.x + piece.width > rail.x &&
          piece.x < rail.x + rail.width) {
        ++inside;
      }
    }
    EXPECT_GT(inside, 3);
    EXPECT_GT(std::ranges::max(positions) - std::ranges::min(positions), 20.0F);
  }

  static void expect_moving(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, composition::NodeId id) {
    const auto node = application.root().tree().node(id);
    ASSERT_TRUE(node.has_value());
    ASSERT_TRUE(visible(node->get(), 1280.0F, 720.0F));
    const auto before = snapshot(application, id);
    run(providers, application, 9);
    const auto middle = snapshot(application, id);
    run(providers, application, 9);
    const auto after = snapshot(application, id);
    EXPECT_TRUE(before != middle);
    EXPECT_TRUE(middle != after);
  }
};

TEST_F(GalleryProgressTest, IndicatorsBelowTheFoldAnimateOnceScrolledIn) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  run(providers, application, 60);
  const auto sweep =
      indicator(application, ProgressViewStyle::Indeterminate::Motion::sweep);
  const auto spin =
      indicator(application, ProgressViewStyle::Indeterminate::Motion::spin);
  ASSERT_TRUE(sweep.has_value());
  ASSERT_TRUE(spin.has_value());
  reveal(providers, application, *sweep);
  expect_moving(providers, application, *sweep);
  expect_moving(providers, application, *spin);
}

TEST_F(GalleryProgressTest, BarPartlyScrolledIntoViewSweeps) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  run(providers, application, 30);
  const auto sweep =
      indicator(application, ProgressViewStyle::Indeterminate::Motion::sweep);
  ASSERT_TRUE(sweep.has_value());
  float distance = 60.0F;
  while (distance < 1800.0F) {
    scroll_by(providers, application,
              provider::PixelPoint{.x = 760.0F, .y = 620.0F}, 60.0F);
    const auto node = application.root().tree().node(*sweep);
    ASSERT_TRUE(node.has_value());
    if (visible(node->get(), 1280.0F, 720.0F)) {
      break;
    }
    distance += 60.0F;
  }
  expect_moving(providers, application, *sweep);
}

TEST_F(GalleryProgressTest, IndicatorsReachedByFocusKeepAnimating) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::typography);
  run(providers, application, 30);
  for (int row = 0;
       row < 20 &&
       !indicator(application, ProgressViewStyle::Indeterminate::Motion::spin)
            .has_value();
       ++row) {
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 12);
  }
  run(providers, application, 30);
  const auto sweep =
      indicator(application, ProgressViewStyle::Indeterminate::Motion::sweep);
  const auto spin =
      indicator(application, ProgressViewStyle::Indeterminate::Motion::spin);
  ASSERT_TRUE(sweep.has_value());
  ASSERT_TRUE(spin.has_value());
  press(providers, application, provider::Button::dpad_right);
  for (int hop = 0; hop < 24; ++hop) {
    const auto node = application.root().tree().node(*spin);
    const auto swept = application.root().tree().node(*sweep);
    ASSERT_TRUE(node.has_value() && swept.has_value());
    if (visible(node->get(), 1280.0F, 720.0F) &&
        visible(swept->get(), 1280.0F, 720.0F)) {
      break;
    }
    press(providers, application, provider::Button::dpad_down);
    run(providers, application, 20);
  }
  run(providers, application, 40);
  expect_moving(providers, application, *sweep);
  expect_moving(providers, application, *spin);
  expect_segment_sweeps(providers, application, *sweep);
}

TEST_F(GalleryProgressTest, DeterminateBarGrowsSmoothlyOnAdvance) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  const auto advance = labelled(application, "Advance");
  ASSERT_TRUE(advance.has_value());
  reveal(providers, application, *advance);
  std::optional<composition::NodeId> bar;
  walk(application.root().tree().root(), [&bar](const composition::Node& node) {
    const auto content = node.content_as<controls::ProgressView::Content>();
    if (!bar.has_value() && content.has_value() &&
        content->get().fraction_completed().has_value() &&
        node.frame().width > 100.0F) {
      bar = node.id();
    }
  });
  ASSERT_TRUE(bar.has_value());
  const auto settled = snapshot(application, *bar);
  ASSERT_TRUE(application.root().activate(*advance));
  run(providers, application, 4);
  const auto moving = snapshot(application, *bar);
  run(providers, application, 4);
  const auto later = snapshot(application, *bar);
  run(providers, application, 60);
  const auto done = snapshot(application, *bar);
  run(providers, application, 10);
  EXPECT_NE(settled, moving);
  EXPECT_NE(moving, later);
  EXPECT_NE(later, done);
  EXPECT_EQ(done, snapshot(application, *bar));
}

}
