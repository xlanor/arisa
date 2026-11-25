#include <arisa/composition/element.hpp>
#include <arisa/composition/presentation_background.hpp>

#include <type_traits>
#include <utility>

namespace arisa::composition {

PresentationBackground::View::View(Alignment alignment, Build build) noexcept
    : alignment_(alignment), build_(std::move(build)) {}

auto PresentationBackground::View::build() const -> Element { return build_(); }

PresentationBackground::PresentationBackground(
    std::shared_ptr<const View> view) noexcept
    : style_(theme::Fill{theme::Colour::clear}), view_(std::move(view)) {}

auto PresentationBackground::clear() const noexcept -> bool {
  if (view_ != nullptr) {
    return false;
  }
  const auto* fill = std::get_if<theme::Fill>(&style_);
  if (fill == nullptr) {
    return false;
  }
  if (fill->opacity() <= 0.0F) {
    return true;
  }
  const theme::Colour* literal = fill->literal();
  return literal != nullptr && literal->alpha <= 0.0F;
}

auto PresentationBackground::clad(Element panel, theme::Shape shape) const
    -> Element {
  if (clear()) {
    return panel;
  }
  if (view_ != nullptr) {
    return std::move(panel).background(
        view_->alignment(),
        view_->build()
            .frame(FrameSpec{.max_width = infinity,
                             .max_height = infinity,
                             .alignment = view_->alignment()})
            .clip_shape(shape)
            .allows_hit_testing(false));
  }
  return std::visit(
      [&panel, shape](const auto& style) -> Element {
        using Chosen = std::decay_t<decltype(style)>;
        if constexpr (std::is_same_v<Chosen, theme::Glass>) {
          return std::move(panel).glass_effect(style, shape);
        } else {
          return std::move(panel).background(style, shape);
        }
      },
      style_);
}

}
