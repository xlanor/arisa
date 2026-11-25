#pragma once

#include <arisa/composition/point.hpp>

#include <interaction/recognizer_signals.hpp>

#include <boost/sml.hpp>

#include <chrono>
#include <cmath>

namespace arisa::interaction {

template <class... Policies>
class BasicLongPressRecognizer {
 public:
  BasicLongPressRecognizer(std::chrono::nanoseconds minimum_duration,
                           float maximum_distance) noexcept {
    tracking_.duration = minimum_duration;
    tracking_.distance = maximum_distance;
  }

  auto begin(composition::Point point, std::chrono::nanoseconds now)
      -> RecognizerSignals {
    RecognizerSignals signals =
        process(Down{.tracking = &tracking_, .point = point, .now = now});
    if (!signals.failed) {
      signals.pressing = true;
      signals.changed = true;
    }
    RecognizerSignals settled = elapse(now);
    settled.pressing =
        settled.pressing.has_value() ? settled.pressing : signals.pressing;
    settled.changed = settled.changed || signals.changed;
    return settled;
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

  auto pressing() const noexcept -> bool {
    return machine_.is(boost::sml::state<Pressing>);
  }

  auto waiting() const noexcept -> bool { return false; }

 private:
  struct Idle {};
  struct Pressing {};
  struct Ended {};
  struct Failed {};

  struct Tracking {
    std::chrono::nanoseconds duration{0};
    float distance{0.0F};
    composition::Point first{};
    std::chrono::nanoseconds started{0};
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
        down.tracking->started = down.now;
      };
      const auto strayed = [](const Move& move) {
        return std::hypot(move.point.x - move.tracking->first.x,
                          move.point.y - move.tracking->first.y) >
               move.tracking->distance;
      };
      const auto due = [](const Elapse& elapse) {
        return elapse.now - elapse.tracking->started >=
               elapse.tracking->duration;
      };
      return boost::sml::make_transition_table(
          *state<Idle> + event<Down> / start = state<Pressing>,
          state<Idle> + event<Cancel> = state<Failed>,
          state<Pressing> + event<Move>[strayed] = state<Failed>,
          state<Pressing> + event<Up> = state<Failed>,
          state<Pressing> + event<Elapse>[due] = state<Ended>,
          state<Pressing> + event<Cancel> = state<Failed>);
    }
  };

  template <class Event>
  auto process(const Event& event) -> RecognizerSignals {
    const bool before = pressing();
    const bool live = possible();
    machine_.process_event(event);
    RecognizerSignals signals;
    signals.failed = live && machine_.is(boost::sml::state<Failed>);
    signals.claimed = live && recognized();
    signals.ended = signals.claimed;
    if (before && !pressing()) {
      signals.pressing = false;
    }
    return signals;
  }

  Tracking tracking_;
  boost::sml::sm<Table, Policies...> machine_;
};

using LongPressRecognizer = BasicLongPressRecognizer<>;

}
