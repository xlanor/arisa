#include <arisa/composition/drag_source.hpp>
#include <arisa/composition/element.hpp>

namespace arisa::composition {

auto DragSource::preview() const -> std::optional<Element> {
  return std::nullopt;
}

}
