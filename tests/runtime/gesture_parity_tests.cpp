#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::Element;
using composition::GestureInputKinds;

class GestureParityTest : public ScrollFixture {
 protected:
  struct Log {
    std::vector<std::string> events;
    std::vector<double> scales;
  };

  static auto area(std::string key) -> Element {
    return components::ZStack()
        .frame({.width = 200.0F, .height = 120.0F})
        .id(std::move(key));
  }

  static auto pointing(provider::PixelPoint position, bool left)
      -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pointer = provider::PointerState{.position = position, .left = left};
    return frame;
  }

  static void click(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::PixelPoint position) {
    providers.input().push(pointing(position, false));
    providers.input().push(pointing(position, true));
    providers.input().push(pointing(position, false));
    frames(application, 4);
  }

  static auto pinching(provider::TouchPhase phase, float scale,
                       provider::PixelPoint position) -> provider::InputFrame {
    provider::InputFrame frame = pointing(position, false);
    frame.pinch = provider::TrackpadPinch{.phase = phase, .scale = scale};
    return frame;
  }
};

TEST_F(GestureParityTest, InputKindsFilterTouchPointerAndController) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          area("pointer").gesture(
              composition::TapGesture(1, GestureInputKinds::pointer)
                  .on_ended([log] { log->events.emplace_back("pointer"); })),
          area("touch").on_tap_gesture(
              1, composition::CoordinateSpace::local,
              GestureInputKinds::direct_touch,
              [log](composition::Point) { log->events.emplace_back("touch"); }),
          area("held").on_long_press_gesture(
              0.2, 10.0F, GestureInputKinds::pointer,
              [log] { log->events.emplace_back("held"); }),
          area("pad").gesture(
              composition::TapGesture(1, GestureInputKinds::controller)
                  .on_ended([log] { log->events.emplace_back("pad"); }))));
  tap(providers, application, centre(application, "pointer"));
  tap(providers, application, centre(application, "touch"));
  touch_down(providers, application, centre(application, "held"));
  step(providers, application, 300ms);
  touch_up(providers, application, centre(application, "held"));
  EXPECT_EQ(log->events, (std::vector<std::string>{"touch"}));
  click(providers, application, centre(application, "pointer"));
  click(providers, application, centre(application, "touch"));
  EXPECT_EQ(log->events, (std::vector<std::string>{"touch", "pointer"}));
  tap(providers, application, centre(application, "pad"));
  EXPECT_EQ(log->events.size(), 2U);
  const auto pad = node_named(application, "pad");
  ASSERT_TRUE(pad.has_value());
  EXPECT_TRUE(pad->get().interactions().focusable.value_or(false));
}

TEST_F(GestureParityTest, ControllerKindFiresTapFromConfirm) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers,
            components::VStack(area("pad").on_tap_gesture(
                1, composition::CoordinateSpace::local,
                GestureInputKinds::controller, [log](composition::Point at) {
                  log->events.push_back(std::to_string(static_cast<int>(at.x)));
                })));
  EXPECT_EQ(focused_key(application), "pad");
  press(providers, application, provider::Button::a);
  EXPECT_EQ(log->events, (std::vector<std::string>{"100"}));
}

TEST_F(GestureParityTest, MagnifyAndRotateFilterAndTrackpadPinchDrives) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          area("magnify").gesture(
              composition::MagnifyGesture(0.01,
                                          GestureInputKinds::indirect_touch)
                  .on_changed(
                      [log](const composition::MagnifyGesture::Value& value) {
                        log->scales.push_back(value.magnification);
                      })),
          area("rotate").gesture(
              composition::RotateGesture(motion::Angle::degrees(1.0),
                                         GestureInputKinds::direct_touch)
                  .on_changed([log](const composition::RotateGesture::Value&) {
                    log->events.emplace_back("rotate");
                  }))));
  const provider::PixelPoint at = centre(application, "magnify");
  providers.input().push(pinching(provider::TouchPhase::began, 1.0F, at));
  providers.input().push(pinching(provider::TouchPhase::moved, 1.5F, at));
  providers.input().push(pinching(provider::TouchPhase::ended, 1.0F, at));
  frames(application, 4);
  ASSERT_FALSE(log->scales.empty());
  EXPECT_NEAR(log->scales.back(), 1.5, 0.05);
  const provider::PixelPoint turn = centre(application, "rotate");
  providers.input().push(pinching(provider::TouchPhase::began, 1.0F, turn));
  providers.input().push(pinching(provider::TouchPhase::moved, 1.5F, turn));
  providers.input().push(pinching(provider::TouchPhase::ended, 1.0F, turn));
  frames(application, 4);
  EXPECT_TRUE(log->events.empty());
}

TEST_F(GestureParityTest, NamedGesturesReportTheRecognisedName) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          area("named").gesture(
              composition::DragGesture(4.0F).on_changed(
                  [log](const auto&) { log->events.emplace_back("drag"); }),
              "pan"),
          area("simultaneous")
              .simultaneous_gesture(composition::DragGesture(4.0F), "both"),
          area("high").high_priority_gesture(composition::DragGesture(4.0F),
                                             "first", false)));
  const provider::PixelPoint from = centre(application, "named");
  touch_down(providers, application, from);
  touch_move(providers, application,
             provider::PixelPoint{.x = from.x + 30.0F, .y = from.y});
  EXPECT_EQ(application.interaction().arena().recognized(), "pan");
  touch_up(providers, application,
           provider::PixelPoint{.x = from.x + 30.0F, .y = from.y});
  EXPECT_FALSE(log->events.empty());
  const auto high = node_named(application, "high");
  ASSERT_TRUE(high.has_value());
  ASSERT_EQ(high->get().interactions().gestures.size(), 1U);
  EXPECT_EQ(high->get().interactions().gestures.front().name, "first");
  EXPECT_EQ(high->get().interactions().gestures.front().mask,
            composition::GestureMask::subviews);
}

TEST_F(GestureParityTest, TypedCompositesMapAndExposeTheirParts) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  auto exclusive =
      composition::TapGesture(2).exclusively(composition::TapGesture(1));
  EXPECT_EQ(exclusive.first().count(), 2);
  EXPECT_EQ(exclusive.second().count(), 1);
  auto sequence =
      composition::LongPressGesture(0.1).sequenced(composition::DragGesture());
  EXPECT_DOUBLE_EQ(sequence.first().minimum_duration(), 0.1);
  EXPECT_FLOAT_EQ(sequence.second().minimum_distance(),
                  composition::DragGesture::default_distance);
  Application application = start(
      providers,
      components::VStack(
          area("exclusive")
              .gesture(std::move(exclusive).on_ended([log](const auto& value) {
                log->events.emplace_back(value.is_first() ? "double"
                                                          : "single");
                EXPECT_NE(value.second_value(), nullptr);
              })),
          area("mapped").gesture(
              composition::DragGesture(2.0F)
                  .map([](const composition::DragGesture::Value& value) {
                    return static_cast<int>(value.translation.width);
                  })
                  .on_ended([log](const int& moved) {
                    log->events.push_back(std::to_string(moved));
                  }))));
  tap(providers, application, centre(application, "exclusive"));
  step(providers, application, 600ms);
  frames(application, 2);
  EXPECT_EQ(log->events, (std::vector<std::string>{"single"}));
  const provider::PixelPoint from = centre(application, "mapped");
  touch_down(providers, application, from);
  touch_move(providers, application,
             provider::PixelPoint{.x = from.x + 20.0F, .y = from.y});
  touch_up(providers, application,
           provider::PixelPoint{.x = from.x + 20.0F, .y = from.y});
  EXPECT_EQ(log->events.back(), "20");
}

class GestureStateProbe final : public composition::Component {
 public:
  static constexpr composition::GestureState<float> pulled{
      "parity-pulled", 0.0F,
      [](const float&, motion::Transaction& transaction) {
        transaction.set_disables_animations(true);
      }};
  static constexpr composition::GestureState<int> touched{
      "parity-touched", [] { return motion::Transaction{}; }};
  static constexpr composition::StateKey<int> resets{"parity-resets", 0};

  auto operator==(const GestureStateProbe&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto pull = context.state(pulled);
    const auto touch = context.state(touched);
    const composition::GestureStateGesture tracked{
        composition::DragGesture(2.0F), pull,
        [](const composition::DragGesture::Value& value, float& held) {
          held = value.translation.width;
        }};
    EXPECT_FLOAT_EQ(tracked.state().get(), pull.get());
    EXPECT_FLOAT_EQ(tracked.base().minimum_distance(), 2.0F);
    return components::VStack(
        components::Text(std::to_string(static_cast<int>(pull.get()))),
        components::ZStack()
            .frame({.width = 200.0F, .height = 120.0F})
            .gesture(composition::GestureStateGesture{
                composition::DragGesture(2.0F), pull,
                [](const composition::DragGesture::Value& value, float& held,
                   motion::Transaction&) {
                  held = value.translation.width;
                }}.on_ended([touch](const auto&) { static_cast<void>(touch); }))
            .id("pad"));
  }
};

TEST_F(GestureParityTest, GestureStateGestureUpdatesAndResets) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, GestureStateProbe{});
  EXPECT_EQ(GestureStateProbe::pulled.key().initial(), 0.0F);
  EXPECT_NE(GestureStateProbe::pulled.reset(), nullptr);
  EXPECT_NE(GestureStateProbe::touched.reset_transaction(), nullptr);
  const provider::PixelPoint from = centre(application, "pad");
  touch_down(providers, application, from);
  touch_move(providers, application,
             provider::PixelPoint{.x = from.x + 30.0F, .y = from.y});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "30"));
  touch_up(providers, application,
           provider::PixelPoint{.x = from.x + 30.0F, .y = from.y});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "0"));
}

TEST_F(GestureParityTest, HeldTouchRepeatsAndSkipsTheTrailingTap) {
  provider::headless::HeadlessProviderFactory providers;
  auto count = std::make_shared<int>(0);
  Application application =
      start(providers,
            components::VStack(components::Button("step", [count] { ++*count; })
                                   .frame({.width = 120.0F, .height = 60.0F})
                                   .id("step"))
                .environment(components::Environment::button_repeat_behavior,
                             components::ButtonRepeatBehavior::enabled));
  tap(providers, application, centre(application, "step"));
  EXPECT_EQ(*count, 1);
  touch_down(providers, application, centre(application, "step"));
  for (int index = 0; index < 50; ++index) {
    step(providers, application, 16ms);
  }
  touch_up(providers, application, centre(application, "step"));
  frames(application, 2);
  EXPECT_GE(*count, 4);
  const int after = *count;
  frames(application, 3);
  EXPECT_EQ(*count, after);
}

TEST_F(GestureParityTest, HoverFollowsContentMovingUnderAStillPointer) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  static constexpr composition::StateKey<float> shift{"parity-shift", 0.0F};
  class Mover final : public composition::Component {
   public:
    explicit Mover(std::shared_ptr<Log> log) : log_(std::move(log)) {}

    auto operator==(const Mover& other) const -> bool {
      return log_ == other.log_;
    }

    auto body(composition::Context& context) const -> Element override {
      const auto moved = context.state(shift);
      return components::ZStack(
                 components::ZStack()
                     .frame({.width = 100.0F, .height = 100.0F})
                     .on_hover([log = log_](bool inside) {
                       log->events.emplace_back(inside ? "in" : "out");
                     })
                     .offset(moved.get(), 0.0F)
                     .id("target"),
                 components::Button("move", [moved] { moved.set(300.0F); })
                     .frame({.width = 60.0F, .height = 40.0F})
                     .offset(0.0F, 200.0F)
                     .id("move"))
          .frame({.width = 600.0F, .height = 400.0F});
    }

   private:
    std::shared_ptr<Log> log_;
  };
  Application application = start(providers, Mover{log});
  const provider::PixelPoint inside = centre(application, "target");
  providers.input().push(pointing(
      provider::PixelPoint{.x = inside.x + 1.0F, .y = inside.y}, false));
  providers.input().push(pointing(inside, false));
  frames(application, 3);
  ASSERT_FALSE(log->events.empty());
  EXPECT_EQ(log->events.back(), "in");
  tap(providers, application, centre(application, "move"));
  frames(application, 4);
  EXPECT_EQ(log->events.back(), "out");
}

TEST_F(GestureParityTest, DeferredSystemGesturesBlockTheHomeButton) {
  provider::headless::HeadlessProviderFactory providers;
  static constexpr composition::StateKey<bool> deferring{"parity-defer", true};
  class Deferring final : public composition::Component {
   public:
    auto operator==(const Deferring&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto on = context.state(deferring);
      Element content =
          components::Button("toggle", [on] { on.set(!on.get()); })
              .frame({.width = 100.0F, .height = 50.0F})
              .id("toggle");
      if (on.get()) {
        content =
            std::move(content).defers_system_gestures(theme::Edge::bottom);
      }
      return components::VStack(std::move(content));
    }
  };
  Application application = start(providers, Deferring{});
  frames(application, 2);
  EXPECT_EQ(providers.lifecycle().live_home_button_blocks(), 1U);
  tap(providers, application, centre(application, "toggle"));
  frames(application, 2);
  EXPECT_EQ(providers.lifecycle().live_home_button_blocks(), 0U);
}

}
