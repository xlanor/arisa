#pragma once

#include <arisa/provider/input/navigation_tuning.hpp>

#include <boost/sml.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace arisa::interaction {

template <class... Policies>
class BasicPressTracker {
 public:
  struct Events {
    bool pressed{false};
    bool released{false};
    std::uint32_t repeats{0};
    bool long_press{false};
  };

  auto update(bool held, std::chrono::nanoseconds now,
              const provider::NavigationTuning& tuning) -> Events {
    timing_.events = Events{};
    timing_.tuning = tuning;
    if (held && !this->held()) {
      machine_.process_event(Press{.timing = &timing_, .now = now});
    } else if (!held && this->held()) {
      machine_.process_event(Release{.timing = &timing_});
    }
    if (held) {
      machine_.process_event(Elapse{.timing = &timing_, .now = now});
    }
    return timing_.events;
  }

  auto held() const noexcept -> bool {
    return !machine_.is(boost::sml::state<Idle>);
  }

  auto repeating() const noexcept -> bool {
    return machine_.is(boost::sml::state<Repeating>);
  }

 private:
  struct Idle {};
  struct Pressed {};
  struct Repeating {};

  struct Timing {
    std::chrono::nanoseconds pressed_at{0};
    std::chrono::nanoseconds next{0};
    std::chrono::nanoseconds interval{0};
    bool long_fired{false};
    Events events{};
    provider::NavigationTuning tuning{};
  };

  struct Press {
    Timing* timing{nullptr};
    std::chrono::nanoseconds now{0};
  };

  struct Release {
    Timing* timing{nullptr};
  };

  struct Elapse {
    Timing* timing{nullptr};
    std::chrono::nanoseconds now{0};
  };

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      const auto start = [](const Press& press) {
        Timing& timing = *press.timing;
        timing.pressed_at = press.now;
        timing.next = press.now + timing.tuning.repeat_delay;
        timing.interval = timing.tuning.repeat_interval;
        timing.long_fired = false;
        timing.events.pressed = true;
      };
      const auto finish = [](const Release& release) {
        release.timing->events.released = true;
      };
      const auto due = [](const Elapse& elapse) {
        return elapse.now >= elapse.timing->next;
      };
      const auto advance = [](const Elapse& elapse) {
        Timing& timing = *elapse.timing;
        if (!timing.long_fired &&
            elapse.now - timing.pressed_at >= timing.tuning.long_press) {
          timing.long_fired = true;
          timing.events.long_press = true;
        }
        while (timing.next <= elapse.now) {
          ++timing.events.repeats;
          timing.next += timing.interval;
          const auto shortened =
              std::chrono::duration_cast<std::chrono::nanoseconds>(
                  timing.interval *
                  static_cast<double>(timing.tuning.repeat_acceleration));
          timing.interval = std::max(
              shortened, std::chrono::duration_cast<std::chrono::nanoseconds>(
                             timing.tuning.repeat_minimum));
        }
      };
      return boost::sml::make_transition_table(
          *state<Idle> + event<Press> / start = state<Pressed>,
          state<Pressed> + event<Release> / finish = state<Idle>,
          state<Pressed> + event<Elapse>[due] / advance = state<Repeating>,
          state<Pressed> + event<Elapse> / advance,
          state<Repeating> + event<Elapse> / advance,
          state<Repeating> + event<Release> / finish = state<Idle>);
    }
  };

  Timing timing_;
  boost::sml::sm<Table, Policies...> machine_;
};

using PressTracker = BasicPressTracker<>;

}
