#include <arisa/provider/standard/directory_storage.hpp>

#include <gtest/gtest.h>

#include <unistd.h>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace arisa::provider::standard {

class DirectoryStorageTest : public ::testing::Test {
 protected:
  void SetUp() override {
    base_ = std::filesystem::temp_directory_path() /
            ("arisa-directory-storage-" + std::to_string(::getpid()));
    std::error_code ignored;
    std::filesystem::remove_all(base_, ignored);
  }

  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove_all(base_, ignored);
  }

  static auto path(std::string_view text) -> AssetPath {
    auto created = AssetPath::create(text);
    EXPECT_TRUE(created.has_value());
    return *created;
  }

  std::filesystem::path base_;
};

TEST_F(DirectoryStorageTest, CreatesMissingDirectoriesAndTheirParents) {
  DirectoryStorage storage{base_ / "arisa"};
  ASSERT_FALSE(std::filesystem::exists(base_));
  ASSERT_TRUE(storage.create_directories(path("themes")).has_value());
  EXPECT_TRUE(std::filesystem::is_directory(base_ / "arisa" / "themes"));
  EXPECT_TRUE(storage.create_directories(path("themes")).has_value());
  EXPECT_EQ(storage.location(path("themes/edited.toml")),
            (base_ / "arisa" / "themes" / "edited.toml").string());
}

TEST_F(DirectoryStorageTest, WritesIntoAFolderThatDoesNotExistYet) {
  DirectoryStorage storage{base_ / "arisa"};
  const std::string_view text = "accent = \"#00C800\"\n";
  ASSERT_TRUE(storage
                  .write(path("themes/nested/edited.toml"),
                         std::as_bytes(std::span{text}))
                  .has_value());
  const auto read = storage.read(path("themes/nested/edited.toml"));
  ASSERT_TRUE(read.has_value());
  EXPECT_EQ(read->size(), text.size());
}

TEST_F(DirectoryStorageTest, AFileInTheWayIsAnErrorWithItsCode) {
  DirectoryStorage storage{base_};
  const std::string_view text = "x";
  ASSERT_TRUE(storage.write(path("themes"), std::as_bytes(std::span{text}))
                  .has_value());
  const auto written =
      storage.write(path("themes/edited.toml"), std::as_bytes(std::span{text}));
  ASSERT_FALSE(written.has_value());
  EXPECT_TRUE(written.error().native_result().has_value());
}

}
