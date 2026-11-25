#pragma once

#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/assets/file_stamp.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/storage/directory_entry.hpp>

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace arisa::provider {

class StorageProvider {
 public:
  virtual ~StorageProvider() = default;

  [[nodiscard]] virtual auto read(const AssetPath& path) const
      -> std::expected<AssetBlob, ProviderError> = 0;

  [[nodiscard]] virtual auto write(const AssetPath& path,
                                   std::span<const std::byte> bytes)
      -> std::expected<void, ProviderError> = 0;

  [[nodiscard]] virtual auto list(const AssetPath& directory) const
      -> std::expected<std::vector<std::string>, ProviderError> = 0;

  [[nodiscard]] virtual auto create_directories(const AssetPath& directory)
      -> std::expected<void, ProviderError> = 0;

  [[nodiscard]] virtual auto entries(const std::optional<AssetPath>& directory)
      const -> std::expected<std::vector<DirectoryEntry>, ProviderError> {
    if (!directory.has_value()) {
      return std::unexpected(ProviderError::unsupported_capability());
    }
    auto names = list(*directory);
    if (!names.has_value()) {
      return std::unexpected(names.error());
    }
    std::vector<DirectoryEntry> found;
    found.reserve(names->size());
    for (std::string& name : *names) {
      found.push_back(DirectoryEntry{.name = std::move(name),
                                     .kind = DirectoryEntry::Kind::file,
                                     .size = 0});
    }
    return found;
  }

  [[nodiscard]] virtual auto remove(const AssetPath&)
      -> std::expected<void, ProviderError> {
    return std::unexpected(ProviderError::unsupported_capability());
  }

  virtual auto location(const AssetPath& path) const -> std::string {
    return std::string{path.value()};
  }

  virtual auto stamp(const AssetPath& path) const -> std::optional<FileStamp> {
    static_cast<void>(path);
    return std::nullopt;
  }
};

}
