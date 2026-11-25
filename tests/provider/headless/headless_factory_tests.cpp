#include <arisa/conformance/assets_suite.hpp>
#include <arisa/conformance/fonts_suite.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <span>

namespace arisa::provider::headless {

using namespace std::chrono_literals;

TEST(HeadlessProviderFactoryTest, SuppliesEverything) {
  const HeadlessProviderFactory factory;
  EXPECT_EQ(factory.info().capabilities, Capability::all);
  EXPECT_TRUE(factory.supplies(Capability::browser));
  EXPECT_TRUE(factory.supplies(Capability::required));
}

TEST(HeadlessProviderFactoryTest, PreloadsTheConformanceProbe) {
  EXPECT_EQ(HeadlessProviderFactory::probe_path,
            conformance::AssetsSuite::probe_path);
  EXPECT_EQ(HeadlessProviderFactory::probe_text,
            conformance::AssetsSuite::probe_text);
  HeadlessProviderFactory factory;
  const auto path = AssetPath::create(HeadlessProviderFactory::probe_path);
  ASSERT_TRUE(path.has_value());
  const auto blob = factory.assets().read(*path);
  ASSERT_TRUE(blob.has_value());
  EXPECT_TRUE(std::ranges::equal(
      blob->bytes(),
      std::as_bytes(std::span{HeadlessProviderFactory::probe_text})));
}

TEST(HeadlessProviderFactoryTest, PreloadsAStandardFace) {
  HeadlessProviderFactory factory;
  EXPECT_TRUE(factory.supplies(Capability::fonts));
  const auto standard = factory.fonts().system_font(SystemFont::standard);
  ASSERT_TRUE(standard.has_value());
  EXPECT_TRUE(conformance::FontsSuite::has_sfnt_tag(standard->bytes()));
  EXPECT_FALSE(factory.fonts().available(SystemFont::korean));
}

TEST(HeadlessProviderFactoryTest, TypedAccessorsDriveTheProviders) {
  HeadlessProviderFactory factory;
  ProviderFactory& generic = factory;
  const Instant before = generic.clock().now();
  ASSERT_TRUE(factory.clock().advance(1s).has_value());
  EXPECT_EQ(generic.clock().now() - before, 1s);
  factory.lifecycle().push(LifecycleEvent::suspending);
  EXPECT_EQ(generic.lifecycle().poll(), LifecycleEvent::suspending);
  generic.diagnostics().report(Diagnostic{
      .severity = Severity::warning, .category = "c", .message = "m"});
  EXPECT_EQ(factory.diagnostics().snapshot().size(), 1U);
  ASSERT_TRUE(generic.audio().play(SoundRole::sidebar).has_value());
  EXPECT_EQ(factory.audio().played().size(), 1U);
}

}
