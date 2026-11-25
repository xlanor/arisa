#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/text/typesetter.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::runtime {

class ImageRenderer {
 public:
  using TypesetterRef = std::optional<std::reference_wrapper<text::Typesetter>>;

  enum class Failure : std::uint8_t { empty, layout, paint, capture };

  static constexpr float measuring_extent = 4096.0F;

  explicit ImageRenderer(composition::Element content);

  void set_scale(float scale) noexcept;

  auto scale() const noexcept -> float { return scale_; }

  void set_proposed_size(std::optional<composition::Size> size) noexcept {
    proposed_ = size;
  }

  auto proposed_size() const noexcept -> std::optional<composition::Size> {
    return proposed_;
  }

  void set_opaque(bool opaque) noexcept { opaque_ = opaque; }

  auto is_opaque() const noexcept -> bool { return opaque_; }

  [[nodiscard]] auto image(TypesetterRef typesetter = std::nullopt)
      -> std::expected<provider::Image, Failure>;

 private:
  auto measured(TypesetterRef typesetter) -> std::optional<composition::Size>;

  std::unique_ptr<composition::Tree> tree_;
  float scale_{1.0F};
  std::optional<composition::Size> proposed_;
  bool opaque_{false};
};

}
