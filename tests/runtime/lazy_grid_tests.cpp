#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_item.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/interaction/interaction_options.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::infinity;
using provider::Button;

class LazyGridTest : public ScrollFixture {
 protected:
  static constexpr int cover_count = 5000;
  static constexpr float cell_height = 80.0F;
  static constexpr float row_spacing = 8.0F;

  struct Log {
    std::optional<composition::StateHandle<float>> width;
    std::optional<composition::StateHandle<int>> load;
    std::optional<composition::ScrollViewProxy> proxy;
  };

  class Library final : public composition::Component {
   public:
    static constexpr composition::StateKey<float> width_key{"grid-width",
                                                            600.0F};
    static constexpr composition::StateKey<int> load_key{"grid-load", 0};

    Library(std::shared_ptr<Log> log, float overscan) noexcept
        : log_(std::move(log)), overscan_(overscan) {}

    auto operator==(const Library& other) const -> bool {
      return log_ == other.log_ && overscan_ == other.overscan_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto width = context.state(width_key);
      const auto load = context.state(load_key);
      log_->width = width;
      log_->load = load;
      const int loaded = load.get();
      const float overscan = overscan_;
      return components::ScrollViewReader(
          [log = log_, wide = width.get(), loaded,
           overscan](composition::ScrollViewProxy proxy) -> Element {
            log->proxy = proxy;
            return components::VStack(
                components::VStackOptions{
                    .alignment = components::HorizontalAlignment::leading,
                    .spacing = 8.0F},
                components::Button("Reload", [] {})
                    .frame({.width = 120.0F, .height = 40.0F})
                    .id("reload"),
                components::ScrollView(
                    components::LazyVGrid(
                        components::LazyVGridOptions{
                            .columns = {components::GridItem{
                                components::GridItem::Size::adaptive(100.0F)}},
                            .spacing = row_spacing,
                            .overscan = overscan,
                            .retention = 1},
                        components::ForEach(
                            std::views::iota(0, cover_count),
                            [](int index) {
                              return std::format("c-{}", index);
                            },
                            [loaded](int index) {
                              return components::Button(
                                         [] {}, components::Text(std::format(
                                                    "{} {}", index, loaded)))
                                  .frame({.height = cell_height,
                                          .max_width = infinity});
                            })))
                    .frame({.width = wide, .height = 400.0F})
                    .id("scroll"));
          });
    }

   private:
    std::shared_ptr<Log> log_;
    float overscan_;
  };

  static auto library(std::shared_ptr<Log> log, float overscan = 80.0F)
      -> Element {
    return Library{std::move(log), overscan};
  }

  static auto lanes(Application& application) -> int {
    const float first = frame_of(application, "c-0").y;
    int count = 0;
    while (node_of(application, std::format("c-{}", count)).has_value() &&
           frame_of(application, std::format("c-{}", count)).y == first) {
      ++count;
    }
    return count;
  }

  static void move(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, Button which, int times = 1) {
    for (int index = 0; index < times; ++index) {
      providers.input().push(pad({which}));
      providers.input().push(pad({}));
      frames(application, 3);
    }
  }
};

TEST_F(LazyGridTest, OnlyTheVisibleRowsOfFiveThousandCellsAreBuilt) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, library(log));
  frames(application, 6);
  const int columns = lanes(application);
  EXPECT_EQ(columns, 5);
  const std::size_t built = count_keyed(application.root().tree().root(), "c-");
  EXPECT_GE(built, 5U * static_cast<std::size_t>(columns));
  EXPECT_LE(built, 9U * static_cast<std::size_t>(columns));
  EXPECT_LT(application.root().tree().size(), 200U);
  log->proxy->scroll_to("c-4000", motion::UnitPoint::top);
  frames(application, 8);
  ASSERT_TRUE(node_of(application, "c-4000").has_value());
  EXPECT_NEAR(frame_of(application, "c-4000").y -
                  frame_of(application, "scroll").y -
                  offset(application, "scroll").y,
              0.0F, 0.6F);
  EXPECT_FALSE(node_of(application, "c-0").has_value());
  EXPECT_LE(count_keyed(application.root().tree().root(), "c-"),
            12U * static_cast<std::size_t>(columns));
  for (int step = 1; step <= 4; ++step) {
    log->proxy->scroll_to(std::format("c-{}", 4000 + (step * 10)),
                          motion::UnitPoint::top);
    frames(application, 6);
  }
  EXPECT_GT(application.root().tree().recycles(), 0U);
}

TEST_F(LazyGridTest, ColumnsFollowTheWidthAndCellsKeepIdentityAndFocus) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, library(log));
  frames(application, 6);
  EXPECT_EQ(lanes(application), 5);
  move(providers, application, Button::dpad_down);
  move(providers, application, Button::dpad_right, 3);
  ASSERT_EQ(focused_key(application), "c-3");
  const auto zero = node_of(application, "c-0");
  const auto three = node_of(application, "c-3");
  log->width->set(900.0F);
  frames(application, 8);
  EXPECT_EQ(lanes(application), 8);
  EXPECT_EQ(node_of(application, "c-0"), zero);
  EXPECT_EQ(node_of(application, "c-3"), three);
  EXPECT_EQ(focused_key(application), "c-3");
  log->width->set(600.0F);
  frames(application, 8);
  EXPECT_EQ(lanes(application), 5);
  EXPECT_EQ(node_of(application, "c-3"), three);
  EXPECT_EQ(focused_key(application), "c-3");
}

TEST_F(LazyGridTest, MovingDownPastTheBuiltRowsBuildsTheNextRow) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, library(log, 0.0F),
            interaction::InteractionOptions{.focus_scroll_margin = 0.0F});
  frames(application, 6);
  move(providers, application, Button::dpad_down);
  move(providers, application, Button::dpad_right, 2);
  ASSERT_EQ(focused_key(application), "c-2");
  move(providers, application, Button::dpad_down, 12);
  EXPECT_EQ(focused_key(application), std::format("c-{}", 2 + (12 * 5)));
  settle(providers, application);
  const composition::Rect scroll = frame_of(application, "scroll");
  const composition::Rect cell = frame_of(application, "c-62");
  const float shown = cell.y - offset(application, "scroll").y;
  EXPECT_GE(shown, scroll.y - 0.6F);
  EXPECT_LE(shown + cell.height, scroll.y + scroll.height + 0.6F);
}

TEST_F(LazyGridTest, ZlAndZrPageByWholeRows) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, library(log));
  frames(application, 6);
  move(providers, application, Button::dpad_down);
  move(providers, application, Button::zr);
  settle(providers, application);
  const float pitch = cell_height + row_spacing;
  const float paged = offset(application, "scroll").y;
  EXPECT_GT(paged, pitch);
  EXPECT_NEAR(std::remainder(paged, pitch), 0.0F, 0.6F);
  const std::string focused = focused_key(application);
  ASSERT_TRUE(focused.starts_with("c-"));
  const composition::Rect scroll = frame_of(application, "scroll");
  const composition::Rect cell = frame_of(application, focused);
  EXPECT_GE(cell.y - paged, scroll.y - 0.6F);
  EXPECT_LE(cell.y - paged + cell.height, scroll.y + scroll.height + 0.6F);
  move(providers, application, Button::zr);
  settle(providers, application);
  const float again = offset(application, "scroll").y;
  EXPECT_GT(again, paged);
  EXPECT_NEAR(std::remainder(again, pitch), 0.0F, 0.6F);
  move(providers, application, Button::zl);
  settle(providers, application);
  const float back = offset(application, "scroll").y;
  EXPECT_LT(back, again);
  EXPECT_NEAR(std::remainder(back, pitch), 0.0F, 0.6F);
}

TEST_F(LazyGridTest, FocusSurvivesAReloadOfTheGridData) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, library(log));
  frames(application, 6);
  move(providers, application, Button::dpad_down, 2);
  move(providers, application, Button::dpad_right, 2);
  ASSERT_EQ(focused_key(application), "c-7");
  const auto before = application.interaction().focused();
  log->load->set(1);
  frames(application, 6);
  EXPECT_TRUE(shows(application, "7 1"));
  EXPECT_EQ(focused_key(application), "c-7");
  EXPECT_EQ(application.interaction().focused(), before);
}

TEST_F(LazyGridTest, LeavingAndReturningRestoresTheLastFocusedCell) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, library(log));
  frames(application, 6);
  move(providers, application, Button::dpad_down, 2);
  move(providers, application, Button::dpad_right, 3);
  ASSERT_EQ(focused_key(application), "c-8");
  move(providers, application, Button::dpad_up);
  ASSERT_EQ(focused_key(application), "c-3");
  move(providers, application, Button::dpad_up);
  ASSERT_EQ(focused_key(application), "reload");
  move(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "c-3");
}

class LazyReuseTest : public ScrollFixture {
 protected:
  static constexpr int shelf_count = 500;

  struct Cover {
    int id{0};
    int value{0};

    auto operator==(const Cover&) const -> bool = default;
  };

  struct Probe {
    std::optional<composition::StateHandle<int>> tick;
    std::optional<composition::StateHandle<int>> bumped;
    std::size_t built{0};
  };

  class CoverCell {
   public:
    explicit CoverCell(std::shared_ptr<Probe> probe) noexcept
        : probe_(std::move(probe)) {}

    auto operator()(const Cover& cover) const -> Element {
      ++probe_->built;
      return components::Text(std::format("{}:{}", cover.id, cover.value))
          .frame({.height = 40.0F, .max_width = infinity});
    }

    auto operator==(const CoverCell&) const -> bool = default;

   private:
    std::shared_ptr<Probe> probe_;
  };

  class Shelf final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> tick_key{"shelf-tick", 0};
    static constexpr composition::StateKey<int> bumped_key{"shelf-bumped", -1};

    Shelf(std::shared_ptr<Probe> probe, bool capturing) noexcept
        : probe_(std::move(probe)), capturing_(capturing) {}

    auto operator==(const Shelf& other) const -> bool {
      return probe_ == other.probe_ && capturing_ == other.capturing_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto tick = context.state(tick_key);
      const auto bumped = context.state(bumped_key);
      probe_->tick = tick;
      probe_->bumped = bumped;
      const int marked = bumped.get();
      std::vector<Cover> covers =
          std::views::iota(0, shelf_count) |
          std::views::transform([marked](int index) {
            return Cover{.id = index, .value = index == marked ? 1 : 0};
          }) |
          std::ranges::to<std::vector<Cover>>();
      const auto key = [](const Cover& cover) { return cover.id; };
      const components::LazyVGridOptions options{
          .columns = {components::GridItem{
              components::GridItem::Size::adaptive(100.0F)}},
          .spacing = row_spacing,
          .overscan = 80.0F,
          .retention = 1};
      Element grid =
          capturing_
              ? Element{components::LazyVGrid(
                    options,
                    components::ForEach(
                        std::move(covers), key,
                        [probe = probe_,
                         ticked = tick.get()](const Cover& cover) -> Element {
                          ++probe->built;
                          return components::Text(
                                     std::format("{}@{}", cover.id, ticked))
                              .frame({.height = 40.0F, .max_width = infinity});
                        }))}
              : Element{components::LazyVGrid(
                    options, components::ForEach(std::move(covers), key,
                                                 CoverCell{probe_}))};
      return components::VStack(
          components::Text(std::format("tick {}", tick.get())),
          components::ScrollView(std::move(grid))
              .frame({.width = 600.0F, .height = 400.0F}));
    }

   private:
    static constexpr float row_spacing = 8.0F;

    std::shared_ptr<Probe> probe_;
    bool capturing_;
  };
};

TEST_F(LazyReuseTest, AParentChangeKeepsCellsWhoseItemsAreUnchanged) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, Shelf{probe, false});
  frames(application, 6);
  ASSERT_GT(probe->built, 0U);
  ASSERT_TRUE(shows(application, "7:0"));
  const std::size_t built = probe->built;
  const std::uint64_t evaluations = application.root().tree().evaluations();
  probe->tick->set(1);
  frames(application, 4);
  ASSERT_TRUE(shows(application, "tick 1"));
  const std::size_t rebuilt = probe->built - built;
  const std::uint64_t evaluated =
      application.root().tree().evaluations() - evaluations;
  EXPECT_EQ(rebuilt, 0U);
  EXPECT_LE(evaluated, 4U);
}

TEST_F(LazyReuseTest, AnItemThatChangesUnderItsIdStillUpdates) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, Shelf{probe, false});
  frames(application, 6);
  ASSERT_TRUE(shows(application, "3:0"));
  const auto three = application.root().tree().find(composition::ElementKey{3});
  const std::size_t built = probe->built;
  probe->bumped->set(3);
  frames(application, 4);
  EXPECT_TRUE(shows(application, "3:1"));
  EXPECT_FALSE(shows(application, "3:0"));
  EXPECT_TRUE(shows(application, "4:0"));
  EXPECT_EQ(probe->built - built, 1U);
  EXPECT_EQ(application.root().tree().find(composition::ElementKey{3}), three);
}

TEST_F(LazyReuseTest, ACapturedValueThatChangesStillReachesEveryCell) {
  provider::headless::HeadlessProviderFactory providers;
  auto probe = std::make_shared<Probe>();
  Application application = start(providers, Shelf{probe, true});
  frames(application, 6);
  ASSERT_TRUE(shows(application, "0@0"));
  ASSERT_TRUE(shows(application, "7@0"));
  probe->tick->set(1);
  frames(application, 4);
  EXPECT_TRUE(shows(application, "0@1"));
  EXPECT_TRUE(shows(application, "7@1"));
  EXPECT_FALSE(shows(application, "7@0"));
}

class GridFocusTest : public FocusFixture {
 protected:
  class Uneven final : public composition::Component {
   public:
    auto operator==(const Uneven&) const -> bool = default;

    auto body(composition::Context&) const -> Element override {
      return components::Grid(
          components::GridRow(button("a0"), button("a1"), button("a2")),
          components::GridRow(button("b0", 260.0F).grid_cell_columns(3)),
          components::GridRow(button("c0"), button("c1"), button("c2")));
    }
  };
};

TEST_F(GridFocusTest, VerticalMovesKeepTheirColumnAcrossAShortRow) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Uneven{});
  press(providers, application, Button::dpad_right);
  press(providers, application, Button::dpad_right);
  ASSERT_EQ(focused_key(application), "a2");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b0");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "c2");
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "b0");
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "a2");
  press(providers, application, Button::dpad_left);
  ASSERT_EQ(focused_key(application), "a1");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b0");
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "c1");
}

}
