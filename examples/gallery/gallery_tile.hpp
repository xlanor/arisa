#pragma once

#include <arisa/composition/ut_type.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::examples {

class GalleryTile {
 public:
  explicit GalleryTile(std::string name) noexcept : name_(std::move(name)) {}

  static auto content_type() -> composition::UTType {
    return composition::UTType{"dev.arisa.gallery.tile"};
  }

  auto encoded() const -> std::string { return name_; }

  static auto decoded(std::string_view text) -> std::optional<GalleryTile> {
    if (text.empty()) {
      return std::nullopt;
    }
    return GalleryTile{std::string{text}};
  }

  auto name() const noexcept -> const std::string& { return name_; }

  friend auto operator==(const GalleryTile&, const GalleryTile&)
      -> bool = default;

 private:
  std::string name_;
};

}
