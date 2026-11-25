#pragma once

#include <arisa/provider/assets/asset_blob.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/assets/file_stamp.hpp>
#include <arisa/provider/error.hpp>

#include <expected>
#include <optional>

namespace arisa::provider {

class AssetProvider {
 public:
  virtual ~AssetProvider() = default;

  [[nodiscard]] virtual auto read(const AssetPath& path) const
      -> std::expected<AssetBlob, ProviderError> = 0;

  virtual auto exists(const AssetPath& path) const -> bool = 0;

  virtual auto stamp(const AssetPath& path) const -> std::optional<FileStamp> {
    static_cast<void>(path);
    return std::nullopt;
  }
};

}
