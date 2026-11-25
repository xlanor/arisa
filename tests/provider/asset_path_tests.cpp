#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/asset_path.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::provider {

TEST(AssetPathTest, AcceptsRelativePaths) {
  const auto path = AssetPath::create("fonts/ui-regular.ttf");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ(path->value(), "fonts/ui-regular.ttf");
  EXPECT_TRUE(AssetPath::create("a").has_value());
  EXPECT_TRUE(AssetPath::create("a/.hidden/b..c").has_value());
}

TEST(AssetPathTest, RejectsUnsafePaths) {
  constexpr std::array rejected{
      std::string_view{""},       std::string_view{"/abs"},
      std::string_view{"a/../b"}, std::string_view{".."},
      std::string_view{"../a"},   std::string_view{"a\\b"},
      std::string_view{"a//b"},   std::string_view{"a/"},
      std::string_view{"./a"},    std::string_view{"C:/a"},
  };
  for (const std::string_view candidate : rejected) {
    const auto path = AssetPath::create(candidate);
    ASSERT_FALSE(path.has_value()) << candidate;
    EXPECT_EQ(path.error().code(), ProviderErrorCode::invalid_argument);
  }
}

TEST(AssetPathTest, OrdersByValue) {
  EXPECT_LT(*AssetPath::create("a"), *AssetPath::create("b"));
  EXPECT_EQ(*AssetPath::create("a/b"), *AssetPath::create("a/b"));
}

TEST(AssetBlobTest, OwnsBytesAndMovesThem) {
  AssetBlob source{std::vector<std::byte>{std::byte{1}, std::byte{2}}};
  EXPECT_EQ(source.size(), 2U);
  AssetBlob target{std::move(source)};
  EXPECT_TRUE(source.empty());
  ASSERT_EQ(target.bytes().size(), 2U);
  EXPECT_EQ(target.bytes()[1], std::byte{2});
  source = std::move(target);
  EXPECT_EQ(source.size(), 2U);
  EXPECT_TRUE(target.empty());
}

}
