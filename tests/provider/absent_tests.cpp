#include <arisa/provider/audio/absent.hpp>
#include <arisa/provider/browser/absent.hpp>
#include <arisa/provider/clipboard/absent.hpp>
#include <arisa/provider/diagnostics/absent.hpp>
#include <arisa/provider/fonts/absent.hpp>
#include <arisa/provider/gpu/absent.hpp>
#include <arisa/provider/haptics/absent.hpp>
#include <arisa/provider/input/absent.hpp>
#include <arisa/provider/lifecycle/absent.hpp>
#include <arisa/provider/surface/absent.hpp>
#include <arisa/provider/system_info/absent.hpp>
#include <arisa/provider/text_input/absent.hpp>

#include <gtest/gtest.h>

namespace arisa::provider {

class AbsentProvidersTest : public ::testing::Test {
 protected:
  template <class Result>
  static auto unsupported(const Result& result) -> bool {
    return !result.has_value() &&
           result.error().code() == ProviderErrorCode::unsupported_capability;
  }
};

TEST_F(AbsentProvidersTest, LifecycleIsAlwaysActiveWithoutEvents) {
  AbsentLifecycleProvider lifecycle;
  EXPECT_EQ(lifecycle.phase(), ScenePhase::active);
  EXPECT_FALSE(lifecycle.poll().has_value());
  EXPECT_TRUE(unsupported(lifecycle.block_home_button()));
}

TEST_F(AbsentProvidersTest, SystemInfoIsUnsupported) {
  AbsentSystemInfoProvider system_info;
  EXPECT_TRUE(unsupported(system_info.battery()));
  EXPECT_TRUE(unsupported(system_info.network()));
  EXPECT_TRUE(unsupported(system_info.inhibit_dimming()));
}

TEST_F(AbsentProvidersTest, HapticsAndAudioAreUnsupported) {
  AbsentHapticsProvider haptics;
  AbsentAudioProvider audio;
  for (const SensoryFeedback& feedback : SensoryFeedback::catalogue()) {
    EXPECT_TRUE(unsupported(haptics.play(feedback, HapticStrength{})));
  }
  for (const SoundRole role : all_sound_roles) {
    EXPECT_TRUE(unsupported(audio.play(role)));
  }
}

TEST_F(AbsentProvidersTest, ClipboardAndBrowserAreUnsupported) {
  AbsentClipboardProvider clipboard;
  AbsentBrowserProvider browser;
  EXPECT_TRUE(unsupported(clipboard.read_text()));
  EXPECT_TRUE(unsupported(clipboard.write_text("arisa")));
  const auto url = Url::create("https://example.com");
  ASSERT_TRUE(url.has_value());
  EXPECT_TRUE(unsupported(browser.open(*url)));
}

TEST_F(AbsentProvidersTest, FontsAreUnsupported) {
  const AbsentFontProvider fonts;
  for (const SystemFont font : all_system_fonts) {
    EXPECT_TRUE(unsupported(fonts.system_font(font)));
    EXPECT_FALSE(fonts.available(font));
  }
}

TEST_F(AbsentProvidersTest, SurfaceIsEmptyAndQuiet) {
  AbsentSurfaceProvider surface;
  EXPECT_TRUE(surface.size().empty());
  EXPECT_EQ(surface.scale(), SurfaceScale::unit());
  EXPECT_FALSE(surface.poll().has_value());
}

TEST_F(AbsentProvidersTest, InputIsEmptyButKeepsTuning) {
  AbsentInputProvider input;
  EXPECT_EQ(input.sample(), InputFrame{});
  const auto tuning = InputTuning::create(0.4F);
  ASSERT_TRUE(tuning.has_value());
  input.set_tuning(*tuning);
  EXPECT_EQ(input.tuning(), *tuning);
}

TEST_F(AbsentProvidersTest, GpuIsUnsupported) {
  AbsentGpuProvider gpu;
  EXPECT_TRUE(unsupported(gpu.create_texture({.width = 1, .height = 1})));
  EXPECT_TRUE(unsupported(gpu.begin_frame()));
  EXPECT_TRUE(unsupported(gpu.capture()));
  Texture texture{TextureId{1}, {}, support::ScopedCallback{}};
  EXPECT_TRUE(unsupported(gpu.update_texture(texture, {}, {})));
}

TEST_F(AbsentProvidersTest, TextInputIsUnsupported) {
  AbsentTextInputProvider text_input;
  EXPECT_TRUE(unsupported(text_input.begin(TextInputRequest{})));
  const TextInputSession empty{nullptr};
  EXPECT_FALSE(empty.active());
}

TEST_F(AbsentProvidersTest, DiagnosticsDropsSilently) {
  AbsentDiagnosticsProvider diagnostics;
  diagnostics.report(Diagnostic{
      .severity = Severity::error, .category = "test", .message = "dropped"});
  SUCCEED();
}

}
