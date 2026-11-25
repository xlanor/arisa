#pragma once

#include <arisa/provider/storage/provider.hpp>

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::provider::headless {

class MemoryStorage final : public StorageProvider {
 public:
  void add(AssetPath path, std::string_view text);

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

  auto stamp(const AssetPath& path) const -> std::optional<FileStamp> override;

  auto has_directory(const AssetPath& directory) const -> bool;

  auto text(const AssetPath& path) const -> std::string;

  auto stamps() const -> std::size_t;

  auto writes() const noexcept -> std::size_t { return writes_; }

 private:
  mutable std::mutex mutex_;
  std::flat_map<AssetPath, std::vector<std::byte>, std::less<>> files_;
  std::flat_map<AssetPath, std::uint64_t, std::less<>> revisions_;
  std::flat_set<std::string, std::less<>> directories_;
  std::uint64_t revision_{0};
  mutable std::size_t stamps_{0};
  std::size_t writes_{0};
};

}
