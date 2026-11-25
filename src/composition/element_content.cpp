#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/section_parts.hpp>

namespace arisa::composition {

auto ElementContent::kind() const noexcept -> ElementType { return type(); }

auto ElementContent::composes() const noexcept -> bool { return false; }

auto ElementContent::passes_through() const noexcept -> bool { return false; }

auto ElementContent::draws() const noexcept -> bool { return true; }

auto ElementContent::body(Context&) const -> std::vector<Element> { return {}; }

auto ElementContent::same_as(const ElementContent&) const -> bool {
  return false;
}

auto ElementContent::paints_like(const ElementContent&) const -> bool {
  return true;
}

auto ElementContent::adopt(ElementContent&) -> bool { return false; }

auto ElementContent::ignores_change(Identity, const EnvironmentOverride::Value&,
                                    const EnvironmentOverride::Value&) const
    -> bool {
  return false;
}

auto ElementContent::activate() -> bool { return false; }

auto ElementContent::activate(Context&, const Node&, std::optional<Point>)
    -> bool {
  return activate();
}

auto ElementContent::steer(Context&, const Node&, MoveCommandDirection)
    -> bool {
  return false;
}

void ElementContent::enter(Context&, const Node&, MoveCommandDirection) {}

auto ElementContent::press(Context&, const Node&, const KeyPress&) -> bool {
  return false;
}

auto ElementContent::activatable() const noexcept -> bool { return false; }

auto ElementContent::framed() const noexcept -> bool { return false; }

auto ElementContent::accessibility() const -> AccessibilityDefaults {
  return {};
}

auto ElementContent::localized_accessibility(const text::Locale&,
                                             const text::StringCatalog&) const
    -> AccessibilityDefaults {
  return accessibility();
}

auto ElementContent::notes() const -> std::vector<std::string> { return {}; }

auto ElementContent::hosts_sidebar() const noexcept -> bool { return false; }

auto ElementContent::distributes_decoration() const noexcept -> bool {
  return false;
}

auto ElementContent::contributes_section() const noexcept -> bool {
  return false;
}

auto ElementContent::section_parts() const -> std::optional<SectionParts> {
  return std::nullopt;
}

auto ElementContent::materialize(Context&, const Node&, MoveCommandDirection)
    -> bool {
  return false;
}

auto ElementContent::reveals() const noexcept -> bool { return false; }

auto ElementContent::reveal(Context&, const Node&, const Rect&) -> bool {
  return false;
}

auto ElementContent::locate(Context&, const Node&, const ElementKey&)
    -> std::optional<Rect> {
  return std::nullopt;
}

auto ElementContent::page_stop(Context&, const Node&, float)
    -> std::optional<float> {
  return std::nullopt;
}

auto ElementContent::reads_geometry() const noexcept -> bool { return false; }

void ElementContent::place(Context&, const GeometryProxy&) {}

auto ElementContent::isolates(Identity) const noexcept -> bool { return false; }

auto ElementContent::reads_preferences() const noexcept -> bool {
  return false;
}

void ElementContent::resolve(Context&, const Node&) {}

auto ElementContent::refreshes() const noexcept -> bool { return false; }

auto ElementContent::refresh(Context&, const Node&) -> bool { return false; }

}
