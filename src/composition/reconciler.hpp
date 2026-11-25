#pragma once

#include <arisa/composition/change_observer.hpp>
#include <arisa/composition/departure_hold.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_override.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/preference.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/support/full_order_walk.hpp>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <flat_map>
#include <flat_set>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace arisa::composition {

class ToolbarItem;

class Reconciler {
 public:
  explicit Reconciler(Tree& tree) noexcept : tree_(tree) {}

  void run(bool holding);

 private:
  struct Position {
    ElementType type;
    const ElementKey* key;
    std::size_t ordinal;

    friend auto operator<=>(const Position& left, const Position& right)
        -> std::strong_ordering {
      if (const auto order = left.type <=> right.type; order != 0) {
        return order;
      }
      if (const auto order = (left.key != nullptr) <=> (right.key != nullptr);
          order != 0) {
        return order;
      }
      if (left.key != nullptr) {
        if (const auto order = *left.key <=> *right.key; order != 0) {
          return order;
        }
      }
      return left.ordinal <=> right.ordinal;
    }

    friend auto operator==(const Position& left, const Position& right)
        -> bool {
      return (left <=> right) == 0;
    }
  };

  struct Shift {
    Identity key;
    const EnvironmentOverride::Value* before;
    const EnvironmentOverride::Value* after;
  };

  struct Planned {
    std::unique_ptr<Node> kept;
    std::optional<Element> built;
    std::size_t source{0};
    std::optional<Position> position;
  };

  struct Departure {
    NodeId node;
    std::shared_ptr<const DepartureHold::Token> token;
  };

  struct Change {
    NodeId node;
    std::size_t index;
    std::unique_ptr<ChangeObserver> previous;
  };

  struct Covering {
    NodeId node;
    bool covered;
  };

  struct Parked {
    Identity source;
    NodeId host;
    std::shared_ptr<const DepartureHold::Token> token;
    std::vector<std::unique_ptr<Node>> children;
  };

  struct Waiting {
    Identity source;
    NodeId node;
  };

  class Positions {
   public:
    auto next(ElementType type, const std::optional<ElementKey>& key)
        -> Position;

    void reset() noexcept;

   private:
    std::size_t unkeyed_{0};
    std::flat_map<Position, std::size_t> keyed_;
  };

  struct Scratch {
    std::flat_map<Position, std::size_t> matches;
    Positions existing;
    Positions incoming;
    std::vector<std::unique_ptr<Node>> previous;
    std::vector<const Node*> order;
    std::vector<Planned> planned;
    std::size_t kept_from{0};
  };

  void collect();

  auto declared(Node& node, Element& element) -> std::vector<Preference>;
  void lend_declarer(
      const Node& node,
      const std::vector<std::shared_ptr<const ToolbarItem>>& items);

  void prefer(Node& node, std::vector<Preference> incoming,
              std::vector<std::shared_ptr<PreferenceObserver>> observers,
              bool fresh);

  void reconcile_debug(Node& root);

  void reconcile_overlays(Node& root);

  auto overlay_source(const Node& root) const -> const Node*;

  static void inherit_environment(Element& element, const Node& source);

  auto contain() -> bool;

  auto nearest_boundary(const Node& node) -> std::optional<NodeId>;

  auto children_for(Node& node, std::vector<Element> declared)
      -> std::vector<Element>;

  void adopt_tasks(Node& node, std::vector<NodeTask> incoming);

  void start_tasks();

  auto next_dirty() -> std::optional<std::reference_wrapper<Node>>;

  enum class Arrival : std::uint8_t {
    none,
    evaluate,
    update,
    revive,
    mount,
    deliver,
    forward,
    reconcile,
  };

  enum class Work : std::uint8_t { none, reconciling, delivering, forwarding };

  struct Handoff {
    Arrival arrival{Arrival::none};
    std::optional<Element> element{};
    std::vector<Element> elements{};
    std::unique_ptr<Node> owned{};
    Node* parent{nullptr};
    bool adopt{false};
    bool returned{false};
    bool inserted{false};
    std::optional<motion::Transaction> transaction{};
  };

  struct Visit {
    std::unique_ptr<Node> owned{};
    Node* parent{nullptr};
    bool adopt{false};
    bool returned{false};
    bool evaluated{false};
    bool updated{false};
    bool inserted{false};
    Arrival arrival{Arrival::none};
    Work work{Work::none};
    std::optional<motion::Transaction> transaction{};
    std::size_t depth{0};
    std::size_t planned{0};
    std::size_t position{0};
    std::vector<Element> elements{};
    std::optional<Element> forwarded{};
  };

  struct NodeAccess {
    using Node = composition::Node;

    static auto children(Node& node) noexcept
        -> std::vector<std::unique_ptr<Node>>& {
      return node.children_;
    }
  };

  using Walk = support::FullOrderWalk<NodeAccess, Visit>;

  class Walker {
   public:
    explicit Walker(Reconciler& reconciler) noexcept
        : reconciler_(reconciler) {}

    auto enter(Node& node, const Visit& above) -> Walk::Step;

    auto next(Node& node, Visit& visit) -> Node*;

    void leave(Node& node, Visit& visit);

   private:
    std::reference_wrapper<Reconciler> reconciler_;
  };

  void start(Node& node, Handoff handoff);

  auto open(Node& node) -> Walk::Step;

  auto advance(Node& node, Visit& visit) -> Node*;

  void close(Node& node, Visit& visit);

  void evaluate(Node& node);

  void evaluate_root(Node& root);

  void evaluate_head(Node& node, Visit& visit);

  void produce_head(Node& node, Visit& visit);

  void begin_reconcile(Node& parent, Visit& visit,
                       std::vector<Element> elements);

  auto next_planned(Node& parent, Visit& visit) -> Node*;

  void finish_reconcile(Node& parent, Visit& visit);

  auto next_delivery(Node& node, Visit& visit) -> Node*;

  auto arrive(Node& parent, Element element) -> Handoff;

  auto mount_head(Node& parent, Element element) -> Handoff;

  static auto placeholder(const Node& node) -> std::optional<Identity>;

  void park(Node& node,
            const std::shared_ptr<const DepartureHold::Token>& token);

  void transplant();

  auto adopter(Identity source) -> std::optional<NodeId>;

  static void reparent(Node& node, NodeId parent, std::size_t depth);

  void update(Node& node, Element element);

  void update_head(Node& node, Element element, Visit& visit);

  void refresh_head(Node& node, Element element, Visit& visit);

  void hold_exclusive(Node& node);

  static auto repaints(const Node& node, const Element& element) -> bool;

  auto top() const noexcept -> const motion::Transaction&;

  auto derive(Node& node, Element& element, bool mounted)
      -> motion::Transaction;

  void observe(Node& node,
               std::vector<std::unique_ptr<ChangeObserver>> incoming);

  void sense(const Node& node, const Interactions& incoming);

  void introduce(Node& root, const motion::Transaction& transaction);

  auto withdraw(Node& root, const motion::Transaction& transaction,
                const std::shared_ptr<const DepartureHold::Token>& token)
      -> bool;

  auto escape(Node& node,
              const std::shared_ptr<const DepartureHold::Token>& token) -> bool;

  static auto implicit(const Node& node, motion::Transaction transaction)
      -> motion::Transaction;

  static auto layered(const Node& node) noexcept -> bool;

  static auto attaches(Node& root) -> bool;

  void hold(Node& target, const motion::Transition& transition,
            const motion::Transaction& transaction,
            const std::shared_ptr<const DepartureHold::Token>& token);

  auto obscured(const Node& node) -> bool;

  void announce(Node& node, bool appearing, bool root);

  static auto carrier(Node& root) -> Node&;

  void release_transitions();

  void enlist(Node& node, support::Sparse<MotionDeclarations> incoming,
              bool mounted);

  void unlist(const Node& node);

  void pair();

  void zoom();

  void scope(Node& node);

  auto place(Node& parent, Element element) -> std::unique_ptr<Node>;

  void rekey(Node& node, NodeId parent, std::size_t depth);

  auto recyclable(const Node& parent, const Node& node) -> bool;

  void stash(const Node& parent, std::unique_ptr<Node> node);

  void wipe(Node& node);

  void prime(Node& node) const;

  void reconcile(Node& parent, std::vector<Element> elements);

  void host_swipes(const Node& parent, std::vector<Element>& elements) const;

  auto lookup(const Node& node, Identity key) const
      -> const EnvironmentOverride::Value*;

  void plan(Scratch& here, std::vector<Element>& elements);

  static auto keep(Scratch& here, const ElementKey& key)
      -> std::optional<std::size_t>;

  static constexpr std::size_t recycled_limit = 48;

  void begin_departure(Node& root);

  void depart(Node& node,
              const std::shared_ptr<const DepartureHold::Token>& token);

  void apply(Node& node, std::vector<EnvironmentOverride> overrides);

  void invalidate(Node& node, const Shift& shift, bool origin);

  void notify();

  auto sweep() -> bool;

  void retire(Node& node);

  static void conclude(Node& node);

  std::reference_wrapper<Tree> tree_;
  std::flat_set<NodeId> evaluated_;
  std::vector<NodeId> appearing_;
  std::vector<NodeId> starting_;
  std::vector<Departure> disappearing_;
  std::vector<Change> changes_;
  std::vector<Covering> coverings_;
  std::vector<Parked> parked_;
  std::vector<Waiting> waiting_;
  std::vector<motion::Transaction> transactions_;
  std::vector<NodeId> arrivals_;
  std::vector<NodeId> zooms_;
  std::vector<NodeId> departures_;
  std::size_t mounting_{0};
  std::deque<Scratch> scratch_;
  Walk walk_;
  Handoff handoff_;
  std::unique_ptr<Node> returned_;
  std::size_t depth_{0};
  bool debugging_{false};
  bool holding_{false};
};

}
