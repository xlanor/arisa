#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/shape.hpp>

#include <functional>
#include <memory>
#include <variant>

namespace arisa::composition {

class Element;

class PresentationBackground {
 public:
  using Style = std::variant<theme::Material, theme::Glass, theme::Fill>;

  class View {
   public:
    using Build = std::function<Element()>;

    View(Alignment alignment, Build build) noexcept;

    auto alignment() const noexcept -> Alignment { return alignment_; }

    auto build() const -> Element;

   private:
    Alignment alignment_;
    Build build_;
  };

  explicit PresentationBackground(theme::Material material) noexcept
      : style_(material) {}

  explicit PresentationBackground(theme::Glass glass) noexcept
      : style_(glass) {}

  explicit PresentationBackground(theme::Fill fill) noexcept : style_(fill) {}

  explicit PresentationBackground(std::shared_ptr<const View> view) noexcept;

  auto style() const noexcept -> const Style& { return style_; }

  auto view() const noexcept -> const std::shared_ptr<const View>& {
    return view_;
  }

  auto clear() const noexcept -> bool;

  auto clad(Element panel, theme::Shape shape) const -> Element;

  friend auto operator==(const PresentationBackground&,
                         const PresentationBackground&) -> bool = default;

 private:
  Style style_;
  std::shared_ptr<const View> view_;
};

}
