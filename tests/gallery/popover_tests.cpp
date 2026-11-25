#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <iterator>
#include <optional>

namespace arisa::runtime {

class GalleryPopoverTest : public GalleryFixture {
 protected:
  static auto actions(Application& application)
      -> std::optional<composition::NodeId> {
    std::optional<composition::NodeId> found;
    walk(application.root().tree().root(), [&](const composition::Node& node) {
      if (found.has_value() ||
          !node.content_as<controls::Menu::Content>().has_value()) {
        return;
      }
      bool titled = false;
      walk(node, [&titled](const composition::Node& inner) {
        const auto text = inner.content_as<components::Text::Content>();
        titled =
            titled || (text.has_value() && text->get().text() == "Actions");
      });
      if (titled) {
        walk(node, [&found](const composition::Node& inner) {
          if (!found.has_value() &&
              inner.content_as<components::Button::Content>().has_value()) {
            found = inner.id();
          }
        });
      }
    });
    return found;
  }

  static auto panel(Application& application)
      -> std::optional<composition::Rect> {
    std::optional<composition::Rect> found;
    walk(application.root().tree().root(), [&found](
                                               const composition::Node& node) {
      const auto layer = node.content_as<composition::Layer::Content>();
      if (found.has_value() || !layer.has_value() ||
          !layer->get().anchoring().has_value() || node.child_count() < 2) {
        return;
      }
      const composition::Node& shown = *std::next(node.children().begin());
      if (shown.presented().has_value()) {
        found = shown.presented()->bounds;
      }
    });
    return found;
  }

  static auto inside(const composition::Rect& inner,
                     const composition::Rect& outer) -> bool {
    constexpr float slack = 1.0F;
    return inner.x >= outer.x - slack && inner.y >= outer.y - slack &&
           inner.x + inner.width <= outer.x + outer.width + slack &&
           inner.y + inner.height <= outer.y + outer.height + slack;
  }
};

TEST_F(GalleryPopoverTest, MenuUnderTheHintBarScrollsInAndStaysClearOfTheBars) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  const auto anchor = actions(application);
  ASSERT_TRUE(anchor.has_value());
  const auto shown = [&] {
    return application.root().tree().node(*anchor)->get().presented();
  };
  for (int attempt = 0; attempt < 40; ++attempt) {
    const auto frame = shown();
    ASSERT_TRUE(frame.has_value() && frame->clip.has_value());
    const float bottom = frame->clip->y + frame->clip->height;
    if (frame->bounds.y < bottom &&
        frame->bounds.y + frame->bounds.height > bottom) {
      break;
    }
    scroll_by(providers, application,
              provider::PixelPoint{.x = 760.0F, .y = 600.0F},
              frame->bounds.y > bottom ? 25.0F : -25.0F);
  }
  const auto before = shown();
  ASSERT_TRUE(before.has_value() && before->clip.has_value());
  ASSERT_GT(before->bounds.y + before->bounds.height,
            before->clip->y + before->clip->height);
  ASSERT_TRUE(application.root().activate(*anchor));
  run(providers, application, 90);
  const auto after = shown();
  ASSERT_TRUE(after.has_value() && after->clip.has_value());
  EXPECT_TRUE(inside(after->bounds, *after->clip));
  const auto menu = panel(application);
  ASSERT_TRUE(menu.has_value());
  EXPECT_TRUE(inside(*menu, *after->clip));
  EXPECT_FALSE(menu->y < after->bounds.y + after->bounds.height &&
               menu->y + menu->height > after->bounds.y);
}

}
