#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/haptics/rumble_motor.hpp>
#include <arisa/provider/haptics/rumble_queue.hpp>
#include <arisa/provider/haptics/rumble_shape.hpp>
#include <arisa/provider/standard/std_threading.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace arisa::provider {

class RumbleQueueTest : public ::testing::Test {
 protected:
  static constexpr std::chrono::seconds patience{5};

  class Gate {
   public:
    void record(const RumblePulse& pulse) {
      std::unique_lock lock{mutex_};
      pulses_.push_back(pulse);
      const bool audible =
          pulse.low_amplitude > 0.0F || pulse.high_amplitude > 0.0F;
      if (audible && !entered_) {
        entered_ = true;
        changed_.notify_all();
        changed_.wait(lock, [this] { return open_; });
      }
      changed_.notify_all();
    }

    void open() {
      {
        const std::scoped_lock lock{mutex_};
        open_ = true;
      }
      changed_.notify_all();
    }

    auto wait_entered() -> bool {
      std::unique_lock lock{mutex_};
      return changed_.wait_for(lock, patience, [this] { return entered_; });
    }

    auto wait_pulses(std::size_t count) -> bool {
      std::unique_lock lock{mutex_};
      return changed_.wait_for(
          lock, patience, [this, count] { return pulses_.size() >= count; });
    }

    auto pulses() const -> std::vector<RumblePulse> {
      const std::scoped_lock lock{mutex_};
      return pulses_;
    }

   private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<RumblePulse> pulses_;
    bool entered_{false};
    bool open_{false};
  };

  class GatedMotor final : public RumbleMotor {
   public:
    explicit GatedMotor(Gate& gate) noexcept : gate_(gate) {}

    void prepare() override {}

    void vibrate(const RumblePulse& pulse) override { gate_.record(pulse); }

   private:
    Gate& gate_;
  };

  static auto tone(float amplitude, int milliseconds) -> RumblePulse {
    return RumblePulse{.low_amplitude = amplitude,
                       .low_frequency = 160.0F,
                       .high_amplitude = amplitude,
                       .high_frequency = 320.0F,
                       .duration = std::chrono::milliseconds{milliseconds}};
  }
};

TEST_F(RumbleQueueTest, PlayNeverWaitsForTheHapticsThread) {
  standard::StdThreadingProvider threading;
  Gate gate;
  auto queue =
      RumbleQueue::create(threading, std::make_unique<GatedMotor>(gate));
  ASSERT_TRUE(queue.has_value());
  queue->play(RumbleShape{tone(0.3F, 5)});
  ASSERT_TRUE(gate.wait_entered());
  const RumbleQueue::Counters parked = queue->counters();
  constexpr int burst = 1000;
  for (int index = 0; index < burst; ++index) {
    queue->play(
        RumbleShape{tone(0.01F + (0.0001F * static_cast<float>(index)), 2)});
  }
  const RumbleQueue::Counters held = queue->counters();
  EXPECT_EQ(held.requested, burst + 1U);
  EXPECT_LE(parked.contended, 1U);
  EXPECT_EQ(held.contended, parked.contended);
  EXPECT_EQ(held.started, 1U);
  gate.open();
  ASSERT_TRUE(gate.wait_pulses(3));
  const std::vector<RumblePulse> pulses = gate.pulses();
  ASSERT_EQ(pulses.size(), 3U);
  EXPECT_EQ(pulses[0], tone(0.3F, 5));
  EXPECT_EQ(pulses[1], tone(0.01F + (0.0001F * (burst - 1)), 2));
  EXPECT_EQ(pulses[2], RumblePulse{});
  const RumbleQueue::Counters drained = queue->counters();
  EXPECT_EQ(drained.started, 2U);
  EXPECT_EQ(drained.interrupted, 1U);
}

TEST_F(RumbleQueueTest, AShapePlaysEveryPulseThenSilence) {
  standard::StdThreadingProvider threading;
  Gate gate;
  gate.open();
  auto queue =
      RumbleQueue::create(threading, std::make_unique<GatedMotor>(gate));
  ASSERT_TRUE(queue.has_value());
  queue->play(RumbleShape{tone(0.2F, 3), tone(0.0F, 3), tone(0.4F, 3)});
  ASSERT_TRUE(gate.wait_pulses(4));
  const std::vector<RumblePulse> pulses = gate.pulses();
  ASSERT_GE(pulses.size(), 4U);
  EXPECT_EQ(pulses[0], tone(0.2F, 3));
  EXPECT_EQ(pulses[1], tone(0.0F, 3));
  EXPECT_EQ(pulses[2], tone(0.4F, 3));
  EXPECT_EQ(pulses[3], RumblePulse{});
  EXPECT_EQ(queue->counters().interrupted, 0U);
}

TEST_F(RumbleQueueTest, StoppingSilencesTheMotor) {
  standard::StdThreadingProvider threading;
  Gate gate;
  gate.open();
  {
    auto queue =
        RumbleQueue::create(threading, std::make_unique<GatedMotor>(gate));
    ASSERT_TRUE(queue.has_value());
    queue->play(RumbleShape{tone(0.2F, 2000)});
    ASSERT_TRUE(gate.wait_pulses(1));
  }
  const std::vector<RumblePulse> pulses = gate.pulses();
  ASSERT_GE(pulses.size(), 2U);
  EXPECT_EQ(pulses.back(), RumblePulse{});
}

TEST_F(RumbleQueueTest, ASilentShapeSendsNothingToTheMotor) {
  standard::StdThreadingProvider threading;
  Gate gate;
  gate.open();
  auto queue =
      RumbleQueue::create(threading, std::make_unique<GatedMotor>(gate));
  ASSERT_TRUE(queue.has_value());
  const RumbleShape muted = RumbleShape{tone(0.3F, 3), tone(0.5F, 3)}.scaled(
      HapticStrength::from_percent(0).value());
  EXPECT_TRUE(muted.silent());
  queue->play(muted);
  EXPECT_EQ(queue->counters().requested, 0U);
  queue->play(RumbleShape{tone(0.2F, 3)});
  ASSERT_TRUE(gate.wait_pulses(2));
  const std::vector<RumblePulse> pulses = gate.pulses();
  ASSERT_EQ(pulses.size(), 2U);
  EXPECT_EQ(pulses[0], tone(0.2F, 3));
  EXPECT_EQ(pulses[1], RumblePulse{});
  EXPECT_EQ(queue->counters().requested, 1U);
  EXPECT_EQ(queue->counters().started, 1U);
}

TEST_F(RumbleQueueTest, StrengthRidesTheShapeAcrossTheThreadHandOff) {
  standard::StdThreadingProvider threading;
  Gate gate;
  gate.open();
  auto queue =
      RumbleQueue::create(threading, std::make_unique<GatedMotor>(gate));
  ASSERT_TRUE(queue.has_value());
  const RumbleShape base{tone(0.25F, 1), tone(0.75F, 1)};
  std::vector<float> allowed{0.0F};
  constexpr int rounds = 2000;
  for (int index = 0; index < rounds; ++index) {
    const HapticStrength strength =
        HapticStrength::from_percent(5 + ((index * 5) % 200));
    const RumbleShape scaled = base.scaled(strength.value()).clamped(1.0F);
    for (const RumblePulse& pulse : scaled.pulses()) {
      allowed.push_back(pulse.low_amplitude);
    }
    queue->play(scaled);
  }
  const HapticStrength last = HapticStrength::from_percent(200);
  queue->play(base.scaled(last.value()).clamped(1.0F));
  allowed.push_back(0.5F);
  allowed.push_back(1.0F);
  ASSERT_TRUE(gate.wait_pulses(3));
  const RumbleQueue::Counters counted = queue->counters();
  EXPECT_EQ(counted.requested, rounds + 1U);
  for (const RumblePulse& pulse : gate.pulses()) {
    EXPECT_LE(pulse.low_amplitude, 1.0F);
    EXPECT_EQ(pulse.low_amplitude, pulse.high_amplitude);
    EXPECT_TRUE(std::ranges::contains(allowed, pulse.low_amplitude))
        << pulse.low_amplitude;
  }
}

}
