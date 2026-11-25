#pragma once

#include <arisa/components/panel_radius.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <memory>
#include <vector>

namespace arisa::components {

class GlassPanel : public composition::Element {
 public:
  class Content;

  static constexpr float small_radius = 12.0F;
  static constexpr float medium_radius = 18.0F;
  static constexpr float large_radius = 24.0F;
  static constexpr float modal_radius = 28.0F;

  GlassPanel(PanelRadius radius, composition::Element content);

  static constexpr auto points(PanelRadius radius) noexcept -> float {
    switch (radius) {
      case PanelRadius::small:
        return small_radius;
      case PanelRadius::medium:
        return medium_radius;
      case PanelRadius::large:
        return large_radius;
      case PanelRadius::modal:
        return modal_radius;
    }
    return small_radius;
  }

  static constexpr auto shape(PanelRadius radius) noexcept -> theme::Shape {
    return theme::Shape::rounded(points(radius), true);
  }
};

class GlassPanel::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source content, PanelRadius radius) noexcept
      : content_(std::move(content)), radius_(radius) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto radius() const noexcept -> PanelRadius { return radius_; }

  void hold(Source content) noexcept { content_ = std::move(content); }

 private:
  Source content_;
  PanelRadius radius_;
};

}
