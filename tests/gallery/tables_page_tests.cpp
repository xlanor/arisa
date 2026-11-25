#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>

#include <gallery_fixture.hpp>
#include <tables_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <print>

namespace arisa::runtime {

class GalleryTablesTest : public GalleryFixture {
 protected:
  using Page = examples::TablesPage;

  static constexpr std::uint64_t table_budget = 8;

  static auto pixel_centre(Application& application, std::string_view key)
      -> provider::PixelPoint {
    const composition::Rect frame = presented_of(application, key);
    return provider::PixelPoint{.x = frame.x + (frame.width * 0.5F),
                                .y = frame.y + (frame.height * 0.5F)};
  }

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"tables-host", true};

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
                     Application& application, std::string_view segment) {
    const composition::Rect picker =
        presented_of(application, Page::demo_picker);
    const float third = picker.width / 3.0F;
    const float offset = segment == "Files"     ? 0.5F
                         : segment == "Outline" ? 1.5F
                                                : 2.5F;
    tap(providers, application,
        provider::PixelPoint{.x = picker.x + (third * offset),
                             .y = picker.y + (picker.height * 0.5F)});
    run(providers, application, 30);
  }
};

TEST_F(GalleryTablesTest, EveryDemoShows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(node_of(application, Page::files_table).has_value());
  EXPECT_TRUE(shows(application, "Name"));
  EXPECT_TRUE(shows(application, "Alpha 1"));
  choose(providers, application, "Outline");
  EXPECT_TRUE(node_of(application, Page::outline_table).has_value());
  EXPECT_TRUE(shows(application, "Folders"));
  EXPECT_TRUE(shows(application, "Documents"));
  choose(providers, application, "Sections");
  EXPECT_TRUE(node_of(application, Page::sections_list).has_value());
  EXPECT_TRUE(node_of(application, Page::swipe_stack).has_value());
  EXPECT_TRUE(shows(application, "Inbox"));
  EXPECT_TRUE(shows(application, "12"));
}

TEST_F(GalleryTablesTest, SortingTheFilesTableReordersRows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(shows(application, "Alpha 1"));
  tap(providers, application,
      pixel_centre(application, "arisa.table.header.name"));
  run(providers, application, 20);
  EXPECT_FALSE(shows(application, "Alpha 1"));
  EXPECT_TRUE(node_of(application, "arisa.table.sort-indicator").has_value());
}

TEST_F(GalleryTablesTest, ScrollingTheTableRealisesRowsInBatches) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const composition::Rect table = presented_of(application, Page::files_table);
  const provider::PixelPoint from{.x = table.x + (table.width * 0.5F),
                                  .y = table.y + table.height - 40.0F};
  int frames = 0;
  int laid_out = 0;
  std::uint64_t evaluations = 0;
  for (int round = 0; round < 4; ++round) {
    providers.input().push(touching(from, provider::TouchPhase::began));
    run(providers, application, 1);
    for (int index = 1; index <= 24; ++index) {
      providers.input().push(touching(
          provider::PixelPoint{
              .x = from.x,
              .y = from.y - (400.0F * static_cast<float>(index) / 24.0F)},
          provider::TouchPhase::moved));
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application.frame().has_value());
      if (round == 3) {
        ++frames;
        laid_out += application.last_frame().laid_out ? 1 : 0;
        evaluations += application.last_frame().evaluations;
      }
    }
    providers.input().push(
        touching(provider::PixelPoint{.x = from.x, .y = from.y - 400.0F},
                 provider::TouchPhase::ended));
    run(providers, application, 60);
  }
  std::println("table scroll: {} frames, {} layouts, {} evaluations", frames,
               laid_out, evaluations);
  EXPECT_LE(laid_out * 3, frames);
  EXPECT_LE(evaluations, static_cast<std::uint64_t>(laid_out) * table_budget);
}

TEST_F(GalleryTablesTest, ASettledTableAllocatesAndPresentsNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  run(providers, application, 60);
  std::uint64_t used = 0;
  int presented = 0;
  for (int index = 0; index < 60; ++index) {
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    EXPECT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
    presented += application.last_frame().presented ? 1 : 0;
  }
  EXPECT_EQ(presented, 0);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

}
