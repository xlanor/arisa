#include <arisa/components/button.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_mask.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/magnify_gesture.hpp>
#include <arisa/composition/rotate_gesture.hpp>
#include <arisa/composition/spatial_tap_gesture.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <format>
#include <memory>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;
using composition::AnyGesture;
using composition::DragGesture;
using composition::Element;
using composition::LongPressGesture;
using composition::MagnifyGesture;
using composition::RotateGesture;
using composition::SpatialTapGesture;
using composition::TapGesture;

class GestureCombinatorTest : public ScrollFixture {
 protected:
  static constexpr provider::TouchId thumb{8};

  struct Log {
    int taps{0};
    int ended{0};
    int parent{0};
    int child{0};
    std::vector<composition::Point> locations;
    std::vector<double> magnifications;
    std::vector<double> rotations;
    std::vector<std::string> stages;
  };

  static auto box(std::string key, float size = 200.0F) -> Element {
    return components::VStack()
        .frame({.width = size, .height = size})
        .background(theme::ColourRole::accent)
        .id(std::move(key));
  }

  static void pinch(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::PixelPoint centre,
                    float from, float to, float turn = 0.0F, int steps = 5) {
    const auto at = [&](float spread, float angle,
                        bool first) -> provider::PixelPoint {
      const float sign = first ? -1.0F : 1.0F;
      return provider::PixelPoint{
          .x = centre.x + (sign * spread * std::cos(angle)),
          .y = centre.y + (sign * spread * std::sin(angle))};
    };
    providers.input().push(
        touching(at(from, 0.0F, true), provider::TouchPhase::began));
    frames(application, 1);
    provider::InputFrame second;
    second.touches.push_back(
        provider::Touch{.id = finger,
                        .position = at(from, 0.0F, true),
                        .phase = provider::TouchPhase::stationary});
    second.touches.push_back(
        provider::Touch{.id = thumb,
                        .position = at(from, 0.0F, false),
                        .phase = provider::TouchPhase::began});
    providers.input().push(second);
    frames(application, 1);
    for (int index = 1; index <= steps; ++index) {
      const float progress =
          static_cast<float>(index) / static_cast<float>(steps);
      const float spread = from + ((to - from) * progress);
      const float angle = turn * progress;
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      provider::InputFrame moving;
      moving.touches.push_back(
          provider::Touch{.id = finger,
                          .position = at(spread, angle, true),
                          .phase = provider::TouchPhase::moved});
      moving.touches.push_back(
          provider::Touch{.id = thumb,
                          .position = at(spread, angle, false),
                          .phase = provider::TouchPhase::moved});
      providers.input().push(moving);
      frames(application, 1);
    }
    provider::InputFrame lifted;
    lifted.touches.push_back(
        provider::Touch{.id = thumb,
                        .position = at(to, turn, false),
                        .phase = provider::TouchPhase::ended});
    lifted.touches.push_back(
        provider::Touch{.id = finger,
                        .position = at(to, turn, true),
                        .phase = provider::TouchPhase::ended});
    providers.input().push(lifted);
    frames(application, 2);
  }

  class Tapped final : public composition::Component {
   public:
    static constexpr composition::GestureState<int> flashes{"flashes", 0};

    explicit Tapped(Log* log) noexcept : log_(log) {}

    auto operator==(const Tapped&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto flash = context.state(flashes);
      Log* log = log_;
      return components::VStack(
          box("target").gesture(
              TapGesture().updating(flash,
                                    [log](const std::monostate&, int& held) {
                                      held = 1;
                                      ++log->taps;
                                    })),
          components::Text(std::format("now {}", flash.get())).id("label"));
    }

   private:
    Log* log_;
  };

  class Pair final {
   public:
    explicit Pair(std::shared_ptr<Log> log) noexcept : log_(std::move(log)) {}

    auto body() const {
      return TapGesture(2).on_ended([log = log_] { ++log->taps; });
    }

   private:
    std::shared_ptr<Log> log_;
  };
};

TEST_F(GestureCombinatorTest, SpatialTapReportsTheLocalLocation) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(box("target").gesture(SpatialTapGesture().on_ended(
          [log](const SpatialTapGesture::Value& value) {
            log->locations.push_back(value.location);
          }))));
  const composition::Rect frame = frame_of(application, "target");
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + 30.0F, .y = frame.y + 40.0F});
  ASSERT_EQ(log->locations.size(), 1U);
  EXPECT_FLOAT_EQ(log->locations.front().x, 30.0F);
  EXPECT_FLOAT_EQ(log->locations.front().y, 40.0F);
}

TEST_F(GestureCombinatorTest,
       TapGestureWithCoordinateSpaceGivesTheGlobalPoint) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, components::VStack(box("target").on_tap_gesture(
                           1, composition::CoordinateSpace::global,
                           [log](composition::Point point) {
                             log->locations.push_back(point);
                           })));
  const composition::Rect frame = frame_of(application, "target");
  tap(providers, application,
      provider::PixelPoint{.x = frame.x + 10.0F, .y = frame.y + 20.0F});
  ASSERT_EQ(log->locations.size(), 1U);
  EXPECT_FLOAT_EQ(log->locations.front().x, frame.x + 10.0F);
  EXPECT_FLOAT_EQ(log->locations.front().y, frame.y + 20.0F);
}

TEST_F(GestureCombinatorTest, TapGestureUpdatingFlashesAndResets) {
  provider::headless::HeadlessProviderFactory providers;
  Log log;
  Application application = start(providers, Tapped{&log});
  tap(providers, application, centre(application, "target"));
  frames(application, 2);
  EXPECT_EQ(log.taps, 1);
  EXPECT_TRUE(shows(application, "now 0"));
}

TEST_F(GestureCombinatorTest, MagnifyFollowsTwoTouchesAndEnds) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(box("target").gesture(
                     MagnifyGesture()
                         .on_changed([log](const MagnifyGesture::Value& value) {
                           log->magnifications.push_back(value.magnification);
                         })
                         .on_ended([log](const MagnifyGesture::Value& value) {
                           ++log->ended;
                           log->magnifications.push_back(value.magnification);
                         }))));
  pinch(providers, application, centre(application, "target"), 40.0F, 80.0F);
  ASSERT_FALSE(log->magnifications.empty());
  EXPECT_EQ(log->ended, 1);
  EXPECT_NEAR(log->magnifications.back(), 2.0, 0.01);
}

TEST_F(GestureCombinatorTest, RotateMeasuresTheTurnBetweenTouches) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(box("target").gesture(
          RotateGesture().on_ended([log](const RotateGesture::Value& value) {
            log->rotations.push_back(value.rotation.degrees());
          }))));
  pinch(providers, application, centre(application, "target"), 50.0F, 50.0F,
        std::numbers::pi_v<float> / 4.0F);
  ASSERT_EQ(log->rotations.size(), 1U);
  EXPECT_NEAR(log->rotations.front(), 45.0, 0.5);
}

TEST_F(GestureCombinatorTest, SimultaneousMagnifyAndRotateBothRecognise) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers,
            components::VStack(box("target").gesture(
                MagnifyGesture()
                    .on_ended([log](const MagnifyGesture::Value& value) {
                      log->magnifications.push_back(value.magnification);
                    })
                    .simultaneously(RotateGesture().on_ended(
                        [log](const RotateGesture::Value& value) {
                          log->rotations.push_back(value.rotation.degrees());
                        }))
                    .on_ended([log](const auto& value) {
                      if (value.first.has_value() && value.second.has_value()) {
                        ++log->ended;
                      }
                    }))));
  pinch(providers, application, centre(application, "target"), 40.0F, 60.0F,
        std::numbers::pi_v<float> / 6.0F);
  ASSERT_EQ(log->magnifications.size(), 1U);
  ASSERT_EQ(log->rotations.size(), 1U);
  EXPECT_NEAR(log->magnifications.front(), 1.5, 0.01);
  EXPECT_NEAR(log->rotations.front(), 30.0, 0.5);
  EXPECT_EQ(log->ended, 1);
}

TEST_F(GestureCombinatorTest, PinchBeatsTheScrollViewAroundIt) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::ScrollView(
          components::VStack(
              box("target", 300.0F)
                  .gesture(MagnifyGesture().on_ended(
                      [log](const MagnifyGesture::Value&) { ++log->ended; })),
              box("filler", 900.0F)))
          .frame({.width = 400.0F, .height = 400.0F})
          .id("scroller"));
  pinch(providers, application, centre(application, "target"), 40.0F, 90.0F);
  EXPECT_EQ(log->ended, 1);
  EXPECT_FLOAT_EQ(offset(application, "scroller").y, 0.0F);
}

TEST_F(GestureCombinatorTest, SequencedLongPressThenDragRunsInOrder) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(box("target").gesture(
                     LongPressGesture(0.3)
                         .sequenced(DragGesture().on_ended(
                             [log](const DragGesture::Value& value) {
                               log->locations.push_back(composition::Point{
                                   .x = value.translation.width,
                                   .y = value.translation.height});
                             }))
                         .on_changed([log](const auto& value) {
                           log->stages.push_back(
                               value.second.has_value() ? "dragging" : "held");
                         })
                         .on_ended([log](const auto&) { ++log->ended; }))));
  const provider::PixelPoint at = centre(application, "target");
  touch_down(providers, application, at);
  step(providers, application, 350ms);
  frames(application, 1);
  ASSERT_FALSE(log->stages.empty());
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x + 30.0F, .y = at.y});
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x + 60.0F, .y = at.y});
  touch_up(providers, application,
           provider::PixelPoint{.x = at.x + 60.0F, .y = at.y});
  frames(application, 1);
  EXPECT_EQ(log->ended, 1);
  ASSERT_EQ(log->locations.size(), 1U);
  EXPECT_FLOAT_EQ(log->locations.front().x, 60.0F);
  EXPECT_EQ(log->stages.back(), "dragging");
}

TEST_F(GestureCombinatorTest, DragBeforeTheLongPressFailsTheSequence) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers,
            components::VStack(box("target").gesture(
                LongPressGesture(0.3)
                    .sequenced(DragGesture().on_ended(
                        [log](const DragGesture::Value&) { ++log->child; }))
                    .on_ended([log](const auto&) { ++log->ended; }))));
  const provider::PixelPoint at = centre(application, "target");
  touch_down(providers, application, at);
  touch_move(providers, application,
             provider::PixelPoint{.x = at.x + 40.0F, .y = at.y});
  step(providers, application, 400ms);
  touch_up(providers, application,
           provider::PixelPoint{.x = at.x + 40.0F, .y = at.y});
  frames(application, 2);
  EXPECT_EQ(log->ended, 0);
  EXPECT_EQ(log->child, 0);
}

TEST_F(GestureCombinatorTest, DoubleTapThenDragUsesTheNextTouch) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(box("target").gesture(
                     TapGesture(2)
                         .on_ended([log] { ++log->taps; })
                         .sequenced(DragGesture().on_ended(
                             [log](const DragGesture::Value& value) {
                               log->locations.push_back(composition::Point{
                                   .x = value.translation.width,
                                   .y = value.translation.height});
                             })))));
  const provider::PixelPoint at = centre(application, "target");
  tap(providers, application, at);
  tap(providers, application, at);
  EXPECT_EQ(log->taps, 1);
  drag(providers, application, at,
       provider::PixelPoint{.x = at.x, .y = at.y + 50.0F}, 5);
  frames(application, 1);
  ASSERT_EQ(log->locations.size(), 1U);
  EXPECT_FLOAT_EQ(log->locations.front().y, 50.0F);
}

TEST_F(GestureCombinatorTest, MaskGestureSilencesSubviews) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(box("child").on_tap_gesture([log] { ++log->child; }))
          .gesture(TapGesture().on_ended([log] { ++log->parent; }),
                   composition::GestureMask::gesture));
  tap(providers, application, centre(application, "child"));
  EXPECT_EQ(log->child, 0);
  EXPECT_EQ(log->parent, 1);
}

TEST_F(GestureCombinatorTest, MaskSubviewsAndDisabledGestureLeaveChildrenOn) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(
          components::VStack(
              box("child").on_tap_gesture([log] { ++log->child; }))
              .gesture(TapGesture(2).on_ended([log] { ++log->parent; }),
                       composition::GestureMask::subviews))
          .gesture(TapGesture().on_ended([log] { log->ended += 10; }), false));
  tap(providers, application, centre(application, "child"));
  EXPECT_EQ(log->child, 1);
  EXPECT_EQ(log->parent, 0);
  EXPECT_EQ(log->ended, 0);
}

TEST_F(GestureCombinatorTest, MaskNoneSilencesBoth) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(components::Button("child", [log] { ++log->child; })
                             .frame({.width = 100.0F, .height = 50.0F})
                             .id("child"))
          .gesture(TapGesture().on_ended([log] { ++log->parent; }),
                   composition::GestureMask::none));
  tap(providers, application, centre(application, "child"));
  EXPECT_EQ(log->child, 0);
  EXPECT_EQ(log->parent, 0);
}

TEST_F(GestureCombinatorTest, AnyGestureAndCustomGesturesErase) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  AnyGesture<std::monostate> erased{
      TapGesture(2).on_ended([log] { log->parent += 1; })};
  Application application = start(
      providers, components::VStack(
                     box("custom").gesture(Pair{log}),
                     box("erased").gesture(std::move(erased).on_ended(
                         [log](const std::monostate&) { log->child += 1; }))));
  tap(providers, application, centre(application, "custom"));
  tap(providers, application, centre(application, "custom"));
  EXPECT_EQ(log->taps, 1);
  tap(providers, application, centre(application, "erased"));
  tap(providers, application, centre(application, "erased"));
  EXPECT_EQ(log->parent, 1);
  EXPECT_EQ(log->child, 1);
}

TEST_F(GestureCombinatorTest, OptionDragPinchesWithThePointer) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::VStack(box("target", 600.0F)
                             .gesture(MagnifyGesture().on_ended(
                                 [log](const MagnifyGesture::Value& value) {
                                   log->magnifications.push_back(
                                       value.magnification);
                                 })))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity}));
  const provider::PixelPoint middle = centre(application, "target");
  provider::InputFrame option;
  option.keys.push_back(
      provider::KeyEvent{.key = provider::Key::left_alt, .down = true});
  providers.input().push(option);
  frames(application, 1);
  const auto pointer = [](provider::PixelPoint at, bool left) {
    provider::InputFrame frame;
    frame.pointer = provider::PointerState{.position = at, .left = left};
    return frame;
  };
  const composition::Rect area = application.root().tree().root().frame();
  const float centre_x = area.x + (area.width * 0.5F);
  const float centre_y = area.y + (area.height * 0.5F);
  providers.input().push(pointer(
      provider::PixelPoint{.x = centre_x + 40.0F, .y = centre_y}, true));
  frames(application, 1);
  for (int index = 1; index <= 4; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    providers.input().push(
        pointer(provider::PixelPoint{.x = centre_x + 40.0F + (10.0F * index),
                                     .y = centre_y},
                true));
    frames(application, 1);
  }
  providers.input().push(pointer(
      provider::PixelPoint{.x = centre_x + 80.0F, .y = centre_y}, false));
  frames(application, 2);
  static_cast<void>(middle);
  ASSERT_EQ(log->magnifications.size(), 1U);
  EXPECT_NEAR(log->magnifications.front(), 2.0, 0.01);
}

}
