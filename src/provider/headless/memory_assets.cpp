#include <arisa/provider/headless/memory_assets.hpp>

#include <ranges>
#include <utility>

namespace arisa::provider::headless {

void MemoryAssets::add(AssetPath path, std::span<const std::byte> bytes) {
  const std::scoped_lock lock{mutex_};
  revisions_.insert_or_assign(path, ++revision_);
  assets_.insert_or_assign(std::move(path),
                           std::vector<std::byte>(bytes.begin(), bytes.end()));
}

void MemoryAssets::add(AssetPath path, std::string_view text) {
  add(std::move(path), std::as_bytes(std::span{text}));
}

auto MemoryAssets::read(const AssetPath& path) const
    -> std::expected<AssetBlob, ProviderError> {
  const std::scoped_lock lock{mutex_};
  const auto found = assets_.find(path);
  if (found == assets_.end()) {
    return std::unexpected(ProviderError{ProviderErrorCode::not_found});
  }
  return AssetBlob{found->second};
}

auto MemoryAssets::exists(const AssetPath& path) const -> bool {
  const std::scoped_lock lock{mutex_};
  return assets_.contains(path);
}

auto MemoryAssets::stamp(const AssetPath& path) const
    -> std::optional<FileStamp> {
  const std::scoped_lock lock{mutex_};
  const auto found = revisions_.find(path);
  if (found == revisions_.end()) {
    return std::nullopt;
  }
  return FileStamp{found->second};
}

}
