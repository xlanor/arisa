#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_hstack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_target_behavior.hpp>
#include <arisa/composition/scroll_transition_phase.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/interaction/interaction_options.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/space.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Axes;
using composition::Element;
using provider::Button;

class ScrollTest : public ScrollFixture {
 protected:
  static constexpr float row_height = 50.0F;

  struct Log {
    int taps{0};
    std::vector<composition::ScrollPhase> phases;
    std::vector<float> offsets;
    std::optional<composition::ScrollViewProxy> proxy;
  };

  static auto row(int index, std::shared_ptr<Log> log = nullptr) -> Element {
    return components::Button(std::format("Row {}", index),
                              [log] {
                                if (log != nullptr) {
                                  ++log->taps;
                                }
                              })
        .frame({.width = 280.0F, .height = row_height})
        .id(std::format("b{}", index));
  }

  static auto list(int count, std::shared_ptr<Log> log = nullptr) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < count; ++index) {
      rows.push_back(row(index, log));
    }
    return components::VStack(components::VStackOptions{.spacing = 0.0F},
                              std::move(rows));
  }

  static auto scroller(Element content) -> Element {
    return components::ScrollView(std::move(content))
        .frame({.width = 300.0F, .height = 400.0F})
        .id("scroll");
  }

  static auto lazy(int count, float overscan, std::size_t retention)
      -> Element {
    return components::LazyVStack(
        components::LazyVStackOptions{.spacing = 0.0F,
                                      .overscan = overscan,
                                      .retention = retention,
                                      .estimated_extent = 40.0F},
        components::ForEach(
            std::views::iota(0, count),
            [](int index) { return std::format("row-{}", index); },
            [](int index) {
              return components::Button(std::format("Lazy {}", index),
                                        [index] { static_cast<void>(index); })
                  .frame({.width = 280.0F, .height = 40.0F});
            }));
  }

  class Remembering final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> hits{"hits", 0};

    explicit Remembering(int index) noexcept : index_(index) {}

    auto operator==(const Remembering&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto count = context.state(hits);
      return components::Button([count] { count.set(count.get() + 1); },
                                components::Text(std::format(
                                    "Row {} hits {}", index_, count.get())))
          .frame({.width = 280.0F, .height = 40.0F})
          .id(std::format("press-{}", index_));
    }

   private:
    int index_;
  };

  static auto top_of(Application& application, std::string_view key) -> float {
    return frame_of(application, key).y - frame_of(application, "scroll").y -
           offset(application, "scroll").y;
  }

  class Reading final : public composition::Component {
   public:
    using Make = std::move_only_function<Element() const>;

    Reading(std::shared_ptr<Log> log, std::shared_ptr<const Make> make) noexcept
        : log_(std::move(log)), make_(std::move(make)) {}

    auto operator==(const Reading& other) const -> bool {
      return log_ == other.log_;
    }

    auto body(composition::Context&) const -> Element override {
      return components::ScrollViewReader(
          [log = log_,
           make = make_](composition::ScrollViewProxy proxy) -> Element {
            log->proxy = proxy;
            return (*make)();
          });
    }

   private:
    std::shared_ptr<Log> log_;
    std::shared_ptr<const Make> make_;
  };

  template <class Make>
  static auto reading(std::shared_ptr<Log> log, Make make) -> Element {
    return Reading{std::move(log),
                   std::make_shared<const Reading::Make>(std::move(make))};
  }
};

TEST_F(ScrollTest, TouchDragScrollsWithoutLayout) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(30)));
  const composition::Rect viewport = frame_of(application, "scroll");
  const provider::PixelPoint from{.x = viewport.x + 150.0F,
                                  .y = viewport.y + 300.0F};
  touch_down(providers, application, from);
  int laid = 0;
  for (const float y : {290.0F, 250.0F, 200.0F, 110.0F}) {
    touch_move(providers, application,
               provider::PixelPoint{.x = from.x, .y = viewport.y + y});
    laid += application.last_frame().laid_out ? 1 : 0;
  }
  EXPECT_EQ(laid, 0);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 180.0F);
  const auto scroll = node_of(application, "scroll");
  ASSERT_TRUE(scroll.has_value());
  const auto scroller = application.interaction().scrolls().scroller(*scroll);
  ASSERT_TRUE(scroller.has_value());
  EXPECT_EQ(scroller->get().phase(std::chrono::nanoseconds{0}),
            composition::ScrollPhase::interacting);
  step(providers, application, 200ms);
  touch_up(providers, application,
           provider::PixelPoint{.x = from.x, .y = viewport.y + 110.0F});
  EXPECT_EQ(settle(providers, application), 0);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 180.0F);
}

TEST_F(ScrollTest, OverscrollRubberBandsAndSpringsBack) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(30)));
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 50.0F},
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 250.0F},
       5, false);
  const float pulled = offset(application, "scroll").y;
  EXPECT_LT(pulled, 0.0F);
  EXPECT_GT(pulled, -190.0F);
  touch_up(
      providers, application,
      provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 250.0F});
  step(providers, application);
  EXPECT_LT(offset(application, "scroll").y, 0.0F);
  EXPECT_EQ(settle(providers, application), 0);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 0.0F);
}

TEST_F(ScrollTest, FlingDeceleratesWithoutLayoutAndStops) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(60)));
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 350.0F},
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 150.0F},
       4);
  const float released = offset(application, "scroll").y;
  step(providers, application, 100ms);
  EXPECT_GT(offset(application, "scroll").y, released + 50.0F);
  EXPECT_EQ(settle(providers, application), 0);
  const float rested = offset(application, "scroll").y;
  step(providers, application, 100ms);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, rested);
}

TEST_F(ScrollTest, AHalfRateSpinnerDoesNotHalveTheRateOfAScroll) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::VStack(controls::ProgressView(), scroller(list(60))));
  const composition::Rect viewport = frame_of(application, "scroll");
  std::vector<std::uint32_t> intervals;
  const auto shown = [&] {
    if (application.last_frame().presented) {
      intervals.push_back(application.last_frame().swap_interval);
    }
  };
  for (int index = 0; index < 10; ++index) {
    step(providers, application);
    shown();
  }
  ASSERT_FALSE(intervals.empty());
  EXPECT_TRUE(std::ranges::all_of(
      intervals, [](std::uint32_t interval) { return interval == 2U; }));
  intervals.clear();
  const float x = viewport.x + 150.0F;
  touch_down(providers, application,
             provider::PixelPoint{.x = x, .y = viewport.y + 350.0F});
  shown();
  for (const float y : {330.0F, 300.0F, 260.0F, 210.0F, 150.0F}) {
    touch_move(providers, application,
               provider::PixelPoint{.x = x, .y = viewport.y + y});
    shown();
  }
  touch_up(providers, application,
           provider::PixelPoint{.x = x, .y = viewport.y + 150.0F});
  shown();
  ASSERT_GE(intervals.size(), 6U);
  EXPECT_TRUE(std::ranges::all_of(
      intervals, [](std::uint32_t interval) { return interval == 1U; }));
  const auto scroll = node_of(application, "scroll");
  ASSERT_TRUE(scroll.has_value());
  const auto held = application.interaction().scrolls().scroller(*scroll);
  ASSERT_TRUE(held.has_value());
  const auto& scroller = held->get();
  int moving = 0;
  int settled = 0;
  for (int index = 0; index < 400 && settled < 5; ++index) {
    const bool was_moving =
        scroller.moving(providers.clock().now().since_origin());
    intervals.clear();
    step(providers, application);
    shown();
    if (intervals.empty()) {
      continue;
    }
    const bool still_moving =
        scroller.moving(providers.clock().now().since_origin());
    if (still_moving) {
      ++moving;
      EXPECT_EQ(intervals.back(), 1U) << "frame " << index;
    } else if (!was_moving) {
      ++settled;
      EXPECT_EQ(intervals.back(), 2U) << "frame " << index;
    }
  }
  EXPECT_GT(moving, 5);
  EXPECT_EQ(settled, 5);
}

TEST_F(ScrollTest, ButtonsStayTappableButADragHandsTheTouchToTheScrollView) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, scroller(list(30, log)));
  tap(providers, application, centre(application, "b1"));
  EXPECT_EQ(log->taps, 1);
  const provider::PixelPoint at = centre(application, "b2");
  drag(providers, application, at,
       provider::PixelPoint{.x = at.x, .y = at.y - 60.0F}, 3);
  EXPECT_EQ(log->taps, 1);
  EXPECT_GE(offset(application, "scroll").y, 39.9F);
}

TEST_F(ScrollTest, FocusFollowAnimatesItemIntoViewWithAMarginAndNoLayout) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(30)));
  EXPECT_EQ(focused_key(application), "b0");
  for (int move = 0; move < 8; ++move) {
    providers.input().push(pad({Button::dpad_down}));
    providers.input().push(pad({}));
    frames(application, 2);
  }
  EXPECT_EQ(focused_key(application), "b8");
  step(providers, application, 50ms);
  const float midway = offset(application, "scroll").y;
  EXPECT_GT(midway, 0.0F);
  int laid = application.last_frame().laid_out ? 1 : 0;
  laid += settle(providers, application);
  EXPECT_EQ(laid, 0);
  const float settled = offset(application, "scroll").y;
  EXPECT_GT(settled, midway);
  const float bottom =
      top_of(application, "b8") + frame_of(application, "b8").height;
  EXPECT_NEAR(bottom, 400.0F - 24.0F, 0.6F);
}

TEST_F(ScrollTest, ProxyScrollsToAnchorsAndAnimatesInsideWithAnimation) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, reading(log, [] { return scroller(list(40)); }));
  ASSERT_TRUE(log->proxy.has_value());
  log->proxy->scroll_to("b10", motion::UnitPoint::top);
  frames(application, 2);
  EXPECT_NEAR(top_of(application, "b10"), 0.0F, 0.01F);
  log->proxy->scroll_to("b20", motion::UnitPoint::center);
  frames(application, 2);
  EXPECT_NEAR(top_of(application, "b20") + (row_height * 0.5F), 200.0F, 0.01F);
  log->proxy->scroll_to("b30", motion::UnitPoint::bottom);
  frames(application, 2);
  EXPECT_NEAR(top_of(application, "b30") + row_height, 400.0F, 0.01F);
  motion::with_animation(motion::Animation::smooth(0.5), [&log] {
    log->proxy->scroll_to("b0", motion::UnitPoint::top);
  });
  frames(application, 1);
  step(providers, application, 100ms);
  const float moving = offset(application, "scroll").y;
  EXPECT_GT(moving, 0.0F);
  EXPECT_EQ(settle(providers, application), 0);
  EXPECT_NEAR(offset(application, "scroll").y, 0.0F, 0.01F);
}

TEST_F(ScrollTest, LazyStacksFillTheProposedWidthOfAVerticalScrollView) {
  provider::headless::HeadlessProviderFactory providers;
  const auto stack = [](components::HorizontalAlignment alignment,
                        std::string_view name, bool flexible) {
    return Element{
        components::LazyVStack(
            components::LazyVStackOptions{.alignment = alignment,
                                          .spacing = 0.0F},
            components::ForEach(
                std::views::iota(0, 20),
                [name](int index) { return std::format("{}-{}", name, index); },
                [flexible](int index) {
                  const float wide =
                      120.0F + (10.0F * static_cast<float>(index % 3));
                  if (flexible) {
                    return components::Text(std::format("Row {}", index))
                        .frame({.height = 40.0F,
                                .min_width = wide,
                                .max_width = composition::infinity,
                                .alignment = composition::Alignment::leading});
                  }
                  return components::Text(std::format("Row {}", index))
                      .frame({.width = wide, .height = 40.0F});
                }))}
        .id(std::string{name});
  };
  const auto scroller = [](Element content) {
    return components::ScrollView(std::move(content))
        .frame({.width = 300.0F, .height = 400.0F});
  };
  Application application = start(
      providers,
      components::HStack(
          components::HStackOptions{.spacing = 0.0F},
          scroller(
              stack(components::HorizontalAlignment::center, "centre", false)),
          scroller(
              stack(components::HorizontalAlignment::leading, "lead", false)),
          scroller(
              stack(components::HorizontalAlignment::leading, "fill", true)
                  .padding(theme::Space{10.0F})))
          .frame({.width = 900.0F, .height = 400.0F}));
  frames(application, 3);
  const composition::Rect centre = frame_of(application, "centre");
  const composition::Rect lead = frame_of(application, "lead");
  const composition::Rect fill = frame_of(application, "fill");
  EXPECT_FLOAT_EQ(centre.width, 300.0F);
  EXPECT_FLOAT_EQ(lead.width, 300.0F);
  EXPECT_FLOAT_EQ(fill.width, 300.0F);
  EXPECT_NEAR(frame_of(application, "lead-0").x, lead.x, 0.5F);
  EXPECT_NEAR(frame_of(application, "lead-1").x, lead.x, 0.5F);
  EXPECT_NEAR(frame_of(application, "centre-0").x - centre.x, 90.0F, 0.5F);
  EXPECT_NEAR(frame_of(application, "centre-1").x - centre.x, 85.0F, 0.5F);
  for (const std::string_view row : {"fill-0", "fill-1", "fill-2"}) {
    EXPECT_NEAR(frame_of(application, row).x, fill.x + 10.0F, 0.5F) << row;
    EXPECT_NEAR(frame_of(application, row).width, 280.0F, 0.5F) << row;
  }
}

TEST_F(ScrollTest, LazyStackBuildsOnlyTheVisibleWindow) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, reading(log, [] { return scroller(lazy(1000, 80.0F, 2)); }));
  frames(application, 3);
  const std::size_t built =
      count_keyed(application.root().tree().root(), "row-");
  EXPECT_GE(built, 10U);
  EXPECT_LE(built, 18U);
  EXPECT_LT(application.root().tree().size(), 120U);
  log->proxy->scroll_to("row-500", motion::UnitPoint::top);
  frames(application, 6);
  ASSERT_TRUE(node_of(application, "row-500").has_value());
  EXPECT_NEAR(top_of(application, "row-500"), 0.0F, 0.5F);
  EXPECT_LE(count_keyed(application.root().tree().root(), "row-"), 26U);
  EXPECT_FALSE(node_of(application, "row-0").has_value());
}

TEST_F(ScrollTest, FocusPastTheLastBuiltRowMaterialisesTheNextOne) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, scroller(lazy(1000, 0.0F, 0)),
            interaction::InteractionOptions{.focus_scroll_margin = 0.0F});
  frames(application, 3);
  EXPECT_FALSE(node_of(application, "row-10").has_value());
  ASSERT_TRUE(node_of(application, "row-9").has_value());
  for (int move = 0; move < 9; ++move) {
    providers.input().push(pad({Button::dpad_down}));
    providers.input().push(pad({}));
    frames(application, 2);
  }
  EXPECT_EQ(focused_key(application), "row-9");
  EXPECT_FALSE(node_of(application, "row-10").has_value());
  providers.input().push(pad({Button::dpad_down}));
  providers.input().push(pad({}));
  frames(application, 4);
  EXPECT_EQ(focused_key(application), "row-10");
  settle(providers, application);
  EXPECT_NEAR(top_of(application, "row-10") + 40.0F, 400.0F, 0.6F);
}

TEST_F(ScrollTest, RecycledRowsComeBackWithFreshStateAndIdentity) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  auto appeared = std::make_shared<std::vector<int>>();
  Application application =
      start(providers, reading(log, [appeared] {
              return scroller(components::LazyVStack(
                  components::LazyVStackOptions{.spacing = 0.0F,
                                                .overscan = 0.0F,
                                                .retention = 2,
                                                .estimated_extent = 40.0F},
                  components::ForEach(
                      std::views::iota(0, 500),
                      [](int index) { return std::format("keep-{}", index); },
                      [appeared](int index) {
                        return Element{Remembering{index}}.on_appear(
                            [appeared, index] { appeared->push_back(index); });
                      })));
            }));
  frames(application, 3);
  const std::optional<composition::NodeId> first =
      node_of(application, "press-0");
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"press-0"}));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "Row 0 hits 1"));
  const std::uint64_t before = application.root().tree().recycles();
  for (const int target : {40, 80, 120, 160, 200}) {
    log->proxy->scroll_to(std::format("keep-{}", target),
                          motion::UnitPoint::top);
    frames(application, 6);
  }
  EXPECT_GT(application.root().tree().recycles(), before);
  EXPECT_FALSE(node_of(application, "press-0").has_value());
  for (int index = 195; index < 205; ++index) {
    const auto shown = node_of(application, std::format("press-{}", index));
    if (!shown.has_value()) {
      continue;
    }
    EXPECT_GT(shown->value(), first->value());
    EXPECT_TRUE(shows(application, std::format("Row {} hits 0", index)));
  }
  EXPECT_TRUE(std::ranges::contains(*appeared, 200));
  log->proxy->scroll_to("keep-0", motion::UnitPoint::top);
  frames(application, 6);
  EXPECT_TRUE(shows(application, "Row 0 hits 0"));
  const auto back = node_of(application, "press-0");
  ASSERT_TRUE(back.has_value());
  EXPECT_NE(back->value(), first->value());
  EXPECT_EQ(std::ranges::count(*appeared, 0), 2);
}

TEST_F(ScrollTest, RecyclingKeepsThePoolBounded) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, reading(log, [] {
              return scroller(components::LazyVStack(
                  components::LazyVStackOptions{.spacing = 0.0F,
                                                .overscan = 0.0F,
                                                .retention = 2,
                                                .estimated_extent = 40.0F},
                  components::ForEach(
                      std::views::iota(0, 2000),
                      [](int index) { return std::format("keep-{}", index); },
                      [](int index) { return Remembering{index}; })));
            }));
  frames(application, 3);
  for (int target = 50; target <= 1950; target += 50) {
    log->proxy->scroll_to(std::format("keep-{}", target),
                          motion::UnitPoint::top);
    frames(application, 4);
  }
  EXPECT_GT(application.root().tree().recycles(), 100U);
  EXPECT_LE(application.root().tree().recyclable(), 48U);
}

TEST_F(ScrollTest, LazyRowsKeepStateInsideTheRetentionWindowOnly) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, reading(log, [] {
              return scroller(components::LazyVStack(
                  components::LazyVStackOptions{.spacing = 0.0F,
                                                .overscan = 0.0F,
                                                .retention = 3,
                                                .estimated_extent = 40.0F},
                  components::ForEach(
                      std::views::iota(0, 500),
                      [](int index) { return std::format("keep-{}", index); },
                      [](int index) { return Remembering{index}; })));
            }));
  frames(application, 3);
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"press-0"}));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "Row 0 hits 1"));
  log->proxy->scroll_to("keep-2", motion::UnitPoint::top);
  frames(application, 4);
  EXPECT_TRUE(shows(application, "Row 0 hits 1"));
  log->proxy->scroll_to("keep-0", motion::UnitPoint::top);
  frames(application, 4);
  EXPECT_TRUE(shows(application, "Row 0 hits 1"));
  log->proxy->scroll_to("keep-200", motion::UnitPoint::top);
  frames(application, 6);
  EXPECT_FALSE(node_of(application, "keep-0").has_value());
  log->proxy->scroll_to("keep-0", motion::UnitPoint::top);
  frames(application, 6);
  EXPECT_TRUE(shows(application, "Row 0 hits 0"));
}

TEST_F(ScrollTest, PagingSnapsToTheNextPage) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> pages;
  for (int index = 0; index < 5; ++index) {
    pages.push_back(components::VStack()
                        .frame({.width = 300.0F, .height = 200.0F})
                        .id(std::format("page{}", index)));
  }
  Application application = start(
      providers,
      components::ScrollView(
          Axes::horizontal,
          components::HStack(components::HStackOptions{.spacing = 0.0F},
                             std::move(pages)))
          .frame({.width = 300.0F, .height = 200.0F})
          .scroll_target_behavior(composition::ScrollTargetBehavior::paging)
          .id("scroll"));
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 200.0F, .y = viewport.y + 100.0F},
       provider::PixelPoint{.x = viewport.x + 120.0F, .y = viewport.y + 100.0F},
       3);
  settle(providers, application);
  EXPECT_NEAR(offset(application, "scroll").x, 300.0F, 0.01F);
}

TEST_F(ScrollTest, ViewAlignedSnapsToTargetLayoutChildren) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> cards;
  for (int index = 0; index < 8; ++index) {
    cards.push_back(components::VStack()
                        .frame({.width = 120.0F, .height = 100.0F})
                        .id(std::format("card{}", index)));
  }
  Application application =
      start(providers,
            components::ScrollView(
                Axes::horizontal,
                components::HStack(components::HStackOptions{.spacing = 10.0F},
                                   std::move(cards))
                    .scroll_target_layout())
                .frame({.width = 300.0F, .height = 100.0F})
                .scroll_target_behavior(
                    composition::ScrollTargetBehavior::view_aligned)
                .id("scroll"));
  const composition::Rect viewport = frame_of(application, "scroll");
  touch_down(
      providers, application,
      provider::PixelPoint{.x = viewport.x + 250.0F, .y = viewport.y + 50.0F});
  touch_move(
      providers, application,
      provider::PixelPoint{.x = viewport.x + 240.0F, .y = viewport.y + 50.0F});
  touch_move(
      providers, application,
      provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 50.0F});
  step(providers, application, 200ms);
  touch_up(
      providers, application,
      provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 50.0F});
  settle(providers, application);
  EXPECT_NEAR(offset(application, "scroll").x, 130.0F, 0.01F);
}

TEST_F(ScrollTest, ScrollTransitionPhasesFollowTheViewportEdges) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> rows;
  for (int index = 0; index < 12; ++index) {
    rows.push_back(
        components::VStack()
            .frame({.width = 280.0F, .height = row_height})
            .scroll_transition([](composition::VisualEffect content,
                                  composition::ScrollTransitionPhase phase) {
              return content.scale_effect(
                  1.0F - (0.5F * static_cast<float>(std::abs(phase.value()))));
            })
            .id(std::format("r{}", index)));
  }
  Application application =
      start(providers,
            components::ScrollView(
                components::VStack(components::VStackOptions{.spacing = 0.0F},
                                   std::move(rows)))
                .content_margins(theme::Edge::top, 25.0F)
                .frame({.width = 300.0F, .height = 400.0F})
                .id("scroll"));
  frames(application, 2);
  EXPECT_NEAR(presented_of(application, "r0").height, row_height, 0.01F);
  EXPECT_NEAR(presented_of(application, "r6").height, row_height, 0.01F);
  EXPECT_NEAR(presented_of(application, "r7").height, row_height * 0.75F,
              0.01F);
  EXPECT_NEAR(presented_of(application, "r8").height, row_height * 0.5F, 0.01F);
}

TEST_F(ScrollTest, VisualEffectReadsScrollGeometry) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> rows;
  for (int index = 0; index < 12; ++index) {
    rows.push_back(
        components::VStack()
            .frame({.width = 200.0F, .height = row_height})
            .visual_effect([](composition::VisualEffect content,
                              composition::GeometryProxy geometry) {
              return content.offset(
                  geometry.frame(composition::CoordinateSpace::scroll_view).y *
                  0.1F);
            })
            .id(std::format("r{}", index)));
  }
  Application application =
      start(providers,
            components::ScrollView(
                components::VStack(components::VStackOptions{.spacing = 0.0F},
                                   std::move(rows)))
                .frame({.width = 300.0F, .height = 400.0F})
                .id("scroll"));
  frames(application, 2);
  const float shift =
      presented_of(application, "r4").x - frame_of(application, "r4").x;
  EXPECT_NEAR(shift, 4.0F * row_height * 0.1F, 0.01F);
  EXPECT_NEAR(presented_of(application, "r0").x, frame_of(application, "r0").x,
              0.01F);
}

TEST_F(ScrollTest, ContentOutsideTheViewportIsClippedForHits) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(providers, scroller(list(30, log)));
  const provider::PixelPoint hidden = centre(application, "b9");
  EXPECT_GT(hidden.y, frame_of(application, "scroll").y + 400.0F);
  tap(providers, application, hidden);
  EXPECT_EQ(log->taps, 0);
  const auto node = node_of(application, "b9");
  const auto& presented =
      application.root().tree().node(*node)->get().presented();
  ASSERT_TRUE(presented.has_value());
  ASSERT_TRUE(presented->clip.has_value());
  EXPECT_NEAR(presented->clip->height, 400.0F, 0.5F);
}

TEST_F(ScrollTest, ScrollPhaseAndGeometryObserversFire) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers,
            scroller(list(30))
                .on_scroll_phase_change([log](composition::ScrollPhase,
                                              composition::ScrollPhase next) {
                  log->phases.push_back(next);
                })
                .on_scroll_geometry_change(
                    [](const composition::ScrollGeometry& geometry) {
                      return geometry.content_offset.y;
                    },
                    [log](const float&, const float& next) {
                      log->offsets.push_back(next);
                    }));
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 300.0F},
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 200.0F},
       3);
  settle(providers, application);
  ASSERT_FALSE(log->phases.empty());
  EXPECT_EQ(log->phases.front(), composition::ScrollPhase::interacting);
  EXPECT_EQ(log->phases.back(), composition::ScrollPhase::idle);
  ASSERT_FALSE(log->offsets.empty());
  EXPECT_FLOAT_EQ(log->offsets.back(), offset(application, "scroll").y);
}

TEST_F(ScrollTest, FocusMovesDuringAPageAnimationWithoutRecollecting) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(40)));
  frames(application, 2);
  ASSERT_EQ(focused_key(application), "b0");
  const auto& engine = application.interaction().engine();
  providers.input().push(pad({Button::zr}));
  step(providers, application);
  providers.input().push(pad({}));
  step(providers, application);
  const std::uint64_t collections = engine.collections();
  const std::uint64_t refreshes = engine.refreshes();
  for (int index = 0; index < 3; ++index) {
    step(providers, application);
  }
  EXPECT_FALSE(application.waiting());
  EXPECT_EQ(engine.collections(), collections);
  const float moving = offset(application, "scroll").y;
  EXPECT_GT(moving, 0.0F);
  const std::string before = focused_key(application);
  ASSERT_TRUE(before.starts_with("b"));
  const int index = std::stoi(before.substr(1));
  providers.input().push(pad({Button::dpad_down}));
  step(providers, application);
  providers.input().push(pad({}));
  step(providers, application);
  EXPECT_EQ(focused_key(application), std::format("b{}", index + 1));
  EXPECT_EQ(engine.collections(), collections);
  EXPECT_GT(engine.refreshes(), refreshes);
  settle(providers, application);
  EXPECT_EQ(engine.collections(), collections);
  const composition::Rect viewport = presented_of(application, "scroll");
  const composition::Rect shown =
      presented_of(application, focused_key(application));
  EXPECT_GE(shown.y, viewport.y - 0.5F);
  EXPECT_LE(shown.y + shown.height, viewport.y + viewport.height + 0.5F);
}

TEST_F(ScrollTest, ScrollContainersFollowRevisionChanges) {
  provider::headless::HeadlessProviderFactory providers;
  auto shown = std::make_shared<composition::Observable<bool>>(false);
  class Toggling final : public composition::Component {
   public:
    explicit Toggling(std::shared_ptr<composition::Observable<bool>> source)
        : source_(std::move(source)) {}

    auto operator==(const Toggling&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      if (!context.read(*source_)) {
        return components::VStack(row(0));
      }
      return scroller(list(30));
    }

   private:
    std::shared_ptr<composition::Observable<bool>> source_;
  };
  Application application = start(providers, Toggling{shown});
  const auto& scrolls = application.interaction().scrolls();
  EXPECT_EQ(scrolls.containers(), 0U);
  for (int round = 0; round < 2; ++round) {
    shown->set(true);
    frames(application, 3);
    ASSERT_EQ(scrolls.containers(), 1U);
    const auto container = node_of(application, "scroll");
    ASSERT_TRUE(container.has_value());
    EXPECT_TRUE(scrolls.scroller(*container).has_value());
    const composition::Rect viewport = frame_of(application, "scroll");
    drag(providers, application,
         provider::PixelPoint{.x = viewport.x + 150.0F,
                              .y = viewport.y + 300.0F},
         provider::PixelPoint{.x = viewport.x + 150.0F,
                              .y = viewport.y + 200.0F},
         3);
    settle(providers, application);
    EXPECT_GT(offset(application, "scroll").y, 50.0F);
    shown->set(false);
    frames(application, 3);
    EXPECT_EQ(scrolls.containers(), 0U);
    EXPECT_FALSE(node_of(application, "scroll").has_value());
    EXPECT_FALSE(scrolls.scroller(*container).has_value());
  }
}

TEST_F(ScrollTest, LazyRowsEnteringMidFlingStayFocusable) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, scroller(lazy(1000, 0.0F, 0)),
            interaction::InteractionOptions{.focus_scroll_margin = 0.0F});
  frames(application, 3);
  ASSERT_FALSE(node_of(application, "row-12").has_value());
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 380.0F},
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 80.0F},
       3);
  for (int index = 0; index < 4; ++index) {
    step(providers, application);
  }
  EXPECT_FALSE(application.waiting());
  ASSERT_GT(offset(application, "scroll").y, 300.0F);
  const auto& engine = application.interaction().engine();
  const std::uint64_t collections = engine.collections();
  const std::string landed = focused_key(application);
  ASSERT_TRUE(landed.starts_with("row-"));
  int index = std::stoi(landed.substr(4));
  const int from = index;
  while (index < from + 8) {
    providers.input().push(pad({Button::dpad_down}));
    step(providers, application);
    providers.input().push(pad({}));
    step(providers, application);
    ++index;
    EXPECT_EQ(focused_key(application), std::format("row-{}", index));
  }
  EXPECT_GE(index, 10);
  EXPECT_GT(engine.collections(), collections);
  settle(providers, application);
  const composition::Rect shown =
      presented_of(application, focused_key(application));
  const composition::Rect port = presented_of(application, "scroll");
  EXPECT_GE(shown.y, port.y - 0.6F);
  EXPECT_LE(shown.y + shown.height, port.y + port.height + 0.6F);
}

TEST_F(ScrollTest, RecycledRowsAreFoundByTheirNewKeyOnly) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, reading(log, [] { return scroller(lazy(1000, 0.0F, 0)); }));
  frames(application, 3);
  const composition::Tree& tree = application.root().tree();
  const std::uint64_t searches = tree.searches();
  const std::uint64_t recycles = tree.recycles();
  log->proxy->scroll_to("row-300", motion::UnitPoint::top);
  frames(application, 6);
  ASSERT_GT(tree.recycles(), recycles);
  EXPECT_FALSE(node_of(application, "row-0").has_value());
  for (int index = 300; index < 305; ++index) {
    const auto found = node_of(application, std::format("row-{}", index));
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(tree.node(*found)->get().key(),
              composition::ElementKey{std::format("row-{}", index)});
  }
  EXPECT_EQ(tree.searches(), searches);
}

TEST_F(ScrollTest, RightStickScrollsTheFocusedScrollView) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(30)));
  provider::InputFrame tilted = pad({});
  tilted.pads[0].right = provider::Stick{.x = 0.0F, .y = -1.0F};
  for (int index = 0; index < 6; ++index) {
    providers.input().push(tilted);
    step(providers, application);
  }
  EXPECT_GT(offset(application, "scroll").y, 100.0F);
  EXPECT_LT(offset(application, "scroll").y, 200.0F);
}

TEST_F(ScrollTest, IndicatorsFadeOutAndTheLoopRests) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, scroller(list(30)));
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 300.0F},
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 250.0F},
       2);
  const auto node = node_of(application, "scroll");
  EXPECT_FLOAT_EQ(application.root()
                      .tree()
                      .node(*node)
                      ->get()
                      .scroll_presentation()
                      .indicators,
                  1.0F);
  settle(providers, application);
  EXPECT_FLOAT_EQ(application.root()
                      .tree()
                      .node(*node)
                      ->get()
                      .scroll_presentation()
                      .indicators,
                  0.0F);
  EXPECT_TRUE(application.waiting());
}

TEST_F(ScrollTest, ScrollDisabledIgnoresDrags) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, scroller(list(30)).scroll_disabled());
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 300.0F},
       provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + 100.0F},
       3);
  settle(providers, application);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 0.0F);
}

TEST_F(ScrollTest, LazyFocusRestoresByKeyAcrossReloads) {
  provider::headless::HeadlessProviderFactory providers;
  auto generation = std::make_shared<composition::Observable<int>>(0);
  class Reloading final : public composition::Component {
   public:
    explicit Reloading(std::shared_ptr<composition::Observable<int>> source)
        : source_(std::move(source)) {}

    auto operator==(const Reloading&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const int load = context.read(*source_);
      return scroller(components::LazyVStack(
          components::LazyVStackOptions{.spacing = 0.0F,
                                        .estimated_extent = 40.0F},
          components::ForEach(
              std::views::iota(0, 200),
              [](int index) { return std::format("row-{}", index); },
              [load](int index) {
                return components::Button([index] { static_cast<void>(index); },
                                          components::Text(std::format(
                                              "Row {} load {}", index, load)))
                    .frame({.width = 280.0F, .height = 40.0F});
              })));
    }

   private:
    std::shared_ptr<composition::Observable<int>> source_;
  };
  Application application = start(providers, Reloading{generation});
  for (int move = 0; move < 3; ++move) {
    providers.input().push(pad({Button::dpad_down}));
    providers.input().push(pad({}));
    frames(application, 2);
  }
  EXPECT_EQ(focused_key(application), "row-3");
  const auto before = node_of(application, "row-3");
  generation->set(1);
  frames(application, 3);
  EXPECT_TRUE(shows(application, "Row 3 load 1"));
  EXPECT_EQ(focused_key(application), "row-3");
  EXPECT_EQ(node_of(application, "row-3"), before);
}

TEST_F(ScrollTest, StaticChildrenSitAroundLazyRowsLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  const auto block = [](float width, float height, std::string key) {
    return components::ZStack()
        .frame({.width = width, .height = height})
        .id(std::move(key));
  };
  Application application =
      start(providers,
            components::VStack(
                components::LazyVStack(
                    components::LazyVStackOptions{
                        .alignment = components::HorizontalAlignment::leading,
                        .spacing = 4.0F},
                    block(40.0F, 10.0F, "h"),
                    components::ForEach(
                        std::views::iota(0, 3),
                        [](int index) { return std::format("r{}", index); },
                        [](int) {
                          return components::ZStack().frame(
                              {.width = 60.0F, .height = 20.0F});
                        }),
                    block(30.0F, 10.0F, "f"))
                    .frame({.width = 200.0F})
                    .id("column"),
                components::LazyHStack(
                    components::LazyHStackOptions{
                        .alignment = components::VerticalAlignment::top,
                        .spacing = 4.0F},
                    block(10.0F, 40.0F, "hh"),
                    components::ForEach(
                        std::views::iota(0, 2),
                        [](int index) { return std::format("q{}", index); },
                        [](int) {
                          return components::ZStack().frame(
                              {.width = 20.0F, .height = 60.0F});
                        }))
                    .frame({.height = 100.0F})
                    .id("row")));
  frames(application, 3);
  const composition::Rect column = frame_of(application, "column");
  EXPECT_FLOAT_EQ(column.width, 200.0F);
  EXPECT_FLOAT_EQ(column.height, 96.0F);
  for (const auto& [key, y] :
       {std::pair{"h", 0.0F}, std::pair{"r0", 14.0F}, std::pair{"r1", 38.0F},
        std::pair{"r2", 62.0F}, std::pair{"f", 86.0F}}) {
    EXPECT_FLOAT_EQ(frame_of(application, key).x - column.x, 0.0F) << key;
    EXPECT_FLOAT_EQ(frame_of(application, key).y - column.y, y) << key;
  }
  const composition::Rect row = frame_of(application, "row");
  EXPECT_FLOAT_EQ(row.width, 58.0F);
  EXPECT_FLOAT_EQ(row.height, 100.0F);
  for (const auto& [key, x] : {std::pair{"hh", 0.0F}, std::pair{"q0", 14.0F},
                               std::pair{"q1", 38.0F}}) {
    EXPECT_FLOAT_EQ(frame_of(application, key).x - row.x, x) << key;
    EXPECT_FLOAT_EQ(frame_of(application, key).y - row.y, 0.0F) << key;
  }
}

TEST_F(ScrollTest, StaticChildrenStayBuiltWhileForEachRowsStayLazy) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, reading(log, [] {
        return scroller(components::LazyVStack(
            components::LazyVStackOptions{.spacing = 0.0F,
                                          .overscan = 0.0F,
                                          .retention = 2,
                                          .estimated_extent = 40.0F},
            components::ZStack()
                .frame({.width = 280.0F, .height = 30.0F})
                .id("header"),
            components::ForEach(
                std::views::iota(0, 500),
                [](int index) { return std::format("row-{}", index); },
                [](int index) {
                  return components::Button(std::format("Lazy {}", index),
                                            [] {})
                      .frame({.width = 280.0F, .height = 40.0F});
                }),
            components::GridSection(
                components::ZStack()
                    .frame({.width = 280.0F, .height = 30.0F})
                    .id("section-header"),
                components::ForEach(
                    std::views::iota(0, 50),
                    [](int index) { return std::format("tail-{}", index); },
                    [](int) {
                      return components::ZStack().frame(
                          {.width = 280.0F, .height = 40.0F});
                    }))));
      }));
  frames(application, 3);
  EXPECT_TRUE(node_of(application, "header").has_value());
  EXPECT_TRUE(node_of(application, "row-0").has_value());
  EXPECT_FALSE(node_of(application, "row-300").has_value());
  EXPECT_LT(count_keyed(application.root().tree().root(), "row-"), 40U);
  EXPECT_LT(count_keyed(application.root().tree().root(), "tail-"), 20U);
  log->proxy->scroll_to("row-450", motion::UnitPoint::top);
  frames(application, 6);
  EXPECT_TRUE(node_of(application, "row-450").has_value());
  EXPECT_FALSE(node_of(application, "row-0").has_value());
  EXPECT_LT(count_keyed(application.root().tree().root(), "row-"), 40U);
}

}
