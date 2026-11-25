#pragma once

#include <arisa/async/job.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node_anchor.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/reduce_motion.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace arisa::composition {

class PhaseTrigger {
 public:
  friend constexpr auto operator==(PhaseTrigger, PhaseTrigger)
      -> bool = default;
};

template <std::equality_comparable Phase,
          std::equality_comparable Trigger = PhaseTrigger>
class PhaseAnimator : public Component {
 public:
  using Content = std::function<Element(const Phase&)>;
  using Animate = std::function<std::optional<motion::Animation>(const Phase&)>;

  PhaseAnimator(std::vector<Phase> phases, Content content,
                Animate animation = default_animation())
    requires std::same_as<Trigger, PhaseTrigger>
      : phases_(std::move(phases)),
        content_(std::move(content)),
        animation_(std::move(animation)) {
    assert(!phases_.empty());
  }

  PhaseAnimator(std::vector<Phase> phases, Trigger trigger, Content content,
                Animate animation = default_animation())
      : phases_(std::move(phases)),
        trigger_(std::move(trigger)),
        content_(std::move(content)),
        animation_(std::move(animation)) {
    assert(!phases_.empty());
  }

  auto operator==(const PhaseAnimator&) const -> bool { return false; }

  static auto default_animation() -> Animate {
    return [](const Phase&) -> std::optional<motion::Animation> {
      return motion::Animation::default_animation();
    };
  }

  auto body(Context& context) const -> Element override {
    Playback& playing = context.retained(playback);
    const bool repeating = !trigger_.has_value();
    if (!playing.started) {
      playing.started = true;
      playing.trigger = trigger_;
      playing.index = 0;
      if (repeating && !context.environment(ReduceMotion::key)) {
        playing.remaining = endless;
        schedule(context, playing, std::chrono::nanoseconds{0});
      }
    } else if (!repeating && !(*playing.trigger == *trigger_)) {
      playing.trigger = trigger_;
      playing.remaining = phases_.size() - playing.index;
      if (playing.wake == nullptr) {
        schedule(context, playing, std::chrono::nanoseconds{0});
      }
    } else if (playing.wake == nullptr && playing.remaining > 0) {
      schedule(context, playing, held(playing));
    }
    playing.index = std::min(playing.index, phases_.size() - 1);
    return content_(phases_[playing.index]);
  }

 private:
  static constexpr std::size_t endless = static_cast<std::size_t>(-1);

  struct Playback {
    bool started{false};
    std::size_t index{0};
    std::size_t remaining{0};
    std::optional<Trigger> trigger;
    std::optional<motion::Animation> arrived;
    std::shared_ptr<async::Job> wake;
  };

  static inline const StateKey<Playback> playback{"phase_animator"};

  static auto held(const Playback& playing) -> std::chrono::nanoseconds {
    double seconds = motion::Animation::default_duration;
    if (playing.arrived.has_value() &&
        std::isfinite(playing.arrived->logical_duration())) {
      seconds = playing.arrived->logical_duration();
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(seconds));
  }

  void schedule(Context& context, Playback& playing,
                std::chrono::nanoseconds delay) const {
    const std::size_t next = (playing.index + 1) % phases_.size();
    std::optional<motion::Animation> animation = animation_(phases_[next]);
    std::weak_ptr<const NodeAnchor> anchor = context.node_anchor();
    Playback* state = &playing;
    playing.wake =
        std::make_shared<async::Job>(context.services().frames.get().after(
            delay, [state, next, animation = std::move(animation),
                    anchor = std::move(anchor)](std::stop_token) mutable {
              const std::shared_ptr<const NodeAnchor> alive = anchor.lock();
              if (alive == nullptr) {
                return;
              }
              state->index = next;
              state->arrived = animation;
              if (state->remaining != endless) {
                --state->remaining;
              }
              std::shared_ptr<async::Job> finished = std::move(state->wake);
              motion::with_animation(std::move(animation),
                                     [&alive] { alive->mark_dirty(); });
            }));
  }

  std::vector<Phase> phases_;
  std::optional<Trigger> trigger_;
  Content content_;
  Animate animation_;
};

template <std::equality_comparable Phase, std::equality_comparable Trigger,
          class Content, class Animate>
  requires std::invocable<Content&, Element, const Phase&>
auto Element::phase_animator(std::vector<Phase> phases, Trigger trigger,
                             Content content, Animate animation) && -> Element {
  std::optional<ElementKey> key = std::exchange(key_, std::nullopt);
  auto source =
      std::make_shared<PlaceholderContentView::Source>(std::move(*this));
  Element animated{PhaseAnimator<Phase, Trigger>{
      std::move(phases), std::move(trigger),
      [content = std::move(content), source](const Phase& phase) {
        return Element{content(Element{PlaceholderContentView{source}}, phase)};
      },
      [animation = std::move(animation)](const Phase& phase)
          -> std::optional<motion::Animation> { return animation(phase); }}};
  if (key.has_value()) {
    return std::move(animated).id(std::move(*key));
  }
  return animated;
}

template <std::equality_comparable Phase, std::equality_comparable Trigger,
          class Content>
  requires std::invocable<Content&, Element, const Phase&>
auto Element::phase_animator(std::vector<Phase> phases, Trigger trigger,
                             Content content) && -> Element {
  return std::move(*this).phase_animator(
      std::move(phases), std::move(trigger), std::move(content),
      [](const Phase&) -> std::optional<motion::Animation> {
        return motion::Animation::default_animation();
      });
}

template <std::equality_comparable Phase, class Content, class Animate>
  requires std::invocable<Content&, Element, const Phase&>
auto Element::phase_animator(std::vector<Phase> phases, Content content,
                             Animate animation) && -> Element {
  std::optional<ElementKey> key = std::exchange(key_, std::nullopt);
  auto source =
      std::make_shared<PlaceholderContentView::Source>(std::move(*this));
  Element animated{PhaseAnimator<Phase>{
      std::move(phases),
      [content = std::move(content), source](const Phase& phase) {
        return Element{content(Element{PlaceholderContentView{source}}, phase)};
      },
      [animation = std::move(animation)](const Phase& phase)
          -> std::optional<motion::Animation> { return animation(phase); }}};
  if (key.has_value()) {
    return std::move(animated).id(std::move(*key));
  }
  return animated;
}

template <std::equality_comparable Phase, class Content>
  requires std::invocable<Content&, Element, const Phase&>
auto Element::phase_animator(std::vector<Phase> phases,
                             Content content) && -> Element {
  return std::move(*this).phase_animator(
      std::move(phases), std::move(content),
      [](const Phase&) -> std::optional<motion::Animation> {
        return motion::Animation::default_animation();
      });
}

}
