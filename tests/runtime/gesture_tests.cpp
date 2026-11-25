#include <arisa/components/button.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/long_press_gesture.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tap_gesture.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::DragGesture;
using composition::Element;
using composition::LongPressGesture;
using composition::TapGesture;

class GestureTest : public ScrollFixture {
 protected:
  struct Log {
    int taps{0};
    int doubles{0};
    int singles{0};
    int parent{0};
    int child{0};
    int performed{0};
    std::vector<bool> pressing;
    std::vector<DragGesture::Value> changes;
    std::vector<DragGesture::Value> ends;
  };

  static auto box(std::string key, float size = 100.0F) -> Element {
    return components::VStack()
        .frame({.width = size, .height = size})
        .background(theme::ColourRole::accent)
        .id(std::move(key));
  }

  class Dragged final : public composition::Component {
   public:
    static constexpr composition::GestureState<float> pulled{"pulled", 0.0F};

    auto operator==(const Dragged&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto pull = context.state(pulled);
      return components::VStack(
          box("handle").gesture(DragGesture().updating(
              pull, [](const DragGesture::Value& value,
                       float& state) { state = value.translation.height; })),
          components::Text(std::format("pulled {}", pull.get())).id("label"));
    }
  };

  class Card final : public composition::Component {
   public:
    static constexpr composition::StateKey<composition::Point> shift{
        "shift", composition::Point{}};

    auto operator==(const Card&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto moved = context.state(shift);
      return components::VStack(
          box("card")
              .offset(moved.get().x, moved.get().y)
              .gesture(
                  DragGesture()
                      .on_changed([moved](const DragGesture::Value& value) {
                        moved.set(
                            composition::Point{.x = value.translation.width,
                                               .y = value.translation.height});
                      })
                      .on_ended([moved](const DragGesture::Value&) {
                        motion::with_animation(
                            motion::Animation::spring(0.5, 0.7),
                            [&moved] { moved.set(composition::Point{}); });
                      })));
    }
  };
};

TEST_F(GestureTest, TapGestureFiresOnRelease) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(box("target").on_tap_gesture([log] { ++log->taps; })));
  tap(providers, application, centre(application, "target"));
  EXPECT_EQ(log->taps, 1);
  tap(providers, application, provider::PixelPoint{.x = 5.0F, .y = 5.0F});
  EXPECT_EQ(log->taps, 1);
}

TEST_F(GestureTest, DoubleTapNeedsBothTapsInsideTheInterval) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, components::VStack(box("target").on_tap_gesture(
                           2, [log] { ++log->doubles; })));
  const provider::PixelPoint at = centre(application, "target");
  tap(providers, application, at);
  EXPECT_EQ(log->doubles, 0);
  tap(providers, application, at);
  EXPECT_EQ(log->doubles, 1);
  tap(providers, application, at);
  step(providers, application, 400ms);
  frames(application, 2);
  tap(providers, application, at);
  EXPECT_EQ(log->doubles, 1);
}

TEST_F(GestureTest, ExclusivelyPrefersTheDoubleTapAndDelaysTheSingle) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, components::VStack(box("target").gesture(
                           TapGesture(2)
                               .on_ended([log] { ++log->doubles; })
                               .exclusively(TapGesture(1).on_ended(
                                   [log] { ++log->singles; })))));
  const provider::PixelPoint at = centre(application, "target");
  tap(providers, application, at);
  EXPECT_EQ(log->singles, 0);
  step(providers, application, 400ms);
  frames(application, 2);
  EXPECT_EQ(log->singles, 1);
  EXPECT_EQ(log->doubles, 0);
  tap(providers, application, at);
  tap(providers, application, at);
  EXPECT_EQ(log->doubles, 1);
  step(providers, application, 400ms);
  frames(application, 2);
  EXPECT_EQ(log->singles, 1);
}

TEST_F(GestureTest, LongPressPerformsAfterTheDurationAndReportsPressing) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(box("target").on_long_press_gesture(
                     0.5, [log] { ++log->performed; },
                     [log](bool down) { log->pressing.push_back(down); })));
  const provider::PixelPoint at = centre(application, "target");
  touch_down(providers, application, at);
  step(providers, application, 200ms);
  EXPECT_EQ(log->performed, 0);
  ASSERT_EQ(log->pressing.size(), 1U);
  EXPECT_TRUE(log->pressing.front());
  step(providers, application, 350ms);
  frames(application, 1);
  EXPECT_EQ(log->performed, 1);
  ASSERT_EQ(log->pressing.size(), 2U);
  EXPECT_FALSE(log->pressing.back());
  touch_up(providers, application, at);
  EXPECT_EQ(log->performed, 1);
}

TEST_F(GestureTest, LongPressIsCancelledByMovingPastTheDistance) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(box("target").on_long_press_gesture(
                     0.5, [log] { ++log->performed; },
                     [log](bool down) { log->pressing.push_back(down); })));
  const provider::PixelPoint at = centre(application, "target");
  touch_down(providers, application, at);
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x + 20.0F, .y = at.y});
  step(providers, application, 700ms);
  frames(application, 1);
  EXPECT_EQ(log->performed, 0);
  ASSERT_EQ(log->pressing.size(), 2U);
  EXPECT_FALSE(log->pressing.back());
  touch_up(providers, application, at);
}

TEST_F(GestureTest, DragReportsTranslationVelocityAndPrediction) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          box("target", 200.0F)
              .gesture(DragGesture()
                           .on_changed([log](const DragGesture::Value& value) {
                             log->changes.push_back(value);
                           })
                           .on_ended([log](const DragGesture::Value& value) {
                             log->ends.push_back(value);
                           }))));
  const provider::PixelPoint at = centre(application, "target");
  const composition::Rect frame = frame_of(application, "target");
  touch_down(providers, application, at);
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x, .y = at.y + 5.0F});
  EXPECT_TRUE(log->changes.empty());
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x, .y = at.y + 20.0F});
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x, .y = at.y + 40.0F});
  touch_up(providers, application,
           provider::PixelPoint{.x = at.x, .y = at.y + 60.0F});
  frames(application, 1);
  ASSERT_EQ(log->changes.size(), 2U);
  EXPECT_FLOAT_EQ(log->changes.front().translation.height, 20.0F);
  EXPECT_FLOAT_EQ(log->changes.back().translation.height, 40.0F);
  EXPECT_FLOAT_EQ(log->changes.front().start_location.x, at.x - frame.x);
  EXPECT_FLOAT_EQ(log->changes.front().start_location.y, at.y - frame.y);
  ASSERT_EQ(log->ends.size(), 1U);
  const DragGesture::Value& ended = log->ends.front();
  EXPECT_FLOAT_EQ(ended.translation.height, 60.0F);
  EXPECT_GT(ended.velocity.height, 800.0F);
  EXPECT_NEAR(ended.predicted_end_translation.height,
              60.0F + (ended.velocity.height * 0.4995F), 1.0F);
  EXPECT_FLOAT_EQ(
      ended.predicted_end_location.y,
      ended.start_location.y + ended.predicted_end_translation.height);
}

TEST_F(GestureTest, GestureStateFollowsTheDragAndResetsOnEnd) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Dragged{});
  const provider::PixelPoint at = centre(application, "handle");
  touch_down(providers, application, at);
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x, .y = at.y + 30.0F});
  frames(application, 1);
  EXPECT_TRUE(shows(application, "pulled 30"));
  touch_up(providers, application,
           provider::PixelPoint{.x = at.x, .y = at.y + 30.0F});
  frames(application, 2);
  EXPECT_TRUE(shows(application, "pulled 0"));
}

TEST_F(GestureTest, ChildGesturesBeatParentsUnlessTheParentIsHighPriority) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application normal =
      start(providers, components::VStack(box("child").on_tap_gesture([log] {
                         ++log->child;
                       })).on_tap_gesture([log] { ++log->parent; }));
  tap(providers, normal, centre(normal, "child"));
  EXPECT_EQ(log->child, 1);
  EXPECT_EQ(log->parent, 0);

  provider::headless::HeadlessProviderFactory other;
  auto high = std::make_shared<Log>();
  Application priority =
      start(other, components::VStack(box("child").on_tap_gesture([high] {
                     ++high->child;
                   })).high_priority_gesture(TapGesture().on_ended([high] {
        ++high->parent;
      })));
  tap(other, priority, centre(priority, "child"));
  EXPECT_EQ(high->child, 0);
  EXPECT_EQ(high->parent, 1);
}

TEST_F(GestureTest, SimultaneousGesturesFireAlongsideTheWinner) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(components::Button("child", [log] { ++log->child; })
                             .frame({.width = 100.0F, .height = 50.0F})
                             .id("child"))
          .simultaneous_gesture(
              TapGesture().on_ended([log] { ++log->parent; })));
  tap(providers, application, centre(application, "child"));
  EXPECT_EQ(log->child, 1);
  EXPECT_EQ(log->parent, 1);
}

TEST_F(GestureTest, ContentShapeLimitsHitsToTheShape) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          box("round").content_shape(theme::Capsule()).on_tap_gesture([log] {
            ++log->taps;
          })));
  const composition::Rect frame = frame_of(application, "round");
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + 4.0F, .y = frame.y + 4.0F});
  EXPECT_EQ(log->taps, 0);
  tap(providers, application, centre(application, "round"));
  EXPECT_EQ(log->taps, 1);
}

TEST_F(GestureTest, ClipShapeConfinesHitsToTheShape) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers,
            components::VStack(components::VStack(box("inner").on_tap_gesture(
                                                      [log] { ++log->child; }))
                                   .clip_shape(theme::Circle())
                                   .on_tap_gesture([log] { ++log->parent; })
                                   .id("clip")));
  const composition::Rect frame = frame_of(application, "clip");
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + 6.0F, .y = frame.y + 6.0F});
  EXPECT_EQ(log->child, 0);
  EXPECT_EQ(log->parent, 0);
  tap(providers, application, centre(application, "clip"));
  EXPECT_EQ(log->child, 1);
}

TEST_F(GestureTest, DragVelocityHandsOffIntoTheSpring) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Card{});
  const provider::PixelPoint at = centre(application, "card");
  touch_down(providers, application, at);
  for (int index = 1; index <= 6; ++index) {
    touch_move(providers, application,
               provider::PixelPoint{
                   .x = at.x, .y = at.y + (20.0F * static_cast<float>(index))});
  }
  touch_up(providers, application,
           provider::PixelPoint{.x = at.x, .y = at.y + 120.0F});
  frames(application, 1);
  step(providers, application);
  const auto node = node_of(application, "card");
  motion::Presentation& presentation = application.root().tree().presentation(
      application.root().tree().node(*node)->get());
  EXPECT_TRUE(presentation.in_flight(motion::Presentation::Property::offset));
  EXPECT_GT(presentation.velocity(motion::Presentation::Property::offset)[1],
            100.0);
  const float shifted =
      presented_of(application, "card").y - frame_of(application, "card").y;
  EXPECT_GT(shifted, 100.0F);
  settle(providers, application);
  EXPECT_NEAR(presented_of(application, "card").y,
              frame_of(application, "card").y, 0.05F);
}

}
