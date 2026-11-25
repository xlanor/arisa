#pragma once

#include <arisa/provider/assets/provider.hpp>

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace arisa::provider::headless {

class MemoryAssets final : public AssetProvider {
 public:
  void add(AssetPath path, std::span<const std::byte> bytes);

  void add(AssetPath path, std::string_view text);

  [[nodiscard]] auto read(const AssetPath& path) const
      -> std::expected<AssetBlob, ProviderError> override;

  auto exists(const AssetPath& path) const -> bool override;

  auto stamp(const AssetPath& path) const -> std::optional<FileStamp> override;

 private:
  mutable std::mutex mutex_;
  std::flat_map<AssetPath, std::vector<std::byte>, std::less<>> assets_;
  std::flat_map<AssetPath, std::uint64_t, std::less<>> revisions_;
  std::uint64_t revision_{0};
};

}
