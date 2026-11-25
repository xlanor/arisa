#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>

namespace arisa::runtime {

class GalleryFloatingSidebarFocusTest : public GalleryFixture {
 protected:
  static auto start(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.theme = "arisa-theme-glass";
    options.policy.defaults.ui_scale = appearance::UiScales{
        .handheld = 1.25F, .docked = 1.25F, .desktop = 1.25F};
    return start_gallery(
        providers, examples::GalleryPage::glass, theme::ColorScheme::light,
        provider::headless::ScriptedSurface::default_size, std::move(options));
  }

  static auto in_sidebar(Application& application) -> bool {
    const auto focused = application.interaction().focused();
    return focused.has_value() &&
           within(application, *focused, "arisa.tab.sidebar");
  }
};

TEST_F(GalleryFloatingSidebarFocusTest, LeftFromTheSidebarStaysInTheSidebar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers);
  run(providers, application, 20);
  ASSERT_TRUE(in_sidebar(application)) << focused_key(application);
  const std::string before = focused_key(application);
  press(providers, application, provider::Button::dpad_left);
  run(providers, application, 4);
  EXPECT_EQ(focused_key(application), before);
}

TEST_F(GalleryFloatingSidebarFocusTest, UpFromTheToggleNeverMovesDown) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers);
  run(providers, application, 20);
  for (int hop = 0; hop < 40 && focused_key(application) !=
                                    structure::Scaffold::sidebar_toggle_key;
       ++hop) {
    press(providers, application, provider::Button::dpad_up);
    run(providers, application, 2);
  }
  ASSERT_EQ(focused_key(application), structure::Scaffold::sidebar_toggle_key);
  const auto toggle = application.interaction().focused();
  const float top =
      presented_of(application, structure::Scaffold::sidebar_toggle_key).y;
  press(providers, application, provider::Button::dpad_up);
  run(providers, application, 4);
  const auto now = application.interaction().focused();
  ASSERT_TRUE(now.has_value());
  if (*now != *toggle) {
    const auto node = application.root().tree().node(*now);
    ASSERT_TRUE(node.has_value() && node->get().presented().has_value());
    EXPECT_LE(node->get().presented()->bounds.y, top + 1.0F)
        << "up moved focus down to " << focused_key(application);
  }
}

}
