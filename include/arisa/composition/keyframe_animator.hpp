#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/keyframe_repeat.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/reduce_motion.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/keyframe_timeline.hpp>
#include <arisa/motion/keyframes.hpp>

#include <chrono>
#include <cmath>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

template <class Value, std::equality_comparable Trigger>
class KeyframeAnimator : public Component {
 public:
  using Content = std::function<Element(const Value&)>;
  using Builder = std::function<motion::Keyframes<Value>(const Value&)>;

  KeyframeAnimator(Value initial_value, Trigger trigger, Content content,
                   Builder keyframes)
      : initial_(std::move(initial_value)),
        trigger_(std::move(trigger)),
        content_(std::move(content)),
        keyframes_(std::move(keyframes)) {}

  auto operator==(const KeyframeAnimator&) const -> bool { return false; }

  auto body(Context& context) const -> Element override {
    if constexpr (std::same_as<Trigger, KeyframeRepeat>) {
      return repeat(context);
    } else {
      return play(context);
    }
  }

 private:
  struct Playback {
    std::optional<Trigger> trigger;
    std::optional<Value> settled;
    std::optional<motion::KeyframeTimeline<Value>> timeline;
    std::chrono::nanoseconds start{0};
  };

  static inline const StateKey<Playback> playback{"keyframe_animator"};

  auto repeat(Context& context) const -> Element {
    if (trigger_.repeating() && context.environment(ReduceMotion::key)) {
      return content_(initial_);
    }
    Playback& playing = context.retained(playback);
    const std::chrono::nanoseconds now = context.frame_time();
    if (!playing.trigger.has_value()) {
      playing.trigger = trigger_;
      playing.timeline.emplace(initial_, keyframes_(initial_));
      playing.start = now;
    }
    if (!playing.timeline.has_value()) {
      return content_(playing.settled.value_or(initial_));
    }
    const double duration = playing.timeline->duration();
    double time = elapsed(playing, now);
    if (duration <= 0.0) {
      time = 0.0;
    } else if (trigger_.repeating()) {
      time = std::fmod(time, duration);
      context.request_frame();
    } else if (time < duration) {
      context.request_frame();
    } else {
      playing.settled = playing.timeline->value(duration);
      playing.timeline.reset();
      return content_(*playing.settled);
    }
    return content_(playing.timeline->value(time));
  }

  auto play(Context& context) const -> Element {
    Playback& playing = context.retained(playback);
    const std::chrono::nanoseconds now = context.frame_time();
    if (!playing.trigger.has_value()) {
      playing.trigger = trigger_;
      playing.settled = initial_;
    } else if (!(*playing.trigger == trigger_)) {
      playing.trigger = trigger_;
      const Value current = shown(playing, now);
      playing.timeline.emplace(current, keyframes_(current));
      playing.start = now;
    }
    const Value value = shown(playing, now);
    if (playing.timeline.has_value()) {
      if (elapsed(playing, now) < playing.timeline->duration()) {
        context.request_frame();
      } else {
        playing.settled = value;
        playing.timeline.reset();
      }
    }
    return content_(value);
  }

  static auto elapsed(const Playback& playing, std::chrono::nanoseconds now)
      -> double {
    return std::chrono::duration<double>(now - playing.start).count();
  }

  auto shown(const Playback& playing, std::chrono::nanoseconds now) const
      -> Value {
    if (playing.timeline.has_value()) {
      return playing.timeline->value(elapsed(playing, now));
    }
    return playing.settled.value_or(initial_);
  }

  Value initial_;
  Trigger trigger_;
  Content content_;
  Builder keyframes_;
};

template <class Value, class Trigger, class Content, class Builder>
KeyframeAnimator(Value, Trigger, Content, Builder)
    -> KeyframeAnimator<Value, Trigger>;

template <class Value, std::equality_comparable Trigger, class Content,
          class Builder>
auto Element::keyframe_animator(Value initial_value, Trigger trigger,
                                Content content,
                                Builder keyframes) && -> Element {
  std::optional<ElementKey> key = std::exchange(key_, std::nullopt);
  auto source =
      std::make_shared<PlaceholderContentView::Source>(std::move(*this));
  Element animated{KeyframeAnimator<Value, Trigger>{
      std::move(initial_value), std::move(trigger),
      [content = std::move(content), source](const Value& value) {
        return Element{content(Element{PlaceholderContentView{source}}, value)};
      },
      std::move(keyframes)}};
  if (key.has_value()) {
    return std::move(animated).id(std::move(*key));
  }
  return animated;
}

}
