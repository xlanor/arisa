#include <arisa/components/column_resizer.hpp>

#include <arisa/components/zstack.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <memory>
#include <utility>

namespace arisa::components {

ColumnResizer::ColumnResizer(Resize resize)
    : Element(std::make_unique<Content>(std::move(resize))) {}

auto ColumnResizer::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ColumnResizer::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  const Resize resize = resize_;
  std::vector<composition::Element> produced;
  if (!resize.active) {
    produced.push_back(ZStack()
                           .frame({.width = 0.0F, .height = 0.0F})
                           .allows_hit_testing(false));
    return produced;
  }
  produced.push_back(
      ZStack()
          .frame({.width = grab_width, .max_height = composition::infinity})
          .content_shape(theme::Shape::rectangle())
          .gesture(
              composition::DragGesture(0.0F)
                  .on_changed(
                      [resize](const composition::DragGesture::Value& value) {
                        const float start =
                            resize.anchor.get().value_or(resize.current);
                        if (!resize.anchor.get().has_value()) {
                          resize.anchor.set(start);
                        }
                        const float next = std::clamp(
                            start +
                                (resize.direction * value.translation.width),
                            resize.minimum, resize.maximum);
                        if (resize.width.get() != next) {
                          resize.width.set(next);
                        }
                      })
                  .on_ended([resize](const composition::DragGesture::Value&) {
                    resize.anchor.set(std::nullopt);
                  })));
  return produced;
}

}
