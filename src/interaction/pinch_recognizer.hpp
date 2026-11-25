#pragma once

#include <arisa/composition/point.hpp>

#include <interaction/recognizer_signals.hpp>

#include <boost/sml.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <numbers>

namespace arisa::interaction {

template <class... Policies>
class BasicPinchRecognizer {
 public:
  enum class Measure : std::uint8_t {
    scale,
    angle,
  };

  struct Sample {
    std::chrono::nanoseconds time{0};
    double magnification{1.0};
    double rotation{0.0};
    double scale_velocity{0.0};
    double angle_velocity{0.0};
    composition::Point anchor{};
  };

  BasicPinchRecognizer(Measure measure, double threshold) noexcept {
    tracking_.measure = measure;
    tracking_.threshold = threshold;
  }

  auto begin(composition::Point, std::chrono::nanoseconds)
      -> RecognizerSignals {
    return process(Down{});
  }

  auto move(composition::Point, std::chrono::nanoseconds) -> RecognizerSignals {
    return process(Single{});
  }

  auto pair(composition::Point first, composition::Point second,
            std::chrono::nanoseconds now) -> RecognizerSignals {
    return process(Pair{
        .tracking = &tracking_, .first = first, .second = second, .now = now});
  }

  auto end(composition::Point, std::chrono::nanoseconds) -> RecognizerSignals {
    return process(Up{});
  }

  auto elapse(std::chrono::nanoseconds) -> RecognizerSignals { return {}; }

  auto cancel() -> RecognizerSignals { return process(Cancel{}); }

  auto possible() const noexcept -> bool {
    return !machine_.is(boost::sml::state<Ended>) &&
           !machine_.is(boost::sml::state<Failed>) &&
           !machine_.is(boost::sml::state<Cancelled>);
  }

  auto waiting() const noexcept -> bool { return false; }

  auto pinching() const noexcept -> bool {
    return machine_.is(boost::sml::state<Pinching>);
  }

  auto sample() const noexcept -> const Sample& { return tracking_.sample; }

 private:
  struct Idle {};
  struct Waiting {};
  struct Tracking {};
  struct Pinching {};
  struct Ended {};
  struct Cancelled {};
  struct Failed {};

  struct Track {
    Measure measure{Measure::scale};
    double threshold{0.0};
    double distance{0.0};
    double angle{0.0};
    double last_angle{0.0};
    double turned{0.0};
    Sample sample{};
  };

  struct Down {};
  struct Single {};
  struct Up {};
  struct Cancel {};
  struct Claim {};

  struct Pair {
    Track* tracking{nullptr};
    composition::Point first{};
    composition::Point second{};
    std::chrono::nanoseconds now{0};
  };

  static auto span(const Pair& pair) noexcept -> double {
    return std::hypot(static_cast<double>(pair.second.x - pair.first.x),
                      static_cast<double>(pair.second.y - pair.first.y));
  }

  static auto heading(const Pair& pair) noexcept -> double {
    return std::atan2(static_cast<double>(pair.second.y - pair.first.y),
                      static_cast<double>(pair.second.x - pair.first.x));
  }

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      const auto start = [](const Pair& pair) {
        Track& track = *pair.tracking;
        track.distance = std::max(span(pair), 1.0);
        track.angle = heading(pair);
        track.last_angle = track.angle;
        track.turned = 0.0;
        track.sample = Sample{.time = pair.now,
                              .magnification = 1.0,
                              .rotation = 0.0,
                              .scale_velocity = 0.0,
                              .angle_velocity = 0.0,
                              .anchor = composition::Point{
                                  .x = (pair.first.x + pair.second.x) * 0.5F,
                                  .y = (pair.first.y + pair.second.y) * 0.5F}};
      };
      const auto measure = [](const Pair& pair) {
        Track& track = *pair.tracking;
        const double current = heading(pair);
        double delta = current - track.last_angle;
        while (delta > std::numbers::pi) {
          delta -= 2.0 * std::numbers::pi;
        }
        while (delta < -std::numbers::pi) {
          delta += 2.0 * std::numbers::pi;
        }
        track.last_angle = current;
        track.turned += delta;
        const double magnification = span(pair) / track.distance;
        const double seconds =
            std::chrono::duration<double>(pair.now - track.sample.time).count();
        if (seconds > 0.0) {
          track.sample.scale_velocity =
              (magnification - track.sample.magnification) / seconds;
          track.sample.angle_velocity =
              (track.turned - track.sample.rotation) / seconds;
        }
        track.sample.time = pair.now;
        track.sample.magnification = magnification;
        track.sample.rotation = track.turned;
      };
      return boost::sml::make_transition_table(
          *state<Idle> + event<Down> = state<Waiting>,
          state<Idle> + event<Pair> / start = state<Tracking>,
          state<Idle> + event<Cancel> = state<Failed>,
          state<Waiting> + event<Pair> / start = state<Tracking>,
          state<Waiting> + event<Up> = state<Failed>,
          state<Waiting> + event<Cancel> = state<Failed>,
          state<Tracking> + event<Pair> / measure = state<Tracking>,
          state<Tracking> + event<Single> = state<Failed>,
          state<Tracking> + event<Up> = state<Failed>,
          state<Tracking> + event<Cancel> = state<Failed>,
          state<Pinching> + event<Pair> / measure = state<Pinching>,
          state<Pinching> + event<Single> = state<Ended>,
          state<Pinching> + event<Up> = state<Ended>,
          state<Pinching> + event<Cancel> = state<Cancelled>,
          state<Tracking> + event<Claim> = state<Pinching>);
    }
  };

  template <class Event>
  auto process(const Event& event) -> RecognizerSignals {
    const bool was = pinching();
    const bool live = possible();
    machine_.process_event(event);
    if constexpr (std::same_as<Event, Pair>) {
      if (machine_.is(boost::sml::state<Tracking>) && crosses()) {
        machine_.process_event(Claim{});
      }
    }
    RecognizerSignals signals;
    signals.failed = live && machine_.is(boost::sml::state<Failed>);
    signals.claimed = !was && pinching();
    signals.changed = pinching() && std::same_as<Event, Pair>;
    signals.ended = was && machine_.is(boost::sml::state<Ended>);
    return signals;
  }

  auto crosses() const noexcept -> bool {
    if (tracking_.measure == Measure::scale) {
      return std::abs(tracking_.sample.magnification - 1.0) >=
             tracking_.threshold;
    }
    return std::abs(tracking_.sample.rotation) >= tracking_.threshold;
  }

  Track tracking_;
  boost::sml::sm<Table, Policies...> machine_;
};

using PinchRecognizer = BasicPinchRecognizer<>;

}
