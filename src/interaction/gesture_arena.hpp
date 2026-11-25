#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/gesture_attachment.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/gesture_layout.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/support/full_order_walk.hpp>

#include <interaction/drag_recognizer.hpp>
#include <interaction/invocation.hpp>
#include <interaction/lift_recognizer.hpp>
#include <interaction/long_press_recognizer.hpp>
#include <interaction/pinch_recognizer.hpp>
#include <interaction/recognizer_signals.hpp>
#include <interaction/scroll_coordinator.hpp>
#include <interaction/tap_recognizer.hpp>
#include <interaction/velocity_tracker.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace arisa::interaction {

class GestureArena {
 public:
  static constexpr float scroll_slop = 10.0F;
  static constexpr std::chrono::milliseconds lift_duration{500};
  static constexpr float lift_distance = 10.0F;
  static constexpr std::chrono::milliseconds sequence_window{500};

  struct Lift {
    composition::NodeId node{0};
    composition::Point point{};
  };

  struct Effects {
    std::vector<Invocation> invocations;
    std::vector<provider::SoundRole> sounds;
    std::optional<composition::NodeId> focus;
    std::optional<Lift> lifted;
    std::optional<composition::Point> dragged;
    std::optional<composition::Point> dropped;
    bool lift_cancelled{false};
    bool changed{false};
  };

  static auto chain(const composition::Tree& tree, composition::Point point)
      -> std::vector<composition::NodeId>;

  static auto space_origin(const composition::Tree& tree,
                           const composition::Node& node,
                           const composition::CoordinateSpace& space)
      -> composition::Point;

  void down(composition::Tree& tree, composition::Point point,
            std::chrono::nanoseconds now, ScrollCoordinator& scrolls,
            std::optional<composition::NodeId> confined, Effects& effects);

  void down(composition::Tree& tree, composition::Point point,
            std::chrono::nanoseconds now, ScrollCoordinator& scrolls,
            std::optional<composition::NodeId> confined,
            composition::GestureInputKinds kind, Effects& effects);

  auto input_kind() const noexcept -> composition::GestureInputKinds {
    return kind_;
  }

  auto recognized() const -> std::optional<std::string>;

  void move(composition::Tree& tree, composition::Point point,
            std::chrono::nanoseconds now, ScrollCoordinator& scrolls,
            Effects& effects);

  void up(composition::Tree& tree, composition::Point point,
          std::chrono::nanoseconds now, ScrollCoordinator& scrolls,
          Effects& effects);

  void second(composition::Tree& tree, std::optional<composition::Point> point,
              std::chrono::nanoseconds now, ScrollCoordinator& scrolls,
              Effects& effects);

  void elapse(composition::Tree& tree, std::chrono::nanoseconds now,
              ScrollCoordinator& scrolls, Effects& effects);

  void abandon(composition::Tree& tree, ScrollCoordinator& scrolls,
               Effects& effects);

  auto pressed() const noexcept -> std::optional<composition::NodeId> {
    return pressed_;
  }

  auto active() const noexcept -> bool { return !contenders_.empty(); }

  auto touched() const -> std::optional<composition::NodeId>;

  auto winner() const -> std::optional<composition::NodeId>;

 private:
  enum class Kind : std::uint8_t {
    gesture,
    activation,
    scroll,
    lift,
    focus,
  };

  using Recognizer =
      std::variant<TapRecognizer, LongPressRecognizer, DragRecognizer,
                   PinchRecognizer, LiftRecognizer>;

  struct Contender {
    Kind kind{Kind::gesture};
    composition::NodeId node{0};
    std::size_t attachment{0};
    std::size_t leaf{0};
    std::size_t group{0};
    composition::GesturePriority priority{composition::GesturePriority::normal};
    std::size_t depth{0};
    Recognizer recognizer;
    std::vector<std::size_t> blockers;
    composition::Point origin{};
    composition::Size extent{};
    composition::CoordinateSpace space{composition::CoordinateSpace::local};
    composition::Point anchor{};
    composition::Point start{};
    std::chrono::nanoseconds deadline{0};
    bool armed{true};
    bool awaiting{false};
    bool waiting{false};
    bool fired{false};
    bool done{false};
    bool lost{false};
    bool succeeded{false};
  };

  struct Group {
    std::vector<composition::GestureLayout::Branch> branches;
    std::vector<std::size_t> members;
    std::optional<composition::ScrollHandoff> handoff;
    std::string name;
  };

  struct Hit {
    composition::NodeId node{0};
    bool disabled{false};
  };

  struct Event {
    enum class Kind : std::uint8_t {
      down,
      move,
      up,
      elapse,
    };

    Kind kind{Kind::elapse};
    composition::Point point{};
    std::chrono::nanoseconds now{0};
    std::optional<composition::Point> second{};
  };

  struct Deferred {
    std::reference_wrapper<const composition::Node> node;
    std::vector<Hit> path;
    bool disabled{false};
    std::int32_t z{0};
    std::size_t order{0};
  };

  static void search(const composition::Node& node, composition::Point point,
                     bool disabled, bool reachable,
                     const composition::ExclusiveScopes& exclusive,
                     std::vector<Hit>& path, std::vector<Hit>& best,
                     std::vector<Deferred>& deferred);

  struct Probe {
    bool blocked{false};
    bool within{false};
    bool entered{false};
    bool arranged{false};
    std::size_t position{0};
    std::size_t base{0};
    std::size_t end{0};
  };

  struct NodeAccess {
    using Node = const composition::Node;

    static auto children(const composition::Node& node)
        -> decltype(std::declval<const composition::Node&>().children()) {
      return node.children();
    }
  };

  using Search = support::FullOrderWalk<NodeAccess, Probe>;

  class Searcher {
   public:
    Searcher(composition::Point point,
             const composition::ExclusiveScopes& exclusive,
             std::vector<Hit>& path, std::vector<Hit>& best,
             std::vector<Deferred>& deferred) noexcept
        : point_(point),
          exclusive_(exclusive),
          path_(path),
          best_(best),
          deferred_(deferred) {}

    auto enter(const composition::Node& node, const Probe& above)
        -> Search::Step;

    auto next(const composition::Node& node, Probe& probe)
        -> const composition::Node*;

    void leave(const composition::Node& node, Probe& probe);

   private:
    composition::Point point_;
    std::reference_wrapper<const composition::ExclusiveScopes> exclusive_;
    std::reference_wrapper<std::vector<Hit>> path_;
    std::reference_wrapper<std::vector<Hit>> best_;
    std::reference_wrapper<std::vector<Deferred>> deferred_;
    std::vector<const composition::Node*> order_;
  };

  static auto reaches(const composition::ExclusiveScopes& exclusive,
                      composition::OverlayLayer layer) noexcept -> bool;

  static auto locate(const composition::Tree& tree, composition::Point point)
      -> std::vector<Hit>;

  static auto signal(Recognizer& recognizer, const Event& event)
      -> RecognizerSignals;

  static auto possible(const Recognizer& recognizer) -> bool;

  static auto continuous() noexcept -> motion::Transaction;

  static auto waiting(const Recognizer& recognizer) -> bool;

  static auto recognizer_for(const composition::GestureLayout::Leaf& leaf)
      -> Recognizer;

  static auto space_for(const composition::GestureLayout::Leaf& leaf)
      -> composition::CoordinateSpace;

  static auto single_finger(const Recognizer& recognizer) -> bool;

  static auto kinds_of(const composition::GestureLayout::Leaf& leaf)
      -> composition::GestureInputKinds;

  void build(composition::Tree& tree, composition::Point point,
             const ScrollCoordinator& scrolls,
             std::optional<composition::NodeId> confined);

  void feed(composition::Tree& tree, const Event& event,
            ScrollCoordinator& scrolls, Effects& effects);

  void handle(composition::Tree& tree, std::size_t index,
              const RecognizerSignals& signals, const Event& event,
              ScrollCoordinator& scrolls, Effects& effects);

  void resolve(composition::Tree& tree, const Event& event,
               ScrollCoordinator& scrolls, Effects& effects);

  void arm(composition::Tree& tree, const Event& event,
           ScrollCoordinator& scrolls, Effects& effects);

  auto unblocked(std::size_t index) const -> bool;

  auto allied(std::size_t first, std::size_t second) const -> bool;

  auto relation(std::size_t first, std::size_t second) const
      -> std::optional<composition::GestureLayout::Relation>;

  auto gated(std::size_t index) const -> bool;

  auto stranded(std::size_t index) const -> bool;

  auto ended(const Group& group, std::size_t begin, std::size_t end) const
      -> bool;

  auto settled(const Group& group, std::size_t begin, std::size_t end) const
      -> bool;

  auto handoff() const -> std::optional<std::size_t>;

  auto declines(std::size_t index, const ScrollCoordinator& scrolls) const
      -> bool;

  auto overflows(std::size_t index, const Event& event,
                 const ScrollCoordinator& scrolls) const -> bool;

  void transfer(composition::Tree& tree, std::size_t from, std::size_t to,
                const Event& event, ScrollCoordinator& scrolls,
                Effects& effects);

  void claim(composition::Tree& tree, std::size_t index, const Event& event,
             ScrollCoordinator& scrolls, Effects& effects);

  void change(composition::Tree& tree, std::size_t index, const Event& event,
              ScrollCoordinator& scrolls, Effects& effects);

  void finish(composition::Tree& tree, std::size_t index, const Event& event,
              ScrollCoordinator& scrolls, Effects& effects);

  void cancel(composition::Tree& tree, std::size_t index,
              ScrollCoordinator& scrolls, Effects& effects);

  void lose(composition::Tree& tree, std::size_t index, Effects& effects);

  void press(composition::Tree& tree, std::size_t index, bool pressing,
             Effects& effects);

  void notify_changed(composition::Tree& tree, std::size_t index,
                      const Event& event, Effects& effects);

  void notify_ended(composition::Tree& tree, std::size_t index,
                    const Event& event, Effects& effects);

  auto base(const Contender& contender) const -> composition::Point;

  auto value(const Contender& contender, const Event& event) const
      -> composition::DragGesture::Value;

  auto magnified(const Contender& contender) const
      -> composition::MagnifyGesture::Value;

  auto rotated(const Contender& contender) const
      -> composition::RotateGesture::Value;

  auto gesture_of(const composition::Tree& tree,
                  const Contender& contender) const
      -> std::shared_ptr<composition::Gesture>;

  void tidy();

  void clear();

  std::vector<Contender> contenders_;
  std::vector<Group> groups_;
  std::optional<std::size_t> winner_;
  std::optional<composition::NodeId> pressed_;
  composition::Point start_{};
  composition::Point last_{};
  std::optional<composition::Point> second_;
  VelocityTracker velocity_;
  std::chrono::nanoseconds last_now_{0};
  bool touching_{false};
  composition::GestureInputKinds kind_{
      composition::GestureInputKinds::direct_touch};
};

}
