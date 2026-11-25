#include <arisa/provider/gpu/overlay_slots.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace arisa::provider {

using namespace std::chrono_literals;

class OverlaySlotsTest : public ::testing::Test {
 protected:
  static constexpr SurfaceSize painted{.width = SurfaceWidth{160},
                                       .height = SurfaceHeight{90}};

  static auto presented(std::uint32_t, OverlaySlots::Standing standing)
      -> bool {
    return standing.last_use == 0 || standing.presented;
  }

  static auto use(OverlaySlots& slots) -> std::optional<OverlaySlots::Use> {
    auto used = slots.use(std::nullopt);
    EXPECT_TRUE(used.has_value());
    return used.value_or(std::nullopt);
  }

  static auto reached(std::uint64_t value) {
    return [value](std::uint32_t, OverlaySlots::Standing standing) {
      return standing.last_use <= value;
    };
  }
};

TEST_F(OverlaySlotsTest, NothingIsUsedBeforeAScenePublishes) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::dropped};
  EXPECT_FALSE(use(slots).has_value());
  const auto claimed = slots.claim(presented);
  ASSERT_TRUE(claimed.has_value());
  EXPECT_FALSE(slots.claim(presented).has_value());
  EXPECT_FALSE(use(slots).has_value());
  EXPECT_TRUE(slots.publish(*claimed, false, painted));
  const auto first = use(slots);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->slot, *claimed);
  EXPECT_TRUE(first->fresh);
  const auto again = use(slots);
  ASSERT_TRUE(again.has_value());
  EXPECT_FALSE(again->fresh);
  EXPECT_GT(again->value, first->value);
}

TEST_F(OverlaySlotsTest, AnEmptySceneLeavesNothingToRecord) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::dropped};
  const auto drawn = slots.claim(presented);
  ASSERT_TRUE(drawn.has_value());
  ASSERT_TRUE(slots.publish(*drawn, false, painted));
  ASSERT_TRUE(use(slots).has_value());
  const auto blank = slots.claim(presented);
  ASSERT_TRUE(blank.has_value());
  ASSERT_TRUE(slots.publish(*blank, true, painted));
  EXPECT_FALSE(slots.current().has_value());
  EXPECT_FALSE(use(slots).has_value());
}

TEST_F(OverlaySlotsTest, AnEmptySceneCountsAsOneUse) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::dropped};
  const auto blank = slots.claim(presented);
  ASSERT_TRUE(blank.has_value());
  ASSERT_TRUE(slots.publish(*blank, true, painted));
  EXPECT_EQ(slots.uses(), 0U);
  EXPECT_FALSE(use(slots).has_value());
  EXPECT_EQ(slots.uses(), 1U);
  EXPECT_EQ(slots.await(0, 10s), 1U);
  EXPECT_FALSE(use(slots).has_value());
  EXPECT_EQ(slots.uses(), 1U);
}

TEST_F(OverlaySlotsTest, AwaitSceneWakesOnEveryPublish) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::dropped};
  EXPECT_EQ(slots.scenes(), 0U);
  EXPECT_EQ(slots.await_scene(0, 1ms), 0U);
  const auto drawn = slots.claim(presented);
  ASSERT_TRUE(drawn.has_value());
  ASSERT_TRUE(slots.publish(*drawn, false, painted));
  EXPECT_EQ(slots.await_scene(0, 10s), 1U);
  const auto blank = slots.claim(presented);
  ASSERT_TRUE(blank.has_value());
  ASSERT_TRUE(slots.publish(*blank, true, painted));
  EXPECT_EQ(slots.await_scene(1, 10s), 2U);
  EXPECT_EQ(slots.scenes(), 2U);
}

TEST_F(OverlaySlotsTest, AUsedSlotWaitsForItsPresent) {
  OverlaySlots slots{2, OverlaySlots::Leftovers::dropped};
  const auto first = slots.claim(presented);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(slots.publish(*first, false, painted));
  ASSERT_TRUE(use(slots).has_value());
  const auto second = slots.claim(presented);
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(*second, *first);
  ASSERT_TRUE(slots.publish(*second, false, painted));
  EXPECT_FALSE(slots.standing(*first).presented);
  EXPECT_FALSE(slots.claim(presented).has_value());
  const OverlaySlots::Presented fenced = slots.present([](std::uint32_t) {});
  EXPECT_TRUE(fenced.test(*first));
  EXPECT_FALSE(fenced.test(*second));
  EXPECT_EQ(slots.claim(presented), first);
}

TEST_F(OverlaySlotsTest, AbandonedSlotsReturnAndClosedSlotsRefuse) {
  OverlaySlots slots{2, OverlaySlots::Leftovers::dropped};
  const auto claimed = slots.claim(presented);
  ASSERT_TRUE(claimed.has_value());
  slots.abandon(*claimed);
  const auto reclaimed = slots.claim(presented);
  ASSERT_TRUE(reclaimed.has_value());
  slots.close();
  EXPECT_FALSE(slots.publish(*reclaimed, false, painted));
  EXPECT_FALSE(slots.claim(presented).has_value());
  EXPECT_FALSE(use(slots).has_value());
  EXPECT_TRUE(slots.closed());
}

TEST_F(OverlaySlotsTest, AnUnusedSceneIsCarriedIntoTheNextFreshUse) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::carried};
  const auto first = slots.claim(presented);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(slots.publish(*first, false, painted));
  const auto second = slots.claim(presented);
  ASSERT_TRUE(second.has_value());
  ASSERT_TRUE(slots.publish(*second, false, painted));
  const auto third = slots.claim(presented);
  ASSERT_TRUE(third.has_value());
  ASSERT_TRUE(slots.publish(*third, true, painted));
  EXPECT_FALSE(slots.claim(presented).has_value());
  EXPECT_EQ(slots.current(), third);
  const auto used = use(slots);
  ASSERT_TRUE(used.has_value());
  EXPECT_EQ(used->slot, *third);
  ASSERT_EQ(used->carrying, 2U);
  EXPECT_EQ(used->carried[0], *first);
  EXPECT_EQ(used->carried[1], *second);
  EXPECT_FALSE(slots.claim(presented).has_value());
  EXPECT_EQ(slots.present([](std::uint32_t) {}).count(), 3U);
  EXPECT_TRUE(slots.claim(presented).has_value());
}

TEST_F(OverlaySlotsTest, ADroppedSceneWaitsForEveryUseBeforeIt) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::dropped};
  const auto shown = slots.claim(reached(0));
  ASSERT_TRUE(shown.has_value());
  ASSERT_TRUE(slots.publish(*shown, false, painted));
  const auto used = use(slots);
  ASSERT_TRUE(used.has_value());
  const auto skipped = slots.claim(reached(0));
  ASSERT_TRUE(skipped.has_value());
  ASSERT_TRUE(slots.publish(*skipped, false, painted));
  const auto latest = slots.claim(reached(0));
  ASSERT_TRUE(latest.has_value());
  ASSERT_TRUE(slots.publish(*latest, false, painted));
  EXPECT_GE(slots.standing(*skipped).last_use, used->value);
  EXPECT_FALSE(slots.claim(reached(used->value - 1)).has_value());
  EXPECT_TRUE(slots.claim(reached(used->value)).has_value());
}

TEST_F(OverlaySlotsTest, AnAbandonedOrEmptySlotWaitsForEveryUseBeforeIt) {
  OverlaySlots slots{3, OverlaySlots::Leftovers::dropped};
  const auto shown = slots.claim(reached(0));
  ASSERT_TRUE(shown.has_value());
  ASSERT_TRUE(slots.publish(*shown, false, painted));
  const auto used = use(slots);
  ASSERT_TRUE(used.has_value());
  const auto abandoned = slots.claim(reached(0));
  ASSERT_TRUE(abandoned.has_value());
  slots.abandon(*abandoned);
  EXPECT_GE(slots.standing(*abandoned).last_use, used->value);
  const auto blank = slots.claim(reached(used->value));
  ASSERT_TRUE(blank.has_value());
  ASSERT_TRUE(slots.publish(*blank, true, painted));
  EXPECT_GE(slots.standing(*blank).last_use, used->value);
  EXPECT_FALSE(slots.claim(reached(used->value - 1)).has_value());
}

TEST_F(OverlaySlotsTest, AMismatchedTargetIsRefusedWithoutAUse) {
  OverlaySlots slots{2, OverlaySlots::Leftovers::dropped};
  const auto claimed = slots.claim(presented);
  ASSERT_TRUE(claimed.has_value());
  ASSERT_TRUE(slots.publish(*claimed, false, painted));
  const auto refused = slots.use(
      SurfaceSize{.width = SurfaceWidth{100}, .height = SurfaceHeight{90}});
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().detail(), ProviderErrorDetail::overlay_extent);
  EXPECT_EQ(slots.uses(), 0U);
  const auto fitted = slots.use(painted);
  ASSERT_TRUE(fitted.has_value());
  ASSERT_TRUE(fitted->has_value());
  EXPECT_TRUE((*fitted)->fresh);
  EXPECT_EQ(slots.uses(), 1U);
}

TEST_F(OverlaySlotsTest, AwaitWakesOnAUseAndOtherwiseTimesOut) {
  OverlaySlots slots{2, OverlaySlots::Leftovers::dropped};
  EXPECT_EQ(slots.await(0, 1ms), 0U);
  const auto claimed = slots.claim(presented);
  ASSERT_TRUE(claimed.has_value());
  ASSERT_TRUE(slots.publish(*claimed, false, painted));
  ASSERT_TRUE(use(slots).has_value());
  EXPECT_EQ(slots.await(0, 10s), 1U);
}

TEST_F(OverlaySlotsTest, APresentSignalsBeforeAClaimCanSeeIt) {
  OverlaySlots slots{2, OverlaySlots::Leftovers::dropped};
  const auto first = slots.claim(presented);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(slots.publish(*first, false, painted));
  ASSERT_TRUE(use(slots).has_value());
  const auto second = slots.claim(presented);
  ASSERT_TRUE(second.has_value());
  ASSERT_TRUE(slots.publish(*second, false, painted));
  standard::StdThreadingProvider threads;
  std::atomic<bool> signalled{false};
  std::atomic<bool> claimed{false};
  std::optional<Thread> claimer;
  const OverlaySlots::Presented fenced =
      slots.present([&](std::uint32_t index) {
        EXPECT_EQ(index, *first);
        auto started = threads.start(
            ThreadSpec{.name = "test-claimer", .role = ThreadRole::app},
            [&](const std::stop_token&) {
              const auto taken = slots.claim(
                  [&](std::uint32_t, OverlaySlots::Standing standing) {
                    return standing.presented && signalled.load();
                  });
              claimed.store(taken.has_value());
            });
        ASSERT_TRUE(started.has_value());
        claimer.emplace(std::move(*started));
        std::this_thread::sleep_for(20ms);
        signalled.store(true);
      });
  claimer.reset();
  EXPECT_TRUE(fenced.test(*first));
  EXPECT_TRUE(claimed.load());
}

}
