#pragma once

#include <arisa/provider/error.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::composition {

class FileURL {
 public:
  static auto create(std::string_view path) -> std::optional<FileURL>;

  auto path() const noexcept -> std::string_view { return path_; }

  auto last_path_component() const noexcept -> std::string_view;

  auto deleting_last_path_component() const -> std::optional<FileURL>;

  auto appending(std::string_view component) const -> std::optional<FileURL>;

  friend auto operator==(const FileURL&, const FileURL&) -> bool = default;

 private:
  explicit FileURL(std::string path) noexcept : path_(std::move(path)) {}

  std::string path_;
};

class FileError {
 public:
  enum class Stage : std::uint8_t {
    storage,
    list,
    read,
    write,
    remove,
    name,
  };

  FileError(Stage stage, provider::ProviderError cause) noexcept
      : stage_(stage), cause_(cause) {}

  auto stage() const noexcept -> Stage { return stage_; }

  auto cause() const noexcept -> const provider::ProviderError& {
    return cause_;
  }

  static auto stage_name(Stage stage) noexcept -> std::string_view;

  auto explanation() const -> std::string;

  friend auto operator==(const FileError&, const FileError&) -> bool = default;

 private:
  Stage stage_;
  provider::ProviderError cause_;
};

}
