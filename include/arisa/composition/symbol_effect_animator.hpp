#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/image_environment.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/reduce_motion.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/symbol_effect.hpp>

#include <chrono>
#include <cmath>
#include <concepts>
#include <memory>
#include <optional>
#include <utility>

namespace arisa::composition {

template <std::equality_comparable Trigger>
class SymbolEffectAnimator : public Component {
 public:
  SymbolEffectAnimator(SymbolEffect effect, Trigger trigger,
                       SymbolEffect::Playback mode,
                       std::shared_ptr<PlaceholderContentView::Source> source)
      : effect_(effect),
        trigger_(std::move(trigger)),
        mode_(mode),
        source_(std::move(source)) {}

  SymbolEffectAnimator(SymbolEffect effect, Trigger trigger,
                       std::shared_ptr<PlaceholderContentView::Source> source,
                       std::optional<ElementKey> key)
      : effect_(effect),
        trigger_(std::move(trigger)),
        mode_(SymbolEffect::Playback::presented),
        source_(std::move(source)),
        key_(std::move(key)) {}

  auto operator==(const SymbolEffectAnimator& other) const -> bool {
    return effect_ == other.effect_ && trigger_ == other.trigger_ &&
           mode_ == other.mode_ && source_ == other.source_ &&
           key_ == other.key_;
  }

  auto body(Context& context) const -> Element override {
    if (context.environment(ImageEnvironment::symbol_effects_removed)) {
      return keyed(Element{PlaceholderContentView{source_}});
    }
    if (mode_ == SymbolEffect::Playback::presented) {
      return presented();
    }
    Playback& playing = context.retained(playback);
    const std::chrono::nanoseconds now = context.frame_time();
    const bool reduced = context.environment(ReduceMotion::key) && moves();
    SymbolEffect::Pose pose;
    if (mode_ == SymbolEffect::Playback::endless) {
      if (active() && !reduced) {
        if (!playing.start.has_value()) {
          playing.start = now;
        }
        const double length = effect_.cycle() / effect_.speed_factor();
        const double local = std::fmod(seconds(now - *playing.start), length);
        pose = effect_.pose(local);
        context.request_frame();
      } else {
        playing.start.reset();
      }
    } else {
      if (!playing.trigger.has_value()) {
        playing.trigger = trigger_;
      } else if (!(*playing.trigger == trigger_)) {
        playing.trigger = trigger_;
        playing.start = now;
      }
      if (playing.start.has_value() && !reduced) {
        const double elapsed = seconds(now - *playing.start);
        if (elapsed < effect_.duration()) {
          pose = effect_.pose(elapsed);
          context.request_frame();
        } else {
          playing.start.reset();
        }
      } else {
        playing.start.reset();
      }
    }
    return SymbolEffect::posed(Element{PlaceholderContentView{source_}}, pose);
  }

 private:
  struct Playback {
    std::optional<Trigger> trigger;
    std::optional<std::chrono::nanoseconds> start;
  };

  static inline const StateKey<Playback> playback{"symbol_effect"};

  auto presented() const -> Element {
    const bool shows = active();
    const bool hides = effect_.kind() == SymbolEffect::Kind::appear ||
                       effect_.kind() == SymbolEffect::Kind::replace;
    SymbolEffect::Pose pose =
        shows ? effect_.active_pose() : SymbolEffect::Pose{};
    if (hides && !shows) {
      pose = SymbolEffect::disappear().active_pose();
    }
    const std::optional<motion::Animation> animation =
        shows && !hides ? effect_.active_animation()
                        : motion::Animation::smooth(0.35);
    return keyed(Element{PlaceholderContentView{source_}}.animation(
        animation, [&pose](Element content) {
          return SymbolEffect::posed(std::move(content), pose);
        }));
  }

  auto keyed(Element element) const -> Element {
    if (key_.has_value()) {
      return std::move(element).id(*key_);
    }
    return element;
  }

  static auto seconds(std::chrono::nanoseconds span) -> double {
    return std::chrono::duration<double>(span).count();
  }

  auto active() const -> bool {
    if constexpr (std::same_as<Trigger, bool>) {
      return trigger_;
    } else {
      return true;
    }
  }

  auto moves() const -> bool {
    using Kind = SymbolEffect::Kind;
    const Kind kind = effect_.kind();
    return kind != Kind::pulse && kind != Kind::variable_color;
  }

  SymbolEffect effect_;
  Trigger trigger_;
  SymbolEffect::Playback mode_{SymbolEffect::Playback::triggered};
  std::shared_ptr<PlaceholderContentView::Source> source_;
  std::optional<ElementKey> key_;
};

template <std::equality_comparable T>
  requires(!std::same_as<T, bool>)
auto Element::symbol_effect(SymbolEffect effect, T value) && -> Element {
  std::optional<ElementKey> key = std::exchange(key_, std::nullopt);
  auto source =
      std::make_shared<PlaceholderContentView::Source>(std::move(*this));
  Element animated{SymbolEffectAnimator<T>{effect, std::move(value),
                                           SymbolEffect::Playback::triggered,
                                           std::move(source)}};
  if (key.has_value()) {
    return std::move(animated).id(std::move(*key));
  }
  return animated;
}

}
