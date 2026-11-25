#include <arisa/components/button.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/tab_view.hpp>

#include <chrome_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::runtime {

class GalleryChromeTest : public GalleryFixture {
 protected:
  using Page = examples::ChromePage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"chrome-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    load_fonts(providers);
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }

  static void choose(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, std::string_view tile) {
    ASSERT_TRUE(application.root().activate(composition::ElementKey{tile}));
    run(providers, application, 40);
  }

  static auto settled_allocations(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) -> std::uint64_t {
    run(providers, application, 60);
    std::uint64_t used = 0;
    for (int index = 0; index < 60; ++index) {
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      const std::uint64_t before =
          provider::AllocationMeter::count().value_or(0);
      EXPECT_TRUE(application.frame().has_value());
      used += provider::AllocationMeter::count().value_or(0) - before;
    }
    return used;
  }
};

TEST_F(GalleryChromeTest, EveryTileOpensItsDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(shows(application, "Chrome"));
  for (const std::string_view tile :
       {Page::large_button, Page::mail_button, Page::onboarding_button,
        Page::tabs_button, Page::toolbar_button, Page::bottom_button}) {
    choose(providers, application, tile);
    EXPECT_TRUE(shows(application, "\u2039 Back")) << tile;
    press(providers, application, provider::Button::b);
    run(providers, application, 60);
    EXPECT_FALSE(shows(application, "\u2039 Back")) << tile;
  }
}

TEST_F(GalleryChromeTest, LargeTitleIsShownFirst) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::large_button);
  EXPECT_TRUE(
      node_of(application, presentation::NavigationStack::large_title_id)
          .has_value());
  EXPECT_TRUE(shows(application, "Collapses on scroll"));
}

TEST_F(GalleryChromeTest, ToolbarDemoShowsGroupsOverflowAndBottomBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::toolbar_button);
  EXPECT_TRUE(node_of(application, presentation::ToolbarLayout::overflow_key)
                  .has_value());
  EXPECT_TRUE(node_of(application, presentation::ToolbarLayout::title_menu_key)
                  .has_value());
  EXPECT_TRUE(node_of(application, presentation::NavigationStack::bottom_bar_id)
                  .has_value());
  EXPECT_TRUE(shows(application, "Previous"));
}

TEST_F(GalleryChromeTest, TabsDemoShowsSectionsBadgesAndSearch) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::tabs_button);
  EXPECT_TRUE(shows(application, "Library"));
  EXPECT_TRUE(shows(application, "Store"));
  EXPECT_TRUE(shows(application, "Search"));
  EXPECT_TRUE(shows(application, "New"));
}

TEST_F(GalleryChromeTest, TabsDemoSidebarShowsItsHeaderAndFooter) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::tabs_button);
  EXPECT_TRUE(node_of(application, "arisa.tab.sidebar-header").has_value());
  EXPECT_TRUE(node_of(application, "arisa.tab.sidebar-footer").has_value());
  const composition::Rect header =
      frame_of(application, "arisa.tab.sidebar-header");
  const composition::Rect footer =
      frame_of(application, "arisa.tab.sidebar-footer");
  EXPECT_LT(header.y, footer.y);
}

TEST_F(GalleryChromeTest, OnboardingShowsPageDots) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::onboarding_button);
  EXPECT_TRUE(node_of(application, structure::TabView::dots_id).has_value());
  EXPECT_TRUE(shows(application, "Welcome"));
}

TEST_F(GalleryChromeTest, ToolbarItemsDemoCustomizesAndOverflows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::items_button);
  EXPECT_TRUE(shows(application, "Toolbar items"));
  EXPECT_TRUE(node_of(application, presentation::ToolbarLayout::overflow_key)
                  .has_value());
  tap(providers, application,
      centre(application, presentation::ToolbarLayout::overflow_key));
  run(providers, application, 20);
  EXPECT_TRUE(shows(application, "Mark all read"));
  EXPECT_TRUE(shows(application,
                    std::string{presentation::ToolbarLayout::customize_label}));
}

TEST_F(GalleryChromeTest, TabExtrasDemoShowsProminentTabAndAccessory) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::extras_button);
  EXPECT_TRUE(
      node_of(application, structure::TabView::prominent_id).has_value());
  EXPECT_TRUE(
      node_of(application, structure::TabView::accessory_id).has_value());
  EXPECT_TRUE(shows(application, "Now playing"));
}

TEST_F(GalleryChromeTest, SearchDemoStartsMinimizedAndOpensFromTheBar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::search_button);
  EXPECT_TRUE(node_of(application, "arisa.search.button").has_value());
  EXPECT_FALSE(node_of(application, "arisa.search.bar").has_value());
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{"chrome-search-toggle"}));
  run(providers, application, 30);
  EXPECT_TRUE(node_of(application, "arisa.search.bar").has_value());
}

TEST_F(GalleryChromeTest, DialogsDemoPresentsItemAndErrorDialogs) {
  const auto labelled = [](Application& application, std::string_view label) {
    bool found = false;
    const auto search = [&](this const auto& self,
                            const composition::Node& node) -> void {
      if (const auto pressed = node.content_as<components::Button::Content>();
          pressed.has_value() && pressed->get().label() == label &&
          node.phase() != composition::NodeLifecyclePhase::departing) {
        found = true;
      }
      for (const composition::Node& child : node.children()) {
        self(child);
      }
    };
    search(application.root().tree().root());
    return found;
  };
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::dialogs_button);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{"chrome-dialog-item"}));
  run(providers, application, 40);
  EXPECT_TRUE(labelled(application, "Delete item 3"));
  press(providers, application, provider::Button::b);
  run(providers, application, 40);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{"chrome-dialog-error"}));
  run(providers, application, 40);
  EXPECT_TRUE(shows(application, "Sync failed (503)"));
  EXPECT_TRUE(labelled(application, "Retry"));
}

TEST_F(GalleryChromeTest, SettledDemosAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view tile :
       {Page::mail_button, Page::tabs_button, Page::toolbar_button,
        Page::bottom_button}) {
    choose(providers, application, tile);
    const std::uint64_t used = settled_allocations(providers, application);
    if (provider::AllocationMeter::counting()) {
      EXPECT_EQ(used, 0U) << tile;
    }
    press(providers, application, provider::Button::b);
    run(providers, application, 40);
  }
}

}
