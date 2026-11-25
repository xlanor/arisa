#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/scroll_anchor_role.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_deceleration_rate.hpp>
#include <arisa/composition/scroll_geometry.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/scroll_phase.hpp>
#include <arisa/composition/scroll_phase_change_context.hpp>
#include <arisa/composition/scroll_position.hpp>
#include <arisa/composition/scroll_target.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/scroll_content_offset_adjustment_behavior.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Axes;
using composition::Axis;
using composition::Element;
using composition::ScrollIndicatorVisibility;

class ScrollModifierTest : public ScrollFixture {
 protected:
  static constexpr float row_height = 50.0F;

  static auto list(int count) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < count; ++index) {
      rows.push_back(components::Button(std::format("Row {}", index), [] {})
                         .frame({.width = 280.0F, .height = row_height})
                         .id(std::format("b{}", index)));
    }
    return components::VStack(components::VStackOptions{.spacing = 0.0F},
                              std::move(rows));
  }

  static auto scroller(Element content) -> Element {
    return components::ScrollView(std::move(content))
        .frame({.width = 300.0F, .height = 400.0F})
        .id("scroll");
  }

  static auto presentation(Application& application)
      -> composition::ScrollPresentation {
    const auto node = node_named(application, "scroll");
    if (!node.has_value()) {
      return composition::ScrollPresentation{};
    }
    return node->get().scroll_presentation();
  }

  static auto limit(Application& application) -> composition::Point {
    const auto node = node_of(application, "scroll");
    if (!node.has_value()) {
      ADD_FAILURE() << "no node keyed scroll";
      return composition::Point{};
    }
    const auto scroller = application.interaction().scrolls().scroller(*node);
    if (!scroller.has_value()) {
      ADD_FAILURE() << "scroll is not a scroller";
      return composition::Point{};
    }
    return scroller->get().limit();
  }

  static void pull(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, float from_y, float to_y,
                   int steps, bool release = true) {
    const composition::Rect viewport = frame_of(application, "scroll");
    drag(providers, application,
         provider::PixelPoint{.x = viewport.x + 150.0F,
                              .y = viewport.y + from_y},
         provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + to_y},
         steps, release);
  }

  template <class T>
  class Holder final : public composition::Component {
   public:
    using Make = std::function<Element(composition::StateHandle<T>)>;

    Holder(const composition::StateKey<T>* key,
           std::shared_ptr<std::optional<composition::StateHandle<T>>> seen,
           std::shared_ptr<const Make> make) noexcept
        : key_(key), seen_(std::move(seen)), make_(std::move(make)) {}

    auto operator==(const Holder& other) const -> bool {
      return seen_ == other.seen_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto state = context.state(*key_);
      *seen_ = state;
      return (*make_)(state);
    }

   private:
    const composition::StateKey<T>* key_;
    std::shared_ptr<std::optional<composition::StateHandle<T>>> seen_;
    std::shared_ptr<const Make> make_;
  };

  template <class T, class Make>
  static auto holding(
      const composition::StateKey<T>& key,
      std::shared_ptr<std::optional<composition::StateHandle<T>>> seen,
      Make make) -> Element {
    return Holder<T>{
        &key, std::move(seen),
        std::make_shared<const typename Holder<T>::Make>(std::move(make))};
  }

  struct EveryHundred {
    std::shared_ptr<int> calls;

    void update_target(composition::ScrollTarget& target,
                       const composition::ScrollTargetContext& context) const {
      ++*calls;
      EXPECT_FLOAT_EQ(context.container_size.height, 400.0F);
      target.rect.y = std::round(target.rect.y / 100.0F) * 100.0F;
    }

    friend auto operator==(const EveryHundred&, const EveryHundred&)
        -> bool = default;
  };

  static auto backgrounds(const composition::Node& node) -> int {
    int found = node.modifiers().background.has_value() ? 1 : 0;
    for (const composition::Node& child : node.children()) {
      found += backgrounds(child);
    }
    return found;
  }

  static auto scroll_node(const composition::Node& node)
      -> std::optional<std::reference_wrapper<const composition::Node>> {
    if (node.content_as<components::ScrollView::Content>().has_value()) {
      return std::cref(node);
    }
    for (const composition::Node& child : node.children()) {
      if (const auto found = scroll_node(child)) {
        return found;
      }
    }
    return std::nullopt;
  }

  static auto prefixed(int prefix) -> Element {
    std::vector<Element> rows;
    for (int index = 0; index < prefix; ++index) {
      rows.push_back(components::Button(std::format("Prefix {}", index), [] {})
                         .frame({.width = 280.0F, .height = row_height})
                         .id(std::format("p{}", index)));
    }
    for (int index = 0; index < 30; ++index) {
      rows.push_back(components::Button(std::format("Row {}", index), [] {})
                         .frame({.width = 280.0F, .height = row_height})
                         .id(std::format("b{}", index)));
    }
    return scroller(components::VStack(
        components::VStackOptions{.spacing = 0.0F}, std::move(rows)));
  }

  static auto grown(motion::ScrollContentOffsetAdjustmentBehavior behavior)
      -> std::pair<float, float> {
    provider::headless::HeadlessProviderFactory providers;
    auto seen =
        std::make_shared<std::optional<composition::StateHandle<int>>>();
    Application application = start(
        providers, holding(counter, seen,
                           [](auto count) { return prefixed(count.get()); }));
    pull(providers, application, 350.0F, 150.0F, 8);
    settle(providers, application);
    const float before = offset(application, "scroll").y;
    motion::Transaction transaction;
    transaction.set_scroll_content_offset_adjustment_behavior(behavior);
    motion::with_transaction(transaction, [&seen] { (*seen)->set(2); });
    settle(providers, application);
    return {before, offset(application, "scroll").y};
  }

  static constexpr composition::StateKey<int> counter{"counter", 0};
  static constexpr composition::StateKey<int> rows_key{"rows", 30};
  static const composition::StateKey<composition::ScrollPosition> placed_key;
};

const composition::StateKey<composition::ScrollPosition>
    ScrollModifierTest::placed_key{"placed", composition::ScrollPosition{}};

TEST_F(ScrollModifierTest,
       IndicatorsHiddenFromAnAncestorLeaveOnlyTheOtherAxis) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, components::VStack(scroller(list(30)))
                           .scroll_indicators(ScrollIndicatorVisibility::hidden,
                                              Axes::vertical));
  pull(providers, application, 300.0F, 250.0F, 2);
  const composition::ScrollPresentation shown = presentation(application);
  EXPECT_FLOAT_EQ(shown.indicators, 1.0F);
  EXPECT_FALSE(shown.indicator_axes.contains(Axis::vertical));
  EXPECT_TRUE(shown.indicator_axes.contains(Axis::horizontal));
}

TEST_F(ScrollModifierTest, ScrollClipDisabledLetsContentDrawPastTheViewport) {
  const auto clip_height = [](bool disabled) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        components::VStack(components::ScrollView(
                               components::VStack()
                                   .frame({.width = 80.0F, .height = 400.0F})
                                   .id("content"))
                               .scroll_clip_disabled(disabled)
                               .frame({.width = 100.0F, .height = 100.0F})
                               .id("scroller")));
    frames(application, 3);
    const auto node = node_named(application, "content");
    if (!node.has_value() || !node->get().presented().has_value() ||
        !node->get().presented()->clip.has_value()) {
      return 0.0F;
    }
    return node->get().presented()->clip->height;
  };
  const float clipped = clip_height(false);
  EXPECT_NEAR(clipped, 100.0F, 1.0F);
  EXPECT_GT(clip_height(true), clipped + 50.0F);
}

TEST_F(ScrollModifierTest, TheInnermostIndicatorVisibilityWins) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(scroller(list(30)).scroll_indicators(
                                        ScrollIndicatorVisibility::visible))
                     .scroll_indicators(ScrollIndicatorVisibility::never));
  EXPECT_TRUE(
      presentation(application).indicator_axes.contains(Axis::vertical));
}

TEST_F(ScrollModifierTest, IndicatorsFlashOnAppearAndWhenTheTriggerChanges) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::optional<composition::StateHandle<int>>>();
  Application application =
      start(providers, holding(counter, seen, [](auto count) {
              return scroller(list(30))
                  .scroll_indicators_flash(true)
                  .scroll_indicators_flash(count.get());
            }));
  step(providers, application);
  EXPECT_FLOAT_EQ(presentation(application).indicators, 0.0F);
  provider::headless::HeadlessProviderFactory again;
  Application appearing =
      start(again, scroller(list(30)).scroll_indicators_flash(true));
  step(again, appearing);
  EXPECT_FLOAT_EQ(presentation(appearing).indicators, 1.0F);
  settle(again, appearing);
  EXPECT_FLOAT_EQ(presentation(appearing).indicators, 0.0F);
  (*seen)->set(1);
  step(providers, application);
  step(providers, application);
  EXPECT_FLOAT_EQ(presentation(application).indicators, 1.0F);
  settle(providers, application);
  EXPECT_FLOAT_EQ(presentation(application).indicators, 0.0F);
}

TEST_F(ScrollModifierTest, BounceBasedOnSizeKeepsShortContentStill) {
  provider::headless::HeadlessProviderFactory providers;
  Application bouncing = start(providers, scroller(list(3)));
  pull(providers, bouncing, 50.0F, 250.0F, 5, false);
  EXPECT_LT(offset(bouncing, "scroll").y, 0.0F);
  provider::headless::HeadlessProviderFactory still;
  Application sized =
      start(still, scroller(list(3)).scroll_bounce_behavior(
                       composition::ScrollBounceBehavior::based_on_size));
  pull(still, sized, 50.0F, 250.0F, 5, false);
  EXPECT_FLOAT_EQ(offset(sized, "scroll").y, 0.0F);
  provider::headless::HeadlessProviderFactory tall;
  Application long_list =
      start(tall, scroller(list(30)).scroll_bounce_behavior(
                      composition::ScrollBounceBehavior::based_on_size));
  pull(tall, long_list, 50.0F, 250.0F, 5, false);
  EXPECT_LT(offset(long_list, "scroll").y, 0.0F);
}

TEST_F(ScrollModifierTest, AFastDecelerationRateStopsAFlingSooner) {
  const auto fling =
      [](std::optional<composition::ScrollDecelerationRate> rate) {
        provider::headless::HeadlessProviderFactory providers;
        Element view = scroller(list(400));
        if (rate.has_value()) {
          view = std::move(view).scroll_deceleration_rate(*rate);
        }
        Application application = start(providers, std::move(view));
        pull(providers, application, 350.0F, 150.0F, 4);
        settle(providers, application);
        return offset(application, "scroll").y;
      };
  const float normal = fling(std::nullopt);
  const float fast = fling(composition::ScrollDecelerationRate::fast());
  EXPECT_GT(normal, fast + 100.0F);
  EXPECT_GT(fast, 200.0F);
}

TEST_F(ScrollModifierTest,
       ABottomAnchorStartsAtTheEndAndKeepsItAsContentGrows) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::optional<composition::StateHandle<int>>>();
  Application application =
      start(providers, holding(rows_key, seen, [](auto count) {
              return scroller(list(count.get()))
                  .default_scroll_anchor(motion::UnitPoint::bottom);
            }));
  step(providers, application);
  const float first = limit(application).y;
  EXPECT_GT(first, 1000.0F);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, first);
  (*seen)->set(34);
  settle(providers, application);
  EXPECT_NEAR(limit(application).y, first + (4.0F * row_height), 0.5F);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, limit(application).y);
}

TEST_F(ScrollModifierTest, AnInitialOffsetAnchorAloneDoesNotFollowGrowth) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::optional<composition::StateHandle<int>>>();
  Application application =
      start(providers, holding(rows_key, seen, [](auto count) {
              return scroller(list(count.get()))
                  .default_scroll_anchor(
                      motion::UnitPoint::bottom,
                      composition::ScrollAnchorRole::initial_offset);
            }));
  step(providers, application);
  const float first = offset(application, "scroll").y;
  EXPECT_FLOAT_EQ(first, limit(application).y);
  (*seen)->set(34);
  settle(providers, application);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, first);
}

TEST_F(ScrollModifierTest, TheAlignmentAnchorPlacesShortContentAtTheBottom) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      scroller(list(3)).default_scroll_anchor(
          motion::UnitPoint::bottom, composition::ScrollAnchorRole::alignment));
  step(providers, application);
  const composition::Rect viewport = presented_of(application, "scroll");
  const composition::Rect last = presented_of(application, "b2");
  EXPECT_NEAR(last.y + last.height, viewport.y + viewport.height, 0.5F);
  EXPECT_FLOAT_EQ(presentation(application).offset.y,
                  -(400.0F - (3.0F * row_height)));
}

TEST_F(ScrollModifierTest, ACustomTargetBehaviorChoosesWhereAFlingLands) {
  provider::headless::HeadlessProviderFactory providers;
  auto calls = std::make_shared<int>(0);
  Application application = start(
      providers,
      scroller(list(200)).scroll_target_behavior(EveryHundred{.calls = calls}));
  pull(providers, application, 350.0F, 180.0F, 4);
  settle(providers, application);
  const float landed = offset(application, "scroll").y;
  EXPECT_GT(*calls, 0);
  EXPECT_GT(landed, 150.0F);
  EXPECT_NEAR(std::fmod(landed, 100.0F), 0.0F, 0.01F);
}

TEST_F(ScrollModifierTest, AScrollPositionScrollsToEdgesViewsAndPoints) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<
      std::optional<composition::StateHandle<composition::ScrollPosition>>>();
  Application application =
      start(providers, holding(placed_key, seen, [](auto placed) {
              return scroller(list(30)).scroll_position(
                  composition::Binding<composition::ScrollPosition>{placed});
            }));
  step(providers, application);
  const auto move = [&](auto change) {
    composition::ScrollPosition next = (*seen)->get();
    change(next);
    (*seen)->set(next);
    settle(providers, application);
  };
  move([](composition::ScrollPosition& next) {
    next.scroll_to(theme::Edge::bottom);
  });
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, limit(application).y);
  move([](composition::ScrollPosition& next) {
    next.scroll_to(composition::ElementKey{"b10"});
  });
  EXPECT_NEAR(offset(application, "scroll").y, 10.0F * row_height, 0.5F);
  move([](composition::ScrollPosition& next) { next.scroll_to_y(120.0F); });
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 120.0F);
  EXPECT_FALSE((*seen)->get().is_positioned_by_user());
  pull(providers, application, 300.0F, 200.0F, 3);
  settle(providers, application);
  const composition::ScrollPosition written = (*seen)->get();
  EXPECT_TRUE(written.is_positioned_by_user());
  ASSERT_TRUE(written.point().has_value());
  EXPECT_NEAR(written.point()->y, offset(application, "scroll").y, 0.5F);
}

TEST_F(ScrollModifierTest, ASingleAxisScrollPositionMovesOnlyThatAxis) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<
      std::optional<composition::StateHandle<composition::ScrollPosition>>>();
  Application application =
      start(providers, holding(placed_key, seen, [](auto placed) {
              return scroller(list(30)).scroll_position(
                  composition::Binding<composition::ScrollPosition>{placed});
            }));
  step(providers, application);
  const auto place = [&](composition::ScrollPosition next) {
    (*seen)->set(std::move(next));
    settle(providers, application);
  };
  place(composition::ScrollPosition{Axis::vertical, 120.0F});
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 120.0F);
  EXPECT_FLOAT_EQ(offset(application, "scroll").x, 0.0F);
  place(composition::ScrollPosition{Axis::vertical, 260.0F});
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 260.0F);
  place(composition::ScrollPosition{Axis::horizontal, 0.0F});
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 260.0F);
  EXPECT_FALSE((*seen)->get().is_positioned_by_user());
}

TEST_F(ScrollModifierTest, ScrollGeometryReportsTheContentInsets) {
  provider::headless::HeadlessProviderFactory providers;
  auto latest = std::make_shared<std::optional<composition::ScrollGeometry>>();
  Application application =
      start(providers,
            scroller(list(30))
                .content_margins(theme::Edge::vertical, theme::Space{20.0F})
                .content_margins(theme::Edge::leading, theme::Space{10.0F})
                .on_scroll_geometry_change(
                    [](const composition::ScrollGeometry& geometry) {
                      return geometry;
                    },
                    [latest](const composition::ScrollGeometry&,
                             const composition::ScrollGeometry& next) {
                      *latest = next;
                    }));
  pull(providers, application, 300.0F, 200.0F, 3);
  settle(providers, application);
  ASSERT_TRUE(latest->has_value());
  const composition::ScrollGeometry& geometry = **latest;
  EXPECT_FLOAT_EQ(geometry.content_insets.top, 20.0F);
  EXPECT_FLOAT_EQ(geometry.content_insets.leading, 10.0F);
  EXPECT_FLOAT_EQ(geometry.content_insets.bottom, 20.0F);
  EXPECT_FLOAT_EQ(geometry.content_insets.trailing, 0.0F);
  EXPECT_EQ(geometry.bounds(),
            (composition::Rect{.x = geometry.content_offset.x,
                               .y = geometry.content_offset.y,
                               .width = geometry.container_size.width,
                               .height = geometry.container_size.height}));
  EXPECT_FLOAT_EQ(geometry.content_offset.y,
                  offset(application, "scroll").y - 20.0F);
  EXPECT_FLOAT_EQ(geometry.content_offset.x, -10.0F);
  EXPECT_FLOAT_EQ(geometry.container_size.width, 300.0F - 10.0F);
  EXPECT_FLOAT_EQ(geometry.container_size.height, 400.0F - 40.0F);
  EXPECT_FLOAT_EQ(geometry.content_size.height, 30.0F * row_height);
  EXPECT_FLOAT_EQ(geometry.visible_rect.height, 400.0F);
  EXPECT_FLOAT_EQ(geometry.visible_rect.y, geometry.content_offset.y);
}

TEST_F(ScrollModifierTest, AnAnimatedScrollPositionGlidesToItsTarget) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<
      std::optional<composition::StateHandle<composition::ScrollPosition>>>();
  Application application =
      start(providers, holding(placed_key, seen, [](auto placed) {
              return scroller(list(30)).scroll_position(
                  composition::Binding<composition::ScrollPosition>{placed});
            }));
  step(providers, application);
  motion::with_animation(motion::Animation::linear(1.0), [&seen] {
    composition::ScrollPosition next = (*seen)->get();
    next.scroll_to_y(400.0F);
    (*seen)->set(next);
  });
  step(providers, application);
  step(providers, application, std::chrono::milliseconds{500});
  const float halfway = offset(application, "scroll").y;
  EXPECT_GT(halfway, 120.0F);
  EXPECT_LT(halfway, 300.0F);
  settle(providers, application);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 400.0F);
  (*seen)->set(
      composition::ScrollPosition{composition::Point{.x = 0.0F, .y = 40.0F}});
  step(providers, application);
  step(providers, application);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 40.0F);
}

TEST_F(ScrollModifierTest,
       ScrollDisabledReachesScrollViewsThroughTheHierarchy) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(scroller(list(30))).scroll_disabled());
  pull(providers, application, 300.0F, 100.0F, 3);
  settle(providers, application);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 0.0F);
  provider::headless::HeadlessProviderFactory listed;
  std::vector<Element> rows;
  for (int index = 0; index < 40; ++index) {
    rows.push_back(components::Text(std::format("Item {}", index)));
  }
  Application list_application =
      start(listed, structure::List(std::move(rows))
                        .scroll_disabled()
                        .frame({.width = 300.0F, .height = 300.0F})
                        .id("list"));
  const auto node = scroll_node(list_application.root().tree().root());
  ASSERT_TRUE(node.has_value());
  EXPECT_FALSE(list_application.interaction()
                   .scrolls()
                   .scrollable(node->get().id())
                   .has_value());
}

TEST_F(ScrollModifierTest, ATwoAxisScrollViewLeavesItsContentUnconstrained) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> lines;
  for (int row = 0; row < 9; ++row) {
    std::vector<Element> tiles;
    for (int column = 0; column < 12; ++column) {
      tiles.push_back(components::VStack()
                          .frame({.width = 100.0F, .height = 100.0F})
                          .id(std::format("t{}-{}", row, column)));
    }
    lines.push_back(components::HStack(
        components::HStackOptions{.spacing = 0.0F}, std::move(tiles)));
  }
  Application application = start(
      providers, components::ScrollView(
                     Axes::all, components::VStack(
                                    components::VStackOptions{.spacing = 0.0F},
                                    std::move(lines)))
                     .frame({.width = 300.0F, .height = 400.0F})
                     .id("scroll"));
  step(providers, application);
  EXPECT_FLOAT_EQ(limit(application).x, 1200.0F - 300.0F);
  EXPECT_FLOAT_EQ(limit(application).y, 900.0F - 400.0F);
  const composition::Rect viewport = frame_of(application, "scroll");
  drag(providers, application,
       provider::PixelPoint{.x = viewport.x + 250.0F, .y = viewport.y + 350.0F},
       provider::PixelPoint{.x = viewport.x + 100.0F, .y = viewport.y + 200.0F},
       4, false);
  EXPECT_GT(offset(application, "scroll").x, 100.0F);
  EXPECT_GT(offset(application, "scroll").y, 100.0F);
  step(providers, application, 200ms);
  touch_up(
      providers, application,
      provider::PixelPoint{.x = viewport.x + 100.0F, .y = viewport.y + 200.0F});
  settle(providers, application);
  const composition::Point dragged = offset(application, "scroll");
  provider::InputFrame tilted = pad({});
  tilted.pads[0].right = provider::Stick{.x = 1.0F, .y = -1.0F};
  for (int index = 0; index < 6; ++index) {
    providers.input().push(tilted);
    step(providers, application);
  }
  EXPECT_GT(offset(application, "scroll").x, dragged.x + 20.0F);
  EXPECT_GT(offset(application, "scroll").y, dragged.y + 20.0F);
}

TEST_F(ScrollModifierTest,
       ContentBackgroundShowsOrHidesTheScrollableBackground) {
  const auto listed = [](std::optional<composition::Visibility> visibility) {
    provider::headless::HeadlessProviderFactory providers;
    Element view = structure::List(components::Text("One"))
                       .list_style(structure::InsetGroupedListStyle{});
    if (visibility.has_value()) {
      view = std::move(view).scroll_content_background(*visibility);
    }
    Application application = start(providers, std::move(view));
    const auto node = scroll_node(application.root().tree().root());
    return node.has_value() && node->get().modifiers().background.has_value();
  };
  EXPECT_TRUE(listed(std::nullopt));
  EXPECT_TRUE(listed(composition::Visibility::visible));
  EXPECT_FALSE(listed(composition::Visibility::hidden));
  const auto edited = [](std::optional<composition::Visibility> visibility) {
    provider::headless::HeadlessProviderFactory providers;
    Element view = controls::TextEditor(
                       composition::Binding<std::string>::constant("Notes"))
                       .frame({.width = 200.0F, .height = 100.0F})
                       .id("editor");
    if (visibility.has_value()) {
      view = std::move(view).scroll_content_background(*visibility);
    }
    Application application = start(providers, std::move(view));
    return backgrounds(application.root().tree().root());
  };
  EXPECT_EQ(edited(composition::Visibility::hidden) + 1, edited(std::nullopt));
}

TEST_F(ScrollModifierTest,
       AnAutomaticListBackgroundMatchesSwiftUIAndStaysClearUnderGlass) {
  using composition::Visibility;
  const theme::Theme opaque = theme::Theme::defaults();
  const theme::Theme glass =
      opaque.with(theme::GlassDefaults{.sidebar = theme::Glass::regular(),
                                       .toolbar = std::nullopt,
                                       .sheet = std::nullopt});
  const structure::ListStyle plain = structure::ListStyle::plain();
  const structure::ListStyle inset = structure::ListStyle::inset();
  const structure::ListStyle grouped = structure::ListStyle::inset_grouped();
  const structure::ListStyle sidebar = structure::ListStyle::sidebar();
  EXPECT_EQ(plain.content_background(Visibility::automatic, opaque),
            theme::ColourRole::system_background);
  EXPECT_EQ(inset.content_background(Visibility::automatic, opaque),
            theme::ColourRole::system_background);
  EXPECT_EQ(grouped.content_background(Visibility::automatic, opaque),
            theme::ColourRole::system_grouped_background);
  EXPECT_EQ(sidebar.content_background(Visibility::automatic, opaque),
            std::nullopt);
  EXPECT_EQ(plain.content_background(Visibility::hidden, opaque), std::nullopt);
  EXPECT_EQ(opaque.colour(theme::ColourRole::system_background,
                          theme::ColorScheme::light),
            theme::Colour::hex(0xFFFFFF));
  EXPECT_EQ(inset.content_background(Visibility::automatic, glass),
            std::nullopt);
  EXPECT_EQ(grouped.content_background(Visibility::automatic, glass),
            std::nullopt);
  EXPECT_EQ(grouped.content_background(Visibility::visible, glass),
            theme::ColourRole::system_grouped_background);
  EXPECT_EQ(structure::ListStyle::bordered().content_background(
                Visibility::hidden, glass),
            theme::ColourRole::system_background);
  const auto painted = [](structure::ListStyle style) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(
        providers,
        structure::List(components::Text("One")).list_style(std::move(style)));
    const auto node = scroll_node(application.root().tree().root());
    return node.has_value() && node->get().modifiers().background.has_value()
               ? std::optional{node->get().modifiers().background->fill}
               : std::nullopt;
  };
  EXPECT_EQ(painted(structure::ListStyle::plain()),
            graphics::ShapeStyle{theme::ColourRole::system_background});
  EXPECT_EQ(painted(structure::ListStyle::inset_grouped()),
            graphics::ShapeStyle{theme::ColourRole::system_grouped_background});
  EXPECT_EQ(painted(structure::ListStyle::sidebar()), std::nullopt);
}

TEST_F(ScrollModifierTest, PhaseChangesCarryGeometryAndVelocity) {
  struct Seen {
    composition::ScrollPhase before;
    composition::ScrollPhase after;
    composition::ScrollPhaseChangeContext context;
  };
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<Seen>>();
  auto plain = std::make_shared<int>(0);
  Application application = start(
      providers,
      scroller(list(30))
          .on_scroll_phase_change(
              [seen](composition::ScrollPhase before,
                     composition::ScrollPhase after,
                     const composition::ScrollPhaseChangeContext& context) {
                seen->push_back(
                    Seen{.before = before, .after = after, .context = context});
              })
          .on_scroll_phase_change(
              [plain](composition::ScrollPhase, composition::ScrollPhase) {
                ++*plain;
              }));
  pull(providers, application, 350.0F, 100.0F, 3);
  settle(providers, application);
  ASSERT_GE(seen->size(), 2U);
  EXPECT_EQ(static_cast<int>(seen->size()), *plain);
  EXPECT_EQ(seen->front().after, composition::ScrollPhase::interacting);
  ASSERT_TRUE(seen->front().context.velocity.has_value());
  bool decelerated = false;
  for (const Seen& each : *seen) {
    if (each.after == composition::ScrollPhase::decelerating) {
      decelerated = true;
      ASSERT_TRUE(each.context.velocity.has_value());
      EXPECT_GT(each.context.velocity->height, 0.0F);
    }
  }
  EXPECT_TRUE(decelerated);
  const Seen& last = seen->back();
  EXPECT_EQ(last.after, composition::ScrollPhase::idle);
  EXPECT_FALSE(last.context.velocity.has_value());
  EXPECT_FLOAT_EQ(last.context.geometry.content_offset.y,
                  offset(application, "scroll").y);
  EXPECT_FLOAT_EQ(last.context.geometry.container_size.height, 400.0F);
}

TEST_F(ScrollModifierTest,
       ScrollPositionAnchorsComeFromTheModifierOrTransaction) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<
      std::optional<composition::StateHandle<composition::ScrollPosition>>>();
  Application application =
      start(providers, holding(placed_key, seen, [](auto placed) {
              return scroller(list(30)).scroll_position(
                  composition::Binding<composition::ScrollPosition>{placed},
                  motion::UnitPoint::bottom);
            }));
  step(providers, application);
  composition::ScrollPosition next = (*seen)->get();
  next.scroll_to(composition::ElementKey{"b10"});
  (*seen)->set(next);
  settle(providers, application);
  EXPECT_NEAR(offset(application, "scroll").y, (11.0F * row_height) - 400.0F,
              0.5F);
  next.scroll_to(composition::ElementKey{"b20"});
  motion::Transaction centred;
  centred.set_scroll_target_anchor(motion::UnitPoint::center);
  motion::with_transaction(centred, [&seen, &next] { (*seen)->set(next); });
  settle(providers, application);
  EXPECT_NEAR(offset(application, "scroll").y,
              (20.0F * row_height) + (row_height * 0.5F) - 200.0F, 0.5F);
  next.scroll_to(composition::ElementKey{"b4"}, motion::UnitPoint::top);
  motion::with_transaction(centred, [&seen, &next] { (*seen)->set(next); });
  settle(providers, application);
  EXPECT_NEAR(offset(application, "scroll").y, 4.0F * row_height, 0.5F);
}

TEST_F(ScrollModifierTest, AnIdBindingFromAnyModelScrollsAndIsWrittenBack) {
  provider::headless::HeadlessProviderFactory providers;
  auto model = std::make_shared<std::optional<std::string>>();
  Application application =
      start(providers, scroller(list(30)).scroll_position(
                           composition::Binding<std::optional<std::string>>{
                               [model] { return *model; },
                               [model](std::optional<std::string> value) {
                                 *model = std::move(value);
                               }},
                           motion::UnitPoint::top));
  step(providers, application);
  *model = "b12";
  settle(providers, application);
  EXPECT_NEAR(offset(application, "scroll").y, 12.0F * row_height, 0.5F);
  pull(providers, application, 300.0F, 250.0F, 4);
  settle(providers, application);
  ASSERT_TRUE(model->has_value());
  EXPECT_NE(**model, "b12");
}

TEST_F(ScrollModifierTest, APreservingPositionUpdateKeepsTheFlingGoing) {
  const auto flung = [](bool preserves) {
    provider::headless::HeadlessProviderFactory providers;
    auto seen = std::make_shared<
        std::optional<composition::StateHandle<composition::ScrollPosition>>>();
    Application application =
        start(providers, holding(placed_key, seen, [](auto placed) {
                return scroller(list(60)).scroll_position(
                    composition::Binding<composition::ScrollPosition>{placed});
              }));
    pull(providers, application, 350.0F, 100.0F, 3);
    step(providers, application);
    motion::Transaction transaction;
    transaction.set_scroll_position_update_preserves_velocity(preserves);
    motion::with_transaction(transaction, [&seen] {
      composition::ScrollPosition next = (*seen)->get();
      next.scroll_to_y(100.0F);
      (*seen)->set(next);
    });
    step(providers, application);
    step(providers, application);
    const auto node = node_of(application, "scroll");
    const bool moving = node.has_value() && application.interaction()
                                                .scrolls()
                                                .scroller(*node)
                                                ->get()
                                                .decelerating();
    const float jumped = offset(application, "scroll").y;
    return std::pair{moving, jumped};
  };
  const auto [kept, kept_at] = flung(true);
  EXPECT_TRUE(kept);
  EXPECT_GT(kept_at, 100.0F);
  const auto [stopped, stopped_at] = flung(false);
  EXPECT_FALSE(stopped);
  EXPECT_FLOAT_EQ(stopped_at, 100.0F);
}

TEST_F(ScrollModifierTest, AutomaticOffsetAdjustmentKeepsVisibleContentSteady) {
  const auto [steady_before, steady_after] =
      grown(motion::ScrollContentOffsetAdjustmentBehavior::automatic);
  EXPECT_GT(steady_before, 50.0F);
  EXPECT_NEAR(steady_after, steady_before + (2.0F * row_height), 0.5F);
  const auto [fixed_before, fixed_after] =
      grown(motion::ScrollContentOffsetAdjustmentBehavior::disabled);
  EXPECT_NEAR(fixed_after, fixed_before, 0.5F);
}

TEST_F(ScrollModifierTest, AnAxisScrollViewSpaceFindsTheScrollViewOnThatAxis) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<float>>(3, -1.0F);
  Application application = start(
      providers,
      components::ScrollView(
          components::VStack(
              components::VStackOptions{.spacing = 0.0F},
              components::Text("Top").frame({.width = 280.0F, .height = 60.0F}),
              components::ScrollView(
                  Axes::horizontal,
                  components::HStack(
                      components::HStackOptions{.spacing = 0.0F},
                      components::Text("Lead").frame(
                          {.width = 30.0F, .height = 40.0F}),
                      components::Text("Probe")
                          .frame({.width = 40.0F, .height = 40.0F})
                          .on_geometry_change(
                              std::type_identity<composition::Rect>{},
                              [](const composition::GeometryProxy& proxy) {
                                return composition::Rect{
                                    .x = proxy
                                             .frame(
                                                 composition::CoordinateSpace::
                                                     scroll_view_along(
                                                         Axis::horizontal))
                                             .x,
                                    .y = proxy
                                             .frame(
                                                 composition::CoordinateSpace::
                                                     scroll_view_along(
                                                         Axis::vertical))
                                             .y,
                                    .width =
                                        proxy
                                            .frame(
                                                composition::CoordinateSpace::
                                                    scroll_view)
                                            .y,
                                    .height = 0.0F};
                              },
                              [seen](composition::Rect frame) {
                                (*seen)[0] = frame.x;
                                (*seen)[1] = frame.y;
                                (*seen)[2] = frame.width;
                              })
                          .id("probe")))
                  .frame({.width = 200.0F, .height = 40.0F})
                  .id("inner"),
              components::Text("Bottom").frame(
                  {.width = 280.0F, .height = 600.0F})))
          .frame({.width = 300.0F, .height = 400.0F})
          .id("scroll"));
  frames(application, 2);
  const composition::Rect probe = frame_of(application, "probe");
  EXPECT_NEAR((*seen)[0], probe.x - frame_of(application, "inner").x, 0.5F);
  EXPECT_NEAR((*seen)[1], probe.y - frame_of(application, "scroll").y, 0.5F);
  EXPECT_NEAR((*seen)[2], probe.y - frame_of(application, "inner").y, 0.5F);
  EXPECT_NEAR((*seen)[1], 60.0F, 0.5F);
}

}
