#pragma once

#include <arisa/components/graphics_context.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/components/text_proxy.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/proposed_view_size.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <memory>
#include <utility>

namespace arisa::components {

class TextRenderer {
 public:
  TextRenderer() = default;
  TextRenderer(const TextRenderer&) = default;
  auto operator=(const TextRenderer&) -> TextRenderer& = default;
  TextRenderer(TextRenderer&&) = default;
  auto operator=(TextRenderer&&) -> TextRenderer& = default;
  virtual ~TextRenderer() = default;

  virtual void draw(const Text::Layout& layout,
                    GraphicsContext& context) const = 0;

  virtual auto size_that_fits(composition::ProposedViewSize proposal,
                              const TextProxy& text) const
      -> composition::Size {
    return text.size_that_fits(proposal);
  }

  virtual auto display_padding() const -> theme::EdgeInsets { return {}; }

  static auto applied(composition::Element content,
                      std::shared_ptr<const TextRenderer> renderer)
      -> composition::Element;

 private:
  static auto drawn(const std::shared_ptr<const TextRenderer>& renderer,
                    const Text::LayoutKey::Value& layouts)
      -> composition::Element;

  static auto negated(theme::Space space) noexcept -> theme::Space;
};

}

namespace arisa::composition {

template <class Renderer>
auto Element::text_renderer(Renderer renderer) && -> Element {
  static_assert(std::derived_from<Renderer, components::TextRenderer>);
  return components::TextRenderer::applied(
      std::move(*this), std::make_shared<const Renderer>(std::move(renderer)));
}

}
