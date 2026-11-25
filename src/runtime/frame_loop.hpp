#pragma once

#include <arisa/runtime/frame_timings.hpp>

#include <boost/sml.hpp>

namespace arisa::runtime {

template <class... Policies>
class BasicFrameLoop {
 public:
  struct Signals {
    bool work{false};
    bool animating{false};
    bool exclusive{false};
  };

  auto plan(Signals signals) -> FrameKind {
    const bool switched = signals.exclusive ? enter() : leave();
    if (signals.work || signals.animating || switched) {
      machine_.process_event(Wake{});
      return signals.work || switched ? FrameKind::update
                                      : FrameKind::animation;
    }
    machine_.process_event(Rest{});
    return FrameKind::idle;
  }

  auto hold(bool exclusive) -> FrameKind {
    static_cast<void>(exclusive ? enter() : leave());
    machine_.process_event(Rest{});
    return FrameKind::idle;
  }

  auto wake() -> bool { return machine_.process_event(Wake{}); }

  auto rest() -> bool { return machine_.process_event(Rest{}); }

  auto enter() -> bool { return mode_.process_event(Enter{}); }

  auto leave() -> bool { return mode_.process_event(Leave{}); }

  auto waiting() const noexcept -> bool {
    return machine_.is(boost::sml::state<Waiting>);
  }

  auto exclusive() const noexcept -> bool {
    return mode_.is(boost::sml::state<Exclusive>);
  }

  void start_waiting(bool waiting)
    requires(sizeof...(Policies) > 0)
  {
    if (waiting) {
      machine_.set_current_states(boost::sml::state<Waiting>);
    } else {
      machine_.set_current_states(boost::sml::state<Animating>);
    }
  }

  void start_exclusive(bool exclusive)
    requires(sizeof...(Policies) > 0)
  {
    if (exclusive) {
      mode_.set_current_states(boost::sml::state<Exclusive>);
    } else {
      mode_.set_current_states(boost::sml::state<Shared>);
    }
  }

 private:
  struct Animating {};
  struct Waiting {};
  struct Shared {};
  struct Exclusive {};

  struct Wake {};
  struct Rest {};
  struct Enter {};
  struct Leave {};

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      return boost::sml::make_transition_table(
          *state<Animating> + event<Rest> = state<Waiting>,
          state<Waiting> + event<Wake> = state<Animating>);
    }
  };

  struct Mode {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      return boost::sml::make_transition_table(
          *state<Shared> + event<Enter> = state<Exclusive>,
          state<Exclusive> + event<Leave> = state<Shared>);
    }
  };

  boost::sml::sm<Table, Policies...> machine_;
  boost::sml::sm<Mode, Policies...> mode_;
};

using FrameLoop = BasicFrameLoop<>;

}
