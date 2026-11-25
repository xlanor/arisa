#pragma once

#include <arisa/composition/file_url.hpp>

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class FileStorage;

class FileWrapper {
 public:
  static auto regular_file(std::vector<std::byte> contents) -> FileWrapper;

  static auto regular_file(std::string_view text) -> FileWrapper;

  static auto directory(std::vector<FileWrapper> children) -> FileWrapper;

  [[nodiscard]] static auto read(const FileStorage& storage,
                                 const composition::FileURL& file)
      -> std::expected<FileWrapper, composition::FileError>;

  [[nodiscard]] auto write(const FileStorage& storage,
                           const composition::FileURL& file) const
      -> std::expected<void, composition::FileError>;

  auto is_regular_file() const noexcept -> bool { return !directory_; }

  auto is_directory() const noexcept -> bool { return directory_; }

  auto regular_file_contents() const noexcept
      -> std::optional<std::span<const std::byte>>;

  auto text() const -> std::optional<std::string>;

  auto file_wrappers() const noexcept -> std::span<const FileWrapper> {
    return children_;
  }

  auto filename() const noexcept -> const std::optional<std::string>& {
    return filename_;
  }

  void set_filename(std::string filename) { filename_ = std::move(filename); }

  auto preferred_filename() const noexcept
      -> const std::optional<std::string>& {
    return preferred_filename_;
  }

  void set_preferred_filename(std::string filename) {
    preferred_filename_ = std::move(filename);
  }

  friend auto operator==(const FileWrapper& left, const FileWrapper& right)
      -> bool;

 private:
  FileWrapper(std::shared_ptr<const std::vector<std::byte>> contents,
              std::vector<FileWrapper> children, bool directory) noexcept;

  auto name_or(std::string_view fallback) const -> std::string;

  std::shared_ptr<const std::vector<std::byte>> contents_;
  std::vector<FileWrapper> children_;
  bool directory_{false};
  std::optional<std::string> filename_;
  std::optional<std::string> preferred_filename_;
};

}
