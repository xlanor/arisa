#pragma once

#include <arisa/components/image.hpp>

#include <optional>
#include <string>
#include <utility>

namespace arisa::components {

class SharePreview {
 public:
  explicit SharePreview(std::string title) : title_(std::move(title)) {}

  SharePreview(std::string title, const Image& image)
      : title_(std::move(title)), image_(image.encoded()) {}

  SharePreview(std::string title, const Image& image, const Image& icon)
      : title_(std::move(title)),
        image_(image.encoded()),
        icon_(icon.encoded()) {}

  static auto with_icon(std::string title, const Image& icon) -> SharePreview {
    SharePreview made{std::move(title)};
    made.icon_ = icon.encoded();
    return made;
  }

  auto title() const noexcept -> const std::string& { return title_; }

  auto image() const -> std::optional<Image> {
    return image_.has_value() ? Image::decoded(*image_) : std::nullopt;
  }

  auto icon() const -> std::optional<Image> {
    return icon_.has_value() ? Image::decoded(*icon_) : std::nullopt;
  }

  friend auto operator==(const SharePreview&, const SharePreview&)
      -> bool = default;

 private:
  std::string title_;
  std::optional<std::string> image_;
  std::optional<std::string> icon_;
};

}
