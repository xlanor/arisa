#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace arisa::provider {

class AssetBlob {
 public:
  explicit AssetBlob(std::vector<std::byte> bytes) noexcept;

  AssetBlob(const AssetBlob&) = delete;
  auto operator=(const AssetBlob&) -> AssetBlob& = delete;
  AssetBlob(AssetBlob&& other) noexcept;
  auto operator=(AssetBlob&& other) noexcept -> AssetBlob&;
  ~AssetBlob() = default;

  auto bytes() const noexcept -> std::span<const std::byte> { return bytes_; }

  auto size() const noexcept -> std::size_t { return bytes_.size(); }

  auto empty() const noexcept -> bool { return bytes_.empty(); }

 private:
  std::vector<std::byte> bytes_;
};

}
