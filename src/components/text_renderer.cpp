#include <arisa/components/text_renderer.hpp>

#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/preference_reader.hpp>
#include <arisa/composition/text_environment.hpp>

#include <utility>

namespace arisa::components {

auto TextRenderer::drawn(const std::shared_ptr<const TextRenderer>& renderer,
                         const Text::LayoutKey::Value& layouts)
    -> composition::Element {
  const theme::EdgeInsets padding = renderer->display_padding();
  return GeometryReader(
             [renderer, layouts](const composition::GeometryProxy& proxy) {
               GraphicsContext context{proxy.size()};
               for (const Text::LayoutKey::AnchoredLayout& anchored : layouts) {
                 const composition::Point origin = proxy[anchored.origin];
                 const graphics::AffineTransform saved = context.transform();
                 context.translate_by(origin.x, origin.y);
                 renderer->draw(anchored.layout, context);
                 context.set_transform(saved);
               }
               return std::move(context).finish();
             })
      .padding(theme::Insets{.top = negated(padding.top),
                             .leading = negated(padding.leading),
                             .bottom = negated(padding.bottom),
                             .trailing = negated(padding.trailing)})
      .allows_hit_testing(false);
}

auto TextRenderer::negated(theme::Space space) noexcept -> theme::Space {
  const float* points = space.points();
  return theme::Space{points != nullptr ? -*points : 0.0F};
}

auto TextRenderer::applied(composition::Element content,
                           std::shared_ptr<const TextRenderer> renderer)
    -> composition::Element {
  return std::move(content)
      .environment(composition::TextEnvironment::publishes_layout, true)
      .environment(composition::TextEnvironment::custom_rendered, true)
      .overlay_preference_value(Text::LayoutKey::key,
                                [renderer = std::move(renderer)](
                                    const Text::LayoutKey::Value& layouts) {
                                  return drawn(renderer, layouts);
                                });
}

}
