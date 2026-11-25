#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <utility>
#include <vector>

namespace arisa::components {

class GlassEffectContainer : public composition::Element {
 public:
  class Content;

  static constexpr float default_spacing = 20.0F;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit GlassEffectContainer(Children&&... children)
      : GlassEffectContainer(theme::Space{default_spacing},
                             collect(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit GlassEffectContainer(theme::Space spacing, Children&&... children)
      : GlassEffectContainer(spacing,
                             collect(std::forward<Children>(children)...)) {}

  GlassEffectContainer(theme::Space spacing,
                       std::vector<composition::Element> children);
};

class GlassEffectContainer::Content final : public composition::ElementContent {
 public:
  explicit Content(theme::Space spacing) noexcept : spacing_(spacing) {}

  auto type() const noexcept -> composition::ElementType override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto spacing() const noexcept -> theme::Space { return spacing_; }

 private:
  theme::Space spacing_;
};

}
