#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/tab_view.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

namespace arisa::runtime {

class GallerySidebarTest : public GalleryFixture {
 protected:
  static auto row(Application& application, std::size_t index)
      -> std::optional<composition::NodeId> {
    return application.root().tree().find(structure::TabView::item_key(index));
  }

  static auto bounds(Application& application, composition::NodeId id)
      -> composition::Rect {
    const auto node = application.root().tree().node(id);
    EXPECT_TRUE(node.has_value() && node->get().presented().has_value());
    return node->get().presented()->bounds;
  }

  static void expect_selected(Application& application, std::size_t index,
                              std::string_view title) {
    const auto shown = row(application, index);
    ASSERT_TRUE(shown.has_value());
    EXPECT_EQ(application.interaction().focused(), shown);
    const auto capsule = application.root().tree().find(
        composition::ElementKey{structure::TabView::capsule_id});
    ASSERT_TRUE(capsule.has_value());
    const composition::Rect ring = bounds(application, *shown);
    const composition::Rect fill = bounds(application, *capsule);
    EXPECT_NEAR(ring.y, fill.y, 1.0F);
    EXPECT_NEAR(ring.height, fill.height, 1.0F);
    int seen = 0;
    for (const std::string& text : texts(application)) {
      seen += text == title ? 1 : 0;
    }
    EXPECT_EQ(seen, 2) << title << " is not the shown page";
  }
};

TEST_F(GallerySidebarTest, DpadInTheSidebarSelectsThePaneUnderFocus) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::typography);
  run(providers, application, 30);
  expect_selected(application, 0, "Typography");
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 60);
  expect_selected(application, 1, "Languages");
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 60);
  expect_selected(application, 2, "Cards");
}

TEST_F(GallerySidebarTest, ShoulderButtonsKeepSidebarFocusOnTheSelectedRow) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::typography);
  run(providers, application, 30);
  press(providers, application, provider::Button::r);
  run(providers, application, 60);
  expect_selected(application, 1, "Languages");
  press(providers, application, provider::Button::r);
  run(providers, application, 60);
  expect_selected(application, 2, "Cards");
  press(providers, application, provider::Button::l);
  run(providers, application, 60);
  expect_selected(application, 1, "Languages");
}

TEST_F(GallerySidebarTest, RightEntersTheContentAndLeftReturnsToTheSelection) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  run(providers, application, 30);
  const auto controls = row(application, 12);
  ASSERT_TRUE(controls.has_value());
  EXPECT_EQ(application.interaction().focused(), controls);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 30);
  const auto inside = application.interaction().focused();
  ASSERT_TRUE(inside.has_value());
  EXPECT_TRUE(in_page(application, *inside));
  press(providers, application, provider::Button::r);
  run(providers, application, 60);
  EXPECT_TRUE(application.interaction().focused().has_value());
  press(providers, application, provider::Button::dpad_left);
  run(providers, application, 30);
  for (int index = 0;
       index < 12 && application.interaction().focused() &&
       in_page(application, *application.interaction().focused());
       ++index) {
    const auto before = application.interaction().focused();
    press(providers, application, provider::Button::dpad_left);
    run(providers, application, 30);
    if (application.interaction().focused() == before) {
      press(providers, application, provider::Button::dpad_up);
      run(providers, application, 10);
    }
  }
  expect_selected(application, 13, "Lists");
}

}
