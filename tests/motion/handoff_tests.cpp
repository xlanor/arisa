#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string_view>

namespace arisa::motion {

using namespace std::chrono_literals;

class HandoffTest : public ::testing::Test {
 protected:
  using Property = Presentation::Property;
  using Tint = Presentation::Tint;

  static auto moment(const Transaction& transaction,
                     std::chrono::nanoseconds now) -> Presentation::Moment {
    return Presentation::Moment{
        .transaction = transaction, .now = now, .reduce_motion = false};
  }

  static auto tracked() -> Transaction {
    Transaction tracking;
    tracking.set_disables_animations(true);
    return tracking;
  }
};

TEST_F(HandoffTest, TrackingRecordsVelocityAndASpringRetargetStartsFromIt) {
  Presentation presentation;
  const Transaction tracking = tracked();
  for (int step = 0; step <= 4; ++step) {
    presentation.animate(Property::offset, AnimatableVector{10.0 * step},
                         moment(tracking, step * 16ms));
  }
  EXPECT_FALSE(presentation.animating());
  EXPECT_NEAR(presentation.velocity(Property::offset)[0], 625.0, 1.0e-6);
  const Transaction release{Animation::interactive_spring()};
  const AnimatableVector start = presentation.animate(
      Property::offset, AnimatableVector{100.0}, moment(release, 64ms));
  EXPECT_DOUBLE_EQ(start[0], 40.0);
  const AnimatableVector next = presentation.animate(
      Property::offset, AnimatableVector{100.0}, moment(release, 65ms));
  const double speed = (next[0] - start[0]) / 0.001;
  EXPECT_NEAR(speed, 625.0, 40.0);
  Presentation still;
  still.animate(Property::offset, AnimatableVector{0.0}, moment(tracking, 0ms));
  still.animate(Property::offset, AnimatableVector{40.0},
                moment(tracking, 300ms));
  const AnimatableVector rest = still.animate(
      Property::offset, AnimatableVector{100.0}, moment(release, 301ms));
  const AnimatableVector moved = still.animate(
      Property::offset, AnimatableVector{100.0}, moment(release, 302ms));
  EXPECT_LT((moved[0] - rest[0]) / 0.001, speed * 0.25);
}

TEST_F(HandoffTest, StaleTrackingVelocityIsDropped) {
  Presentation presentation;
  const Transaction tracking = tracked();
  presentation.animate(Property::offset, AnimatableVector{0.0},
                       moment(tracking, 0ms));
  presentation.animate(Property::offset, AnimatableVector{10.0},
                       moment(tracking, 16ms));
  EXPECT_GT(presentation.velocity(Property::offset)[0], 0.0);
  presentation.animate(Property::offset, AnimatableVector{11.0},
                       moment(tracking, 500ms));
  EXPECT_DOUBLE_EQ(presentation.velocity(Property::offset)[0], 0.0);
}

TEST_F(HandoffTest, MorphStartsFromTheMatchedFrameForEachPropertySet) {
  const AnimatableVector source{10.0, 20.0, 100.0, 50.0};
  const AnimatableVector own{200.0, 100.0, 300.0, 150.0};
  EXPECT_EQ(Presentation::matched(source, own, MatchedGeometryProperties::frame,
                                  UnitPoint::center),
            source);
  EXPECT_EQ(Presentation::matched(source, own, MatchedGeometryProperties::size,
                                  UnitPoint::center),
            AnimatableVector(300.0, 150.0, 100.0, 50.0));
  EXPECT_EQ(
      Presentation::matched(source, own, MatchedGeometryProperties::position,
                            UnitPoint::top_leading),
      AnimatableVector(10.0, 20.0, 300.0, 150.0));
  EXPECT_EQ(
      Presentation::matched(source, own, MatchedGeometryProperties::position,
                            UnitPoint::center),
      AnimatableVector(-90.0, -30.0, 300.0, 150.0));
  Presentation presentation;
  const Transaction linear{Animation::linear(1.0)};
  presentation.morph(
      Presentation::Morph{
          .source =
              Presentation::Snapshot{
                  .frame = source,
                  .velocity = {},
                  .corner_radius = 4.0,
                  .background = theme::Colour{0.0F, 0.0F, 0.0F, 1.0F}},
          .properties = MatchedGeometryProperties::frame |
                        MatchedGeometryProperties::corner_radius,
          .anchor = UnitPoint::center},
      0ms);
  EXPECT_TRUE(presentation.animating());
  EXPECT_EQ(presentation.animate(Property::frame, own, moment(linear, 0ms)),
            source);
  EXPECT_TRUE(presentation.morphing());
  EXPECT_EQ(presentation.animate(Property::corner_radius,
                                 AnimatableVector{20.0}, moment(linear, 0ms)),
            AnimatableVector{4.0});
  const theme::Colour white{1.0F, 1.0F, 1.0F, 1.0F};
  EXPECT_EQ(presentation.animate(Tint::background, white, moment(linear, 0ms)),
            white);
  const AnimatableVector half =
      presentation.animate(Property::frame, own, moment(linear, 500ms));
  EXPECT_NEAR(half[0], 105.0, 1.0e-6);
  EXPECT_NEAR(half[2], 200.0, 1.0e-6);
  EXPECT_EQ(presentation.animate(Property::frame, own, moment(linear, 1s)),
            own);
  EXPECT_FALSE(presentation.morphing());
}

TEST_F(HandoffTest, MorphDropsTheInsertionTransitionAndConcealFinishesRemoval) {
  const Transaction linear{Animation::linear(1.0)};
  Presentation arriving;
  arriving.insert(Transition::opacity(), linear);
  arriving.morph(Presentation::Morph{}, 0ms);
  EXPECT_FALSE(arriving.appearance(moment(linear, 0ms)).has_value());
  Presentation leaving;
  leaving.remove(Transition::opacity(), linear);
  EXPECT_FALSE(leaving.removal_finished());
  leaving.conceal();
  EXPECT_TRUE(leaving.concealed());
  EXPECT_TRUE(leaving.removal_finished());
  EXPECT_FALSE(leaving.animating());
}

TEST_F(HandoffTest, ContentExchangeCrossesFromTheOutgoingText) {
  Presentation presentation;
  const Transaction none;
  const Transaction linear{Animation::linear(1.0)};
  EXPECT_FALSE(presentation.exchange("19", moment(none, 0ms)).has_value());
  EXPECT_FALSE(presentation.exchange("20", moment(none, 10ms)).has_value());
  EXPECT_FALSE(presentation.exchange("20", moment(linear, 20ms)).has_value());
  const auto started = presentation.exchange("21", moment(linear, 1s));
  ASSERT_TRUE(started.has_value());
  EXPECT_EQ(started->outgoing, std::string_view{"20"});
  EXPECT_DOUBLE_EQ(started->progress, 0.0);
  const auto half = presentation.exchange("21", moment(linear, 1500ms));
  ASSERT_TRUE(half.has_value());
  EXPECT_NEAR(half->progress, 0.5, 1.0e-9);
  EXPECT_FALSE(presentation.exchange("21", moment(linear, 2s)).has_value());
}

}
