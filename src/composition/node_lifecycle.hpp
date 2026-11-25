#pragma once

#include <arisa/composition/node_lifecycle_phase.hpp>

#include <boost/sml.hpp>

namespace arisa::composition {

template <class... Policies>
class BasicNodeLifecycle {
 public:
  auto appear() -> bool { return machine_.process_event(Appear{}); }

  auto depart() -> bool { return machine_.process_event(Depart{}); }

  auto retire() -> bool { return machine_.process_event(Retire{}); }

  auto abandon() -> bool { return machine_.process_event(Abandon{}); }

  auto phase() const noexcept -> NodeLifecyclePhase {
    if (machine_.is(boost::sml::state<Live>)) {
      return NodeLifecyclePhase::live;
    }
    if (machine_.is(boost::sml::state<Departing>)) {
      return NodeLifecyclePhase::departing;
    }
    if (machine_.is(boost::sml::state<Retired>)) {
      return NodeLifecyclePhase::retired;
    }
    return NodeLifecyclePhase::mounting;
  }

  void start_in(NodeLifecyclePhase phase)
    requires(sizeof...(Policies) > 0)
  {
    switch (phase) {
      case NodeLifecyclePhase::mounting:
        machine_.set_current_states(boost::sml::state<Mounting>);
        return;
      case NodeLifecyclePhase::live:
        machine_.set_current_states(boost::sml::state<Live>);
        return;
      case NodeLifecyclePhase::departing:
        machine_.set_current_states(boost::sml::state<Departing>);
        return;
      case NodeLifecyclePhase::retired:
        machine_.set_current_states(boost::sml::state<Retired>);
        return;
    }
  }

 private:
  struct Mounting {};
  struct Live {};
  struct Departing {};
  struct Retired {};

  struct Appear {};
  struct Depart {};
  struct Retire {};
  struct Abandon {};

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      return boost::sml::make_transition_table(
          *state<Mounting> + event<Appear> = state<Live>,
          state<Mounting> + event<Abandon> = state<Retired>,
          state<Live> + event<Depart> = state<Departing>,
          state<Live> + event<Abandon> = state<Retired>,
          state<Departing> + event<Retire> = state<Retired>,
          state<Departing> + event<Abandon> = state<Retired>);
    }
  };

  boost::sml::sm<Table, Policies...> machine_;
};

using NodeLifecycle = BasicNodeLifecycle<>;

}
