#include <arisa/components/button.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

namespace arisa::runtime {

class GalleryFocusTraversalTest : public GalleryFixture {
 protected:
  static auto describe(Application& application, composition::NodeId id)
      -> std::string {
    const auto node = application.root().tree().node(id);
    if (!node.has_value()) {
      return "gone";
    }
    std::string text;
    walk(node->get(), [&text](const composition::Node& inner) {
      if (const auto written = inner.content_as<components::Text::Content>()) {
        if (text.empty()) {
          text = written->get().text();
        }
      }
    });
    const composition::Rect& frame = node->get().frame();
    return std::format("{} '{}' at {:.0f},{:.0f}", id.value(), text, frame.x,
                       frame.y);
  }

  static void expect_reachable(examples::GalleryPage page) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start_gallery(providers, page);
    run(providers, application, 10);
    press(providers, application, provider::Button::dpad_right);
    run(providers, application, 20);
    const std::vector<composition::NodeId> initial = focusables(application);
    const std::vector<composition::NodeId> reached =
        traverse(providers, application).reached;
    ASSERT_FALSE(reached.empty());
    for (const composition::NodeId id : initial) {
      if (!in_page(application, id) || lazy(application, id)) {
        continue;
      }
      EXPECT_TRUE(std::ranges::contains(reached, id))
          << describe(application, id) << " is never reached by the D-pad";
    }
  }
};

TEST_F(GalleryFocusTraversalTest, DownFromTheSegmentedPickerOpensOntoFlavour) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  run(providers, application, 10);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 20);
  const Traversal traversal = traverse(providers, application);
  std::optional<composition::NodeId> segmented;
  std::optional<composition::NodeId> flavour;
  walk(application.root().tree().root(), [&](const composition::Node& node) {
    if (!segmented.has_value() &&
        node.content_as<controls::PickerStyle::Segmented::Content>()
            .has_value()) {
      segmented = node.id();
    }
    if (!flavour.has_value() &&
        node.content_as<controls::Menu::Content>().has_value() &&
        node.frame().y > 0.0F) {
      walk(node, [&](const composition::Node& inner) {
        if (!flavour.has_value() &&
            inner.content_as<components::Button::Content>().has_value()) {
          flavour = inner.id();
        }
      });
    }
  });
  ASSERT_TRUE(segmented.has_value());
  ASSERT_TRUE(flavour.has_value());
  ASSERT_TRUE(travel(providers, application, traversal.edges, *segmented));
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 10);
  EXPECT_EQ(application.interaction().focused(), flavour);
}

class GalleryPageTraversalTest
    : public GalleryFocusTraversalTest,
      public ::testing::WithParamInterface<examples::GalleryPage> {};

TEST_P(GalleryPageTraversalTest, DpadReachesEveryControl) {
  expect_reachable(GetParam());
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryPageTraversalTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}
