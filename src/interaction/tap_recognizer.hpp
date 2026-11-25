#pragma once

#include <arisa/composition/point.hpp>

#include <interaction/recognizer_signals.hpp>

#include <boost/sml.hpp>

#include <chrono>
#include <cmath>

namespace arisa::interaction {

template <class... Policies>
class BasicTapRecognizer {
 public:
  static constexpr float default_slop = 10.0F;
  static constexpr float default_spread = 40.0F;
  static constexpr std::chrono::milliseconds default_interval{300};

  explicit BasicTapRecognizer(
      int count = 1, float slop = default_slop,
      std::chrono::nanoseconds interval = default_interval) noexcept {
    tracking_.count = count < 1 ? 1 : count;
    tracking_.slop = slop;
    tracking_.interval = interval;
  }

  auto begin(composition::Point point, std::chrono::nanoseconds now)
      -> RecognizerSignals {
    return process(Down{.tracking = &tracking_, .point = point, .now = now});
  }

  auto move(composition::Point point, std::chrono::nanoseconds now)
      -> RecognizerSignals {
    return process(Move{.tracking = &tracking_, .point = point, .now = now});
  }

  auto end(composition::Point point, std::chrono::nanoseconds now)
      -> RecognizerSignals {
    return process(Up{.tracking = &tracking_, .point = point, .now = now});
  }

  auto elapse(std::chrono::nanoseconds now) -> RecognizerSignals {
    return process(Elapse{.tracking = &tracking_, .now = now});
  }

  auto cancel() -> RecognizerSignals {
    return process(Cancel{.tracking = &tracking_});
  }

  auto possible() const noexcept -> bool {
    return !machine_.is(boost::sml::state<Ended>) &&
           !machine_.is(boost::sml::state<Failed>);
  }

  auto recognized() const noexcept -> bool {
    return machine_.is(boost::sml::state<Ended>);
  }

  auto waiting() const noexcept -> bool {
    return machine_.is(boost::sml::state<Released>);
  }

  auto taps() const noexcept -> int { return tracking_.taps; }

 private:
  struct Idle {};
  struct Pressed {};
  struct Released {};
  struct Ended {};
  struct Failed {};

  struct Tracking {
    int count{1};
    float slop{default_slop};
    std::chrono::nanoseconds interval{default_interval};
    composition::Point first{};
    int taps{0};
    std::chrono::nanoseconds deadline{0};
  };

  struct Down {
    Tracking* tracking{nullptr};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
  };

  struct Move {
    Tracking* tracking{nullptr};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
  };

  struct Up {
    Tracking* tracking{nullptr};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
  };

  struct Elapse {
    Tracking* tracking{nullptr};
    std::chrono::nanoseconds now{0};
  };

  struct Cancel {
    Tracking* tracking{nullptr};
  };

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      const auto start = [](const Down& down) {
        down.tracking->first = down.point;
        down.tracking->taps = 0;
      };
      const auto strayed = [](const Move& move) {
        return distance(move.tracking->first, move.point) > move.tracking->slop;
      };
      const auto away = [](const Up& up) {
        return distance(up.tracking->first, up.point) > up.tracking->slop;
      };
      const auto complete = [](const Up& up) {
        return up.tracking->taps + 1 >= up.tracking->count;
      };
      const auto finish = [](const Up& up) { ++up.tracking->taps; };
      const auto counted = [](const Up& up) {
        ++up.tracking->taps;
        up.tracking->deadline = up.now + up.tracking->interval;
      };
      const auto near = [](const Down& down) {
        return distance(down.tracking->first, down.point) <= default_spread;
      };
      const auto expired = [](const Elapse& elapse) {
        return elapse.now > elapse.tracking->deadline;
      };
      return boost::sml::make_transition_table(
          *state<Idle> + event<Down> / start = state<Pressed>,
          state<Idle> + event<Cancel> = state<Failed>,
          state<Pressed> + event<Move>[strayed] = state<Failed>,
          state<Pressed> + event<Up>[away] = state<Failed>,
          state<Pressed> + event<Up>[complete] / finish = state<Ended>,
          state<Pressed> + event<Up> / counted = state<Released>,
          state<Pressed> + event<Cancel> = state<Failed>,
          state<Released> + event<Down>[near] = state<Pressed>,
          state<Released> + event<Down> = state<Failed>,
          state<Released> + event<Elapse>[expired] = state<Failed>,
          state<Released> + event<Cancel> = state<Failed>);
    }
  };

  static auto distance(composition::Point from, composition::Point to) noexcept
      -> float {
    return std::hypot(to.x - from.x, to.y - from.y);
  }

  template <class Event>
  auto process(const Event& event) -> RecognizerSignals {
    const bool before = possible();
    machine_.process_event(event);
    RecognizerSignals signals;
    signals.failed = before && machine_.is(boost::sml::state<Failed>);
    signals.claimed = before && recognized();
    signals.ended = signals.claimed;
    return signals;
  }

  Tracking tracking_;
  boost::sml::sm<Table, Policies...> machine_;
};

using TapRecognizer = BasicTapRecognizer<>;

}
