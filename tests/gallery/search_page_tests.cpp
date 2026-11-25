#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <scroll_views_page.hpp>
#include <search_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace arisa::runtime {

class GallerySearchTest : public GalleryFixture {
 protected:
  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"search-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return examples::SearchPage{context.state(shown)};
    }
  };

  class Views final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"views-host", true};

    auto operator==(const Views&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return examples::ScrollViewsPage{context.state(shown)};
    }
  };

  static auto covers(const composition::Node& node) -> int {
    int found = 0;
    if (node.phase() == composition::NodeLifecyclePhase::live &&
        node.key().has_value() && node.key()->as<int>().has_value() &&
        node.parent().has_value()) {
      ++found;
    }
    for (const composition::Node& child : node.children()) {
      found += covers(child);
    }
    return found;
  }
};

TEST_F(GallerySearchTest, SearchFiltersTwoThousandTitlesFromLiveDrafts) {
  using examples::SearchPage;
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  Application application = start(providers, Host{});
  run(providers, application, 30);
  EXPECT_TRUE(shows(application, "2000 titles"));
  EXPECT_LT(covers(application.root().tree().root()), 200);
  providers.text_input().push(std::string{"kingdom"},
                              std::vector<std::string>{"k", "ki", "kin"});
  tap(providers, application, centre(application, "arisa.search.field"));
  run(providers, application, 12);
  EXPECT_TRUE(shows(
      application,
      std::format(
          "{} results",
          SearchPage::matches("kin", {}, SearchPage::Shelf::all).size())));
  run(providers, application, 20);
  const std::size_t found =
      SearchPage::matches("kingdom", {}, SearchPage::Shelf::all).size();
  EXPECT_GT(found, 0U);
  EXPECT_LT(found, 2000U);
  EXPECT_TRUE(shows(application, std::format("{} results", found)));
  EXPECT_TRUE(shows(application, "Installed"));
  tap(providers, application, centre(application, SearchPage::done_button));
  run(providers, application, 20);
  EXPECT_TRUE(shows(application, "2000 titles"));
}

TEST_F(GallerySearchTest, ScopesAndTokensNarrowTheResults) {
  using examples::SearchPage;
  const std::size_t all =
      SearchPage::matches("", {}, SearchPage::Shelf::all).size();
  const std::size_t installed =
      SearchPage::matches("", {}, SearchPage::Shelf::installed).size();
  const std::size_t racing =
      SearchPage::matches("", {SearchPage::Genre::racing},
                          SearchPage::Shelf::installed)
          .size();
  EXPECT_EQ(all, 2000U);
  EXPECT_LT(installed, all);
  EXPECT_LT(racing, installed);
  EXPECT_GT(racing, 0U);
  EXPECT_EQ(
      SearchPage::matches("STAR racer", {}, SearchPage::Shelf::all).size(),
      SearchPage::matches("star racer", {}, SearchPage::Shelf::all).size());
}

TEST_F(GallerySearchTest, SettledSearchFramesAllocateNothing) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not counted in this build";
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Host{});
  providers.text_input().push(std::string{"star"});
  tap(providers, application, centre(application, "arisa.search.field"));
  run(providers, application, 240);
  std::uint64_t used = 0;
  for (int index = 0; index < 120; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
  }
  EXPECT_EQ(used, 0U);
}

TEST_F(GallerySearchTest, TheChatLogStartsAtTheBottom) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  Application application = start(providers, Views{});
  run(providers, application, 30);
  const auto id = node_of(application, examples::ScrollViewsPage::chat_view);
  ASSERT_TRUE(id.has_value());
  const auto scroller = application.interaction().scrolls().scroller(*id);
  ASSERT_TRUE(scroller.has_value());
  EXPECT_GT(scroller->get().limit().y, 100.0F);
  EXPECT_NEAR(offset(application, examples::ScrollViewsPage::chat_view).y,
              scroller->get().limit().y, 0.5F);
}

}
