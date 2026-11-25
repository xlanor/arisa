#include <arisa/provider/assets/asset_blob.hpp>

#include <utility>

namespace arisa::provider {

AssetBlob::AssetBlob(std::vector<std::byte> bytes) noexcept
    : bytes_(std::move(bytes)) {}

AssetBlob::AssetBlob(AssetBlob&& other) noexcept
    : bytes_(std::exchange(other.bytes_, {})) {}

auto AssetBlob::operator=(AssetBlob&& other) noexcept -> AssetBlob& {
  if (this != &other) {
    bytes_ = std::exchange(other.bytes_, {});
  }
  return *this;
}

}
