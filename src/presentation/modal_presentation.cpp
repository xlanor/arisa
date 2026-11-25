#include <arisa/presentation/modal_presentation.hpp>
#include <arisa/presentation/presentations.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace arisa::presentation {

using composition::Element;
using composition::PresentationDetent;
using theme::ColourRole;

const composition::StateKey<std::shared_ptr<composition::Size>>
    ModalPresentation::largest_key{"arisa.sheet.largest", nullptr};

ModalPresentation::ModalPresentation(
    Kind kind, PresentationLayer::Presence presence,
    PresentationLayer::Dismissed on_dismiss, Element content,
    std::optional<composition::Layer::Anchoring> anchoring)
    : kind_(kind),
      presence_(std::move(presence)),
      on_dismiss_(std::make_shared<PresentationLayer::Dismissed>(
          std::move(on_dismiss))),
      options_(
          content.interactions().presentation != nullptr
              ? content.interactions().presentation
              : std::make_shared<const composition::PresentationOptions>()),
      background_(content.interactions().presentation_background),
      locked_(content.interactions().dismiss_disabled),
      anchoring_(anchoring),
      source_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content))) {}

auto ModalPresentation::options() const
    -> const composition::PresentationOptions& {
  return *options_;
}

auto ModalPresentation::content() const -> Element {
  return composition::PlaceholderContentView(source_);
}

auto ModalPresentation::dismissed() const -> PresentationLayer::Dismissed {
  return [held = on_dismiss_] {
    if (*held) {
      (*held)();
    }
  };
}

auto ModalPresentation::body(composition::Context& context) const -> Element {
  const bool wide_compact =
      context.environment(components::Environment::horizontal_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const bool tall_compact =
      context.environment(components::Environment::vertical_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const bool compact = wide_compact || tall_compact;
  const composition::PresentationAdaptation adaptation =
      wide_compact ? options().compact_adaptation
                   : options().vertical_adaptation;
  if (compact &&
      adaptation == composition::PresentationAdaptation::full_screen_cover) {
    return PresentationLayer::cover_layer(presence_, content(), background_,
                                          locked_, dismissed());
  }
  if (kind_ == Kind::popover &&
      !(compact && adaptation == composition::PresentationAdaptation::sheet)) {
    return PresentationLayer::popover_layer(
        presence_, anchoring_.value_or(composition::Layer::Anchoring{}),
        content(), background_, locked_);
  }
  return sheet(context);
}

auto ModalPresentation::largest(float screen) const -> float {
  const float share =
      options().sizing.kind() == composition::PresentationSizing::Kind::form
          ? PresentationLayer::form_height
          : PresentationLayer::sheet_height;
  return screen * share;
}

auto ModalPresentation::detents(float screen,
                                composition::StateHandle<int> chosen) const
    -> Detents {
  static constexpr float nominal = 1000.0F;
  Detents found{
      .ordered = options().detents, .index = 0, .largest = largest(screen)};
  const float measure = found.largest > 0.0F ? found.largest : nominal;
  std::erase_if(found.ordered, [measure](const PresentationDetent& detent) {
    return !detent.available(measure);
  });
  if (found.ordered.empty()) {
    found.ordered.push_back(PresentationDetent::large);
  }
  std::ranges::stable_sort(found.ordered, {},
                           [measure](const PresentationDetent& detent) {
                             return detent.resolve(measure);
                           });
  if (options().selection.has_value()) {
    const PresentationDetent wanted = options().selection->get();
    const auto at = std::ranges::find(found.ordered, wanted);
    found.index = at != found.ordered.end()
                      ? static_cast<std::size_t>(at - found.ordered.begin())
                      : 0U;
  } else {
    found.index = static_cast<std::size_t>(std::clamp(
        chosen.get(), 0, static_cast<int>(found.ordered.size()) - 1));
  }
  return found;
}

auto ModalPresentation::nearest(const std::vector<PresentationDetent>& ordered,
                                float largest, float height) -> std::size_t {
  std::size_t best = 0;
  float distance = -1.0F;
  for (std::size_t index = 0; index < ordered.size(); ++index) {
    const float gap = std::abs(ordered[index].resolve(largest) - height);
    if (distance < 0.0F || gap < distance) {
      best = index;
      distance = gap;
    }
  }
  return best;
}

auto ModalPresentation::select(composition::StateHandle<int> chosen,
                               std::vector<PresentationDetent> ordered) const
    -> std::function<void(std::size_t)> {
  return [chosen, selection = options().selection,
          ordered = std::move(ordered)](std::size_t index) {
    if (index >= ordered.size()) {
      return;
    }
    motion::with_animation(motion::Animation::smooth(0.35), [&] {
      if (selection.has_value()) {
        selection->set(ordered[index]);
      } else {
        chosen.set(static_cast<int>(index));
      }
    });
  };
}

auto ModalPresentation::exit(composition::StateHandle<bool> confirming) const
    -> std::function<void()> {
  const bool confirm = options().dismissal_confirmation.has_value() &&
                       options().dismissal_confirmation->should_present;
  return [dismiss = presence_.dismiss, confirm, confirming] {
    if (confirm) {
      confirming.set(true);
      return;
    }
    dismiss();
  };
}

auto ModalPresentation::confirmed(
    Element panel, composition::StateHandle<bool> confirming) const -> Element {
  const composition::PresentationOptions::Confirmation& confirmation =
      *options().dismissal_confirmation;
  std::vector<Element> members;
  if (confirmation.actions) {
    members = components::Group::members(confirmation.actions());
  }
  for (Element& member : members) {
    const auto button = components::Button::content(member);
    if (button.has_value() &&
        button->get().role() != components::ButtonRole::cancel) {
      button->get().then(presence_.dismiss);
    }
  }
  std::optional<Element> message;
  if (confirmation.message) {
    message = confirmation.message();
  }
  return PresentationLayer::confirmation_dialog(
      std::move(panel),
      Presentations::presence(composition::Binding<bool>{confirming}),
      confirmation.title, composition::Visibility::visible,
      Element{components::Group(std::move(members))}, std::move(message));
}

auto ModalPresentation::grabber(bool visible) -> Element {
  std::optional<Element> mark;
  if (visible) {
    mark = components::ZStack()
               .frame({.width = grabber_width, .height = grabber_height})
               .background(ColourRole::tertiary_label, theme::Capsule());
  }
  return components::ZStack(std::move(mark))
      .frame({.height = grabber_strip, .max_width = composition::infinity})
      .content_shape(theme::Shape::rectangle());
}

auto ModalPresentation::sheet(composition::Context& context) const -> Element {
  const auto chosen = context.state(detent_key);
  const auto pull = context.state(pull_key);
  const auto extent = context.state(extent_key);
  const auto confirming = context.state(confirming_key);
  const auto scheme = context.state(scheme_key);
  const composition::PresentationOptions& declared = options();
  const float screen = extent.get();
  Detents found = detents(screen, chosen);
  const bool declares = found.ordered.size() > 1 ||
                        found.ordered.front() != PresentationDetent::large;
  const bool detented = declares && screen > 0.0F;
  const float target = detented
                           ? found.ordered[found.index].resolve(found.largest)
                           : found.largest;
  const float smallest =
      detented ? found.ordered.front().resolve(found.largest) : found.largest;
  const float raw = target - pull.get();
  const float shown =
      std::clamp(raw, smallest, std::max(found.largest, smallest));
  const float drop = std::max(smallest - raw, 0.0F);
  const bool interactive =
      declared.background_interaction.allows(target, found.largest);
  std::shared_ptr<composition::Size>& reached = context.retained(largest_key);
  if (reached == nullptr) {
    reached = std::make_shared<composition::Size>();
  }
  Element panel = PresentationLayer::sheet_panel(
      content(), background_, declared,
      detented ? std::optional<float>{shown} : std::nullopt, reached);
  using Placement = composition::PresentationPlacement::Kind;
  if (declared.placement.kind() == Placement::leading) {
    panel = std::move(panel).padding(theme::Edge::leading, theme::space::xl);
  } else if (declared.placement.kind() == Placement::trailing) {
    panel = std::move(panel).padding(theme::Edge::trailing, theme::space::xl);
  }
  const std::function<void()> leave = exit(confirming);
  const std::function<void(std::size_t)> pick = select(chosen, found.ordered);
  if (detented) {
    const bool visible =
        declared.drag_indicator == composition::Visibility::visible ||
        (declared.drag_indicator == composition::Visibility::automatic &&
         found.ordered.size() > 1);
    const auto resizing = [pull, pick, leave, ordered = found.ordered,
                           largest = found.largest, target, smallest,
                           locked = locked_] {
      return composition::DragGesture()
          .on_changed([pull](const composition::DragGesture::Value& value) {
            pull.set(value.translation.height);
          })
          .on_ended([pull, pick, leave, ordered, largest, target, smallest,
                     locked](const composition::DragGesture::Value& value) {
            const float projected =
                target - value.predicted_end_translation.height;
            motion::with_animation(motion::Animation::smooth(0.35),
                                   [&] { pull.set(0.0F); });
            if (!locked && projected < smallest * dismiss_fraction) {
              leave();
              return;
            }
            pick(nearest(ordered, largest, projected));
          });
    };
    if (declared.content_interaction ==
        composition::PresentationContentInteraction::resizes) {
      panel = std::move(panel)
                  .overlay(composition::Alignment::top, grabber(visible))
                  .gesture(resizing());
    } else {
      panel = std::move(panel).overlay(composition::Alignment::top,
                                       grabber(visible).gesture(resizing()));
      if (declared.content_interaction ==
          composition::PresentationContentInteraction::automatic) {
        panel = PresentationLayer::handoff(
            std::move(panel), resizing(),
            composition::ScrollHandoff{.expands = found.index + 1 <
                                                  found.ordered.size()});
      }
    }
  }
  if (drop > 0.0F) {
    panel = std::move(panel).offset(0.0F, drop);
  }
  if (presence_.identity.has_value()) {
    panel = std::move(panel).id(*presence_.identity);
  }
  if (declared.dismissal_confirmation.has_value()) {
    panel = confirmed(std::move(panel), confirming);
  }
  if (detented) {
    panel = std::move(panel).margin(
        theme::Insets{.bottom = (screen - found.largest) * 0.5F});
  }
  PresentationLayer::Measured measured;
  if (declares) {
    measured = [extent](const float& height) {
      if (height != extent.get()) {
        extent.set(height);
      }
    };
  }
  Element layer = PresentationLayer::modal(
      presence_,
      PresentationLayer::Modal{
          .layer = composition::OverlayLayer::sheet,
          .alignment = PresentationLayer::placed(
              detented ? composition::Alignment::bottom
                       : composition::Alignment::center,
              declared.placement),
          .exit_label = std::string{PresentationLayer::close_label},
          .dismiss_disabled = locked_,
          .scrim = !interactive,
          .scrim_dismisses = !interactive,
          .transition = motion::Transition::move(theme::Edge::bottom),
          .anchoring = std::nullopt,
          .input = interactive ? composition::OverlayInput::cooperative
                               : composition::OverlayInput::modal,
          .measured = std::move(measured),
          .scheme = scheme},
      std::move(panel), dismissed(), leave);
  if (detented && found.ordered.size() > 1) {
    layer = std::move(layer).on_command(
        resize_button, std::string{resize_label},
        [pick, index = found.index, count = found.ordered.size()] {
          pick((index + 1) % count);
        });
  }
  return layer;
}

}
