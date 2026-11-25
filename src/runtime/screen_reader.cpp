#include <arisa/runtime/screen_reader.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <interaction/input_router.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::runtime {

void ScreenReader::configure(
    composition::Tree& tree,
    composition::AccessibilityTechnologies technologies, bool large_content) {
  const bool was_running = running();
  voice_over_ =
      technologies.contains(composition::AccessibilityTechnologies::voice_over);
  switch_control_ = technologies.contains(
      composition::AccessibilityTechnologies::switch_control);
  large_content_ = large_content;
  if (was_running && !running()) {
    tree.present_overlay(caption_overlay, std::nullopt);
    owned_ = {};
    rotors_.clear();
    choices_.clear();
  }
  if (!large_content_ && large_shown_.has_value()) {
    tree.present_overlay(large_content_overlay, std::nullopt);
    large_shown_.reset();
  }
  if (!was_running && running()) {
    read_.reset();
    caption_dirty_ = true;
  }
}

auto ScreenReader::wrapped(std::size_t index, int direction, std::size_t count)
    -> std::size_t {
  if (count == 0) {
    return 0;
  }
  const auto signed_count = static_cast<long long>(count);
  long long next = static_cast<long long>(index) + direction;
  next %= signed_count;
  if (next < 0) {
    next += signed_count;
  }
  return static_cast<std::size_t>(next);
}

auto ScreenReader::has_escape() const -> bool {
  return current_.has_value() &&
         std::ranges::any_of(current_->actions(), [](const auto& action) {
           return action.kind() ==
                  composition::AccessibilityActionKind::Kind::escape;
         });
}

auto ScreenReader::owns_confirm() const -> bool {
  if (!current_.has_value()) {
    return false;
  }
  const bool picked =
      rotor_ < rotors_.size() && rotors_[rotor_].kind == RotorKind::actions &&
      item_ < choices_.size() && choices_[item_].kind != ChoiceKind::activate;
  return picked || current_->activation_point().has_value();
}

void ScreenReader::filter(provider::InputFrame& frame, provider::Button confirm,
                          provider::Button back) {
  if (!running()) {
    return;
  }
  provider::Buttons now;
  for (const provider::PadState& pad : frame.pads) {
    if (!pad.connected) {
      continue;
    }
    for (const provider::Button button : provider::all_buttons) {
      if (pad.buttons.contains(button)) {
        now = now.with(button);
      }
    }
  }
  const auto pressed = [this, &now](provider::Button button) {
    return now.contains(button) && !held_.contains(button);
  };
  Commands& commands = *pending_;
  if (pressed(next_item)) {
    ++commands.step;
  }
  if (pressed(previous_item)) {
    --commands.step;
  }
  if (pressed(next_rotor)) {
    commands.next_rotor = true;
  }
  if (pressed(confirm) && owns_confirm()) {
    owned_ = owned_.with(confirm);
    commands.perform = true;
  }
  if (pressed(back) && has_escape()) {
    owned_ = owned_.with(back);
    commands.escape = true;
  }
  for (const provider::Button button : {confirm, back}) {
    if (owned_.contains(button) && !now.contains(button)) {
      owned_ = owned_.without(button);
    }
  }
  held_ = now;
  for (provider::PadState& pad : frame.pads) {
    for (const provider::Button button :
         {previous_item, next_item, next_rotor, confirm, back}) {
      const bool reader_owned = button == previous_item ||
                                button == next_item || button == next_rotor ||
                                owned_.contains(button);
      if (reader_owned) {
        pad.buttons = pad.buttons.without(button);
      }
    }
  }
  if (inject_move_) {
    frame.pads[0].connected = true;
    frame.pads[0].buttons =
        frame.pads[0].buttons.with(components::CommandMapping::move);
    inject_move_ = false;
  }
}

void ScreenReader::announce(std::string text,
                            composition::AccessibilitySpeech speech) {
  provider::Utterance utterance{.text = std::move(text),
                                .pitch = speech.pitch(),
                                .spells_out_characters = speech.spells_out(),
                                .includes_punctuation = speech.punctuated(),
                                .queued = speech.queued()};
  if (speaks_ && voice_over_) {
    static_cast<void>(speech_.get().speak(utterance));
  }
  caption_ = utterance.text;
  caption_dirty_ = true;
  if (spoken_.size() >= history_limit) {
    spoken_.erase(spoken_.begin());
  }
  spoken_.push_back(std::move(utterance));
}

auto ScreenReader::rotor() const noexcept -> std::string_view {
  return rotor_ < rotors_.size() ? std::string_view{rotors_[rotor_].name}
                                 : std::string_view{};
}

auto ScreenReader::rotors() const -> std::vector<std::string> {
  std::vector<std::string> names;
  names.reserve(rotors_.size());
  for (const Rotor& each : rotors_) {
    names.push_back(each.name);
  }
  return names;
}

void ScreenReader::rebuild(const components::AccessibilityTree& accessible) {
  const std::string kept{rotor()};
  rotors_.clear();
  choices_.clear();
  item_ = 0;
  entry_ = 0;
  if (current_.has_value()) {
    choices_.push_back(Choice{.kind = ChoiceKind::activate,
                              .name = std::string{activate_item}});
    for (std::size_t index = 0; index < current_->actions().size(); ++index) {
      const auto& action = current_->actions()[index];
      using Kind = composition::AccessibilityActionKind::Kind;
      if (action.kind() == Kind::escape || action.kind() == Kind::standard) {
        continue;
      }
      choices_.push_back(Choice{.kind = ChoiceKind::action,
                                .index = index,
                                .name = std::string{action.name()}});
    }
    if (current_->scrollable()) {
      for (const auto& [edge, name] :
           {std::pair{theme::Edge::top, "Scroll up"},
            std::pair{theme::Edge::bottom, "Scroll down"},
            std::pair{theme::Edge::leading, "Scroll left"},
            std::pair{theme::Edge::trailing, "Scroll right"}}) {
        choices_.push_back(
            Choice{.kind = ChoiceKind::scroll, .edge = edge, .name = name});
      }
    }
    if (current_->zoomable()) {
      choices_.push_back(
          Choice{.kind = ChoiceKind::zoom, .inward = true, .name = "Zoom in"});
      choices_.push_back(Choice{
          .kind = ChoiceKind::zoom, .inward = false, .name = "Zoom out"});
    }
    for (std::size_t index = 0; index < current_->drag_points().size();
         ++index) {
      choices_.push_back(
          Choice{.kind = ChoiceKind::drag,
                 .index = index,
                 .name = std::format(
                     "Drag {}", current_->drag_points()[index].description)});
    }
    for (std::size_t index = 0; index < current_->drop_points().size();
         ++index) {
      choices_.push_back(
          Choice{.kind = ChoiceKind::drop,
                 .index = index,
                 .name = std::format(
                     "Drop {}", current_->drop_points()[index].description)});
    }
    if (choices_.size() > 1) {
      rotors_.push_back(Rotor{.kind = RotorKind::actions,
                              .name = std::string{actions_rotor}});
    }
    if (current_->adjustable()) {
      rotors_.push_back(
          Rotor{.kind = RotorKind::adjust, .name = std::string{adjust_rotor}});
    }
  }
  bool custom_headings = false;
  bool custom_links = false;
  {
    for (const composition::AccessibilityRotor& declared :
         accessible.rotors()) {
      if (const auto* system = declared.system()) {
        custom_headings =
            custom_headings ||
            system->kind() ==
                composition::AccessibilitySystemRotor::Kind::headings;
        custom_links = custom_links ||
                       system->kind() ==
                           composition::AccessibilitySystemRotor::Kind::links;
      }
      rotors_.push_back(Rotor{.kind = RotorKind::custom,
                              .name = accessible.label(declared),
                              .custom = declared});
    }
  }
  if (!custom_headings) {
    rotors_.push_back(Rotor{
        .kind = RotorKind::headings,
        .name = composition::AccessibilitySystemRotor::headings().name()});
  }
  if (!custom_links) {
    rotors_.push_back(
        Rotor{.kind = RotorKind::links,
              .name = composition::AccessibilitySystemRotor::links().name()});
  }
  rotor_ = 0;
  const bool element_rotor =
      !rotors_.empty() && (rotors_.front().kind == RotorKind::actions ||
                           rotors_.front().kind == RotorKind::adjust);
  for (std::size_t index = 0; !element_rotor && index < rotors_.size();
       ++index) {
    if (rotors_[index].name == kept) {
      rotor_ = index;
    }
  }
  selection_ = choices_.empty() ? std::string{} : choices_.front().name;
}

void ScreenReader::step_custom(composition::Tree& tree,
                               interaction::InputRouter& router,
                               const components::AccessibilityTree& accessible,
                               const composition::AccessibilityRotor& rotor,
                               int direction) {
  const std::vector<composition::AccessibilityRotorEntry> entries =
      accessible.entries(rotor);
  if (entries.empty()) {
    return;
  }
  entry_ = wrapped(entry_, direction, entries.size());
  const composition::AccessibilityRotorEntry& entry = entries[entry_];
  entry.prepare();
  selection_ = accessible.label(entry);
  const auto found = accessible.target(entry);
  if (!found.has_value() || !router.focus(tree, found->anchor())) {
    announce(selection_);
  }
}

void ScreenReader::step_system(composition::Tree& tree,
                               interaction::InputRouter& router,
                               const components::AccessibilityTree& accessible,
                               RotorKind kind, int direction) {
  const composition::AccessibilitySystemRotor system =
      kind == RotorKind::headings
          ? composition::AccessibilitySystemRotor::headings()
          : composition::AccessibilitySystemRotor::links();
  std::vector<components::AccessibilityElement> matching;
  for (components::AccessibilityElement& each : accessible.elements()) {
    if (system.matches(each.traits(), each.heading())) {
      matching.push_back(std::move(each));
    }
  }
  if (matching.empty()) {
    announce(std::format("No {}", system.name()));
    return;
  }
  entry_ = wrapped(entry_, direction, matching.size());
  const components::AccessibilityElement& found = matching[entry_];
  selection_ = std::string{found.label()};
  if (!router.focus(tree, found.anchor())) {
    announce(found.description(), found.speech());
  }
}

void ScreenReader::step(composition::Tree& tree,
                        interaction::InputRouter& router,
                        const components::AccessibilityTree& accessible,
                        int direction) {
  if (rotor_ >= rotors_.size()) {
    return;
  }
  const Rotor& chosen = rotors_[rotor_];
  switch (chosen.kind) {
    case RotorKind::actions:
      item_ = wrapped(item_, direction, choices_.size());
      selection_ = choices_.empty() ? std::string{} : choices_[item_].name;
      announce(selection_);
      break;
    case RotorKind::adjust:
      if (current_.has_value() &&
          current_->adjust(
              tree,
              direction > 0
                  ? composition::AccessibilityAdjustmentDirection::increment
                  : composition::AccessibilityAdjustmentDirection::decrement)) {
        reread_ = true;
      }
      break;
    case RotorKind::custom:
      if (chosen.custom.has_value()) {
        const composition::AccessibilityRotor held = *chosen.custom;
        step_custom(tree, router, accessible, held, direction);
      }
      break;
    case RotorKind::headings:
    case RotorKind::links:
      step_system(tree, router, accessible, chosen.kind, direction);
      break;
  }
}

void ScreenReader::perform(composition::Tree& tree) {
  if (!current_.has_value()) {
    return;
  }
  const bool in_actions = rotor_ < rotors_.size() &&
                          rotors_[rotor_].kind == RotorKind::actions &&
                          item_ < choices_.size();
  const Choice chosen = in_actions ? choices_[item_] : Choice{};
  switch (chosen.kind) {
    case ChoiceKind::activate:
      static_cast<void>(current_->activate(tree));
      break;
    case ChoiceKind::action:
      static_cast<void>(current_->actions()[chosen.index].perform(tree));
      break;
    case ChoiceKind::scroll:
      static_cast<void>(current_->scroll(chosen.edge));
      break;
    case ChoiceKind::zoom: {
      const composition::Rect& frame = current_->frame();
      static_cast<void>(
          current_->zoom(composition::AccessibilityZoomGestureAction{
              .direction = chosen.inward
                               ? composition::AccessibilityZoomGestureAction::
                                     Direction::zoom_in
                               : composition::AccessibilityZoomGestureAction::
                                     Direction::zoom_out,
              .location = motion::UnitPoint::center,
              .point =
                  composition::Point{.x = frame.x + (frame.width * 0.5F),
                                     .y = frame.y + (frame.height * 0.5F)}}));
      break;
    }
    case ChoiceKind::drag:
    case ChoiceKind::drop:
      inject_move_ = true;
      break;
  }
  reread_ = true;
}

void ScreenReader::escape(composition::Tree& tree) {
  if (!current_.has_value()) {
    return;
  }
  for (const auto& action : current_->actions()) {
    if (action.kind() == composition::AccessibilityActionKind::Kind::escape) {
      static_cast<void>(action.perform(tree));
      return;
    }
  }
}

auto ScreenReader::follow(composition::Tree& tree,
                          interaction::InputRouter& router) -> bool {
  if (!active()) {
    return false;
  }
  const std::optional<composition::NodeId> focused = router.focused();
  const Commands commands = std::exchange(*pending_, Commands{});
  const bool moved = focused != read_;
  const bool commanded = commands.step != 0 || commands.next_rotor ||
                         commands.perform || commands.escape;
  if (!moved && !commanded && !reread_ && !caption_dirty_) {
    return false;
  }
  ++narrations_;
  const components::AccessibilityTree accessible{tree};
  bool changed = false;
  if (moved || reread_) {
    const bool reread = reread_ && !moved;
    read_ = focused;
    reread_ = false;
    current_ =
        focused.has_value() ? accessible.element(*focused) : std::nullopt;
    if (moved) {
      rebuild(accessible);
    }
    if (running() && current_.has_value()) {
      const bool silent =
          router.modality() == composition::InputModality::touch &&
          current_->direct_touch().has_value() &&
          current_->direct_touch()->contains(
              composition::AccessibilityDirectTouchOptions::silent_on_touch);
      if (!silent) {
        announce(reread && current_->value().has_value()
                     ? *current_->value()
                     : current_->description(),
                 current_->speech());
      }
    }
    if (large_content_) {
      show_large_content(tree);
      changed = true;
    }
  }
  if (running()) {
    if (commands.next_rotor && !rotors_.empty()) {
      rotor_ = wrapped(rotor_, 1, rotors_.size());
      item_ = 0;
      entry_ = 0;
      selection_ =
          rotors_[rotor_].kind == RotorKind::actions && !choices_.empty()
              ? choices_.front().name
              : std::string{};
      announce(rotors_[rotor_].name);
    }
    if (commands.step != 0) {
      step(tree, router, accessible, commands.step);
    }
    if (commands.perform) {
      perform(tree);
    }
    if (commands.escape) {
      escape(tree);
    }
    if (caption_dirty_) {
      show_caption(tree);
      changed = true;
    }
  }
  return changed || commanded;
}

void ScreenReader::show_caption(composition::Tree& tree) {
  caption_dirty_ = false;
  const std::shared_ptr<Commands> pending = pending_;
  const auto control = [&pending](std::string_view label, auto apply) {
    return composition::Element{
        components::Button(std::string{label}, [pending, apply] {
          apply(*pending);
        }).focusable(false)};
  };
  composition::Element bar =
      components::HStack(
          components::HStackOptions{.spacing = theme::space::sm},
          components::Text(std::string{rotor()})
              .font(theme::Font::caption)
              .foreground_color(theme::ColourRole::secondary_label),
          composition::Element{components::Text(caption_)}.line_limit(2).frame(
              {.max_width = composition::infinity,
               .alignment = composition::Alignment::leading}),
          control(rotor_label, [](Commands& held) { held.next_rotor = true; }),
          control(previous_label, [](Commands& held) { --held.step; }),
          control(next_label, [](Commands& held) { ++held.step; }),
          control(activate_item, [](Commands& held) { held.perform = true; }))
          .padding(theme::space::md)
          .background(theme::Material::regular,
                      theme::Shape::rounded(theme::radius::card))
          .padding(theme::space::lg)
          .accessibility_hidden()
          .id(std::string{caption_key});
  std::vector<composition::Element> children;
  children.push_back(std::move(bar));
  tree.present_overlay(
      caption_overlay,
      composition::Element{composition::Layer(
          composition::OverlayLayer::hud,
          composition::OverlayInput::passthrough,
          composition::Alignment::bottom, std::move(children))});
}

void ScreenReader::show_large_content(composition::Tree& tree) {
  if (!current_.has_value() || !current_->shows_large_content_viewer()) {
    if (large_shown_.has_value()) {
      tree.present_overlay(large_content_overlay, std::nullopt);
      large_shown_.reset();
    }
    return;
  }
  std::optional<composition::Element> custom = current_->large_content();
  composition::Element shown =
      custom.has_value() ? std::move(*custom)
                         : composition::Element{
                               components::Text(std::string{current_->label()})
                                   .font(theme::Font::large_title)};
  std::vector<composition::Element> children;
  children.push_back(std::move(shown)
                         .padding(theme::space::xl)
                         .background(theme::Material::thick,
                                     theme::Shape::rounded(theme::radius::card))
                         .accessibility_hidden());
  tree.present_overlay(
      large_content_overlay,
      composition::Element{composition::Layer(
          composition::OverlayLayer::hud,
          composition::OverlayInput::passthrough,
          composition::Alignment::center, std::move(children))});
  large_shown_ = current_->node();
}

}
