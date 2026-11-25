#include <arisa/composition/pointer_style.hpp>

#include <arisa/composition/element.hpp>

#include <utility>

namespace arisa::composition {

auto PointerStyle::column_resize(HorizontalDirections directions)
    -> PointerStyle {
  PointerStyle style{Kind::column_resize};
  style.horizontal_ = directions;
  return style;
}

auto PointerStyle::row_resize(VerticalDirections directions) -> PointerStyle {
  PointerStyle style{Kind::row_resize};
  style.vertical_ = directions;
  return style;
}

auto PointerStyle::frame_resize(FrameResizePosition position,
                                FrameResizeDirections directions)
    -> PointerStyle {
  PointerStyle style{Kind::frame_resize};
  style.position_ = position;
  style.frame_directions_ = directions;
  return style;
}

auto PointerStyle::image(ImageBuilder image, motion::UnitPoint hot_spot)
    -> PointerStyle {
  PointerStyle style{Kind::image};
  style.image_ = std::make_shared<const ImageBuilder>(std::move(image));
  style.hot_spot_ = hot_spot;
  return style;
}

}
