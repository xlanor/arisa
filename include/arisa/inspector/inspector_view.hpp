#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/theme/colour_value.hpp>

#include <memory>
#include <string_view>
#include <utility>

namespace arisa::inspector {

class InspectorModel;

class InspectorView final : public composition::Component {
 public:
  static constexpr std::string_view panel_key = "arisa.inspector.panel";
  static constexpr std::string_view outline_key = "arisa.inspector.outline";
  static constexpr std::string_view hover_key = "arisa.inspector.hover";
  static constexpr std::string_view cursor_key = "arisa.inspector.cursor";
  static constexpr float cursor_size = 14.0F;
  static constexpr float corner_radius = 16.0F;

  explicit InspectorView(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const InspectorView&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  auto panel() const -> composition::Element;

  auto header() const -> composition::Element;

  auto content() const -> composition::Element;

  static auto outline(const composition::Rect& bounds, theme::ColourRole colour,
                      std::string_view key) -> composition::Element;

  static auto cursor(composition::Point at) -> composition::Element;

  static auto scrolled(composition::Element panel) -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
};

}
