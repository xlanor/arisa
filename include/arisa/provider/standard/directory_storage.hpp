#pragma once

#include <arisa/provider/storage/provider.hpp>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace arisa::provider::standard {

class DirectoryStorage final : public StorageProvider {
 public:
  explicit DirectoryStorage(std::filesystem::path root);

  static auto application_config(std::string_view application)
      -> DirectoryStorage;

  [[nodiscard]] auto read(const AssetPath& path) const
      -> std::expected<AssetBlob, ProviderError> override;

  [[nodiscard]] auto write(const AssetPath& path,
                           std::span<const std::byte> bytes)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto list(const AssetPath& directory) const
      -> std::expected<std::vector<std::string>, ProviderError> override;

  [[nodiscard]] auto create_directories(const AssetPath& directory)
      -> std::expected<void, ProviderError> override;

  [[nodiscard]] auto entries(const std::optional<AssetPath>& directory) const
      -> std::expected<std::vector<DirectoryEntry>, ProviderError> override;

  [[nodiscard]] auto remove(const AssetPath& path)
      -> std::expected<void, ProviderError> override;

  auto location(const AssetPath& path) const -> std::string override;

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

  static auto home() -> std::filesystem::path;

  auto resolve(const AssetPath& path) const -> std::filesystem::path;

  [[nodiscard]] static auto ensure(const std::filesystem::path& directory)
      -> std::expected<void, ProviderError>;

  static auto failure(int error) noexcept -> ProviderError;

  std::filesystem::path root_;
};

}
