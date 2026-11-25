#pragma once

#include <arisa/components/graphics_context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <functional>
#include <vector>

namespace arisa::components {

class Canvas : public composition::Element {
 public:
  class Content;

  using Renderer = std::function<void(GraphicsContext&, composition::Size)>;
  using Symbols = std::function<composition::Element()>;

  explicit Canvas(Renderer renderer, bool opaque = false);

  Canvas(Renderer renderer, bool opaque, Symbols symbols);

  Canvas(bool opaque, bool renders_asynchronously, Renderer renderer,
         Symbols symbols = {});

 private:
  static auto rendered(const Renderer& renderer, composition::Size size,
                       bool opaque, GraphicsContext::Resources resources)
      -> composition::Element;
};

class Canvas::Content final : public composition::ElementContent {
 public:
  static const composition::StateKey<std::vector<composition::Size>> measured;
  static const composition::StateKey<std::vector<GraphicsContext::TextMeasure>>
      texts;

  Content(Renderer renderer, bool opaque, Symbols symbols,
          bool renders_asynchronously = false) noexcept
      : renderer_(std::move(renderer)),
        opaque_(opaque),
        symbols_(std::move(symbols)),
        asynchronous_(renders_asynchronously) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto symbols() const noexcept -> const Symbols& { return symbols_; }

  auto renders_asynchronously() const noexcept -> bool { return asynchronous_; }

 private:
  static auto probes(
      const GraphicsContext::TextLedger& ledger,
      composition::StateHandle<std::vector<GraphicsContext::TextMeasure>> known)
      -> std::vector<composition::Element>;

  Renderer renderer_;
  bool opaque_{false};
  Symbols symbols_;
  bool asynchronous_{false};
};

}
