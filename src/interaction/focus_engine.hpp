#pragma once

#include <arisa/composition/default_focus_evaluation_priority.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/interaction/spatial_search.hpp>
#include <arisa/support/full_order_walk.hpp>

#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <flat_set>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::interaction {

class FocusEngine {
 public:
  enum class MoveOutcome : std::uint8_t {
    moved,
    blocked,
    deferred,
    held,
  };

  struct Synchronisation {
    bool changed{false};
    std::optional<MoveOutcome> deferred;
  };

  static constexpr int deferral_frames = 3;

  explicit FocusEngine(SpatialSearch search = SpatialSearch{}) noexcept
      : search_(search) {}

  auto synchronise(composition::Tree& tree) -> Synchronisation;

  auto move(composition::Tree& tree,
            composition::MoveCommandDirection direction,
            std::optional<composition::NodeId> keep_within = std::nullopt)
      -> MoveOutcome;

  auto focus(composition::Tree& tree, composition::NodeId node) -> bool;

  auto type_select(const composition::Tree& tree, std::string_view prefix) const
      -> std::optional<composition::NodeId>;

  auto hit(const composition::Tree& tree, composition::Point point)
      -> std::optional<composition::NodeId>;

  static auto clipped_out(const composition::Tree& tree,
                          const composition::Node& node,
                          composition::Point point) -> bool;

  auto visible_inside(const composition::Tree& tree,
                      composition::NodeId container,
                      const composition::Rect& viewport)
      -> std::optional<composition::NodeId>;

  auto focused() const noexcept -> std::optional<composition::NodeId> {
    return focused_;
  }

  auto collections() const noexcept -> std::uint64_t { return collections_; }

  auto refreshes() const noexcept -> std::uint64_t { return refreshes_; }

  auto uncovered() const noexcept -> std::uint64_t { return uncovered_; }

  auto origin() const noexcept -> std::optional<composition::NodeId>;

  auto next(const composition::Tree& tree,
            composition::MoveCommandDirection direction) const
      -> std::optional<composition::NodeId> {
    return advance(tree, direction);
  }

  auto confined_to() const noexcept -> std::optional<composition::NodeId>;

 private:
  struct Span {
    std::size_t begin{0};
    std::size_t end{0};
  };

  struct Entry {
    composition::NodeId id{0};
    composition::NodeId section{0};
    composition::Rect frame{};
    bool prefers{false};
    const composition::ElementKey* key{nullptr};
    std::size_t path_begin{0};
    std::size_t path_size{0};
    composition::ElementType type;
    const composition::MatchedGeometry::Key* matched{nullptr};
    Span chain{};
  };

  struct Section {
    std::optional<composition::NodeId> parent;
    composition::Rect frame{};
    Span chain{};
  };

  struct Declaration {
    composition::NodeId declarer{0};
    std::size_t index{0};
  };

  struct Record {
    composition::NodeId node{0};
    std::optional<composition::NodeId> parent;
    std::vector<composition::NodeId> sections;
    composition::Rect frame{};
    std::optional<composition::ElementKey> key;
    std::vector<std::size_t> path;
    composition::ElementType type;
    std::optional<composition::MatchedGeometry::Key> matched;
    composition::NodeId horizon{0};
  };

  struct Restore {
    composition::NodeId trap{0};
    std::optional<composition::NodeId> saved;
  };

  struct Pending {
    composition::MoveCommandDirection direction{
        composition::MoveCommandDirection::down};
    int frames{0};
    std::optional<composition::NodeId> within{};
  };

  struct Trap {
    composition::NodeId id{0};
    std::int32_t z{0};
  };

  struct Goal {
    composition::NodeId node{0};
    bool vertical{true};
    float center{0.0F};
  };

  struct Walk {
    composition::NodeId section{0};
    std::int32_t layer{0};
    bool disabled{false};
    bool hidden{false};
    bool prefers{false};
    const composition::ElementKey* key{nullptr};
    std::size_t path_base{0};
    composition::Point scrolled{};
  };

  struct Stamp {
    std::uint64_t revision{0};
    std::uint64_t visibility{0};
    std::uint64_t geometry{0};
    std::uint64_t exclusive{0};

    auto structural(const Stamp& other) const noexcept -> bool {
      return revision != other.revision || visibility != other.visibility ||
             exclusive != other.exclusive;
    }

    friend auto operator==(const Stamp&, const Stamp&) -> bool = default;
  };

  struct Spanned {
    composition::NodeId id{0};
    Span span;
  };

  struct Unit {
    composition::NodeId id{0};
    bool section{false};
    composition::Rect frame{};
  };

  void collect(const composition::Tree& tree);

  struct Visit {
    Walk walk{};
    std::size_t begin{0};
    std::size_t scope_depth{0};
    std::optional<std::size_t> fallback{};
    std::size_t index{0};
    bool entered{false};
    bool scrolls{false};
    bool pathed{false};
  };

  struct NodeAccess {
    using Node = const composition::Node;

    static auto children(const composition::Node& node)
        -> decltype(std::declval<const composition::Node&>().children()) {
      return node.children();
    }
  };

  using Traversal = support::FullOrderWalk<NodeAccess, Visit>;

  class Visitor {
   public:
    explicit Visitor(FocusEngine& engine) noexcept : engine_(engine) {}

    auto enter(const composition::Node& node, const Visit& above)
        -> Traversal::Step;

    auto next(const composition::Node& node, Visit& visit)
        -> const composition::Node*;

    void leave(const composition::Node& node, Visit& visit);

   private:
    std::reference_wrapper<FocusEngine> engine_;
  };

  auto visit(const composition::Node& node, Walk walk) -> Traversal::Step;

  void leave(const composition::Node& node, Visit& visit);

  auto chain() -> Span;

  auto shown(const composition::Tree& tree, composition::NodeId id,
             Span chain) const -> std::optional<composition::Rect>;

  void refresh(const composition::Tree& tree);

  auto index_of(composition::NodeId id) const -> std::optional<std::size_t>;

  auto inside(composition::NodeId node, composition::NodeId container) const
      -> bool;

  auto span_of(composition::NodeId node) const -> std::optional<Span>;

  auto path_of(const Entry& entry) const -> std::span<const std::size_t>;

  auto eligible(const Entry& entry) const -> bool;

  static auto type_select_label(const composition::Node& node)
      -> std::optional<std::string>;

  auto valid(composition::NodeId id) const -> bool;

  auto boundary() const noexcept -> composition::NodeId;

  auto populated(composition::NodeId section,
                 std::optional<composition::NodeId> excluded) const -> bool;

  auto units(composition::NodeId level,
             std::optional<composition::NodeId> skipped,
             std::optional<composition::NodeId> excluded) const
      -> std::vector<Unit>;

  auto declared(const composition::Tree& tree, composition::NodeId section,
                bool own) const -> std::optional<composition::NodeId>;

  auto declared(
      const composition::Tree& tree, composition::NodeId section, bool own,
      std::optional<composition::DefaultFocusEvaluationPriority> only) const
      -> std::optional<composition::NodeId>;

  auto remembered(composition::NodeId section) const
      -> std::optional<composition::NodeId>;

  auto preferred(const composition::Tree& tree,
                 composition::NodeId section) const
      -> std::optional<composition::NodeId>;

  auto enter(const composition::Tree& tree, composition::NodeId section,
             const composition::Rect& from,
             composition::MoveCommandDirection direction) const
      -> std::optional<composition::NodeId>;

  auto nearest_inside(composition::NodeId container,
                      std::optional<composition::NodeId> section,
                      const composition::Rect& from) const
      -> std::optional<composition::NodeId>;

  auto first_inside(composition::NodeId container) const
      -> std::optional<composition::NodeId>;

  auto advance(const composition::Tree& tree,
               composition::MoveCommandDirection direction) const
      -> std::optional<composition::NodeId>;

  static auto along(composition::MoveCommandDirection direction) noexcept
      -> bool;

  auto aimed(const Entry& current,
             composition::MoveCommandDirection direction) const
      -> composition::Rect;

  void aim(composition::NodeId from, composition::NodeId to,
           composition::MoveCommandDirection direction);

  auto recover(const composition::Tree& tree)
      -> std::optional<composition::NodeId>;

  auto by_key(const Record& record) const -> std::optional<composition::NodeId>;

  auto by_geometry(const Record& record) const
      -> std::optional<composition::NodeId>;

  auto replacement(const Record& record) const
      -> std::optional<composition::NodeId>;

  auto leads(composition::NodeId section, const Entry& current,
             composition::MoveCommandDirection direction) const -> bool;

  static auto underlies(const composition::Rect& section,
                        const composition::Rect& from) noexcept -> bool;

  static auto ahead(const composition::Rect& section,
                    const composition::Rect& from,
                    composition::MoveCommandDirection direction) noexcept
      -> composition::Rect;

  static auto overlap(const composition::Rect& left,
                      const composition::Rect& right) noexcept -> float;

  static auto scoped(const composition::Node& node,
                     composition::Namespace::ID scope)
      -> std::optional<composition::NodeId>;

  void confine(const composition::Tree& tree);

  auto reachable(std::int32_t layer) const noexcept -> bool;

  void restore(composition::Tree& tree);

  auto trapped(composition::NodeId id) const -> bool;

  void requests(const composition::Tree& tree);

  void adopt(composition::Tree& tree, std::optional<composition::NodeId> node);

  void publish(composition::Tree& tree);

  SpatialSearch search_;
  std::optional<composition::NodeId> focused_;
  bool dormant_{false};
  composition::NodeId root_{0};
  composition::NodeId horizon_{0};
  std::size_t order_{0};
  std::vector<Entry> entries_;
  std::flat_map<composition::NodeId, Section> sections_;
  std::vector<Spanned> spans_;
  std::optional<Stamp> stamp_;
  std::uint64_t collections_{0};
  std::uint64_t refreshes_{0};
  std::uint64_t uncovered_{0};
  bool stale_{false};
  std::vector<composition::NodeId> scrollers_;
  std::vector<composition::NodeId> chains_;
  std::vector<std::size_t> path_;
  Traversal traversal_;
  std::vector<std::size_t> paths_;
  std::vector<composition::Namespace::ID> scopes_;
  std::vector<Trap> traps_;
  std::vector<composition::NodeId> bound_;
  std::vector<Declaration> defaults_;
  std::flat_map<composition::NodeId, composition::NodeId> memory_;
  std::optional<Record> record_;
  std::optional<Record> pending_restore_;
  std::vector<Restore> restores_;
  std::flat_set<std::pair<composition::NodeId, std::size_t>> requested_;
  std::optional<Pending> pending_;
  std::optional<std::vector<std::int32_t>> kept_;
  std::optional<composition::NodeId> exclusive_owner_;
  std::optional<Goal> goal_;
};

}
