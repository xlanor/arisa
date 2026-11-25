#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/point.hpp>

#include <interaction/recognizer_signals.hpp>

#include <boost/sml.hpp>

#include <chrono>
#include <cmath>

namespace arisa::interaction {

template <class... Policies>
class BasicDragRecognizer {
 public:
  explicit BasicDragRecognizer(
      float minimum_distance,
      composition::Axes axes = composition::Axes::all) noexcept {
    tracking_.distance = minimum_distance;
    tracking_.axes = axes;
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

  auto elapse(std::chrono::nanoseconds) -> RecognizerSignals { return {}; }

  auto cancel() -> RecognizerSignals {
    return process(Cancel{.tracking = &tracking_});
  }

  auto possible() const noexcept -> bool {
    return machine_.is(boost::sml::state<Idle>) ||
           machine_.is(boost::sml::state<Tracking>) ||
           machine_.is(boost::sml::state<Dragging>);
  }

  auto dragging() const noexcept -> bool {
    return machine_.is(boost::sml::state<Dragging>);
  }

  auto recognized() const noexcept -> bool {
    return machine_.is(boost::sml::state<Ended>);
  }

  auto cancelled() const noexcept -> bool {
    return machine_.is(boost::sml::state<Cancelled>);
  }

  auto waiting() const noexcept -> bool { return false; }

  auto start() const noexcept -> composition::Point { return tracking_.first; }

 private:
  struct Idle {};
  struct Tracking {};
  struct Dragging {};
  struct Ended {};
  struct Cancelled {};
  struct Failed {};

  struct Track {
    float distance{0.0F};
    composition::Axes axes{composition::Axes::all};
    composition::Point first{};
  };

  struct Down {
    Track* tracking{nullptr};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
  };

  struct Move {
    Track* tracking{nullptr};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
  };

  struct Up {
    Track* tracking{nullptr};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
  };

  struct Cancel {
    Track* tracking{nullptr};
  };

  struct Table {
    auto operator()() const noexcept {
      using boost::sml::event;
      using boost::sml::state;
      const auto start = [](const Down& down) {
        down.tracking->first = down.point;
      };
      const auto immediate = [](const Down& down) {
        return down.tracking->distance <= 0.0F;
      };
      const auto far = [](const Move& move) {
        return std::hypot(move.point.x - move.tracking->first.x,
                          move.point.y - move.tracking->first.y) >=
               move.tracking->distance;
      };
      const auto aligned = [](const Move& move) {
        if (std::hypot(move.point.x - move.tracking->first.x,
                       move.point.y - move.tracking->first.y) <
            move.tracking->distance) {
          return false;
        }
        const float dx = std::abs(move.point.x - move.tracking->first.x);
        const float dy = std::abs(move.point.y - move.tracking->first.y);
        const composition::Axes axes = move.tracking->axes;
        if (axes == composition::Axes::vertical) {
          return dy >= dx;
        }
        if (axes == composition::Axes::horizontal) {
          return dx > dy;
        }
        return true;
      };
      return boost::sml::make_transition_table(
          *state<Idle> + event<Down>[immediate] / start = state<Dragging>,
          state<Idle> + event<Down> / start = state<Tracking>,
          state<Idle> + event<Cancel> = state<Failed>,
          state<Tracking> + event<Move>[aligned] = state<Dragging>,
          state<Tracking> + event<Move>[far] = state<Failed>,
          state<Tracking> + event<Up> = state<Failed>,
          state<Tracking> + event<Cancel> = state<Failed>,
          state<Dragging> + event<Move> = state<Dragging>,
          state<Dragging> + event<Up> = state<Ended>,
          state<Dragging> + event<Cancel> = state<Cancelled>);
    }
  };

  template <class Event>
  auto process(const Event& event) -> RecognizerSignals {
    const bool was_dragging = dragging();
    const bool live = possible();
    machine_.process_event(event);
    RecognizerSignals signals;
    signals.failed = live && machine_.is(boost::sml::state<Failed>);
    signals.claimed = !was_dragging && dragging();
    signals.changed = dragging();
    signals.ended = was_dragging && recognized();
    return signals;
  }

  Track tracking_;
  boost::sml::sm<Table, Policies...> machine_;
};

using DragRecognizer = BasicDragRecognizer<>;

}
