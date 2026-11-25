#include <arisa/provider/standard/directory_assets.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <span>
#include <string_view>

namespace arisa::provider::standard {

TEST(DirectoryAssetsTest, ReadsProbeFromFixtures) {
  const DirectoryAssets assets{ARISA_TEST_FIXTURES};
  const auto path = AssetPath::create("arisa-conformance/probe.txt");
  ASSERT_TRUE(path.has_value());
  EXPECT_TRUE(assets.exists(*path));
  const auto blob = assets.read(*path);
  ASSERT_TRUE(blob.has_value());
  const std::string_view expected = "arisa";
  EXPECT_TRUE(
      std::ranges::equal(blob->bytes(), std::as_bytes(std::span{expected})));
}

TEST(DirectoryAssetsTest, MissingFileIsNotFound) {
  const DirectoryAssets assets{ARISA_TEST_FIXTURES};
  const auto path = AssetPath::create("arisa-conformance/absent.bin");
  ASSERT_TRUE(path.has_value());
  EXPECT_FALSE(assets.exists(*path));
  const auto blob = assets.read(*path);
  ASSERT_FALSE(blob.has_value());
  EXPECT_EQ(blob.error().code(), ProviderErrorCode::not_found);
}

TEST(DirectoryAssetsTest, DirectoryIsNotAnAsset) {
  const DirectoryAssets assets{ARISA_TEST_FIXTURES};
  const auto path = AssetPath::create("arisa-conformance");
  ASSERT_TRUE(path.has_value());
  EXPECT_FALSE(assets.exists(*path));
  EXPECT_FALSE(assets.read(*path).has_value());
}

TEST(DirectoryAssetsTest, CurrentDirectoryRootsAtWorkingDirectory) {
  const DirectoryAssets assets = DirectoryAssets::current_directory();
  EXPECT_EQ(assets.root(), std::filesystem::current_path());
}

}
