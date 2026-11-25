#pragma once

#include <arisa/composition/frame_resize_directions.hpp>
#include <arisa/composition/frame_resize_position.hpp>
#include <arisa/composition/horizontal_directions.hpp>
#include <arisa/composition/vertical_directions.hpp>
#include <arisa/motion/unit_point.hpp>

#include <cstdint>
#include <functional>
#include <memory>

namespace arisa::composition {

class Element;

class PointerStyle {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    horizontal_text,
    vertical_text,
    rect_selection,
    grab_idle,
    grab_active,
    link,
    zoom_in,
    zoom_out,
    column_resize,
    row_resize,
    frame_resize,
    image,
  };

  using ImageBuilder = std::function<Element()>;

  static const PointerStyle default_;
  static const PointerStyle horizontal_text;
  static const PointerStyle vertical_text;
  static const PointerStyle rect_selection;
  static const PointerStyle grab_idle;
  static const PointerStyle grab_active;
  static const PointerStyle link;
  static const PointerStyle zoom_in;
  static const PointerStyle zoom_out;

  PointerStyle() noexcept = default;

  static auto column_resize(HorizontalDirections directions =
                                HorizontalDirections::all) -> PointerStyle;

  static auto row_resize(
      VerticalDirections directions = VerticalDirections::all) -> PointerStyle;

  static auto frame_resize(FrameResizePosition position,
                           FrameResizeDirections directions =
                               FrameResizeDirections::all) -> PointerStyle;

  static auto image(ImageBuilder image, motion::UnitPoint hot_spot)
      -> PointerStyle;

  auto kind() const noexcept -> Kind { return kind_; }

  auto horizontal() const noexcept -> HorizontalDirections {
    return horizontal_;
  }

  auto vertical() const noexcept -> VerticalDirections { return vertical_; }

  auto position() const noexcept -> FrameResizePosition { return position_; }

  auto frame_directions() const noexcept -> FrameResizeDirections {
    return frame_directions_;
  }

  auto hot_spot() const noexcept -> motion::UnitPoint { return hot_spot_; }

  auto built() const -> std::shared_ptr<const ImageBuilder> { return image_; }

  friend auto operator==(const PointerStyle&, const PointerStyle&)
      -> bool = default;

 private:
  explicit PointerStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_{Kind::automatic};
  HorizontalDirections horizontal_{HorizontalDirections::all};
  VerticalDirections vertical_{VerticalDirections::all};
  FrameResizePosition position_{FrameResizePosition::bottom_trailing};
  FrameResizeDirections frame_directions_{FrameResizeDirections::all};
  std::shared_ptr<const ImageBuilder> image_;
  motion::UnitPoint hot_spot_{motion::UnitPoint::top_leading};
};

inline const PointerStyle PointerStyle::default_{Kind::automatic};
inline const PointerStyle PointerStyle::horizontal_text{Kind::horizontal_text};
inline const PointerStyle PointerStyle::vertical_text{Kind::vertical_text};
inline const PointerStyle PointerStyle::rect_selection{Kind::rect_selection};
inline const PointerStyle PointerStyle::grab_idle{Kind::grab_idle};
inline const PointerStyle PointerStyle::grab_active{Kind::grab_active};
inline const PointerStyle PointerStyle::link{Kind::link};
inline const PointerStyle PointerStyle::zoom_in{Kind::zoom_in};
inline const PointerStyle PointerStyle::zoom_out{Kind::zoom_out};

}
