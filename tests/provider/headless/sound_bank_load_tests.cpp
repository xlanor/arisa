#include <arisa/provider/audio/sound_bank.hpp>
#include <arisa/provider/headless/memory_assets.hpp>
#include <arisa/provider/headless/recording_diagnostics.hpp>
#include <arisa/provider/standard/directory_assets.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <utility>

namespace arisa::provider::headless {

TEST(SoundBankLoadTest, LoadsPresentSoundsAndReportsMissingOnes) {
  const standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  RecordingDiagnostics diagnostics;
  const SoundBank bank;
  const SoundBank::Sounds sounds = bank.load(assets, diagnostics, "test.audio");
  ASSERT_TRUE(sounds[std::to_underlying(SoundRole::click)].has_value());
  EXPECT_EQ(sounds[std::to_underlying(SoundRole::click)]->sample_rate(),
            48000U);
  EXPECT_FALSE(sounds[std::to_underlying(SoundRole::back)].has_value());
  const auto reported = diagnostics.snapshot();
  EXPECT_EQ(reported.size(), all_sound_roles.size() - 1);
  EXPECT_TRUE(std::ranges::all_of(reported, [](const Diagnostic& diagnostic) {
    return diagnostic.severity == Severity::info &&
           diagnostic.category == "test.audio";
  }));
}

TEST(SoundBankLoadTest, ReportsSoundsThatAreNotPcm16) {
  MemoryAssets assets;
  RecordingDiagnostics diagnostics;
  const SoundBank bank;
  assets.add(bank.path(SoundRole::touch), "not a wav");
  const SoundBank::Sounds sounds = bank.load(assets, diagnostics, "test.audio");
  EXPECT_TRUE(std::ranges::none_of(
      sounds, [](const auto& sound) { return sound.has_value(); }));
  const auto reported = diagnostics.snapshot();
  EXPECT_EQ(std::ranges::count_if(reported,
                                  [](const Diagnostic& diagnostic) {
                                    return diagnostic.severity ==
                                           Severity::warning;
                                  }),
            1);
}

}
