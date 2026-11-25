#include <arisa/provider/haptics/rumble_player.hpp>
#include <arisa/provider/haptics/rumble_value.hpp>
#include <arisa/provider/headless/recording_haptics.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace arisa::provider::headless {

TEST(RecordingRumbleTest, RawRumbleIsRecordedClampedPerPlayer) {
  RecordingHaptics haptics;
  const auto second = RumblePlayer::number(2);
  ASSERT_TRUE(second.has_value());
  ASSERT_TRUE(haptics
                  .rumble(*second, RumbleFrequency{10.0F},
                          RumbleFrequency{320.0F}, RumbleAmplitude{0.5F},
                          RumbleAmplitude{2.0F})
                  .has_value());
  ASSERT_TRUE(haptics
                  .rumble(RumblePlayer::handheld(), RumbleFrequency{160.0F},
                          RumbleFrequency{320.0F}, RumbleAmplitude{0.0F},
                          RumbleAmplitude{0.0F})
                  .has_value());
  const std::vector<RecordingHaptics::Rumble> rumbles = haptics.rumbles();
  ASSERT_EQ(rumbles.size(), 2U);
  EXPECT_EQ(rumbles[0].player, *second);
  EXPECT_FLOAT_EQ(rumbles[0].value.low_frequency().value(),
                  RumbleBands{}.low_minimum);
  EXPECT_FLOAT_EQ(rumbles[0].value.high_amplitude().value(), 1.0F);
  EXPECT_TRUE(rumbles[1].player.is_handheld());
  haptics.clear();
  EXPECT_TRUE(haptics.rumbles().empty());
}

}
