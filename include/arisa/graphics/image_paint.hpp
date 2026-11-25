#pragma once

#include <algorithm>
#include <memory>
#include <utility>

namespace arisa::provider {

class Image;

}

namespace arisa::graphics {

class ImagePaint {
 public:
  struct SourceRect {
    float x{0.0F};
    float y{0.0F};
    float width{1.0F};
    float height{1.0F};

    friend constexpr auto operator==(const SourceRect&, const SourceRect&)
        -> bool = default;
  };

  struct Origin {
    float x{0.0F};
    float y{0.0F};

    friend constexpr auto operator==(const Origin&, const Origin&)
        -> bool = default;
  };

  explicit ImagePaint(std::shared_ptr<const provider::Image> image) noexcept
      : ImagePaint(std::move(image), SourceRect{}, 1.0F, Origin{}) {}

  ImagePaint(std::shared_ptr<const provider::Image> image,
             SourceRect source_rect) noexcept
      : ImagePaint(std::move(image), source_rect, 1.0F, Origin{}) {}

  ImagePaint(std::shared_ptr<const provider::Image> image,
             SourceRect source_rect, float scale) noexcept
      : ImagePaint(std::move(image), source_rect, scale, Origin{}) {}

  ImagePaint(std::shared_ptr<const provider::Image> image,
             SourceRect source_rect, float scale, Origin origin) noexcept
      : image_(std::move(image)),
        source_rect_(clamped(source_rect)),
        scale_(scale > 0.0F ? scale : 1.0F),
        origin_(origin) {}

  auto image() const noexcept -> const std::shared_ptr<const provider::Image>& {
    return image_;
  }

  auto source_rect() const noexcept -> SourceRect { return source_rect_; }

  auto scale() const noexcept -> float { return scale_; }

  auto origin() const noexcept -> Origin { return origin_; }

  friend auto operator==(const ImagePaint&, const ImagePaint&)
      -> bool = default;

 private:
  static constexpr auto clamped(SourceRect rect) noexcept -> SourceRect {
    const float x = std::clamp(rect.x, 0.0F, 1.0F);
    const float y = std::clamp(rect.y, 0.0F, 1.0F);
    return SourceRect{.x = x,
                      .y = y,
                      .width = std::clamp(rect.width, 0.0F, 1.0F - x),
                      .height = std::clamp(rect.height, 0.0F, 1.0F - y)};
  }

  std::shared_ptr<const provider::Image> image_;
  SourceRect source_rect_;
  float scale_{1.0F};
  Origin origin_;
};

}
