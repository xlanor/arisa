#include <arisa/components/button_style.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/shape.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::controls {

Stepper::Stepper(std::string title, Adjust adjust, bool lowers, bool raises)
    : Element(std::make_unique<Content>(std::move(title), std::move(adjust),
                                        lowers, raises)) {}

Stepper::Stepper(std::string title, std::function<void()> on_increment,
                 std::function<void()> on_decrement,
                 std::function<void(bool)> on_editing_changed)
    : Stepper(
          std::move(title),
          adjusted(on_increment, on_decrement, std::move(on_editing_changed)),
          static_cast<bool>(on_decrement), static_cast<bool>(on_increment)) {}

Stepper::Stepper(composition::Element label, std::function<void()> on_increment,
                 std::function<void()> on_decrement,
                 std::function<void(bool)> on_editing_changed)
    : Stepper(Parts{.title = {},
                    .label = source(std::move(label)),
                    .shown = {},
                    .adjust = adjusted(on_increment, on_decrement,
                                       std::move(on_editing_changed)),
                    .lowers = static_cast<bool>(on_decrement),
                    .raises = static_cast<bool>(on_increment)}) {}

Stepper::Stepper(Parts parts)
    : Element(std::make_unique<Content>(
          std::move(parts.title), std::move(parts.adjust), parts.lowers,
          parts.raises, std::move(parts.label), std::move(parts.shown))) {}

auto Stepper::source(composition::Element label) -> Source {
  return std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(label));
}

auto Stepper::adjusted(std::function<void()> on_increment,
                       std::function<void()> on_decrement,
                       std::function<void(bool)> on_editing_changed) -> Adjust {
  return [increment = std::move(on_increment),
          decrement = std::move(on_decrement),
          changed = std::move(on_editing_changed)](int direction) {
    const std::function<void()>& step = direction < 0 ? decrement : increment;
    if (!step) {
      return;
    }
    if (changed) {
      changed(true);
    }
    step();
    if (changed) {
      changed(false);
    }
  };
}

auto Stepper::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Stepper::Content::title_label() const -> composition::Element {
  composition::Element title =
      label_ != nullptr
          ? composition::Element{composition::PlaceholderContentView(label_)}
          : composition::Element{
                components::Text(text::LocalizedStringKey{title_})};
  if (!shown_) {
    return title;
  }
  return components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::first_text_baseline,
          .spacing = theme::space::sm},
      std::move(title),
      components::Text(shown_()).monospaced_digit().foreground_style(
          theme::ColourRole::text_secondary));
}

auto Stepper::Content::segment(bool minus, bool available,
                               composition::StateHandle<std::int64_t> taken,
                               components::ControlSize size) const
    -> composition::Element {
  const theme::ColourValue ink = theme::ColourRole::control_text;
  const float extent =
      size == components::ControlSize::regular
          ? segment_height
          : components::ControlMetrics::of(size).stepper_height;
  const float mark = glyph_extent * extent / segment_height;
  composition::Element glyph = minus ? components::Glyph::minus(ink, mark)
                                     : components::Glyph::plus(ink, mark);
  composition::Element cell =
      components::ZStack(std::move(glyph))
          .frame({.width = size == components::ControlSize::regular
                               ? segment_width
                               : extent,
                  .height = extent})
          .content_shape(theme::Shape::rectangle())
          .content_shape(composition::ContentShapeKinds::focus_effect,
                         theme::Shape::rounded(theme::radius::control, true))
          .opacity(available ? 1.0F : 0.35F)
          .id(minus ? lower_key : raise_key);
  if (!available) {
    return std::move(cell).disabled();
  }
  const int step = minus ? -1 : 1;
  const auto press = [adjust = adjust_, step, taken] {
    adjust(step);
    taken.set(taken.get() + step);
  };
  return std::move(cell)
      .focusable()
      .on_command(
          provider::Button::a, minus ? "Decrease" : "Increase", press,
          composition::CommandOptions{
              .trigger = composition::CommandTrigger::repeat, .hidden = false})
      .on_tap_gesture(press);
}

auto Stepper::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const bool enabled = context.environment(composition::Enablement::is_enabled);
  const auto taken = context.state(steps);
  const components::ControlSize size =
      context.environment(components::Environment::control_size);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  composition::Element control =
      components::HStack(components::HStackOptions{.spacing = 0.0F},
                         segment(true, lowers_, taken, size),
                         components::ZStack()
                             .frame({.width = 1.0F, .height = glyph_extent})
                             .background(theme::ColourRole::tertiary_label),
                         segment(false, raises_, taken, size))
          .background(
              components::ButtonStyle::fill(components::ButtonStyle::fill_role))
          .default_sensory_feedback(
              taken.get(),
              [](std::int64_t before, std::int64_t now)
                  -> std::optional<composition::SensoryFeedback> {
                return now > before ? composition::SensoryFeedback::increase
                                    : composition::SensoryFeedback::decrease;
              });
  const std::optional<components::LabelColumn> column =
      hidden ? std::nullopt : components::LabeledContent::column(context);
  if (column.has_value()) {
    std::vector<composition::Element> produced;
    composition::Element shared = components::LabeledContent::columned(
        *column, title_label(), std::move(control));
    produced.push_back(enabled ? std::move(shared)
                               : std::move(shared).opacity(0.4F));
    return produced;
  }
  if (hidden) {
    std::vector<composition::Element> produced;
    produced.push_back(enabled ? std::move(control)
                               : std::move(control).opacity(0.4F));
    return produced;
  }
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = std::nullopt,
              .distribution = components::Distribution::start,
              .wrap = false},
          title_label(), components::Spacer(), std::move(control))
          .frame({.max_width = composition::infinity});
  if (!enabled) {
    row = std::move(row).opacity(0.4F);
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(row));
  return produced;
}

}
