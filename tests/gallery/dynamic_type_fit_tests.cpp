#include <arisa/components/text.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/interactions.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/theme/dynamic_type.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <string>
#include <vector>

namespace arisa::runtime {

class GalleryDynamicTypeFitTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<theme::DynamicTypeSize> {
 protected:
  static constexpr float tolerance = 1.0F;

  static auto sized(theme::DynamicTypeSize size) -> AppearanceOptions {
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.dynamic_type_size = size;
    return options;
  }

  static void overflowing(const composition::Node& node, float limit,
                          bool sideways, std::vector<std::string>& found) {
    if (node.phase() != composition::NodeLifecyclePhase::live) {
      return;
    }
    const composition::Interactions& declared = node.interactions();
    const bool scrolls_sideways =
        sideways || (declared.scrolling.container.has_value() &&
                     declared.scrolling.container->axes.contains(
                         composition::Axis::horizontal));
    const auto text = node.content_as<components::Text::Content>();
    const bool focusable =
        declared.focusable.value_or(node.content().activatable());
    const composition::Rect& frame = node.frame();
    if (!scrolls_sideways && (text.has_value() || focusable) &&
        frame.width > 0.0F && frame.x + frame.width > limit + tolerance) {
      found.push_back(std::format(
          "{} '{}' ends at {:.1f} past {:.1f}", node.id().value(),
          text.has_value() ? text->get().text() : std::string{"control"},
          frame.x + frame.width, limit));
    }
    for (const composition::Node& child : node.children()) {
      overflowing(child, limit, scrolls_sideways, found);
    }
  }
};

TEST_P(GalleryDynamicTypeFitTest, EveryPageFitsAtThisTextSize) {
  for (const examples::GalleryPage page : examples::GalleryPages::all) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start_gallery(
        providers, page, theme::ColorScheme::light,
        provider::headless::ScriptedSurface::default_size, sized(GetParam()));
    run(providers, application, 8);
    const auto pane = application.root().tree().find(
        composition::ElementKey{structure::TabView::page_id});
    ASSERT_TRUE(pane.has_value());
    const auto node = application.root().tree().node(*pane);
    ASSERT_TRUE(node.has_value());
    const float window = 1280.0F;
    std::vector<std::string> outside;
    overflowing(application.root().tree().root(), window, false, outside);
    for (const std::string& each : outside) {
      ADD_FAILURE() << examples::GalleryPages::name(page) << " at "
                    << theme::DynamicType::name(GetParam())
                    << " past the window: " << each;
    }
  }
}

TEST_F(GalleryFixture, TextGrowsByTheDynamicTypeTable) {
  const auto measure = [](theme::DynamicTypeSize size) {
    provider::headless::HeadlessProviderFactory providers;
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.dynamic_type_size = size;
    Application application = start_gallery(
        providers, examples::GalleryPage::typography, theme::ColorScheme::light,
        provider::headless::ScriptedSurface::default_size, std::move(options));
    run(providers, application, 4);
    float found = 0.0F;
    walk(application.root().tree().root(), [&found](
                                               const composition::Node& node) {
      if (const auto written = node.content_as<components::Text::Content>()) {
        if (written->get().text() == "Large Title") {
          found = node.frame().height;
        }
      }
    });
    return found;
  };
  const float standard = measure(theme::DynamicTypeSize::large);
  const float huge = measure(theme::DynamicTypeSize::accessibility3);
  ASSERT_GT(standard, 0.0F);
  EXPECT_NEAR(huge / standard, 52.0F / 34.0F, 0.08F);
}

INSTANTIATE_TEST_SUITE_P(
    Sizes, GalleryDynamicTypeFitTest,
    ::testing::Values(theme::DynamicTypeSize::x_small,
                      theme::DynamicTypeSize::large,
                      theme::DynamicTypeSize::accessibility3));

}
