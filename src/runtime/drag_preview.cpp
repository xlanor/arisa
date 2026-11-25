#include <runtime/drag_preview.hpp>

#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

auto DragPreview::preview(const composition::Tree& tree,
                          const interaction::DragSession::View& view)
    -> composition::Element {
  const auto source = tree.node(view.source);
  if (source.has_value() && source->get().interactions().drag != nullptr) {
    if (std::optional<composition::Element> built =
            source->get().interactions().drag->preview()) {
      return std::move(*built).shadow(theme::Elevation::high);
    }
  }
  const theme::Shape shape =
      source.has_value() &&
              source->get().interactions().drag_preview_shape.has_value()
          ? *source->get().interactions().drag_preview_shape
          : theme::Shape{theme::RoundedRectangle(
                theme::radius::control, theme::RoundedCornerStyle::continuous)};
  return components::ZStack()
      .frame({.width = view.size.width, .height = view.size.height})
      .background(theme::ColourRole::surface_raised, shape)
      .border(theme::ColourRole::accent, theme::Stroke{outline})
      .shadow(theme::Elevation::high)
      .opacity(lifted_opacity);
}

auto DragPreview::formed(const composition::Tree& tree,
                         const interaction::DragSession::View& view)
    -> composition::Element {
  const std::size_t shown = std::min(view.count, max_copies);
  if (shown <= 1) {
    return preview(tree, view);
  }
  std::vector<composition::Element> copies;
  copies.reserve(shown);
  if (view.formation == composition::DragDropPreviewsFormation::list) {
    for (std::size_t index = 0; index < shown; ++index) {
      copies.push_back(preview(tree, view));
    }
    return components::VStack(
        components::VStackOptions{.spacing = theme::space::xs},
        std::move(copies));
  }
  const bool stacked =
      view.formation == composition::DragDropPreviewsFormation::stack;
  for (std::size_t index = shown; index > 0; --index) {
    const float step = static_cast<float>(index - 1) * copy_offset;
    copies.push_back(preview(tree, view).offset(stacked ? 0.0F : step, step));
  }
  return components::ZStack(components::ZStackOptions{}, std::move(copies))
      .overlay(
          composition::Alignment::top_trailing,
          components::Text(std::to_string(view.count))
              .font(theme::Font::caption)
              .foreground_style(theme::ColourRole::on_accent)
              .padding(
                  theme::Insets::symmetric(theme::space::sm, theme::space::xs))
              .background(theme::ColourRole::accent, theme::Shape::capsule()));
}

void DragPreview::present(
    composition::Tree& tree,
    const std::optional<interaction::DragSession::View>& view) {
  if (!view.has_value()) {
    if (shown_.has_value()) {
      tree.present_overlay(overlay, std::nullopt);
      shown_.reset();
    }
    return;
  }
  if (shown_ == view->revision) {
    return;
  }
  shown_ = view->revision;
  std::vector<composition::Element> children;
  if (view->formation == composition::DragDropPreviewsFormation::none) {
    if (shown_.has_value()) {
      tree.present_overlay(overlay, std::nullopt);
    }
    shown_ = view->revision;
    return;
  }
  children.push_back(formed(tree, *view)
                         .position(view->location)
                         .allows_hit_testing(false)
                         .transition(motion::Transition::identity()));
  tree.present_overlay(
      overlay, composition::Layer(composition::OverlayLayer::drag,
                                  composition::OverlayInput::passthrough,
                                  composition::Alignment::top_leading,
                                  std::move(children)));
}

}
