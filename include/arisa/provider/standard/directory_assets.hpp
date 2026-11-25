#pragma once

#include <arisa/provider/assets/provider.hpp>

#include <cstdio>
#include <filesystem>

namespace arisa::provider::standard {

class DirectoryAssets final : public AssetProvider {
 public:
  explicit DirectoryAssets(std::filesystem::path root);

  static auto current_directory() -> DirectoryAssets;

  [[nodiscard]] auto read(const AssetPath& path) const
      -> std::expected<AssetBlob, ProviderError> override;

  auto exists(const AssetPath& path) const -> bool override;

  auto stamp(const AssetPath& path) const -> std::optional<FileStamp> override;

  auto root() const noexcept -> const std::filesystem::path& { return root_; }

 private:
  struct FileRelease {
    using handle_type = std::FILE*;

    static auto empty() noexcept -> handle_type { return nullptr; }

    static auto is_valid(handle_type file) noexcept -> bool {
      return file != nullptr;
    }

    static void release(handle_type& file) noexcept;
  };

  auto resolve(const AssetPath& path) const -> std::filesystem::path;

  std::filesystem::path root_;
};

}
