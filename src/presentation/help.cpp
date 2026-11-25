#include <arisa/presentation/help.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/input_modality.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/layer_host.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <cstdint>
#include <optional>

namespace arisa::presentation {

using composition::Element;

Help::Help(Element content, std::string text)
    : Element(
          std::make_unique<Content>(nullptr, std::nullopt, std::move(text))) {
  inherit_identity(*this, content);
  inherit(*this, content);
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content)));
}

Help::Help(Element content, std::string title, std::string body)
    : Element(std::make_unique<Content>(nullptr, std::move(title),
                                        std::move(body))) {
  inherit_identity(*this, content);
  inherit(*this, content);
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content)));
}

auto Help::label(const Element& element)
    -> std::optional<std::reference_wrapper<const Element>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return static_cast<const Content&>(element.content()).pending_label();
}

auto Help::relabel(Element help, const std::function<Element(Element)>& change)
    -> Element {
  if (help.type() != composition::ElementType::of<Content>()) {
    return help;
  }
  auto& held = static_cast<Content&>(content_of(help));
  if (std::optional<Element> label = held.take_label()) {
    held.hold(std::make_shared<composition::PlaceholderContentView::Source>(
        change(std::move(*label))));
  }
  return help;
}

auto Help::dwell(composition::StateHandle<bool> dwelled, bool within)
    -> async::Task<void> {
  if (!within) {
    if (dwelled.get()) {
      dwelled.set(false);
    }
    co_return;
  }
  co_await async::sleep(focus_delay);
  dwelled.set(true);
}

auto Help::linger(composition::StateHandle<bool> held) -> async::Task<void> {
  co_await async::sleep(touch_linger);
  held.set(false);
}

auto Help::tip(std::optional<std::string> title, std::string text) -> Element {
  Element shown = components::Text(std::move(text)).font(theme::Font::footnote);
  if (title.has_value()) {
    shown = components::VStack(
                components::VStackOptions{
                    .alignment = components::HorizontalAlignment::leading,
                    .spacing = theme::space::xs},
                components::Text(std::move(*title))
                    .font(theme::Font::footnote)
                    .bold()
                    .foreground_style(theme::ColourRole::text_primary),
                std::move(shown).foreground_style(
                    theme::ColourRole::text_secondary))
                .fixed_size(false, true)
                .frame({.max_width = titled_width,
                        .alignment = composition::Alignment::leading});
  }
  return std::move(shown)
      .padding(theme::Insets::symmetric(theme::space::md, theme::space::sm))
      .background(
          theme::ColourRole::surface_raised,
          theme::RoundedRectangle(theme::radius::control,
                                  theme::RoundedCornerStyle::continuous))
      .border(theme::ColourRole::separator, theme::Stroke::hairline())
      .shadow(theme::Elevation::medium);
}

auto Help::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Help::Content::body(composition::Context& context) const
    -> std::vector<Element> {
  const bool within = context.focus_within();
  const composition::InputModality modality =
      context.environment(components::Environment::input_modality);
  const bool touch = modality == composition::InputModality::touch;
  const bool hovering =
      context.hover_within() &&
      modality == composition::InputModality::keyboard_and_pointer;
  const bool live = (within && !touch) || hovering;
  const auto waited = context.state(dwelled);
  const auto pressed = context.state(held);
  const bool shown = (live && waited.get()) || pressed.get();
  Element label = Element{composition::PlaceholderContentView(label_)}
                      .on_long_press_gesture([pressed] { pressed.set(true); });
  std::optional<Element> layer;
  if (shown) {
    std::vector<Element> children;
    children.push_back(
        components::ZStack().frame({.max_width = composition::infinity,
                                    .max_height = composition::infinity}));
    children.push_back(
        Help::tip(title_, text_).transition(motion::Transition::opacity()));
    layer.emplace(composition::Layer(
        composition::OverlayLayer::toast,
        composition::OverlayInput::passthrough,
        composition::Alignment::top_leading, std::move(children),
        composition::Layer::Anchoring{
            .attachment = composition::PopoverAttachmentAnchor::rect(),
            .arrow_edge = theme::Edge::top,
            .arrow = false}));
  }
  Element hosted =
      composition::LayerHost(std::move(label), std::move(layer))
          .task(
              composition::ElementKey{static_cast<std::int64_t>(live ? 1 : 0)},
              [waited, live] { return Help::dwell(waited, live); });
  if (pressed.get()) {
    hosted = std::move(hosted).task(
        composition::ElementKey{static_cast<std::int64_t>(2)},
        [pressed] { return Help::linger(pressed); });
  }
  std::vector<Element> produced;
  produced.push_back(std::move(hosted));
  return produced;
}

}

namespace arisa::composition {

auto Element::help(std::string text) && -> Element {
  return presentation::Help(std::move(*this), std::move(text));
}

auto Element::help(std::string title, std::string body) && -> Element {
  return presentation::Help(std::move(*this), std::move(title),
                            std::move(body));
}

}
