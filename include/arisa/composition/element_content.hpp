#pragma once

#include <arisa/composition/accessibility.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/string_catalog.hpp>

#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

class Context;
class Element;
class GeometryProxy;
class Node;
struct SectionParts;

class ElementContent {
 public:
  virtual ~ElementContent() = default;

  virtual auto type() const noexcept -> ElementType = 0;

  virtual auto kind() const noexcept -> ElementType;

  virtual auto composes() const noexcept -> bool;

  virtual auto passes_through() const noexcept -> bool;

  virtual auto draws() const noexcept -> bool;

  virtual auto body(Context& context) const -> std::vector<Element>;

  virtual auto same_as(const ElementContent& other) const -> bool;

  virtual auto paints_like(const ElementContent& other) const -> bool;

  virtual auto adopt(ElementContent& incoming) -> bool;

  virtual auto ignores_change(Identity key,
                              const EnvironmentOverride::Value& before,
                              const EnvironmentOverride::Value& after) const
      -> bool;

  virtual auto activate() -> bool;

  virtual auto activate(Context& context, const Node& node,
                        std::optional<Point> at) -> bool;

  virtual auto steer(Context& context, const Node& node,
                     MoveCommandDirection direction) -> bool;

  virtual void enter(Context& context, const Node& node,
                     MoveCommandDirection direction);

  virtual auto press(Context& context, const Node& node, const KeyPress& press)
      -> bool;

  virtual auto activatable() const noexcept -> bool;

  virtual auto framed() const noexcept -> bool;

  virtual auto notes() const -> std::vector<std::string>;

  virtual auto accessibility() const -> AccessibilityDefaults;

  virtual auto localized_accessibility(const text::Locale& locale,
                                       const text::StringCatalog& catalog) const
      -> AccessibilityDefaults;

  virtual auto contributes_section() const noexcept -> bool;

  virtual auto section_parts() const -> std::optional<SectionParts>;

  virtual auto hosts_sidebar() const noexcept -> bool;

  virtual auto distributes_decoration() const noexcept -> bool;

  virtual auto materialize(Context& context, const Node& node,
                           MoveCommandDirection direction) -> bool;

  virtual auto reveals() const noexcept -> bool;

  virtual auto reveal(Context& context, const Node& node, const Rect& visible)
      -> bool;

  virtual auto locate(Context& context, const Node& node, const ElementKey& key)
      -> std::optional<Rect>;

  virtual auto page_stop(Context& context, const Node& node, float position)
      -> std::optional<float>;

  virtual auto reads_geometry() const noexcept -> bool;

  virtual void place(Context& context, const GeometryProxy& proxy);

  virtual auto isolates(Identity preference) const noexcept -> bool;

  virtual auto reads_preferences() const noexcept -> bool;

  virtual void resolve(Context& context, const Node& node);

  virtual auto refreshes() const noexcept -> bool;

  virtual auto refresh(Context& context, const Node& node) -> bool;
};

}
