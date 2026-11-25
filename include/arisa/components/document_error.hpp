#pragma once

#include <arisa/composition/file_url.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::components {

class DocumentError {
 public:
  enum class Stage : std::uint8_t {
    open,
    read,
    decode,
    encode,
    write,
    create,
  };

  DocumentError(Stage stage, composition::FileError cause) noexcept
      : stage_(stage), file_(cause) {}

  DocumentError(Stage stage, std::string reason) noexcept
      : stage_(stage), reason_(std::move(reason)) {}

  auto stage() const noexcept -> Stage { return stage_; }

  auto file_error() const noexcept
      -> const std::optional<composition::FileError>& {
    return file_;
  }

  auto reason() const noexcept -> std::string_view { return reason_; }

  static auto stage_name(Stage stage) noexcept -> std::string_view;

  auto explanation() const -> std::string;

  friend auto operator==(const DocumentError&, const DocumentError&)
      -> bool = default;

 private:
  Stage stage_;
  std::optional<composition::FileError> file_;
  std::string reason_;
};

}
