#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/haptics/rumble_shape.hpp>
#include <arisa/provider/haptics/sensory_feedback.hpp>
#include <arisa/provider/horizon/nx/nx_rumble.hpp>
#include <arisa/provider/horizon/ounce/ounce_rumble.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>

namespace arisa::provider {

template <class Rumble>
class ConsoleRumbleTest : public ::testing::Test {
 protected:
  static constexpr std::chrono::milliseconds longest{300};

  static auto audible(const RumbleShape& shape) -> bool {
    for (const RumblePulse& pulse : shape.pulses()) {
      if (pulse.low_amplitude > 0.0F || pulse.high_amplitude > 0.0F) {
        return true;
      }
    }
    return false;
  }

  static void expect_in_range(const SensoryFeedback& feedback,
                              const RumbleShape& shape) {
    EXPECT_FALSE(shape.pulses().empty()) << feedback.name();
    EXPECT_TRUE(audible(shape)) << feedback.name();
    EXPECT_LE(shape.duration(), longest) << feedback.name();
    EXPECT_LE(shape.peak(), Rumble::amplitude_ceiling) << feedback.name();
    for (const RumblePulse& pulse : shape.pulses()) {
      EXPECT_GT(pulse.duration.count(), 0) << feedback.name();
      EXPECT_GE(pulse.low_amplitude, 0.0F) << feedback.name();
      EXPECT_GE(pulse.high_amplitude, 0.0F) << feedback.name();
      EXPECT_GE(pulse.low_frequency, Rumble::low_band_minimum)
          << feedback.name();
      EXPECT_LE(pulse.low_frequency, Rumble::low_band_maximum)
          << feedback.name();
      EXPECT_GE(pulse.high_frequency, Rumble::high_band_minimum)
          << feedback.name();
      EXPECT_LE(pulse.high_frequency, Rumble::high_band_maximum)
          << feedback.name();
    }
  }
};

using Consoles =
    ::testing::Types<horizon::nx::NxRumble, horizon::ounce::OunceRumble>;
TYPED_TEST_SUITE(ConsoleRumbleTest, Consoles);

TYPED_TEST(ConsoleRumbleTest, EveryKindHasAnInRangeSubtleShape) {
  const std::span<const SensoryFeedback> catalogue =
      SensoryFeedback::catalogue();
  EXPECT_EQ(catalogue.size(), 27U);
  for (const SensoryFeedback& feedback : catalogue) {
    TestFixture::expect_in_range(feedback, TypeParam::shape(feedback));
  }
}

TYPED_TEST(ConsoleRumbleTest, EveryKindFeelsDifferent) {
  const std::span<const SensoryFeedback> catalogue =
      SensoryFeedback::catalogue();
  for (std::size_t left = 0; left < catalogue.size(); ++left) {
    for (std::size_t right = left + 1; right < catalogue.size(); ++right) {
      EXPECT_NE(TypeParam::shape(catalogue[left]),
                TypeParam::shape(catalogue[right]))
          << catalogue[left].name() << " and " << catalogue[right].name();
    }
  }
}

TYPED_TEST(ConsoleRumbleTest,
           PressReleaseAndSelectionVariantsHaveTheirOwnShapes) {
  using Press = SensoryFeedback::PressFeedback;
  using Selection = SensoryFeedback::SelectionFeedback;
  const SensoryFeedback toggle = SensoryFeedback::press(Press::toggle);
  EXPECT_EQ(toggle.kind(), SensoryFeedback::Kind::press);
  EXPECT_EQ(toggle.press_feedback(), Press::toggle);
  EXPECT_FALSE(toggle.release_feedback().has_value());
  const SensoryFeedback released =
      SensoryFeedback::release(SensoryFeedback::ReleaseFeedback::slider);
  EXPECT_EQ(released.release_feedback(),
            SensoryFeedback::ReleaseFeedback::slider);
  const SensoryFeedback limit =
      SensoryFeedback::selection_feedback(Selection::maximum);
  EXPECT_EQ(limit.kind(), SensoryFeedback::Kind::selection);
  EXPECT_EQ(limit.selection_variant(), Selection::maximum);
  EXPECT_FALSE(SensoryFeedback::selection.selection_variant().has_value());
  EXPECT_NE(limit, SensoryFeedback::selection);
  EXPECT_EQ(toggle.name(), "Press toggle");
  EXPECT_EQ(released.name(), "Release slider");
  EXPECT_EQ(limit.name(), "Selection maximum");
  EXPECT_GT(TypeParam::shape(toggle).peak(),
            TypeParam::shape(SensoryFeedback::press(Press::tab)).peak());
  EXPECT_LT(TypeParam::shape(released).peak(),
            TypeParam::shape(SensoryFeedback::press(Press::slider)).peak());
}

TYPED_TEST(ConsoleRumbleTest, ImpactIntensityScalesTheAmplitude) {
  using Weight = SensoryFeedback::Weight;
  const RumbleShape full = TypeParam::shape(SensoryFeedback::impact());
  const RumbleShape half = TypeParam::shape(SensoryFeedback::impact(
      Weight::medium, SensoryFeedback::Intensity{0.5F}));
  EXPECT_FLOAT_EQ(half.peak(), full.peak() * 0.5F);
  EXPECT_EQ(half.duration(), full.duration());
  EXPECT_FLOAT_EQ(
      TypeParam::shape(SensoryFeedback::impact(
                           Weight::heavy, SensoryFeedback::Intensity{4.0F}))
          .peak(),
      TypeParam::shape(SensoryFeedback::impact(Weight::heavy)).peak());
  EXPECT_FALSE(TestFixture::audible(TypeParam::shape(SensoryFeedback::impact(
      Weight::heavy, SensoryFeedback::Intensity{0.0F}))));
}

TYPED_TEST(ConsoleRumbleTest, HeavierImpactsAreStronger) {
  using Weight = SensoryFeedback::Weight;
  const float light =
      TypeParam::shape(SensoryFeedback::impact(Weight::light)).peak();
  const float medium =
      TypeParam::shape(SensoryFeedback::impact(Weight::medium)).peak();
  const float heavy =
      TypeParam::shape(SensoryFeedback::impact(Weight::heavy)).peak();
  EXPECT_LT(light, medium);
  EXPECT_LT(medium, heavy);
  EXPECT_LT(TypeParam::shape(SensoryFeedback::selection).peak(), light);
}

TYPED_TEST(ConsoleRumbleTest, StrengthScalesEveryPulseLinearly) {
  const HapticStrength half = HapticStrength::from_percent(50);
  const HapticStrength more = HapticStrength::from_percent(130);
  for (const SensoryFeedback& feedback : SensoryFeedback::catalogue()) {
    const RumbleShape normal = TypeParam::shape(feedback, HapticStrength{});
    const RumbleShape softer = TypeParam::shape(feedback, half);
    const RumbleShape stronger = TypeParam::shape(feedback, more);
    EXPECT_EQ(normal, TypeParam::shape(feedback));
    ASSERT_EQ(softer.pulses().size(), normal.pulses().size())
        << feedback.name();
    ASSERT_EQ(stronger.pulses().size(), normal.pulses().size())
        << feedback.name();
    for (std::size_t index = 0; index < normal.pulses().size(); ++index) {
      const RumblePulse& base = normal.pulses()[index];
      EXPECT_FLOAT_EQ(softer.pulses()[index].low_amplitude,
                      base.low_amplitude * 0.5F)
          << feedback.name();
      EXPECT_FLOAT_EQ(softer.pulses()[index].high_amplitude,
                      base.high_amplitude * 0.5F)
          << feedback.name();
      EXPECT_FLOAT_EQ(stronger.pulses()[index].low_amplitude,
                      std::min(base.low_amplitude * 1.3F, 1.0F))
          << feedback.name();
      EXPECT_FLOAT_EQ(stronger.pulses()[index].high_amplitude,
                      std::min(base.high_amplitude * 1.3F, 1.0F))
          << feedback.name();
      EXPECT_EQ(stronger.pulses()[index].low_frequency, base.low_frequency);
      EXPECT_EQ(stronger.pulses()[index].high_frequency, base.high_frequency);
      EXPECT_EQ(stronger.pulses()[index].duration, base.duration);
    }
  }
}

TYPED_TEST(ConsoleRumbleTest, FullStrengthClampsAtTheHardwareMaximum) {
  bool clamped = false;
  for (const SensoryFeedback& feedback : SensoryFeedback::catalogue()) {
    const RumbleShape normal = TypeParam::shape(feedback);
    const RumbleShape full =
        TypeParam::shape(feedback, HapticStrength::from_percent(200));
    EXPECT_LE(full.peak(), TypeParam::amplitude_ceiling) << feedback.name();
    for (std::size_t index = 0; index < normal.pulses().size(); ++index) {
      const RumblePulse& base = normal.pulses()[index];
      EXPECT_FLOAT_EQ(full.pulses()[index].low_amplitude,
                      std::min(base.low_amplitude * 2.0F, 1.0F));
      EXPECT_FLOAT_EQ(full.pulses()[index].high_amplitude,
                      std::min(base.high_amplitude * 2.0F, 1.0F));
      clamped = clamped || base.low_amplitude * 2.0F > 1.0F ||
                base.high_amplitude * 2.0F > 1.0F;
    }
  }
  EXPECT_TRUE(clamped);
}

TYPED_TEST(ConsoleRumbleTest, ZeroStrengthIsSilent) {
  for (const SensoryFeedback& feedback : SensoryFeedback::catalogue()) {
    const RumbleShape muted =
        TypeParam::shape(feedback, HapticStrength::from_percent(0));
    EXPECT_TRUE(muted.silent()) << feedback.name();
    EXPECT_FALSE(TestFixture::audible(muted)) << feedback.name();
  }
}

TEST(HapticStrengthTest, ClampsAndSnapsToFivePercentSteps) {
  EXPECT_EQ(HapticStrength{}.percent(), 100);
  EXPECT_EQ(HapticStrength{1.149F}.percent(), 115);
  EXPECT_EQ(HapticStrength{1.12F}.percent(), 110);
  EXPECT_EQ(HapticStrength{-1.0F}.percent(), 0);
  EXPECT_EQ(HapticStrength{9.0F}.percent(), 200);
  EXPECT_TRUE(HapticStrength{0.01F}.silent());
  EXPECT_FALSE(HapticStrength{0.03F}.silent());
  EXPECT_FLOAT_EQ(HapticStrength::from_percent(65).value(), 0.65F);
  EXPECT_EQ(HapticStrength::from_percent(400), HapticStrength{2.0F});
}

}
