#include <arisa/conformance/conformance_suite.hpp>
#include <arisa/conformance/gpu_suite.hpp>
#include <arisa/provider/default_factory.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/standard/directory_assets.hpp>

#include <array>
#include <cstddef>

#include <gtest/gtest.h>

#include <algorithm>
#include <print>
#include <string_view>

namespace arisa::conformance {

class ConformanceTest : public ::testing::Test {
 protected:
  static void expect_passes(const Report& report) {
    for (const CheckResult& result : report) {
      if (!result.passed) {
        std::println("FAILED {}: {}", result.name, result.detail);
      }
    }
    EXPECT_TRUE(report.passed());
  }

  static auto has_check(const Report& report, std::string_view name) -> bool {
    return std::ranges::any_of(report, [name](const CheckResult& result) {
      return result.name == name;
    });
  }
};

TEST_F(ConformanceTest, HeadlessFactoryPasses) {
  provider::headless::HeadlessProviderFactory factory;
  const Report report = ConformanceSuite::run(factory);
  expect_passes(report);
  EXPECT_TRUE(has_check(report, "lifecycle.poll_drains"));
  EXPECT_TRUE(has_check(report, "clipboard.round_trip"));
  EXPECT_TRUE(has_check(report, "browser.url_rules"));
  EXPECT_FALSE(has_check(report, "browser.open"));
  EXPECT_TRUE(has_check(report, "fonts.standard"));
  EXPECT_TRUE(has_check(report, "fonts.system_fonts"));
  EXPECT_TRUE(has_check(report, "threading.self_destruction"));
  EXPECT_TRUE(has_check(report, "text_input.limit_rules"));
  EXPECT_FALSE(has_check(report, "text_input.begin_and_cancel"));
  EXPECT_EQ(factory.lifecycle().live_home_button_blocks(), 0U);
  EXPECT_EQ(factory.system_info().live_dimming_inhibits(), 0U);
  EXPECT_TRUE(factory.browser().opened().empty());
}

TEST_F(ConformanceTest, HeadlessRasterReadsUnwrittenTexelsAsTransparent) {
  provider::headless::HeadlessProviderFactory factory;
  const provider::SurfaceSize surface = factory.surface().size();
  factory.gpu().rasterize(
      surface.empty()
          ? provider::SurfaceSize{.width = provider::SurfaceWidth{64},
                                  .height = provider::SurfaceHeight{64}}
          : surface);
  Report report;
  GpuSuite::run(factory, report);
  const auto found = std::ranges::find_if(report, [](const CheckResult& each) {
    return each.name == "gpu.unwritten_texels";
  });
  ASSERT_NE(found, report.end());
  EXPECT_TRUE(found->passed) << found->detail;
}

TEST_F(ConformanceTest, OpensExternalOnlyWhenAsked) {
  provider::headless::HeadlessProviderFactory factory;
  const Report report =
      ConformanceSuite::run(factory, ConformanceOptions{.open_external = true});
  expect_passes(report);
  EXPECT_TRUE(has_check(report, "browser.open"));
  EXPECT_EQ(factory.browser().opened().size(), 1U);
  EXPECT_TRUE(has_check(report, "text_input.begin_and_cancel"));
  EXPECT_EQ(factory.text_input().cancelled_sessions(), 1U);
  EXPECT_EQ(factory.text_input().live_sessions(), 0U);
}

TEST_F(ConformanceTest, DefaultFactoryPasses) {
  provider::standard::DirectoryAssets assets{ARISA_TEST_FIXTURES};
  provider::DefaultProviderFactory factory{
      provider::Providers{.assets = assets}};
  const Report report = ConformanceSuite::run(factory);
  expect_passes(report);
  EXPECT_TRUE(has_check(report, "lifecycle.absent"));
  EXPECT_TRUE(has_check(report, "browser.absent"));
  EXPECT_TRUE(has_check(report, "fonts.absent"));
  EXPECT_TRUE(has_check(report, "text_input.absent"));
}

TEST_F(ConformanceTest, HeadlessPendingEventsAreDrained) {
  provider::headless::HeadlessProviderFactory factory;
  factory.lifecycle().push(provider::LifecycleEvent::focus_gained);
  factory.lifecycle().push(provider::LifecycleEvent::resumed);
  expect_passes(ConformanceSuite::run(factory));
  EXPECT_FALSE(factory.lifecycle().poll().has_value());
}

TEST_F(ConformanceTest, HeadlessAcceptsEveryValidFaceTag) {
  provider::headless::HeadlessProviderFactory factory;
  static constexpr std::array opentype{std::byte{'O'}, std::byte{'T'},
                                       std::byte{'T'}, std::byte{'O'}};
  static constexpr std::array collection{std::byte{'t'}, std::byte{'t'},
                                         std::byte{'c'}, std::byte{'f'}};
  factory.fonts().add(provider::SystemFont::korean, opentype);
  factory.fonts().add(provider::SystemFont::nintendo_symbols, collection);
  expect_passes(ConformanceSuite::run(factory));
}

TEST_F(ConformanceTest, InvalidFaceFails) {
  provider::headless::HeadlessProviderFactory factory;
  static constexpr std::array garbage{std::byte{'G'}, std::byte{'I'},
                                      std::byte{'F'}, std::byte{'8'}};
  factory.fonts().add(provider::SystemFont::chinese_traditional, garbage);
  const Report report = ConformanceSuite::run(factory);
  EXPECT_FALSE(report.passed());
  const auto fonts =
      std::ranges::find_if(report, [](const CheckResult& result) {
        return result.name == "fonts.system_fonts";
      });
  ASSERT_NE(fonts, report.end());
  EXPECT_FALSE(fonts->passed);
}

TEST_F(ConformanceTest, EmptyStandardFaceFails) {
  provider::headless::HeadlessProviderFactory factory;
  factory.fonts().add(provider::SystemFont::standard, {});
  const Report report = ConformanceSuite::run(factory);
  const auto standard =
      std::ranges::find_if(report, [](const CheckResult& result) {
        return result.name == "fonts.standard";
      });
  ASSERT_NE(standard, report.end());
  EXPECT_FALSE(standard->passed);
}

TEST_F(ConformanceTest, MissingProbeFails) {
  provider::standard::DirectoryAssets assets{ARISA_TEST_FIXTURES
                                             "/arisa-conformance"};
  provider::DefaultProviderFactory factory{
      provider::Providers{.assets = assets}};
  const Report report = ConformanceSuite::run(factory);
  EXPECT_FALSE(report.passed());
  const auto probe = std::ranges::find_if(
      report,
      [](const CheckResult& result) { return result.name == "assets.probe"; });
  ASSERT_NE(probe, report.end());
  EXPECT_FALSE(probe->passed);
}

}
