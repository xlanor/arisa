#include <interaction/input_router.hpp>

#include <arisa/composition/hover_effect_environment.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/point.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::interaction {

auto InputRouter::strongest(provider::Stick current,
                            provider::Stick candidate) noexcept
    -> provider::Stick {
  return std::hypot(candidate.x, candidate.y) > std::hypot(current.x, current.y)
             ? candidate
             : current;
}

auto InputRouter::merge(const provider::InputFrame& input) -> Gamepad {
  Gamepad pad;
  for (const provider::PadState& state : input.pads) {
    if (!state.connected) {
      continue;
    }
    pad.physical = pad.physical | state.buttons;
    pad.left = strongest(pad.left, state.left);
    pad.right = strongest(pad.right, state.right);
  }
  static_cast<void>(input);
  provider::Buttons keyboard;
  for (const provider::Key key : keys_) {
    if (consumed_.contains(key)) {
      continue;
    }
    switch (key) {
      case provider::Key::up:
        keyboard = keyboard.with(provider::Button::dpad_up);
        break;
      case provider::Key::down:
        keyboard = keyboard.with(provider::Button::dpad_down);
        break;
      case provider::Key::left:
        keyboard = keyboard.with(provider::Button::dpad_left);
        break;
      case provider::Key::right:
        keyboard = keyboard.with(provider::Button::dpad_right);
        break;
      case provider::Key::enter:
      case provider::Key::space:
        keyboard = keyboard.with(options_.layout.confirm());
        break;
      case provider::Key::escape:
      case provider::Key::backspace:
        keyboard = keyboard.with(options_.layout.back());
        break;
      default:
        break;
    }
  }
  pad.buttons = pad.physical | keyboard;
  return pad;
}

void InputRouter::sense(const provider::InputFrame& input, const Gamepad& pad,
                        const provider::NavigationTuning& tuning) {
  if (std::ranges::any_of(input.touches, [](const provider::Touch& touch) {
        return touch.phase == provider::TouchPhase::began;
      })) {
    modality_ = composition::InputModality::touch;
    pointing_ = false;
  }
  if (input.pointer.has_value()) {
    const provider::PointerState& pointer = *input.pointer;
    const bool moved =
        pointer_.has_value() &&
        (pointer.position != pointer_->position ||
         pointer.left != pointer_->left || pointer.right != pointer_->right);
    const bool wheeled = pointer.wheel_x != 0.0F || pointer.wheel_y != 0.0F;
    if (moved || wheeled) {
      modality_ = composition::InputModality::keyboard_and_pointer;
      pointing_ = true;
    }
    pointer_ = pointer;
  }
  if (std::ranges::any_of(input.keys, &provider::KeyEvent::down)) {
    modality_ = composition::InputModality::keyboard_and_pointer;
    pointing_ = false;
  }
  const bool fresh =
      std::ranges::any_of(provider::all_buttons, [&](provider::Button button) {
        return pad.physical.contains(button) &&
               !previous_buttons_.contains(button);
      });
  if (fresh || std::hypot(pad.left.x, pad.left.y) >= tuning.stick_engage) {
    modality_ = composition::InputModality::controller;
    pointing_ = false;
  }
}

auto InputRouter::heading(const Gamepad& pad,
                          const provider::NavigationTuning& tuning)
    -> std::optional<composition::MoveCommandDirection> {
  const std::optional<composition::MoveCommandDirection> stick =
      stick_.update(pad.left, tuning);
  if (pad.buttons.contains(provider::Button::dpad_up)) {
    return composition::MoveCommandDirection::up;
  }
  if (pad.buttons.contains(provider::Button::dpad_down)) {
    return composition::MoveCommandDirection::down;
  }
  if (pad.buttons.contains(provider::Button::dpad_left)) {
    return composition::MoveCommandDirection::left;
  }
  if (pad.buttons.contains(provider::Button::dpad_right)) {
    return composition::MoveCommandDirection::right;
  }
  return stick;
}

void InputRouter::feel(composition::Tree& tree,
                       composition::SensoryFeedback feedback) {
  if (const auto focused = engine_.focused()) {
    tree.request_feedback(*focused, feedback,
                          composition::FeedbackOrigin::system);
  }
}

void InputRouter::blocked(composition::Tree& tree,
                          composition::MoveCommandDirection direction,
                          std::chrono::nanoseconds now, Outcome& outcome) {
  for (const composition::NodeId id : chain(tree)) {
    const auto node = tree.node(id);
    if (!node.has_value() || node->get().interactions().disabled) {
      continue;
    }
    for (const std::shared_ptr<composition::Command>& command :
         node->get().interactions().commands) {
      if (command->meets(direction)) {
        outcome.invocations.push_back(Invocation::move(command, direction));
        outcome.changed = true;
        return;
      }
    }
  }
  outcome.sounds.push_back(provider::SoundRole::focus_error);
  outcome.changed = true;
  feel(tree, composition::SensoryFeedback::impact(
                 composition::SensoryFeedback::Flexibility::soft));
  if (const auto focused = engine_.focused()) {
    shake_ = Shake{.node = *focused, .direction = direction, .started = now};
  }
}

void InputRouter::navigate(composition::Tree& tree,
                           composition::MoveCommandDirection direction,
                           bool initial, std::chrono::nanoseconds now,
                           Outcome& outcome) {
  scrolls_.unfollow();
  if (initial && options_.hides_focus_on_touch &&
      arriving_ == composition::InputModality::touch) {
    outcome.changed = true;
    return;
  }
  for (const composition::NodeId id : chain(tree)) {
    const auto node = tree.node(id);
    if (!node.has_value() || node->get().interactions().disabled) {
      continue;
    }
    for (const std::shared_ptr<composition::Command>& command :
         node->get().interactions().commands) {
      if (command->handles(direction)) {
        outcome.invocations.push_back(Invocation::move(command, direction));
        outcome.changed = true;
        return;
      }
    }
  }
  if (const auto focused = engine_.focused();
      focused.has_value() &&
      scrolls_.scrollable(*focused, composition::ScrollInputKind::controller)
          .has_value() &&
      scrolls_.step_toward(*focused, direction, now)) {
    outcome.changed = true;
    return;
  }
  last_move_ = direction;
  const std::optional<composition::NodeId> current = engine_.focused();
  const std::optional<composition::NodeId> keep =
      current.has_value()
          ? scrolls_.scrollable_toward(tree, *current, direction)
          : std::nullopt;
  switch (engine_.move(tree, direction, keep)) {
    case FocusEngine::MoveOutcome::held: {
      const auto node = tree.node(*current);
      if (keep.has_value() && node.has_value() &&
          scrolls_.nudge(*keep, direction, node->get().frame(),
                         options_.focus_scroll_margin, now)) {
        outcome.changed = true;
        break;
      }
      switch (engine_.move(tree, direction)) {
        case FocusEngine::MoveOutcome::moved:
          outcome.sounds.push_back(provider::SoundRole::focus_change);
          outcome.changed = true;
          shake_.reset();
          feel(tree, composition::SensoryFeedback::selection);
          break;
        case FocusEngine::MoveOutcome::blocked:
        case FocusEngine::MoveOutcome::held:
          blocked(tree, direction, now, outcome);
          break;
        case FocusEngine::MoveOutcome::deferred:
          break;
      }
      break;
    }
    case FocusEngine::MoveOutcome::moved:
      outcome.sounds.push_back(provider::SoundRole::focus_change);
      outcome.changed = true;
      shake_.reset();
      feel(tree, composition::SensoryFeedback::selection);
      break;
    case FocusEngine::MoveOutcome::blocked:
      blocked(tree, direction, now, outcome);
      break;
    case FocusEngine::MoveOutcome::deferred:
      break;
  }
}

void InputRouter::steer(composition::Tree& tree, const Gamepad& pad,
                        std::chrono::nanoseconds now,
                        const provider::NavigationTuning& tuning,
                        Outcome& outcome) {
  const std::optional<composition::MoveCommandDirection> wanted =
      heading(pad, tuning);
  if (wanted != heading_) {
    if (heading_.has_value()) {
      static_cast<void>(navigation_.update(false, now, tuning));
    }
    heading_ = wanted;
  }
  const PressTracker::Events events =
      navigation_.update(heading_.has_value(), now, tuning);
  if (!heading_.has_value()) {
    return;
  }
  if (events.pressed) {
    navigate(tree, *heading_, true, now, outcome);
  }
  for (std::uint32_t repeat = 0; repeat < events.repeats; ++repeat) {
    navigate(tree, *heading_, false, now, outcome);
  }
}

void InputRouter::fire(const Binding& binding,
                       composition::CommandTrigger trigger, Outcome& outcome) {
  if (binding.command == nullptr) {
    return;
  }
  const composition::CommandTrigger wanted =
      trigger_of(binding.command->options());
  const bool fires = (trigger == composition::CommandTrigger::press &&
                      (wanted == composition::CommandTrigger::press ||
                       wanted == composition::CommandTrigger::repeat)) ||
                     (trigger == composition::CommandTrigger::repeat &&
                      wanted == composition::CommandTrigger::repeat) ||
                     (trigger == composition::CommandTrigger::long_press &&
                      wanted == composition::CommandTrigger::long_press);
  if (!fires) {
    return;
  }
  outcome.invocations.push_back(Invocation::command(binding.command));
  if (binding.command->kind() == composition::Command::Kind::exit) {
    outcome.sounds.push_back(provider::SoundRole::back);
  }
  outcome.changed = true;
}

void InputRouter::buttons(composition::Tree& tree, const Gamepad& pad,
                          std::chrono::nanoseconds now,
                          const provider::NavigationTuning& tuning,
                          Outcome& outcome) {
  for (std::size_t index = 0; index < provider::all_buttons.size(); ++index) {
    const provider::Button button = provider::all_buttons[index];
    if (button == provider::Button::dpad_up ||
        button == provider::Button::dpad_down ||
        button == provider::Button::dpad_left ||
        button == provider::Button::dpad_right) {
      continue;
    }
    const PressTracker::Events events = trackers_[index].update(
        pad.buttons.contains(button), now, tuned(bindings_[index], tuning));
    if (events.pressed) {
      bindings_[index] = resolve(tree, button);
      if (bindings_[index].has_value() && bindings_[index]->carry.has_value()) {
        perform(tree, *bindings_[index], outcome);
        bindings_[index].reset();
        continue;
      }
      if (bindings_[index].has_value()) {
        if (bindings_[index]->activation.has_value()) {
          pressed_ = bindings_[index]->activation;
          outcome.changed = true;
        } else {
          fire(*bindings_[index], composition::CommandTrigger::press, outcome);
        }
      }
    }
    if (!bindings_[index].has_value()) {
      const std::uint32_t presses = (events.pressed ? 1U : 0U) + events.repeats;
      if (presses > 0 && turn(tree, button, now)) {
        outcome.changed = true;
      }
      continue;
    }
    const Binding& binding = *bindings_[index];
    for (std::uint32_t repeat = 0; repeat < events.repeats; ++repeat) {
      fire(binding, composition::CommandTrigger::repeat, outcome);
    }
    if (events.long_press && binding.held != nullptr) {
      outcome.invocations.push_back(Invocation::command(binding.held));
      feel(tree, composition::SensoryFeedback::impact(
                     composition::SensoryFeedback::Weight::medium));
      pressed_.reset();
      outcome.changed = true;
    }

    if (events.released) {
      if (binding.activation.has_value()) {
        if (pressed_ == binding.activation &&
            engine_.focused() == binding.activation) {
          outcome.invocations.push_back(
              Invocation::activation(*binding.activation));
          outcome.sounds.push_back(provider::SoundRole::click);
        }
        pressed_.reset();
        outcome.changed = true;
      }
      bindings_[index].reset();
    }
  }
}

void InputRouter::interrupt(composition::Tree& tree) {
  GestureArena::Effects effects;
  arena_.abandon(tree, scrolls_, effects);
  std::vector<Invocation> dropped;
  drag_.cancel(tree, dropped);
  second_touch_.reset();
  pinch_centre_.reset();
  trackpad_.reset();
  bindings_.fill(std::nullopt);
  pressed_.reset();
  touch_.reset();
  contact_.reset();
  pointer_down_ = false;
}

void InputRouter::touches(composition::Tree& tree,
                          const provider::InputFrame& input,
                          provider::SurfaceScale scale,
                          std::chrono::nanoseconds now, Outcome& outcome) {
  GestureArena::Effects effects;
  const auto points = [scale](provider::PixelPoint point) {
    return composition::Point{.x = point.x / scale.value(),
                              .y = point.y / scale.value()};
  };
  for (const provider::Touch& touch : input.touches) {
    if (touch.phase == provider::TouchPhase::began && !touch_.has_value()) {
      scrolls_.unfollow();
      touch_ = touch.id;
      second_touch_.reset();
      contact_ = touch.position;
      arena_.down(tree, points(touch.position), now, scrolls_,
                  engine_.confined_to(),
                  composition::GestureInputKinds::direct_touch, effects);
    } else if (touch_ == touch.id) {
      if (touch.phase == provider::TouchPhase::ended) {
        arena_.up(tree, points(touch.position), now, scrolls_, effects);
        touch_.reset();
        second_touch_.reset();
        contact_.reset();
      } else if (touch.phase != provider::TouchPhase::began) {
        contact_ = touch.position;
        arena_.move(tree, points(touch.position), now, scrolls_, effects);
      }
    } else if (touch.phase == provider::TouchPhase::began &&
               touch_.has_value() && !second_touch_.has_value()) {
      second_touch_ = touch.id;
      arena_.second(tree, points(touch.position), now, scrolls_, effects);
    } else if (second_touch_ == touch.id) {
      if (touch.phase == provider::TouchPhase::ended) {
        second_touch_.reset();
        arena_.second(tree, std::nullopt, now, scrolls_, effects);
      } else if (touch.phase == provider::TouchPhase::moved) {
        arena_.second(tree, points(touch.position), now, scrolls_, effects);
      }
    }
  }
  if (input.pointer.has_value()) {
    const provider::PointerState& pointer = *input.pointer;
    const composition::Point at = points(pointer.position);
    const auto mirrored = [this, &at] {
      return composition::Point{.x = (2.0F * pinch_centre_->x) - at.x,
                                .y = (2.0F * pinch_centre_->y) - at.y};
    };
    if (pointer.left && !pointer_down_) {
      pointer_down_ = true;
      contact_ = pointer.position;
      arena_.down(tree, at, now, scrolls_, engine_.confined_to(),
                  composition::GestureInputKinds::pointer, effects);
      if (keys_.contains(provider::Key::left_alt) ||
          keys_.contains(provider::Key::right_alt)) {
        const composition::Rect area = tree.root().frame();
        pinch_centre_ = composition::Point{.x = area.x + (area.width * 0.5F),
                                           .y = area.y + (area.height * 0.5F)};
        arena_.second(tree, mirrored(), now, scrolls_, effects);
      }
    } else if (pointer.left && pointer_down_) {
      contact_ = pointer.position;
      arena_.move(tree, at, now, scrolls_, effects);
      if (pinch_centre_.has_value()) {
        arena_.second(tree, mirrored(), now, scrolls_, effects);
      }
    } else if (!pointer.left && pointer_down_) {
      pointer_down_ = false;
      pinch_centre_.reset();
      contact_.reset();
      arena_.up(tree, at, now, scrolls_, effects);
    }
    if (input.pinch.has_value() && !pointer_down_ && !touch_.has_value()) {
      pinch(tree, *input.pinch, at, now, effects);
    }
    if ((pointer.wheel_x != 0.0F || pointer.wheel_y != 0.0F) &&
        scrolls_.wheel(GestureArena::chain(tree, at), pointer.wheel_x,
                       pointer.wheel_y, now)) {
      outcome.changed = true;
    }
  }
  arena_.elapse(tree, now, scrolls_, effects);
  if (effects.focus.has_value()) {
    const auto node = tree.node(*effects.focus);
    const bool declines =
        node.has_value() &&
        node->get().interactions().focus_interactions.has_value() &&
        (*node->get().interactions().focus_interactions &
         composition::FocusInteractions::edit) ==
            composition::FocusInteractions{};
    if (!declines && engine_.focus(tree, *effects.focus)) {
      shake_.reset();
    }
    touch_focus_ = !declines;
  }
  carry(tree, effects, outcome);
  for (Invocation& invocation : effects.invocations) {
    outcome.invocations.push_back(std::move(invocation));
  }
  for (const provider::SoundRole sound : effects.sounds) {
    outcome.sounds.push_back(sound);
  }
  outcome.changed = outcome.changed || effects.changed;
}

void InputRouter::pinch(composition::Tree& tree,
                        const provider::TrackpadPinch& pinch,
                        composition::Point at, std::chrono::nanoseconds now,
                        GestureArena::Effects& effects) {
  if (pinch.phase == provider::TouchPhase::began) {
    trackpad_ = Trackpad{.centre = at, .spread = trackpad_spread};
    arena_.down(tree,
                composition::Point{.x = at.x - trackpad_->spread, .y = at.y},
                now, scrolls_, engine_.confined_to(),
                composition::GestureInputKinds::indirect_touch, effects);
    arena_.second(tree,
                  composition::Point{.x = at.x + trackpad_->spread, .y = at.y},
                  now, scrolls_, effects);
    return;
  }
  if (!trackpad_.has_value()) {
    return;
  }
  trackpad_->spread *= pinch.scale;
  const composition::Point centre = trackpad_->centre;
  const float spread = trackpad_->spread;
  arena_.move(tree, composition::Point{.x = centre.x - spread, .y = centre.y},
              now, scrolls_, effects);
  arena_.second(tree, composition::Point{.x = centre.x + spread, .y = centre.y},
                now, scrolls_, effects);
  if (pinch.phase == provider::TouchPhase::ended) {
    arena_.second(tree, std::nullopt, now, scrolls_, effects);
    arena_.up(tree, composition::Point{.x = centre.x - spread, .y = centre.y},
              now, scrolls_, effects);
    trackpad_.reset();
  }
}

void InputRouter::carry(composition::Tree& tree, GestureArena::Effects& effects,
                        Outcome& outcome) {
  if (effects.lifted.has_value() &&
      drag_.begin(tree, effects.lifted->node, DragSession::Origin::touch,
                  effects.lifted->point, outcome.invocations)) {
    outcome.changed = true;
  }
  if (!drag_.active() || drag_.origin() != DragSession::Origin::touch) {
    return;
  }
  if (effects.dragged.has_value()) {
    drag_.move(tree, *effects.dragged, outcome.invocations);
    outcome.changed = true;
  }
  if (effects.dropped.has_value()) {
    drag_.move(tree, *effects.dropped, outcome.invocations);
    if (drag_.drop(tree, outcome.invocations)) {
      outcome.sounds.push_back(provider::SoundRole::click);
    }
    outcome.changed = true;
  }
  if (effects.lift_cancelled) {
    drag_.cancel(tree, outcome.invocations);
    outcome.changed = true;
  }
}

void InputRouter::hover(composition::Tree& tree,
                        const provider::InputFrame& input,
                        provider::SurfaceScale scale, Outcome& outcome) {
  if (!pointer_.has_value()) {
    return;
  }
  const provider::PixelPoint position = pointer_->position;
  const bool wheeled =
      input.pointer.has_value() &&
      (input.pointer->wheel_x != 0.0F || input.pointer->wheel_y != 0.0F);
  const bool moved = hover_seen_ != position;
  if (!moved && !wheeled && hover_pointing_ == pointing_ &&
      hover_revision_ == tree.revision() &&
      hover_geometry_ == tree.geometry() &&
      hover_arrangement_ == tree.arrangement()) {
    return;
  }
  hover_geometry_ = tree.geometry();
  hover_arrangement_ = tree.arrangement();
  hover_seen_ = position;
  hover_pointing_ = pointing_;
  hover_point_ = composition::Point{.x = position.x / scale.value(),
                                    .y = position.y / scale.value()};
  std::vector<composition::NodeId> chain;
  if (pointing_) {
    chain = GestureArena::chain(tree, hover_point_);
  }
  hovered_ = chain.empty() ? std::nullopt
                           : std::optional<composition::NodeId>{chain.front()};
  tree.hover(hovered_);
  notify(tree, chain, moved, outcome);
  const composition::PointerStyle style = pointer_style_;
  const composition::Visibility visibility = pointer_visibility_;
  restyle(tree, chain);
  if (style != pointer_style_ || visibility != pointer_visibility_) {
    outcome.changed = true;
  }
  follow(tree, chain, hover_point_, outcome);
  if (chain != hover_chain_ || hover_path_ == nullptr) {
    composition::HoverPath traced = trace(tree, chain);
    if (hover_path_ == nullptr || *hover_path_ != traced) {
      hover_path_ =
          std::make_shared<const composition::HoverPath>(std::move(traced));
      outcome.changed = true;
    }
    hover_chain_ = std::move(chain);
  }
  hover_revision_ = tree.revision();
}

void InputRouter::notify(const composition::Tree& tree,
                         const std::vector<composition::NodeId>& chain,
                         bool moved, Outcome& outcome) {
  std::vector<composition::NodeId> inside;
  std::vector<composition::NodeId> continuous;
  for (const composition::NodeId id : chain) {
    const auto node = tree.node(id);
    if (!node.has_value()) {
      continue;
    }
    if (node->get().interactions().hover != nullptr) {
      inside.push_back(id);
    }
    if (node->get().interactions().continuous_hover != nullptr) {
      continuous.push_back(id);
    }
  }
  const auto handler = [&tree](composition::NodeId id) {
    const auto node = tree.node(id);
    return node.has_value() ? node->get().interactions().hover : nullptr;
  };
  for (const composition::NodeId id : hover_inside_) {
    if (!std::ranges::contains(inside, id)) {
      if (auto action = handler(id)) {
        outcome.invocations.push_back(
            Invocation::callback([action] { (*action)(false); }));
      }
    }
  }
  for (const composition::NodeId id : inside) {
    if (!std::ranges::contains(hover_inside_, id)) {
      if (auto action = handler(id)) {
        outcome.invocations.push_back(
            Invocation::callback([action] { (*action)(true); }));
      }
    }
  }
  for (const composition::NodeId id : continuous_inside_) {
    if (std::ranges::contains(continuous, id)) {
      continue;
    }
    const auto node = tree.node(id);
    if (!node.has_value()) {
      continue;
    }
    if (auto tracked = node->get().interactions().continuous_hover) {
      outcome.invocations.push_back(Invocation::callback(
          [tracked] { tracked->action(composition::HoverPhase::ended); }));
    }
  }
  for (const composition::NodeId id : continuous) {
    if (!moved && std::ranges::contains(continuous_inside_, id)) {
      continue;
    }
    const auto node = tree.node(id);
    auto tracked = node->get().interactions().continuous_hover;
    const composition::Point origin =
        GestureArena::space_origin(tree, node->get(), tracked->space);
    const composition::Point local{.x = hover_point_.x - origin.x,
                                   .y = hover_point_.y - origin.y};
    outcome.invocations.push_back(Invocation::callback([tracked, local] {
      tracked->action(composition::HoverPhase::active(local));
    }));
  }
  hover_inside_ = std::move(inside);
  continuous_inside_ = std::move(continuous);
}

void InputRouter::restyle(const composition::Tree& tree,
                          const std::vector<composition::NodeId>& chain) {
  std::optional<composition::PointerStyle> style;
  composition::Visibility visibility = composition::Visibility::automatic;
  for (const composition::NodeId id : chain) {
    const auto node = tree.node(id);
    if (!node.has_value()) {
      continue;
    }
    const composition::Interactions& declared = node->get().interactions();
    if (!style.has_value() && declared.pointer_style.has_value()) {
      style = declared.pointer_style;
    }
    if (visibility == composition::Visibility::automatic) {
      visibility = declared.pointer_visibility;
    }
  }
  pointer_style_ = style.value_or(composition::PointerStyle::default_);
  pointer_visibility_ = visibility;
}

auto InputRouter::hover_enabled(const composition::Tree& tree,
                                composition::NodeId node) -> bool {
  std::optional<composition::NodeId> at = node;
  while (at.has_value()) {
    const auto found = tree.node(*at);
    if (!found.has_value()) {
      return true;
    }
    if (const auto enabled = found->get().override_of(
            composition::HoverEffectEnvironment::is_hover_effect_enabled);
        enabled.has_value() && !enabled->get()) {
      return false;
    }
    at = found->get().parent();
  }
  return true;
}

void InputRouter::follow(composition::Tree& tree,
                         const std::vector<composition::NodeId>& chain,
                         composition::Point point, Outcome& outcome) {
  hover_target_.reset();
  if (!pointing_ || chain.empty()) {
    return;
  }
  const std::optional<composition::NodeId> candidate = engine_.hit(tree, point);
  if (!candidate.has_value() || !std::ranges::contains(chain, *candidate) ||
      !hover_enabled(tree, *candidate)) {
    return;
  }
  hover_target_ = candidate;
  if (engine_.focused() != candidate && engine_.focus(tree, *candidate)) {
    touch_focus_ = true;
    shake_.reset();
    outcome.changed = true;
  }
}

auto InputRouter::trace(const composition::Tree& tree,
                        std::vector<composition::NodeId> chain) const
    -> composition::HoverPath {
  std::vector<composition::HoverPath::Group> groups;
  const auto add = [&groups](composition::HoverPath::Group group) {
    if (!std::ranges::contains(groups, group)) {
      groups.push_back(std::move(group));
    }
  };
  for (const composition::NodeId id : chain) {
    const auto node = tree.node(id);
    if (!node.has_value()) {
      continue;
    }
    using Kind = composition::Interactions::HoverContainer::Kind;
    if (const auto& container = node->get().interactions().hover_container) {
      if (container->kind == Kind::implicit) {
        add(composition::HoverPath::Group{.container = id,
                                          .named = std::nullopt});
      } else if (container->kind == Kind::named &&
                 container->group->behavior().activates()) {
        add(composition::HoverPath::Group{.container = std::nullopt,
                                          .named = container->group});
      }
    }
    if (const auto& effect = node->get().modifiers().hover) {
      for (const composition::CustomHoverEffect::Layer& layer :
           effect->layers()) {
        if (layer.group.has_value() && layer.group->behavior().activates()) {
          add(composition::HoverPath::Group{.container = std::nullopt,
                                            .named = layer.group});
        }
      }
    }
  }
  return composition::HoverPath{std::move(chain), std::move(groups)};
}

auto InputRouter::pointer() const -> std::optional<Pointer> {
  if (!pointer_.has_value()) {
    return std::nullopt;
  }
  return Pointer{.location = hover_point_,
                 .style = pointer_style_,
                 .visible = pointing_ && pointer_visibility_ !=
                                             composition::Visibility::hidden};
}

auto InputRouter::modifiers() const -> composition::EventModifiers {
  composition::EventModifiers held;
  if (keys_.contains(provider::Key::left_super) ||
      keys_.contains(provider::Key::right_super)) {
    held = held | composition::EventModifiers::command;
  }
  if (keys_.contains(provider::Key::left_ctrl) ||
      keys_.contains(provider::Key::right_ctrl)) {
    held = held | composition::EventModifiers::control;
  }
  if (keys_.contains(provider::Key::left_alt) ||
      keys_.contains(provider::Key::right_alt)) {
    held = held | composition::EventModifiers::option;
  }
  if (keys_.contains(provider::Key::left_shift) ||
      keys_.contains(provider::Key::right_shift)) {
    held = held | composition::EventModifiers::shift;
  }
  return held;
}

auto InputRouter::press_of(const provider::KeyEvent& event) const
    -> composition::KeyPress {
  const composition::EventModifiers held = modifiers();
  const bool shift = held.contains(composition::EventModifiers::shift);
  const composition::KeyEquivalent key =
      composition::KeyEquivalent::pressed(event.key, shift);
  std::string characters;
  if (const std::optional<char> character =
          composition::KeyEquivalent::typed(event.key, shift)) {
    characters.push_back(*character);
  }
  composition::KeyPress::Phases phase = composition::KeyPress::Phases::down;
  if (!event.down) {
    phase = composition::KeyPress::Phases::up;
  } else if (event.repeat) {
    phase = composition::KeyPress::Phases::repeat;
  }
  return composition::KeyPress{.key = key,
                               .characters = std::move(characters),
                               .modifiers = held,
                               .phase = phase};
}

auto InputRouter::deliver(composition::Tree& tree,
                          const composition::KeyPress& press) -> bool {
  for (const composition::NodeId id : chain(tree)) {
    const auto node = tree.node(id);
    if (!node.has_value() || node->get().interactions().disabled) {
      continue;
    }
    for (const std::shared_ptr<composition::KeyPressHandler>& handler :
         node->get().interactions().key_presses) {
      if (handler->matches(press) &&
          handler->perform(press) == composition::KeyPress::Result::handled) {
        return true;
      }
    }
    if (tree.press(id, press)) {
      return true;
    }
  }
  return false;
}

auto InputRouter::seek(const composition::Tree& tree,
                       const composition::Node& node,
                       const composition::KeyPress& press)
    -> std::optional<composition::NodeId> {
  const bool top = !node.parent().has_value();
  if (!top && node.phase() != composition::NodeLifecyclePhase::live) {
    return std::nullopt;
  }
  const composition::Interactions& declared = node.interactions();
  if (declared.disabled || !declared.hit_testing) {
    return std::nullopt;
  }
  if (const auto& presented = node.presented();
      !top && presented.has_value() && !presented->visible) {
    return std::nullopt;
  }
  if (const auto layer = node.content_as<composition::Layer::Content>()) {
    const composition::ExclusiveScopes& exclusive = tree.exclusive();
    if (exclusive.active() && !exclusive.keeps(layer->get().layer())) {
      return std::nullopt;
    }
  }
  if (declared.keyboard_shortcut.has_value() &&
      fires(
          node.layout_direction() == composition::LayoutDirection::right_to_left
              ? declared.keyboard_shortcut->mirrored()
              : *declared.keyboard_shortcut,
          press) &&
      node.content().activatable()) {
    return node.id();
  }
  for (const composition::Node& child : node.children()) {
    if (const auto found = seek(tree, child, press)) {
      return found;
    }
  }
  return std::nullopt;
}

auto InputRouter::fires(const composition::KeyboardShortcut& shortcut,
                        const composition::KeyPress& press) -> bool {
  const char32_t wanted = shortcut.key.character();
  const bool capital = wanted >= U'A' && wanted <= U'Z';
  const composition::KeyEquivalent named =
      capital ? composition::KeyEquivalent{static_cast<char32_t>(wanted - U'A' +
                                                                 U'a')}
              : shortcut.key;
  const composition::EventModifiers implied =
      capital || shortcut.key.shifted()
          ? shortcut.modifiers | composition::EventModifiers::shift
          : shortcut.modifiers;
  if (named == press.key) {
    return press.modifiers == implied;
  }
  return press.key.shifted() && named == press.key.unshifted() &&
         press.modifiers == shortcut.modifiers;
}

auto InputRouter::shortcut(const composition::Tree& tree,
                           const composition::KeyPress& press) const
    -> std::optional<composition::NodeId> {
  if (press.phase != composition::KeyPress::Phases::down) {
    return std::nullopt;
  }
  if (const auto trap = engine_.confined_to()) {
    if (const auto node = tree.node(*trap)) {
      return seek(tree, node->get(), press);
    }
  }
  return seek(tree, tree.root(), press);
}

auto InputRouter::equivalent(const composition::Tree& tree,
                             const provider::KeyEvent& event,
                             const composition::KeyPress& press,
                             Outcome& outcome) const -> bool {
  if (!event.down || event.repeat) {
    return false;
  }
  const auto target = shortcut(tree, press);
  if (!target.has_value()) {
    return false;
  }
  outcome.invocations.push_back(Invocation::activation(*target));
  outcome.sounds.push_back(provider::SoundRole::click);
  return true;
}

void InputRouter::collect_modifier_handlers(
    const composition::Node& node,
    std::vector<std::pair<composition::NodeId,
                          std::shared_ptr<composition::ModifierKeysHandler>>>&
        found) {
  if (node.parent().has_value() &&
      node.phase() != composition::NodeLifecyclePhase::live) {
    return;
  }
  for (const auto& handler : node.interactions().modifier_keys) {
    found.emplace_back(node.id(), handler);
  }
  for (const composition::Node& child : node.children()) {
    collect_modifier_handlers(child, found);
  }
}

void InputRouter::track_modifiers(composition::Tree& tree, Outcome& outcome) {
  const composition::EventModifiers current = modifiers();
  const bool changed = current != held_modifiers_;
  const bool restructured = modifier_revision_ != tree.revision();
  if (!changed && !restructured) {
    return;
  }
  if (restructured) {
    modifier_handlers_.clear();
    collect_modifier_handlers(tree.root(), modifier_handlers_);
    modifier_revision_ = tree.revision();
  }
  const composition::EventModifiers old = held_modifiers_;
  for (const auto& [id, handler] : modifier_handlers_) {
    const bool fresh = !std::ranges::contains(modifier_nodes_, id);
    if (fresh && handler->initial()) {
      outcome.invocations.push_back(Invocation::callback(
          [handler, current] { handler->changed(current, current); }));
    } else if (!fresh && changed && handler->differs(old, current)) {
      outcome.invocations.push_back(Invocation::callback(
          [handler, old, current] { handler->changed(old, current); }));
    }
  }
  modifier_nodes_.clear();
  for (const auto& entry : modifier_handlers_) {
    modifier_nodes_.push_back(entry.first);
  }
  held_modifiers_ = current;
}

auto InputRouter::type_select(composition::Tree& tree,
                              const composition::KeyPress& press,
                              std::chrono::nanoseconds now) -> bool {
  const std::optional<char> typed = press.key.printable();
  if (!typed.has_value() || (*typed == ' ' && type_select_.empty())) {
    return false;
  }
  if (const std::optional<composition::NodeId> focused = engine_.focused()) {
    const auto node = tree.node(*focused);
    if (node.has_value() &&
        node->get().interactions().focus_interactions.has_value() &&
        (*node->get().interactions().focus_interactions &
         composition::FocusInteractions::edit) ==
            composition::FocusInteractions::edit &&
        !node->get().content().activatable()) {
      return false;
    }
  }
  if (now - type_select_at_ > type_select_timeout) {
    type_select_.clear();
  }
  type_select_at_ = now;
  type_select_.push_back(*typed);
  const std::optional<composition::NodeId> target =
      engine_.type_select(tree, type_select_);
  if (!target.has_value()) {
    return false;
  }
  if (target != engine_.focused()) {
    engine_.focus(tree, *target);
  }
  return true;
}

void InputRouter::type(composition::Tree& tree,
                       const provider::InputFrame& input,
                       std::chrono::nanoseconds now, Outcome& outcome) {
  for (const provider::KeyEvent& event : input.keys) {
    if (event.down) {
      keys_.insert(event.key);
    } else {
      keys_.erase(event.key);
    }
    switch (event.key) {
      case provider::Key::left_ctrl:
      case provider::Key::right_ctrl:
      case provider::Key::left_shift:
      case provider::Key::right_shift:
      case provider::Key::left_alt:
      case provider::Key::right_alt:
      case provider::Key::left_super:
      case provider::Key::right_super:
        continue;
      default:
        break;
    }
    const composition::KeyPress press = press_of(event);
    const bool chorded =
        press.modifiers.contains(composition::EventModifiers::command) ||
        press.modifiers.contains(composition::EventModifiers::control) ||
        press.modifiers.contains(composition::EventModifiers::option);
    bool handled = chorded && equivalent(tree, event, press, outcome);
    handled = handled || deliver(tree, press);
    handled = handled || (!chorded && equivalent(tree, event, press, outcome));
    handled = handled ||
              (event.down && !chorded &&
               !press.modifiers.contains(composition::EventModifiers::shift) &&
               type_select(tree, press, now));
    if (!event.down) {
      consumed_.erase(event.key);
    } else if (handled) {
      consumed_.insert(event.key);
    }
    outcome.changed = outcome.changed || handled;
  }
}

void InputRouter::perform(composition::Tree& tree, const Binding& binding,
                          Outcome& outcome) {
  switch (*binding.carry) {
    case Carry::pick:
      if (binding.source.has_value() &&
          drag_.begin(tree, *binding.source, DragSession::Origin::controller,
                      std::nullopt, outcome.invocations)) {
        feel(tree, composition::SensoryFeedback::impact(
                       composition::SensoryFeedback::Weight::medium));
      }
      break;
    case Carry::drop:
      if (drag_.drop(tree, outcome.invocations)) {
        outcome.sounds.push_back(provider::SoundRole::click);
      }
      break;
    case Carry::cancel:
      drag_.cancel(tree, outcome.invocations);
      outcome.sounds.push_back(provider::SoundRole::back);
      break;
  }
  outcome.changed = true;
}

void InputRouter::scroll(composition::Tree& tree, const Gamepad& pad,
                         std::optional<composition::NodeId> before,
                         std::chrono::nanoseconds now,
                         const provider::NavigationTuning& tuning,
                         Outcome& outcome) {
  if (options_.right_stick_scrolls &&
      scrolls_.stick(tree, engine_.focused(), pad.right, tuning.scroll_speed,
                     now)) {
    outcome.changed = true;
  }
  const std::optional<composition::NodeId> focused = engine_.focused();
  if (focused.has_value() && focused != before && !touch_focus_ &&
      !scrolls_.followed().has_value() &&
      scrolls_.reveal(tree, *focused, options_.focus_scroll_margin, now)) {
    outcome.changed = true;
  }
  ScrollCoordinator::Report report;
  scrolls_.step(tree, now, report);
  outcome.changed = outcome.changed || report.changed;
  if (accompany(tree)) {
    outcome.changed = true;
  }
  for (Invocation& invocation : report.invocations) {
    outcome.invocations.push_back(std::move(invocation));
  }
}

auto InputRouter::turn(composition::Tree& tree, provider::Button button,
                       std::chrono::nanoseconds now) -> bool {
  std::optional<ScrollCoordinator::Page> kind;
  switch (button) {
    case provider::Button::zl:
      kind = ScrollCoordinator::Page::up;
      break;
    case provider::Button::zr:
      kind = ScrollCoordinator::Page::down;
      break;
    case provider::Button::left_stick:
      kind = ScrollCoordinator::Page::start;
      break;
    case provider::Button::right_stick:
      kind = ScrollCoordinator::Page::end;
      break;
    default:
      return false;
  }
  return scrolls_.page(tree, engine_.focused(), *kind, now);
}

auto InputRouter::accompany(composition::Tree& tree) -> bool {
  const std::optional<composition::NodeId> container = scrolls_.followed();
  if (!container.has_value()) {
    return false;
  }
  const auto node = tree.node(*container);
  if (!node.has_value() || !node->get().presented().has_value()) {
    return false;
  }
  const composition::PresentedFrame& shown = *node->get().presented();
  composition::Rect area = shown.bounds;
  if (shown.clip.has_value()) {
    const float top = std::max(area.y, shown.clip->y);
    const float bottom =
        std::min(area.y + area.height, shown.clip->y + shown.clip->height);
    area.y = top;
    area.height = std::max(bottom - top, 0.0F);
  }
  const float margin =
      std::min(options_.focus_scroll_margin, area.height * 0.25F);
  area.y += margin;
  area.height = std::max(area.height - (2.0F * margin), 0.0F);
  const std::optional<composition::NodeId> next =
      engine_.visible_inside(tree, *container, area);
  return next.has_value() && engine_.focus(tree, *next);
}

auto InputRouter::chain(const composition::Tree& tree)
    -> std::span<const composition::NodeId> {
  chain_.clear();
  std::optional<composition::NodeId> current = engine_.origin();
  while (current.has_value()) {
    const auto node = tree.node(*current);
    if (!node.has_value()) {
      break;
    }
    chain_.push_back(*current);
    current = node->get().parent();
  }
  return chain_;
}

auto InputRouter::button_for(const composition::Command& command) const
    -> std::optional<provider::Button> {
  switch (command.kind()) {
    case composition::Command::Kind::button:
      return command.pad_button();
    case composition::Command::Kind::exit:
      return options_.layout.back();
    case composition::Command::Kind::play_pause:
      return options_.play_pause;
    case composition::Command::Kind::remove:
      return options_.remove;
    case composition::Command::Kind::page:
      return command.forward() ? options_.page_next : options_.page_previous;
    case composition::Command::Kind::move:
    case composition::Command::Kind::edge:
      return std::nullopt;
  }
  return std::nullopt;
}

auto InputRouter::holds(const composition::Command& command,
                        provider::Button button) noexcept -> bool {
  return command.kind() == composition::Command::Kind::button &&
         command.pad_button() == button &&
         trigger_of(command.options()) ==
             composition::CommandTrigger::long_press;
}

auto InputRouter::trigger_of(
    const composition::CommandOptions& options) noexcept
    -> composition::CommandTrigger {
  return options.hold.has_value() ? composition::CommandTrigger::long_press
                                  : options.trigger;
}

auto InputRouter::tuned(const std::optional<Binding>& binding,
                        const provider::NavigationTuning& tuning) noexcept
    -> provider::NavigationTuning {
  provider::NavigationTuning held = tuning;
  if (binding.has_value() && binding->held != nullptr &&
      binding->held->options().hold.has_value()) {
    held.long_press = *binding->held->options().hold;
  }
  return held;
}

auto InputRouter::resolve(const composition::Tree& tree,
                          provider::Button button) -> std::optional<Binding> {
  const std::optional<composition::NodeId> focused = engine_.focused();
  if (drag_.active() && drag_.origin() == DragSession::Origin::controller) {
    if (button == options_.move || button == options_.layout.confirm()) {
      return Binding{.command = nullptr,
                     .activation = std::nullopt,
                     .held = nullptr,
                     .carry = Carry::drop,
                     .source = std::nullopt};
    }
    if (button == options_.layout.back()) {
      return Binding{.command = nullptr,
                     .activation = std::nullopt,
                     .held = nullptr,
                     .carry = Carry::cancel,
                     .source = std::nullopt};
    }
  }
  std::shared_ptr<composition::Command> held;
  for (const composition::NodeId id : chain(tree)) {
    const auto node = tree.node(id);
    if (!node.has_value() || node->get().interactions().disabled) {
      continue;
    }
    const auto& commands = node->get().interactions().commands;
    const auto found = std::ranges::find_if(
        commands, [button](const std::shared_ptr<composition::Command>& each) {
          return holds(*each, button);
        });
    if (found != commands.end()) {
      held = *found;
      break;
    }
  }
  for (const composition::NodeId id : chain(tree)) {
    const auto node = tree.node(id);
    if (!node.has_value() || node->get().interactions().disabled) {
      continue;
    }
    const auto& commands = node->get().interactions().commands;
    for (const std::shared_ptr<composition::Command>& command : commands) {
      if (command->kind() == composition::Command::Kind::button &&
          command->pad_button() == button && !holds(*command, button)) {
        return Binding{.command = command,
                       .activation = std::nullopt,
                       .held = held,
                       .carry = std::nullopt,
                       .source = std::nullopt};
      }
    }
    if (button == options_.move && node->get().interactions().drag != nullptr) {
      return Binding{.command = nullptr,
                     .activation = std::nullopt,
                     .held = nullptr,
                     .carry = Carry::pick,
                     .source = id};
    }
    if (focused == id && button == options_.layout.confirm() &&
        node->get().content().activatable()) {
      return Binding{.command = nullptr,
                     .activation = id,
                     .held = held,
                     .carry = std::nullopt,
                     .source = std::nullopt};
    }
    for (const std::shared_ptr<composition::Command>& command : commands) {
      if (command->kind() != composition::Command::Kind::button &&
          button_for(*command) == button) {
        return Binding{.command = command,
                       .activation = std::nullopt,
                       .held = held,
                       .carry = std::nullopt,
                       .source = std::nullopt};
      }
    }
  }
  if (held != nullptr) {
    return Binding{.command = nullptr,
                   .activation = std::nullopt,
                   .held = held,
                   .carry = std::nullopt,
                   .source = std::nullopt};
  }
  return std::nullopt;
}

auto InputRouter::rank(provider::Button button) const noexcept -> std::size_t {
  const std::array order{
      options_.layout.confirm(),    options_.layout.back(),
      provider::Button::x,          provider::Button::y,
      provider::Button::l,          provider::Button::r,
      provider::Button::zl,         provider::Button::zr,
      provider::Button::plus,       provider::Button::minus,
      provider::Button::left_stick, provider::Button::right_stick};
  const auto found = std::ranges::find(order, button);
  return static_cast<std::size_t>(found - order.begin());
}

auto InputRouter::describe(const composition::Tree& tree) -> bool {
  described_.clear();
  provider::Buttons seen;
  const std::optional<composition::NodeId> focused = engine_.focused();
  const auto add = [&](provider::Button button, std::string_view label,
                       bool shown) {
    if (seen.contains(button)) {
      return;
    }
    seen = seen.with(button);
    if (shown) {
      described_.push_back(Described{.button = button,
                                     .label = label,
                                     .rank = rank(button),
                                     .order = described_.size()});
    }
  };
  if (drag_.active() && drag_.origin() == DragSession::Origin::controller) {
    add(options_.move, DragSession::drop_label, true);
    add(options_.layout.confirm(), DragSession::drop_label, false);
    add(options_.layout.back(), DragSession::cancel_label, true);
  }
  for (const composition::NodeId id : chain(tree)) {
    const auto node = tree.node(id);
    if (!node.has_value() || node->get().interactions().disabled) {
      continue;
    }
    const auto& commands = node->get().interactions().commands;
    for (const std::shared_ptr<composition::Command>& command : commands) {
      if (command->kind() == composition::Command::Kind::button &&
          !(command->options().hidden &&
            holds(*command, *command->pad_button()))) {
        add(*command->pad_button(), command->label(),
            !command->options().hidden);
      }
    }
    if (node->get().interactions().drag != nullptr) {
      add(options_.move, DragSession::pick_label, true);
    }
    if (focused == id && node->get().content().activatable()) {
      add(options_.layout.confirm(), options_.confirm_label, true);
    }
    for (const std::shared_ptr<composition::Command>& command : commands) {
      if (command->kind() == composition::Command::Kind::button) {
        continue;
      }
      if (const auto button = button_for(*command)) {
        add(*button, command->label(), !command->options().hidden);
      }
    }
  }
  std::ranges::sort(described_, {}, [](const Described& hint) {
    return std::pair{hint.rank, hint.order};
  });
  const bool same = std::ranges::equal(
      described_, hints_,
      [](const Described& fresh, const composition::CommandHint& held) {
        return fresh.button == held.button && fresh.label == held.label;
      });
  if (same) {
    return false;
  }
  hints_.clear();
  for (const Described& fresh : described_) {
    hints_.push_back(composition::CommandHint{
        .button = fresh.button, .label = std::string{fresh.label}});
  }
  return true;
}

auto InputRouter::update(const provider::InputFrame& input,
                         composition::Tree& tree, provider::SurfaceScale scale,
                         std::chrono::nanoseconds now,
                         const provider::NavigationTuning& tuning) -> Outcome {
  Outcome outcome;
  const std::optional<composition::NodeId> before = engine_.focused();
  arriving_ = modality_;
  type(tree, input, now, outcome);
  track_modifiers(tree, outcome);
  const Gamepad pad = merge(input);
  sense(input, pad, tuning);
  outcome.changed = modality_ != arriving_;
  tilt_ = pad.right;
  const FocusEngine::Synchronisation synchronised = engine_.synchronise(tree);
  outcome.changed = outcome.changed || synchronised.changed;
  if (synchronised.deferred == FocusEngine::MoveOutcome::moved) {
    outcome.sounds.push_back(provider::SoundRole::focus_change);
    feel(tree, composition::SensoryFeedback::selection);
  } else if (synchronised.deferred == FocusEngine::MoveOutcome::blocked ||
             synchronised.deferred == FocusEngine::MoveOutcome::held) {
    blocked(tree, last_move_, now, outcome);
  }
  if (pressed_.has_value() && !tree.node(*pressed_).has_value()) {
    pressed_.reset();
    outcome.changed = true;
  }
  scrolls_.synchronise(tree);
  {
    ScrollCoordinator::Report requested;
    scrolls_.requests(tree, now, requested);
    outcome.changed = outcome.changed || requested.changed;
  }
  steer(tree, pad, now, tuning, outcome);
  buttons(tree, pad, now, tuning, outcome);
  touch_focus_ = false;
  touches(tree, input, scale, now, outcome);
  hover(tree, input, scale, outcome);
  if (drag_.active() && drag_.origin() == DragSession::Origin::controller) {
    const std::optional<DragSession::View> before_follow = drag_.view();
    drag_.follow(tree, engine_.focused(), outcome.invocations);
    if (before_follow.has_value() && drag_.view().has_value() &&
        before_follow->revision != drag_.view()->revision) {
      outcome.changed = true;
    }
  }
  if (drag_.active()) {
    const std::optional<composition::NodeId> spring = drag_.spring_target();
    drag_.elapse(now, outcome.invocations);
    if (spring.has_value() && drag_.spring_target() == spring &&
        !outcome.invocations.empty()) {
      outcome.changed = true;
    }
  }
  scroll(tree, pad, before, now, tuning, outcome);
  if (shake_.has_value() && now - shake_->started >= shake_duration) {
    shake_.reset();
  }
  if (describe(tree)) {
    outcome.changed = true;
  }
  previous_buttons_ = pad.physical;
  return outcome;
}

}
