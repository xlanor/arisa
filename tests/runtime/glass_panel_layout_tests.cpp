#include "focus_fixture.hpp"

#include <arisa/components/glass_panel.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/panel_radius.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>

#include <gtest/gtest.h>

#include <format>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GlassPanelLayoutTest : public FocusFixture {
 protected:
  static auto size_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame() : composition::Rect{};
  }

  static auto scroller(const composition::Node& node)
      -> std::optional<composition::Rect> {
    if (node.content_as<components::ScrollView::Content>().has_value()) {
      return node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (auto found = scroller(child)) {
        return found;
      }
    }
    return std::nullopt;
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(GlassPanelLayoutTest, AGlassPanelAroundAFlexibleFrameFillsItsParent) {
  Application application = start(
      providers_,
      components::HStack(
          components::VStack(components::Text("Rail"), components::Spacer())
              .frame({.width = 100.0F, .max_height = composition::infinity})
              .glass_panel(components::PanelRadius::large)
              .id("rail"),
          components::Spacer())
          .frame({.width = 600.0F, .height = 400.0F}));
  const composition::Rect rail = size_of(application, "rail");
  EXPECT_NEAR(rail.width, 100.0F, 0.5F);
  EXPECT_NEAR(rail.height, 400.0F, 0.5F);
}

TEST_F(GlassPanelLayoutTest, AGlassPanelStretchesWhenFramedFromOutside) {
  Application application = start(
      providers_,
      components::HStack(components::GlassPanel(components::PanelRadius::medium,
                                                components::Text("Card"))
                             .frame({.max_width = composition::infinity,
                                     .max_height = composition::infinity})
                             .id("card"))
          .frame({.width = 600.0F, .height = 400.0F}));
  const composition::Rect card = size_of(application, "card");
  EXPECT_NEAR(card.width, 600.0F, 0.5F);
  EXPECT_NEAR(card.height, 400.0F, 0.5F);
}

TEST_F(GlassPanelLayoutTest,
       AFlexiblePanelAroundAScrollViewTakesTheHeightItIsGiven) {
  std::vector<composition::Element> rows;
  for (int index = 0; index < 30; ++index) {
    rows.push_back(components::Text(std::format("Row {}", index))
                       .frame({.height = 40.0F}));
  }
  Application application =
      start(providers_,
            components::VStack(
                components::Text("Settings"),
                components::ScrollView(components::VStack(std::move(rows)))
                    .frame({.max_width = composition::infinity,
                            .max_height = composition::infinity})
                    .glass_panel(components::PanelRadius::large)
                    .id("panel"))
                .frame({.width = 600.0F, .height = 400.0F}));
  const composition::Rect panel = size_of(application, "panel");
  EXPECT_LE(panel.height, 400.0F);
  EXPECT_GT(panel.height, 300.0F);
  const auto scrolled = scroller(application.root().tree().root());
  ASSERT_TRUE(scrolled.has_value());
  EXPECT_NEAR(scrolled->height, panel.height, 0.5F);
}

TEST_F(GlassPanelLayoutTest,
       AFlexiblePanelInAVStackFillsWithoutAMaxFrameOnTheContent) {
  std::vector<composition::Element> rows;
  for (int index = 0; index < 30; ++index) {
    rows.push_back(components::Text(std::format("Row {}", index))
                       .frame({.height = 40.0F}));
  }
  Application application =
      start(providers_,
            components::VStack(
                components::Text("Settings"),
                components::ScrollView(components::VStack(std::move(rows)))
                    .glass_panel(components::PanelRadius::large)
                    .id("panel"))
                .frame({.width = 600.0F, .height = 400.0F}));
  const composition::Rect panel = size_of(application, "panel");
  EXPECT_LE(panel.height, 400.0F);
  EXPECT_GT(panel.height, 300.0F);
}
}
