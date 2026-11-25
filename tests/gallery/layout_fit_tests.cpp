#include <arisa/components/text.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/interactions.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/tab_view.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <string>
#include <vector>

namespace arisa::runtime {

struct FitCase {
  provider::SurfaceSize size;
  std::optional<float> scale;
};

class GalleryLayoutFitTest : public GalleryFixture,
                             public ::testing::WithParamInterface<FitCase> {
 protected:
  static constexpr float page_padding = 24.0F;
  static constexpr float tolerance = 1.0F;

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

TEST_P(GalleryLayoutFitTest, EveryPageFitsTheContentPane) {
  for (const examples::GalleryPage page : examples::GalleryPages::all) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start_gallery(providers, page, theme::ColorScheme::light,
                      GetParam().size, std::nullopt, GetParam().scale);
    run(providers, application, 8);
    const auto pane = application.root().tree().find(
        composition::ElementKey{structure::TabView::page_id});
    ASSERT_TRUE(pane.has_value());
    const auto node = application.root().tree().node(*pane);
    ASSERT_TRUE(node.has_value());
    const composition::Rect& frame = node->get().frame();
    const float window =
        static_cast<float>(GetParam().size.width.value()) /
        GetParam().scale.value_or(
            provider::SurfaceScale::for_pixels(GetParam().size).value());
    EXPECT_NEAR(frame.x + frame.width, window, tolerance)
        << examples::GalleryPages::name(page);
    std::vector<std::string> found;
    overflowing(node->get(), frame.x + frame.width - page_padding, false,
                found);
    for (const std::string& each : found) {
      ADD_FAILURE() << examples::GalleryPages::name(page) << ": " << each;
    }
    std::vector<std::string> outside;
    overflowing(application.root().tree().root(), window, false, outside);
    for (const std::string& each : outside) {
      ADD_FAILURE() << examples::GalleryPages::name(page)
                    << " past the window: " << each;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    Sizes, GalleryLayoutFitTest,
    ::testing::Values(FitCase{.size = {.width = provider::SurfaceWidth{1280},
                                       .height = provider::SurfaceHeight{720}},
                              .scale = std::nullopt},
                      FitCase{.size = {.width = provider::SurfaceWidth{1920},
                                       .height = provider::SurfaceHeight{1080}},
                              .scale = std::nullopt},
                      FitCase{.size = {.width = provider::SurfaceWidth{1280},
                                       .height = provider::SurfaceHeight{720}},
                              .scale = 1.25F}));

}
