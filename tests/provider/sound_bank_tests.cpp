#include <arisa/provider/audio/sound_bank.hpp>

#include <gtest/gtest.h>

namespace arisa::provider {

TEST(SoundBankTest, DefaultsToArisaSounds) {
  const SoundBank bank;
  EXPECT_EQ(bank.path(SoundRole::click).value(), "arisa/sounds/click.wav");
  EXPECT_EQ(bank.path(SoundRole::slider_release).value(),
            "arisa/sounds/slider_release.wav");
  for (const SoundRole role : all_sound_roles) {
    EXPECT_NE(SoundBank::role_name(role), "unknown");
  }
}

TEST(SoundBankTest, WithReplacesExactlyOneRole) {
  const SoundBank bank;
  const auto custom = AssetPath::create("ui/pop.wav");
  ASSERT_TRUE(custom.has_value());
  const SoundBank updated = bank.with(SoundRole::back, *custom);
  EXPECT_EQ(updated.path(SoundRole::back), *custom);
  for (const SoundRole role : all_sound_roles) {
    if (role != SoundRole::back) {
      EXPECT_EQ(updated.path(role), bank.path(role));
    }
  }
  EXPECT_NE(updated, bank);
}

}
