#include <arisa/components/button_style.hpp>
#include <arisa/presentation/modal_presentation.hpp>
#include <arisa/presentation/presentation_layer.hpp>
#include <arisa/theme/metrics.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/any_layout.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/dialog_environment.hpp>
#include <arisa/composition/dismiss_action.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/layer_host.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/preferred_color_scheme.hpp>
#include <arisa/composition/presentation_detent.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::presentation {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto PresentationLayer::handoff(composition::Element panel,
                                composition::Gesture gesture,
                                composition::ScrollHandoff handed)
    -> composition::Element {
  panel.interactions_.edit().gestures.push_back(composition::GestureAttachment{
      .gesture = std::make_shared<composition::Gesture>(std::move(gesture)),
      .priority = composition::GesturePriority::normal,
      .mask = composition::GestureMask::all,
      .handoff = handed,
      .name = {}});
  return panel;
}

auto PresentationLayer::scrim(std::function<void()> exit, bool visible,
                              bool dismisses, Measured measured) -> Element {
  Element shade = components::ZStack().frame(
      {.max_width = infinity, .max_height = infinity});
  if (visible) {
    shade = std::move(shade).background(ColourRole::scrim);
  }
  if (dismisses) {
    shade =
        std::move(shade).on_tap_gesture([exit = std::move(exit)] { exit(); });
  } else if (!visible) {
    shade = std::move(shade).allows_hit_testing(false);
  }
  if (measured) {
    shade = std::move(shade).on_geometry_change(
        std::type_identity<float>{},
        [](const composition::GeometryProxy& proxy) {
          return proxy.size().height;
        },
        std::move(measured));
  }
  return std::move(shade).transition(motion::Transition::opacity());
}

auto PresentationLayer::modal(Presence presence, Modal settings, Element panel,
                              Dismissed on_dismiss, std::function<void()> exit)
    -> Element {
  std::vector<Element> children;
  children.push_back(
      scrim(exit, settings.scrim,
            settings.scrim_dismisses && !settings.dismiss_disabled,
            std::move(settings.measured)));
  if (settings.scheme.has_value()) {
    const composition::StateHandle<std::optional<theme::ColorScheme>> scheme =
        *settings.scheme;
    panel = std::move(panel).on_preference_change(
        composition::PreferredColorScheme::key,
        [scheme](const std::optional<theme::ColorScheme>& preferred) {
          if (scheme.get() != preferred) {
            scheme.set(preferred);
          }
        });
    if (scheme.get().has_value()) {
      panel = std::move(panel).environment(
          components::Environment::colour_scheme, scheme.get());
    }
  }
  children.push_back(std::move(panel).transition(settings.transition));
  Element layer = composition::Layer(
      settings.layer, settings.input, settings.alignment, std::move(children),
      settings.anchoring, std::move(settings.placement));
  if (settings.dismiss_disabled) {
    layer = std::move(layer).on_exit_command(
        std::move(settings.exit_label), composition::Command::Action{},
        composition::CommandOptions{.hidden = true});
  } else {
    layer = std::move(layer).on_exit_command(std::move(settings.exit_label),
                                             std::move(exit));
  }
  return std::move(layer)
      .environment(
          components::Environment::dismiss,
          composition::DismissAction{presence.source, 0, presence.dismiss})
      .environment(components::Environment::is_presented, true)
      .on_disappear([dismiss = presence.dismiss,
                     dismissed = std::move(on_dismiss)]() mutable {
        dismiss();
        if (dismissed) {
          dismissed();
        }
      });
}

auto PresentationLayer::styled(Element button) -> Element {
  return std::move(button).frame(
      {.max_width = infinity,
       .min_height = theme::metric::alert_button_height});
}

auto PresentationLayer::chosen(const std::optional<Element>& first,
                               const std::optional<Element>& second)
    -> Background {
  for (const std::optional<Element>* each : {&first, &second}) {
    if (each->has_value() &&
        (*each)->interactions().presentation_background.has_value()) {
      return (*each)->interactions().presentation_background;
    }
  }
  return std::nullopt;
}

auto PresentationLayer::actions(std::optional<Element> declared,
                                const Presence& presence, bool fallback)
    -> Actions {
  Actions result{.buttons = {},
                 .cancel = std::nullopt,
                 .cancel_action = nullptr,
                 .cancel_label = std::string{cancel_label}};
  if (declared.has_value()) {
    for (Element& member : components::Group::members(std::move(*declared))) {
      const auto content = components::Button::content(member);
      if (!content.has_value()) {
        result.buttons.push_back(std::move(member));
        continue;
      }
      components::Button::Content& button = content->get();
      button.then(presence.dismiss);
      if (button.role() == components::ButtonRole::cancel &&
          !result.cancel.has_value()) {
        if (!button.label().empty()) {
          result.cancel_label = std::string{button.label()};
        }
        result.cancel_action = button.shared_action();
        result.cancel = styled(std::move(member));
        continue;
      }
      result.buttons.push_back(styled(std::move(member)));
    }
  }
  if (fallback && result.buttons.empty() && !result.cancel.has_value()) {
    result.cancel_label = std::string{ok_label};
    result.buttons.push_back(
        styled(components::Button(std::string{ok_label}, presence.dismiss)));
  }
  const auto chosen = std::ranges::find_if(result.buttons, [](Element& each) {
    const auto content = components::Button::content(each);
    return content.has_value() &&
           content->get().role() != components::ButtonRole::destructive;
  });
  if (chosen != result.buttons.end()) {
    *chosen = std::move(*chosen).button_style(
        components::ButtonStyle::bordered_prominent());
  } else if (result.cancel.has_value()) {
    result.cancel =
        std::move(*result.cancel)
            .button_style(components::ButtonStyle::bordered_prominent());
  }
  return result;
}

auto PresentationLayer::exit_action(const Actions& declared,
                                    const Presence& presence)
    -> std::function<void()> {
  if (declared.cancel_action == nullptr) {
    return presence.dismiss;
  }
  return [action = declared.cancel_action] {
    if (*action) {
      (*action)();
    }
  };
}

auto PresentationLayer::message_text(std::optional<Element> message)
    -> std::optional<Element> {
  if (!message.has_value()) {
    return std::nullopt;
  }
  return std::move(*message)
      .foreground_style(ColourRole::text_secondary)
      .frame({.max_width = infinity});
}

auto PresentationLayer::message(std::string text) -> Element {
  return components::Text(std::move(text)).font(theme::Font::footnote);
}

auto PresentationLayer::adapts(const Element& content) -> bool {
  const auto& options = content.interactions().presentation;
  const auto changes = [](composition::PresentationAdaptation adaptation) {
    return adaptation == composition::PresentationAdaptation::sheet ||
           adaptation == composition::PresentationAdaptation::full_screen_cover;
  };
  return options != nullptr && (changes(options->compact_adaptation) ||
                                changes(options->vertical_adaptation));
}

auto PresentationLayer::placed(
    composition::Alignment alignment,
    composition::PresentationPlacement placement) noexcept
    -> composition::Alignment {
  using Kind = composition::PresentationPlacement::Kind;
  const bool bottom = alignment == composition::Alignment::bottom;
  switch (placement.kind()) {
    case Kind::leading:
      return bottom ? composition::Alignment::bottom_leading
                    : composition::Alignment::leading;
    case Kind::trailing:
      return bottom ? composition::Alignment::bottom_trailing
                    : composition::Alignment::trailing;
    case Kind::center:
      return bottom ? composition::Alignment::bottom
                    : composition::Alignment::center;
    case Kind::automatic:
      break;
  }
  return alignment;
}

auto PresentationLayer::Sized::size_that_fits(
    composition::ProposedViewSize proposal,
    const components::LayoutSubviews& subviews) const -> composition::Size {
  if (subviews.empty()) {
    return composition::Size{};
  }
  const composition::Size container = proposal.replacing_unspecified_dimensions(
      composition::Size{.width = 0.0F, .height = 0.0F});
  const components::LayoutSubview& panel = subviews[0];
  const composition::PresentationSizingRoot root{
      [&panel](composition::ProposedViewSize asked) {
        return panel.size_that_fits(asked);
      }};
  const composition::ProposedViewSize wanted = sizing_.proposed_size(
      root, composition::PresentationSizingContext{container});
  composition::Size size = panel.size_that_fits(wanted);
  if (wanted.width.has_value()) {
    size.width = *wanted.width;
  }
  if (wanted.height.has_value()) {
    size.height = *wanted.height;
  }
  if (container.width > 0.0F) {
    size.width = std::min(size.width, container.width);
  }
  if (container.height > 0.0F) {
    size.height = std::min(size.height, container.height);
  }
  if (largest_ != nullptr) {
    if (sizing_.sticky_width()) {
      size.width = std::max(size.width, largest_->width);
      largest_->width = size.width;
    }
    if (sizing_.sticky_height()) {
      size.height = std::max(size.height, largest_->height);
      largest_->height = size.height;
    }
  }
  return size;
}

void PresentationLayer::Sized::place_subviews(
    composition::Rect bounds, composition::ProposedViewSize,
    const components::LayoutSubviews& subviews) const {
  if (subviews.empty()) {
    return;
  }
  subviews[0].place(composition::Point{.x = bounds.x, .y = bounds.y},
                    motion::UnitPoint::top_leading,
                    composition::ProposedViewSize{composition::Size{
                        .width = bounds.width, .height = bounds.height}});
}

auto PresentationLayer::sheet_panel(
    Element content, const Background& background,
    const composition::PresentationOptions& options,
    std::optional<float> height, std::shared_ptr<composition::Size> largest)
    -> Element {
  const theme::Shape outline = theme::RoundedRectangle(
      options.corner_radius.has_value() ? theme::Radius{*options.corner_radius}
                                        : theme::radius::sheet,
      theme::RoundedCornerStyle::continuous);
  composition::FrameSpec frame{.alignment = composition::Alignment::leading};
  using Kind = composition::PresentationSizing::Kind;
  switch (options.sizing.kind()) {
    case Kind::form:
      frame.width = composition::Length::percent(form_width);
      frame.height = composition::Length::percent(form_height);
      break;
    case Kind::fitted:
    case Kind::custom:
      break;
    case Kind::automatic:
    case Kind::page:
      frame.width = composition::Length::percent(sheet_width);
      frame.height = composition::Length::percent(sheet_height);
      break;
  }
  if (height.has_value()) {
    frame.height = composition::Length{*height};
  }
  Element panel =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          std::move(content))
          .padding(theme::space::xl);
  if (options.sizing.plain()) {
    panel = std::move(panel).frame(frame);
  } else {
    panel = components::AnyLayout(Sized{options.sizing, std::move(largest)})(
        std::move(panel));
    if (height.has_value()) {
      panel = std::move(panel).frame({.height = composition::Length{*height}});
    }
  }
  panel = background.has_value()
              ? background->clad(std::move(panel), outline)
              : std::move(panel)
                    .background(ColourRole::background, outline)
                    .default_glass(theme::GlassSlot::sheet, outline);
  return std::move(panel).shadow(theme::Elevation::high);
}

auto PresentationLayer::sheet(Element presenter, Presence presence,
                              Dismissed on_dismiss,
                              std::optional<Element> content) -> Element {
  std::optional<Element> layer;
  if (presence.presented && content.has_value()) {
    if (stateful(*content)) {
      layer.emplace(ModalPresentation{
          ModalPresentation::Kind::sheet, std::move(presence),
          std::move(on_dismiss), std::move(*content), std::nullopt});
    } else {
      const bool locked = content->interactions().dismiss_disabled;
      const Background background = chosen(content);
      const std::shared_ptr<const composition::PresentationOptions> options =
          content->interactions().presentation != nullptr
              ? content->interactions().presentation
              : std::make_shared<const composition::PresentationOptions>();
      Element panel =
          sheet_panel(std::move(*content), background, *options, std::nullopt);
      if (presence.identity.has_value()) {
        panel = std::move(panel).id(*presence.identity);
      }
      std::function<void()> exit = presence.dismiss;
      layer.emplace(modal(
          std::move(presence),
          Modal{.layer = composition::OverlayLayer::sheet,
                .alignment = composition::Alignment::center,
                .exit_label = std::string{close_label},
                .dismiss_disabled = locked,
                .scrim = true,
                .scrim_dismisses = true,
                .transition = motion::Transition::move(theme::Edge::bottom),
                .anchoring = std::nullopt},
          std::move(panel), std::move(on_dismiss), std::move(exit)));
    }
  }
  return composition::LayerHost(std::move(presenter), std::move(layer));
}

auto PresentationLayer::declares_within(const Element& content,
                                        composition::Identity key) -> bool {
  Declaration search{.key = key};
  DeclarationWalk walk;
  walk.run(content, false, search);
  return search.found;
}

auto PresentationLayer::stateful(const Element& content) -> bool {
  if (declares_within(content,
                      composition::PreferredColorScheme::key.identity())) {
    return true;
  }
  const auto& options = content.interactions().presentation;
  if (options == nullptr) {
    return false;
  }
  const bool detented =
      !options->detents.empty() &&
      !(options->detents.size() == 1 &&
        options->detents.front() == composition::PresentationDetent::large);
  return detented || options->dismissal_confirmation.has_value() ||
         options->background_interaction.is_enabled() ||
         options->compact_adaptation ==
             composition::PresentationAdaptation::full_screen_cover ||
         options->vertical_adaptation !=
             composition::PresentationAdaptation::automatic ||
         options->placement != composition::PresentationPlacement::automatic ||
         !options->sizing.plain();
}

auto PresentationLayer::cover_layer(Presence presence, Element content,
                                    const Background& background, bool locked,
                                    Dismissed on_dismiss) -> Element {
  Element panel =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          std::move(content))
          .padding(theme::space::xxl)
          .frame({.max_width = infinity,
                  .max_height = infinity,
                  .alignment = composition::Alignment::leading});
  panel = background.has_value()
              ? background->clad(std::move(panel), theme::Rectangle())
              : std::move(panel).background(ColourRole::background);
  if (presence.identity.has_value()) {
    panel = std::move(panel).id(*presence.identity);
  }
  std::function<void()> exit = presence.dismiss;
  return modal(
      std::move(presence),
      Modal{.layer = composition::OverlayLayer::sheet,
            .alignment = composition::Alignment::center,
            .exit_label = std::string{close_label},
            .dismiss_disabled = locked,
            .scrim = false,
            .scrim_dismisses = false,
            .transition = motion::Transition::move(theme::Edge::bottom),
            .anchoring = std::nullopt},
      std::move(panel), std::move(on_dismiss), std::move(exit));
}

auto PresentationLayer::full_screen_cover(Element presenter, Presence presence,
                                          Dismissed on_dismiss,
                                          std::optional<Element> content)
    -> Element {
  std::optional<Element> layer;
  if (presence.presented && content.has_value()) {
    const bool locked = content->interactions().dismiss_disabled;
    const Background background = chosen(content);
    layer.emplace(cover_layer(std::move(presence), std::move(*content),
                              background, locked, std::move(on_dismiss)));
  }
  return composition::LayerHost(std::move(presenter), std::move(layer));
}

auto PresentationLayer::extras(const std::optional<Element>& first,
                               const std::optional<Element>& second) -> Extras {
  Extras found;
  for (const std::optional<Element>* each : {&first, &second}) {
    if (!each->has_value() || (*each)->interactions().presentation == nullptr) {
      continue;
    }
    const composition::PresentationOptions& options =
        *(*each)->interactions().presentation;
    if (found.icon == nullptr && options.dialog_icon != nullptr) {
      found.icon = options.dialog_icon;
    }
    if (found.severity == composition::DialogSeverity::automatic) {
      found.severity = options.dialog_severity;
    }
    if (!found.suppression.has_value() && options.suppression.has_value()) {
      found.suppression = options.suppression;
    }
  }
  return found;
}

auto PresentationLayer::heading(std::optional<Element> icon,
                                composition::DialogSeverity severity)
    -> std::optional<Element> {
  const bool critical = severity == composition::DialogSeverity::critical;
  if (!icon.has_value() && !critical) {
    return std::nullopt;
  }
  if (!critical) {
    return std::move(*icon).frame(
        {.width = dialog_icon_size, .height = dialog_icon_size});
  }
  Element caution =
      components::Image::system_name(std::string{critical_symbol})
          .font(theme::Font::system(dialog_icon_size))
          .foreground_style(ColourRole::yellow)
          .frame({.width = dialog_icon_size, .height = dialog_icon_size});
  if (!icon.has_value()) {
    return caution;
  }
  return std::move(caution).overlay(
      composition::Alignment::bottom_trailing,
      std::move(*icon).frame(
          {.width = dialog_badge_size, .height = dialog_badge_size}));
}

auto PresentationLayer::held(Element element)
    -> std::shared_ptr<composition::PlaceholderContentView::Source> {
  return std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(element));
}

const composition::StateKey<PresentationLayer::HeldIcon>
    PresentationLayer::DialogPanel::icon_key{"arisa.dialog.icon", HeldIcon{}};

auto PresentationLayer::DialogPanel::operator==(const DialogPanel& other) const
    -> bool {
  return leading_ == other.leading_ && trailing_ == other.trailing_ &&
         eager_.icon == other.eager_.icon &&
         eager_.severity == other.eager_.severity &&
         eager_.suppression == other.eager_.suppression &&
         spacing_ == other.spacing_;
}

auto PresentationLayer::DialogPanel::body(composition::Context& context) const
    -> Element {
  Extras chosen = eager_;
  if (chosen.icon == nullptr) {
    chosen.icon = context.environment(composition::DialogEnvironment::icon);
  }
  if (chosen.severity == composition::DialogSeverity::automatic) {
    chosen.severity =
        context.environment(composition::DialogEnvironment::severity);
  }
  if (!chosen.suppression.has_value()) {
    chosen.suppression =
        context.environment(composition::DialogEnvironment::suppression);
  }
  std::optional<Element> icon;
  if (chosen.icon != nullptr) {
    HeldIcon& kept = context.retained(icon_key);
    if (kept.from != chosen.icon) {
      kept = HeldIcon{.from = chosen.icon,
                      .source = held(std::move(*chosen.icon))};
    }
    icon = composition::PlaceholderContentView(kept.source);
  }
  std::vector<Element> rows;
  rows.reserve(leading_.size() + trailing_.size() + 2);
  if (auto shown = heading(std::move(icon), chosen.severity)) {
    rows.push_back(std::move(*shown));
  }
  for (const Source& each : leading_) {
    rows.push_back(composition::PlaceholderContentView(each));
  }
  if (chosen.suppression.has_value()) {
    rows.push_back(suppression_toggle(*chosen.suppression));
  }
  for (const Source& each : trailing_) {
    rows.push_back(composition::PlaceholderContentView(each));
  }
  return components::VStack(components::VStackOptions{.spacing = spacing_},
                            std::move(rows));
}

auto PresentationLayer::suppression_toggle(
    const composition::PresentationOptions::Suppression& suppression)
    -> Element {
  const composition::Binding<bool> bound = suppression.is_suppressed;
  const bool checked = bound.get();
  return components::Button(
             [bound] { bound.set(!bound.get()); },
             components::HStack(
                 components::HStackOptions{.spacing = theme::space::sm},
                 components::Image::system_name(
                     std::string{checked ? suppressed_symbol
                                         : unsuppressed_symbol})
                     .foreground_style(checked ? ColourRole::accent
                                               : ColourRole::text_secondary),
                 components::Text(suppression.title)
                     .font(theme::Font::footnote)))
      .button_style(components::ButtonStyle::plain())
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto PresentationLayer::alert(Element presenter, Presence presence,
                              std::string title,
                              std::optional<Element> declared,
                              std::optional<Element> message) -> Element {
  std::optional<Element> layer;
  if (presence.presented) {
    const Background background = chosen(declared, message);
    Extras found = extras(declared, message);
    Actions listed = actions(std::move(declared), presence, true);
    std::vector<DialogPanel::Source> leading;
    leading.push_back(held(components::Text(std::move(title))
                               .font(theme::Font::headline)
                               .frame({.max_width = infinity})));
    if (auto shown = message_text(std::move(message))) {
      leading.push_back(held(std::move(*shown)));
    }
    std::vector<Element> rows;
    const std::size_t count =
        listed.buttons.size() + (listed.cancel.has_value() ? 1U : 0U);
    std::vector<Element> buttons;
    if (count <= 2) {
      if (listed.cancel.has_value()) {
        buttons.push_back(std::move(*listed.cancel));
      }
      for (Element& button : listed.buttons) {
        buttons.push_back(std::move(button));
      }
      rows.push_back(components::HStack(
                         components::HStackOptions{
                             .spacing = theme::metric::alert_button_gap},
                         std::move(buttons))
                         .frame({.max_width = infinity}));
    } else {
      for (Element& button : listed.buttons) {
        buttons.push_back(std::move(button));
      }
      if (listed.cancel.has_value()) {
        buttons.push_back(std::move(*listed.cancel));
      }
      rows.push_back(components::VStack(
                         components::VStackOptions{
                             .spacing = theme::metric::alert_button_gap},
                         std::move(buttons))
                         .frame({.max_width = infinity}));
    }
    const theme::Shape card = theme::RoundedRectangle(
        theme::radius::card, theme::RoundedCornerStyle::continuous);
    std::vector<DialogPanel::Source> trailing;
    for (Element& row : rows) {
      trailing.push_back(held(std::move(row)));
    }
    Element panel = Element{DialogPanel{std::move(found), std::move(leading),
                                        std::move(trailing), theme::space::md}}
                        .padding(theme::metric::alert_inset)
                        .frame({.width = theme::metric::alert_width});
    panel = background.has_value()
                ? background->clad(std::move(panel), card)
                : std::move(panel).background(ColourRole::surface_raised, card);
    panel = std::move(panel).shadow(theme::Elevation::high);
    std::function<void()> exit = exit_action(listed, presence);
    std::string label = listed.cancel_label;
    layer.emplace(
        modal(std::move(presence),
              Modal{.layer = composition::OverlayLayer::dialog,
                    .alignment = composition::Alignment::center,
                    .exit_label = std::move(label),
                    .dismiss_disabled = false,
                    .scrim = true,
                    .scrim_dismisses = false,
                    .transition = motion::Transition::scale(1.12F).combined(
                        motion::Transition::opacity()),
                    .anchoring = std::nullopt},
              std::move(panel), Dismissed{}, std::move(exit)));
  }
  return composition::LayerHost(std::move(presenter), std::move(layer));
}

auto PresentationLayer::confirmation_dialog(
    Element presenter, Presence presence, std::string title,
    composition::Visibility title_visibility, std::optional<Element> declared,
    std::optional<Element> message) -> Element {
  std::optional<Element> layer;
  if (presence.presented) {
    const Background background = chosen(declared, message);
    Extras found = extras(declared, message);
    Actions listed = actions(std::move(declared), presence, false);
    if (!listed.cancel.has_value()) {
      auto fallback =
          components::Button(std::string{cancel_label},
                             components::ButtonRole::cancel, presence.dismiss);
      listed.cancel = styled(std::move(fallback));
    }
    std::vector<DialogPanel::Source> leading;
    if (title_visibility == composition::Visibility::visible &&
        !title.empty()) {
      leading.push_back(
          held(components::Text(std::move(title))
                   .font(theme::Font::footnote)
                   .foreground_style(ColourRole::text_secondary)));
    }
    if (auto shown = message_text(std::move(message))) {
      leading.push_back(held(std::move(*shown)));
    }
    std::vector<DialogPanel::Source> trailing;
    for (Element& button : listed.buttons) {
      trailing.push_back(held(std::move(button)));
    }
    const theme::Shape card = theme::RoundedRectangle(
        theme::radius::card, theme::RoundedCornerStyle::continuous);
    Element listed_panel =
        Element{DialogPanel{std::move(found), std::move(leading),
                            std::move(trailing), theme::space::sm}}
            .padding(theme::space::lg)
            .frame({.max_width = infinity});
    listed_panel = background.has_value()
                       ? background->clad(std::move(listed_panel), card)
                       : std::move(listed_panel)
                             .background(ColourRole::surface_raised, card);
    Element panel =
        components::VStack(
            components::VStackOptions{.spacing = theme::space::md},
            std::move(listed_panel).shadow(theme::Elevation::high),
            std::move(*listed.cancel).shadow(theme::Elevation::high))
            .frame({.width = theme::metric::alert_width})
            .padding(theme::Insets{.bottom = theme::space::xl});
    std::function<void()> exit = exit_action(listed, presence);
    std::string label = listed.cancel_label;
    layer.emplace(
        modal(std::move(presence),
              Modal{.layer = composition::OverlayLayer::dialog,
                    .alignment = composition::Alignment::bottom,
                    .exit_label = std::move(label),
                    .dismiss_disabled = false,
                    .scrim = true,
                    .scrim_dismisses = true,
                    .transition = motion::Transition::move(theme::Edge::bottom),
                    .anchoring = std::nullopt},
              std::move(panel), Dismissed{}, std::move(exit)));
  }
  return composition::LayerHost(std::move(presenter), std::move(layer));
}

auto PresentationLayer::popover_layer(Presence presence,
                                      composition::Layer::Anchoring anchoring,
                                      Element content,
                                      const Background& background, bool locked)
    -> Element {
  const theme::Shape card = theme::RoundedRectangle(
      theme::radius::card, theme::RoundedCornerStyle::continuous);
  std::optional<composition::PopoverPlacement> placement =
      content.interactions_.engaged()
          ? std::exchange(content.interactions_.edit().popover_placement,
                          std::nullopt)
          : std::nullopt;
  Element panel =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          std::move(content))
          .padding(theme::space::lg);
  panel = background.has_value()
              ? background->clad(std::move(panel), card)
              : std::move(panel).background(ColourRole::surface_raised, card);
  panel = std::move(panel).shadow(theme::Elevation::high);
  if (presence.identity.has_value()) {
    panel = std::move(panel).id(*presence.identity);
  }
  std::function<void()> exit = presence.dismiss;
  return modal(std::move(presence),
               Modal{.layer = composition::OverlayLayer::dialog,
                     .alignment = composition::Alignment::top_leading,
                     .exit_label = std::string{close_label},
                     .dismiss_disabled = locked,
                     .scrim = true,
                     .scrim_dismisses = true,
                     .transition = motion::Transition::scale(0.92F).combined(
                         motion::Transition::opacity()),
                     .anchoring = anchoring,
                     .placement = std::move(placement)},
               std::move(panel), Dismissed{}, std::move(exit));
}

auto PresentationLayer::placed(Element content,
                               composition::PopoverPlacement placement)
    -> Element {
  content.interactions_.edit().popover_placement = std::move(placement);
  return content;
}

auto PresentationLayer::popover(
    Element presenter, Presence presence,
    composition::PopoverAttachmentAnchor attachment_anchor,
    theme::Edge arrow_edge, std::optional<Element> content) -> Element {
  std::optional<Element> layer;
  if (presence.presented && content.has_value()) {
    const composition::Layer::Anchoring anchoring{
        .attachment = attachment_anchor, .arrow_edge = arrow_edge};
    if (adapts(*content)) {
      layer.emplace(ModalPresentation{ModalPresentation::Kind::popover,
                                      std::move(presence), Dismissed{},
                                      std::move(*content), anchoring});
    } else {
      const bool locked = content->interactions().dismiss_disabled;
      const Background background = chosen(content);
      layer.emplace(popover_layer(std::move(presence), anchoring,
                                  std::move(*content), background, locked));
    }
  }
  return composition::LayerHost(std::move(presenter), std::move(layer));
}

auto PresentationLayer::expire(std::function<void()> dismiss,
                               std::chrono::nanoseconds duration)
    -> async::Task<void> {
  co_await async::sleep(duration);
  dismiss();
}

auto PresentationLayer::toast(Element presenter, Presence presence,
                              std::chrono::nanoseconds duration,
                              composition::ToastOptions options,
                              std::optional<Element> content) -> Element {
  std::optional<Element> layer;
  if (presence.presented && content.has_value()) {
    const Background background = chosen(content);
    Element pill = components::HStack(
                       components::HStackOptions{.spacing = theme::space::sm},
                       std::move(*content))
                       .padding(theme::Insets::symmetric(theme::space::lg,
                                                         theme::space::sm));
    if (background.has_value()) {
      pill = background->clad(std::move(pill), theme::Capsule());
    } else if (options.tint.has_value()) {
      pill = std::move(pill).background(*options.tint, theme::Capsule());
    } else {
      pill = std::move(pill)
                 .background(ColourRole::surface_raised, theme::Capsule())
                 .border(ColourRole::separator, theme::Stroke::hairline());
    }
    const composition::VerticalAlignment edge = options.alignment.vertical;
    const bool top = edge == composition::VerticalAlignment::top;
    const bool bottom =
        edge == composition::VerticalAlignment::bottom ||
        edge == composition::VerticalAlignment::last_text_baseline;
    const theme::Insets lift = top      ? theme::Insets{.top = toast_lift}
                               : bottom ? theme::Insets{.bottom = toast_lift}
                                        : theme::Insets{};
    std::vector<Element> children;
    children.push_back(
        std::move(pill)
            .shadow(theme::Elevation::medium)
            .margin(lift)
            .transition(motion::Transition::move(top ? theme::Edge::top
                                                     : theme::Edge::bottom)
                            .combined(motion::Transition::opacity())));
    std::function<void()> dismiss = presence.dismiss;
    layer.emplace(
        composition::Layer(composition::OverlayLayer::toast,
                           composition::OverlayInput::passthrough,
                           options.alignment, std::move(children))
            .task([dismiss, duration] { return expire(dismiss, duration); })
            .on_disappear([dismiss] { dismiss(); }));
  }
  return composition::LayerHost(std::move(presenter), std::move(layer));
}

}
