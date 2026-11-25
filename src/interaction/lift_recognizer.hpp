#pragma once

#include <arisa/composition/point.hpp>

#include <interaction/recognizer_signals.hpp>

#include <boost/sml.hpp>

#include <chrono>
#include <cmath>

namespace arisa::interaction {

template <class... Policies>
class BasicLiftRecognizer {
 public:
  BasicLiftRecognizer(std::chrono::nanoseconds minimum_duration,
                      float maximum_distance) noexcept {
    tracking_.duration = minimum_duration;
    tracking_.distance = maximum_distance;
  }

  auto begin(composition::Point point, std::chrono::nanoseconds now)
      -> RecognizerSignals {
    return process(Down{.tracking = &tracking_, .point = point, .now = now});
  }

  auto move(composition::Point point, std::chrono::nanoseconds now)
      -> RecognizerSignals {
    return process(Move{.tracking = &tracking_, .point = point, .now = now});
  }

  auto end(composition::Point, std::chrono::nanoseconds) -> RecognizerSignals {
    return process(Up{});
  }

  auto elapse(std::chrono::nanoseconds now) -> RecognizerSignals {
    return process(Elapse{.tracking = &tracking_, .now = now});
  }

  auto cancel() -> RecognizerSignals { return process(Cancel{}); }

  auto possible() const noexcept -> bool {
    return !machine_.is(boost::sml::state<Ended>) &&
           !machine_.is(boost::sml::state<Failed>) &&
           !machine_.is(boost::sml::state<Cancelled>);
  }

  auto waiting() const noexcept -> bool { return false; }

  auto lifted() const noexcept -> bool {
    return machine_.is(boost::sml::state<Lifted>);
  }

 private:
  struct Idle {};
  struct Pressing {};
  struct Lifted {};
  struct Ended {};
  struct Cancelled {};
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

  struct Elapse {
    Tracking* tracking{nullptr};
    std::chrono::nanoseconds now{0};
  };

  struct Up {};
  struct Cancel {};

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
          state<Pressing> + event<Elapse>[due] = state<Lifted>,
          state<Pressing> + event<Cancel> = state<Failed>,
          state<Lifted> + event<Move> = state<Lifted>,
          state<Lifted> + event<Up> = state<Ended>,
          state<Lifted> + event<Cancel> = state<Cancelled>);
    }
  };

  template <class Event>
  auto process(const Event& event) -> RecognizerSignals {
    const bool was = lifted();
    const bool live = possible();
    machine_.process_event(event);
    RecognizerSignals signals;
    signals.failed = live && machine_.is(boost::sml::state<Failed>);
    signals.claimed = !was && lifted();
    signals.changed = was && lifted();
    signals.ended = was && machine_.is(boost::sml::state<Ended>);
    return signals;
  }

  Tracking tracking_;
  boost::sml::sm<Table, Policies...> machine_;
};

using LiftRecognizer = BasicLiftRecognizer<>;

}
