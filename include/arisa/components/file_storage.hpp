#pragma once

#include <arisa/composition/file_url.hpp>
#include <arisa/provider/storage/provider.hpp>

#include <cstddef>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace arisa::components {

class FileStorage {
 public:
  explicit FileStorage(provider::StorageProvider& storage) noexcept
      : storage_(storage) {}

  auto entries(const std::optional<composition::FileURL>& directory) const
      -> std::expected<std::vector<provider::DirectoryEntry>,
                       composition::FileError>;

  auto read(const composition::FileURL& file) const
      -> std::expected<std::vector<std::byte>, composition::FileError>;

  auto write(const composition::FileURL& file,
             std::span<const std::byte> bytes) const
      -> std::expected<void, composition::FileError>;

  auto create_directory(const composition::FileURL& directory) const
      -> std::expected<void, composition::FileError>;

  auto exists(const composition::FileURL& file) const -> bool;

  auto stamp(const composition::FileURL& file) const
      -> std::optional<provider::FileStamp>;

  auto move(const composition::FileURL& file,
            const composition::FileURL& destination) const
      -> std::expected<void, composition::FileError>;

  auto provider() const noexcept -> provider::StorageProvider& {
    return storage_.get();
  }

  friend auto operator==(const FileStorage& left, const FileStorage& right)
      -> bool {
    return &left.storage_.get() == &right.storage_.get();
  }

 private:
  static auto path(const composition::FileURL& file,
                   composition::FileError::Stage stage)
      -> std::expected<provider::AssetPath, composition::FileError>;

  std::reference_wrapper<provider::StorageProvider> storage_;
};

}
