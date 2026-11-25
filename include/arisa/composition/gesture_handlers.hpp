#pragma once

#include <arisa/composition/gesture_state.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace arisa::composition {

template <class Value>
class GestureHandlers {
 public:
  using Handler = std::move_only_function<void(const Value&)>;
  using Reset = std::move_only_function<void()>;

  struct Observer {
    Handler changed;
    Handler ended;
    Reset cancelled;
  };

  void on_changed(Handler handler) { changed_.push_back(std::move(handler)); }

  void on_ended(Handler handler) { ended_.push_back(std::move(handler)); }

  template <class T, class Body>
  void updating(GestureStateHandle<T> state, Body body) {
    updates_.push_back(
        [state, body = std::move(body)](const Value& value) mutable {
          if constexpr (std::invocable<Body&, const Value&, T&,
                                       motion::Transaction&>) {
            state.update(body, value);
          } else {
            auto adapted = [&body](const Value& seen, T& held,
                                   motion::Transaction&) { body(seen, held); };
            state.update(adapted, value);
          }
        });
    resets_.push_back([state] { state.reset(); });
  }

  void observe(Observer observer) { observers_.push_back(std::move(observer)); }

  auto attentive() const noexcept -> bool {
    return touched_ || !observers_.empty();
  }

  void changed(const Value& value) {
    touched_ = true;
    for (Handler& update : updates_) {
      update(value);
    }
    for (Handler& handler : changed_) {
      handler(value);
    }
    for (Observer& observer : observers_) {
      if (observer.changed) {
        observer.changed(value);
      }
    }
  }

  void ended(const Value& value) {
    for (Handler& handler : ended_) {
      handler(value);
    }
    for (Observer& observer : observers_) {
      if (observer.ended) {
        observer.ended(value);
      }
    }
    reset();
  }

  void cancelled() {
    for (Observer& observer : observers_) {
      if (observer.cancelled) {
        observer.cancelled();
      }
    }
    if (touched_) {
      reset();
    }
  }

 private:
  void reset() {
    touched_ = false;
    for (Reset& each : resets_) {
      each();
    }
  }

  std::vector<Handler> changed_;
  std::vector<Handler> ended_;
  std::vector<Handler> updates_;
  std::vector<Reset> resets_;
  std::vector<Observer> observers_;
  bool touched_{false};
};

}
