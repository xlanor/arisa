#include <arisa/provider/haptics/rumble_player.hpp>
#include <arisa/provider/haptics/rumble_value.hpp>
#include <arisa/provider/horizon/nx/nx_rumble.hpp>
#include <arisa/provider/horizon/ounce/ounce_rumble.hpp>

#include <gtest/gtest.h>

#include <limits>

namespace arisa::provider {

TEST(RumbleValueTest, InBandValuesPassThrough) {
  const RumbleValue value{RumbleFrequency{160.0F}, RumbleFrequency{320.0F},
                          RumbleAmplitude{0.25F}, RumbleAmplitude{0.5F}};
  EXPECT_EQ(value.clamped(), value);
}

TEST(RumbleValueTest, OutOfBandValuesClampToTheConsoleBands) {
  const RumbleValue wild{RumbleFrequency{1.0F}, RumbleFrequency{5000.0F},
                         RumbleAmplitude{-0.5F}, RumbleAmplitude{3.0F}};
  const RumbleValue clamped = wild.clamped();
  const RumbleBands bands{};
  EXPECT_FLOAT_EQ(clamped.low_frequency().value(), bands.low_minimum);
  EXPECT_FLOAT_EQ(clamped.high_frequency().value(), bands.high_maximum);
  EXPECT_FLOAT_EQ(clamped.low_amplitude().value(), 0.0F);
  EXPECT_FLOAT_EQ(clamped.high_amplitude().value(), 1.0F);
}

TEST(RumbleValueTest, NonFiniteValuesFallBackToSilence) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  const RumbleValue broken{RumbleFrequency{nan}, RumbleFrequency{inf},
                           RumbleAmplitude{nan}, RumbleAmplitude{-inf}};
  EXPECT_EQ(broken.clamped(), RumbleValue::silent());
}

TEST(RumbleValueTest, DefaultBandsMatchBothConsoles) {
  const RumbleBands bands{};
  EXPECT_FLOAT_EQ(bands.low_minimum, horizon::nx::NxRumble::low_band_minimum);
  EXPECT_FLOAT_EQ(bands.low_maximum, horizon::nx::NxRumble::low_band_maximum);
  EXPECT_FLOAT_EQ(bands.high_minimum,
                  horizon::ounce::OunceRumble::high_band_minimum);
  EXPECT_FLOAT_EQ(bands.high_maximum,
                  horizon::ounce::OunceRumble::high_band_maximum);
}

TEST(RumbleValueTest, CustomBandsNarrowTheRange) {
  const RumbleBands narrow{.low_minimum = 100.0F,
                           .low_maximum = 200.0F,
                           .high_minimum = 200.0F,
                           .high_maximum = 400.0F,
                           .amplitude_maximum = 0.5F};
  const RumbleValue value{RumbleFrequency{50.0F}, RumbleFrequency{900.0F},
                          RumbleAmplitude{0.9F}, RumbleAmplitude{0.1F}};
  const RumbleValue clamped = value.clamped(narrow);
  EXPECT_FLOAT_EQ(clamped.low_frequency().value(), 100.0F);
  EXPECT_FLOAT_EQ(clamped.high_frequency().value(), 400.0F);
  EXPECT_FLOAT_EQ(clamped.low_amplitude().value(), 0.5F);
  EXPECT_FLOAT_EQ(clamped.high_amplitude().value(), 0.1F);
}

TEST(RumblePlayerTest, PlayersAreNumberedFromOne) {
  EXPECT_FALSE(RumblePlayer::number(0).has_value());
  EXPECT_FALSE(RumblePlayer::number(9).has_value());
  const auto first = RumblePlayer::number(1);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->index(), 0);
  EXPECT_EQ(RumblePlayer::number(8)->index(), 7);
  EXPECT_FALSE(RumblePlayer::handheld().index().has_value());
  EXPECT_TRUE(RumblePlayer::handheld().is_handheld());
  EXPECT_TRUE(RumblePlayer::primary().is_primary());
  EXPECT_NE(RumblePlayer::primary(), RumblePlayer::handheld());
}

}
