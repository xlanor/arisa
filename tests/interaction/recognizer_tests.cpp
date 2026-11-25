#include <arisa/composition/axes.hpp>
#include <arisa/composition/point.hpp>

#include <interaction/drag_recognizer.hpp>
#include <interaction/long_press_recognizer.hpp>
#include <interaction/tap_recognizer.hpp>
#include <interaction/velocity_tracker.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace arisa::interaction {

using composition::Point;
using std::chrono::milliseconds;

TEST(TapRecognizerTest, SingleTapClaimsOnRelease) {
  TapRecognizer tap;
  const RecognizerSignals down =
      tap.begin(Point{.x = 10, .y = 10}, milliseconds{0});
  EXPECT_FALSE(down.claimed);
  EXPECT_TRUE(tap.possible());
  EXPECT_FALSE(tap.move(Point{.x = 14, .y = 12}, milliseconds{20}).failed);
  const RecognizerSignals up =
      tap.end(Point{.x = 14, .y = 12}, milliseconds{40});
  EXPECT_TRUE(up.claimed);
  EXPECT_TRUE(up.ended);
  EXPECT_TRUE(tap.recognized());
  EXPECT_FALSE(tap.possible());
}

TEST(TapRecognizerTest, MovingPastTheSlopFails) {
  TapRecognizer tap;
  static_cast<void>(tap.begin(Point{.x = 0, .y = 0}, milliseconds{0}));
  const RecognizerSignals moved =
      tap.move(Point{.x = 0, .y = 11}, milliseconds{10});
  EXPECT_TRUE(moved.failed);
  EXPECT_FALSE(tap.possible());
  EXPECT_FALSE(tap.end(Point{.x = 0, .y = 0}, milliseconds{20}).claimed);
}

TEST(TapRecognizerTest, ReleasingFarAwayFails) {
  TapRecognizer tap;
  static_cast<void>(tap.begin(Point{.x = 0, .y = 0}, milliseconds{0}));
  EXPECT_TRUE(tap.end(Point{.x = 80, .y = 0}, milliseconds{20}).failed);
}

TEST(TapRecognizerTest, DoubleTapNeedsTheSecondTapInsideTheInterval) {
  TapRecognizer twice{2};
  static_cast<void>(twice.begin(Point{}, milliseconds{0}));
  const RecognizerSignals first = twice.end(Point{}, milliseconds{50});
  EXPECT_FALSE(first.claimed);
  EXPECT_TRUE(twice.waiting());
  EXPECT_FALSE(twice.elapse(milliseconds{300}).failed);
  static_cast<void>(twice.begin(Point{.x = 4, .y = 4}, milliseconds{320}));
  EXPECT_TRUE(twice.end(Point{.x = 4, .y = 4}, milliseconds{360}).claimed);
  EXPECT_EQ(twice.taps(), 2);

  TapRecognizer late{2};
  static_cast<void>(late.begin(Point{}, milliseconds{0}));
  static_cast<void>(late.end(Point{}, milliseconds{50}));
  EXPECT_TRUE(late.elapse(milliseconds{351}).failed);

  TapRecognizer far{2};
  static_cast<void>(far.begin(Point{}, milliseconds{0}));
  static_cast<void>(far.end(Point{}, milliseconds{50}));
  EXPECT_TRUE(far.begin(Point{.x = 200, .y = 0}, milliseconds{100}).failed);
}

TEST(TapRecognizerTest, CancelFailsFromEveryLiveState) {
  TapRecognizer idle;
  EXPECT_TRUE(idle.cancel().failed);
  TapRecognizer pressed;
  static_cast<void>(pressed.begin(Point{}, milliseconds{0}));
  EXPECT_TRUE(pressed.cancel().failed);
  TapRecognizer released{2};
  static_cast<void>(released.begin(Point{}, milliseconds{0}));
  static_cast<void>(released.end(Point{}, milliseconds{10}));
  EXPECT_TRUE(released.cancel().failed);
  EXPECT_FALSE(released.cancel().failed);
}

TEST(LongPressRecognizerTest, RecognisesAfterTheMinimumDuration) {
  LongPressRecognizer press{milliseconds{500}, 10.0F};
  const RecognizerSignals down = press.begin(Point{}, milliseconds{0});
  ASSERT_TRUE(down.pressing.has_value());
  EXPECT_TRUE(*down.pressing);
  EXPECT_TRUE(down.changed);
  EXPECT_FALSE(press.elapse(milliseconds{499}).claimed);
  const RecognizerSignals due = press.elapse(milliseconds{500});
  EXPECT_TRUE(due.claimed);
  EXPECT_TRUE(due.ended);
  ASSERT_TRUE(due.pressing.has_value());
  EXPECT_FALSE(*due.pressing);
  EXPECT_TRUE(press.recognized());
}

TEST(LongPressRecognizerTest, MovingTooFarOrReleasingEarlyFails) {
  LongPressRecognizer moved{milliseconds{500}, 10.0F};
  static_cast<void>(moved.begin(Point{}, milliseconds{0}));
  EXPECT_FALSE(moved.move(Point{.x = 6, .y = 6}, milliseconds{100}).failed);
  const RecognizerSignals strayed =
      moved.move(Point{.x = 12, .y = 0}, milliseconds{200});
  EXPECT_TRUE(strayed.failed);
  ASSERT_TRUE(strayed.pressing.has_value());
  EXPECT_FALSE(*strayed.pressing);
  EXPECT_FALSE(moved.elapse(milliseconds{600}).claimed);

  LongPressRecognizer early{milliseconds{500}, 10.0F};
  static_cast<void>(early.begin(Point{}, milliseconds{0}));
  EXPECT_TRUE(early.end(Point{}, milliseconds{300}).failed);
}

TEST(DragRecognizerTest, ClaimsOnlyAfterTheMinimumDistance) {
  DragRecognizer drag{10.0F};
  EXPECT_FALSE(drag.begin(Point{}, milliseconds{0}).claimed);
  EXPECT_FALSE(drag.move(Point{.x = 0, .y = 9}, milliseconds{10}).claimed);
  const RecognizerSignals claimed =
      drag.move(Point{.x = 0, .y = 10}, milliseconds{20});
  EXPECT_TRUE(claimed.claimed);
  EXPECT_TRUE(claimed.changed);
  const RecognizerSignals changed =
      drag.move(Point{.x = 0, .y = 30}, milliseconds{30});
  EXPECT_FALSE(changed.claimed);
  EXPECT_TRUE(changed.changed);
  const RecognizerSignals ended =
      drag.end(Point{.x = 0, .y = 30}, milliseconds{40});
  EXPECT_TRUE(ended.ended);
  EXPECT_TRUE(drag.recognized());
}

TEST(DragRecognizerTest, ZeroDistanceClaimsOnTouchDown) {
  DragRecognizer drag{0.0F};
  const RecognizerSignals down =
      drag.begin(Point{.x = 3, .y = 3}, milliseconds{0});
  EXPECT_TRUE(down.claimed);
  EXPECT_TRUE(drag.dragging());
}

TEST(DragRecognizerTest, AxisFilterFailsOnTheWrongAxis) {
  DragRecognizer sideways{10.0F, composition::Axes::horizontal};
  static_cast<void>(sideways.begin(Point{}, milliseconds{0}));
  EXPECT_TRUE(sideways.move(Point{.x = 3, .y = 20}, milliseconds{10}).failed);
  DragRecognizer upright{10.0F, composition::Axes::vertical};
  static_cast<void>(upright.begin(Point{}, milliseconds{0}));
  EXPECT_TRUE(upright.move(Point{.x = 3, .y = 20}, milliseconds{10}).claimed);
}

TEST(DragRecognizerTest, ReleaseBeforeTheSlopFailsAndCancelStops) {
  DragRecognizer short_drag{10.0F};
  static_cast<void>(short_drag.begin(Point{}, milliseconds{0}));
  EXPECT_TRUE(short_drag.end(Point{.x = 2, .y = 0}, milliseconds{10}).failed);
  DragRecognizer cancelled{10.0F};
  static_cast<void>(cancelled.begin(Point{}, milliseconds{0}));
  static_cast<void>(cancelled.move(Point{.x = 0, .y = 40}, milliseconds{10}));
  static_cast<void>(cancelled.cancel());
  EXPECT_TRUE(cancelled.cancelled());
  EXPECT_FALSE(cancelled.possible());
}

TEST(VelocityTrackerTest, MeasuresTheRecentWindowAndForgetsStaleMotion) {
  VelocityTracker tracker;
  tracker.reset(Point{}, milliseconds{0});
  tracker.add(Point{.x = 0, .y = 10}, milliseconds{10});
  tracker.add(Point{.x = 0, .y = 20}, milliseconds{20});
  const composition::Size measured = tracker.velocity(milliseconds{20});
  EXPECT_NEAR(measured.height, 1000.0F, 0.5F);
  EXPECT_NEAR(measured.width, 0.0F, 0.01F);
  tracker.add(Point{.x = 0, .y = 30}, milliseconds{200});
  EXPECT_NEAR(tracker.velocity(milliseconds{200}).height, 0.0F, 0.01F);
  EXPECT_NEAR(tracker.velocity(milliseconds{400}).height, 0.0F, 0.01F);
}

}
