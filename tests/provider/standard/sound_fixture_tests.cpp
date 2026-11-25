#include <arisa/provider/audio/pcm_sound.hpp>
#include <arisa/provider/audio/sound_bank.hpp>
#include <arisa/provider/standard/directory_assets.hpp>

#include <gtest/gtest.h>

namespace arisa::provider {

TEST(SoundFixtureTest, DefaultClickPathHoldsAPcm16Wav) {
  const standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  const SoundBank bank;
  const auto blob = assets.read(bank.path(SoundRole::click));
  ASSERT_TRUE(blob.has_value());
  const auto sound = PcmSound::parse_wav(blob->bytes());
  ASSERT_TRUE(sound.has_value());
  EXPECT_EQ(sound->channels(), 1);
  EXPECT_EQ(sound->sample_rate(), 48000U);
  EXPECT_EQ(sound->frames(), 1920U);
  EXPECT_FALSE(assets.exists(bank.path(SoundRole::back)));
}

}
