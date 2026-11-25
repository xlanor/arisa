#include <arisa/conformance/conformance_suite.hpp>
#include <arisa/provider/default_factory.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <print>

namespace arisa::provider {

class DefaultProviderFactoryTest : public ::testing::Test {
 protected:
  static void expect_passes(const conformance::Report& report) {
    for (const conformance::CheckResult& result : report) {
      if (!result.passed) {
        std::println("FAILED {}: {}", result.name, result.detail);
      }
    }
    EXPECT_TRUE(report.passed());
    EXPECT_GT(report.size(), 0U);
  }
};

class SilentHaptics final : public HapticsProvider {
 public:
  [[nodiscard]] auto play(const SensoryFeedback&, HapticStrength)
      -> std::expected<void, ProviderError> override {
    ++played_;
    return {};
  }

  auto played() const noexcept -> int { return played_; }

 private:
  int played_{0};
};

class StandardOnlyFonts final : public FontProvider {
 public:
  [[nodiscard]] auto system_font(SystemFont font) const
      -> std::expected<FontData, ProviderError> override {
    if (font != SystemFont::standard) {
      return std::unexpected(ProviderError{ProviderErrorCode::not_found});
    }
    return FontData{face_};
  }

  auto available(SystemFont font) const -> bool override {
    return font == SystemFont::standard;
  }

 private:
  std::array<std::byte, 4> face_{std::byte{'t'}, std::byte{'r'}, std::byte{'u'},
                                 std::byte{'e'}};
};

TEST_F(DefaultProviderFactoryTest, SuppliesOnlyRequiredByDefault) {
  const DefaultProviderFactory factory;
  EXPECT_EQ(factory.info().capabilities, Capability::required);
  EXPECT_EQ(factory.info().abi_version, abi_version);
  EXPECT_TRUE(factory.supplies(Capability::threading));
  EXPECT_FALSE(factory.supplies(Capability::haptics));
}

TEST_F(DefaultProviderFactoryTest, OptionalFallbacksAreAbsent) {
  DefaultProviderFactory factory;
  const auto played =
      factory.haptics().play(SensoryFeedback::selection, HapticStrength{});
  ASSERT_FALSE(played.has_value());
  EXPECT_EQ(played.error().code(), ProviderErrorCode::unsupported_capability);
  EXPECT_EQ(factory.lifecycle().phase(), ScenePhase::active);
  EXPECT_FALSE(factory.supplies(Capability::fonts));
  EXPECT_FALSE(factory.fonts().available(SystemFont::standard));
}

TEST_F(DefaultProviderFactoryTest, OverridesAreUsedAndReported) {
  SilentHaptics haptics;
  standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  DefaultProviderFactory factory{
      Providers{.assets = assets, .haptics = haptics}};
  EXPECT_TRUE(factory.supplies(Capability::haptics));
  EXPECT_EQ(&factory.assets(), &assets);
  EXPECT_TRUE(factory.haptics()
                  .play(SensoryFeedback::success, HapticStrength{})
                  .has_value());
  EXPECT_EQ(haptics.played(), 1);
}

TEST_F(DefaultProviderFactoryTest, PassesConformance) {
  standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  DefaultProviderFactory factory{Providers{.assets = assets}};
  expect_passes(conformance::ConformanceSuite::run(factory));
}

TEST_F(DefaultProviderFactoryTest, PassesConformanceWithOptionalOverride) {
  SilentHaptics haptics;
  standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  DefaultProviderFactory factory{
      Providers{.assets = assets, .haptics = haptics}};
  expect_passes(conformance::ConformanceSuite::run(factory));
  EXPECT_GT(haptics.played(), 0);
}

TEST_F(DefaultProviderFactoryTest, PassesConformanceWithFontOverride) {
  StandardOnlyFonts fonts;
  standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  DefaultProviderFactory factory{Providers{.assets = assets, .fonts = fonts}};
  EXPECT_TRUE(factory.supplies(Capability::fonts));
  expect_passes(conformance::ConformanceSuite::run(factory));
}

}
