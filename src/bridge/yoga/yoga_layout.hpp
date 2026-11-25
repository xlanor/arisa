#pragma once

#include <bridge/yoga/custom_arrangement.hpp>
#include <bridge/yoga/grid_arrangement.hpp>
#include <bridge/yoga/yoga_slot.hpp>

#include <arisa/components/alignment.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/layout/layout_error.hpp>
#include <arisa/layout/viewport.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/support/full_order_walk.hpp>
#include <arisa/text/layout_width.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/text_style.hpp>
#include <arisa/text/typesetter.hpp>
#include <arisa/theme/theme.hpp>

#include <yoga/Yoga.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arisa::bridge::yoga {

class YogaLayout {
  struct Token {
    explicit Token() = default;
  };

 public:
  using TypesetterRef = std::optional<std::reference_wrapper<text::Typesetter>>;

  [[nodiscard]] static auto create()
      -> std::expected<std::unique_ptr<YogaLayout>, layout::LayoutError>;

  YogaLayout(Token, std::shared_ptr<YogaSlot::ConfigHandle> config,
             std::uint64_t id) noexcept;

  YogaLayout(const YogaLayout&) = delete;
  auto operator=(const YogaLayout&) -> YogaLayout& = delete;
  YogaLayout(YogaLayout&&) = delete;
  auto operator=(YogaLayout&&) -> YogaLayout& = delete;
  ~YogaLayout() = default;

  [[nodiscard]] auto run(composition::Tree& tree, layout::Viewport viewport,
                         TypesetterRef typesetter, bool force)
      -> std::expected<bool, layout::LayoutError>;

  auto calculations() const noexcept -> std::uint64_t { return calculations_; }

  auto measurements() const noexcept -> std::uint64_t { return measurements_; }

  auto visits() const noexcept -> std::uint64_t { return visits_; }

  auto placements() const noexcept -> std::uint64_t { return placements_; }

  auto computations() const noexcept -> std::uint64_t { return computations_; }

  using CostRef =
      std::optional<std::reference_wrapper<const composition::NodeCost>>;

  auto measure(std::string_view text, const text::TextStyle& style, bool wraps,
               float width, YGMeasureMode mode, std::optional<float> height,
               const text::TextFitting& fitting,
               std::optional<std::size_t> reserved, CostRef cost) -> YGSize;

  auto baseline(std::string_view text, const text::TextStyle& style, bool wraps,
                float width) -> float;

  auto baselines(std::string_view text, const text::TextStyle& style,
                 bool wraps, float width) -> composition::TextBaselines;

 private:
  enum class Role : std::uint8_t {
    column_item,
    row_item,
    overlay,
    layer,
  };

  struct Scope {
    std::reference_wrapper<const theme::Theme> theme;
    std::reference_wrapper<const text::Locale> locale;
    std::uint64_t geometry{0};
    components::LayoutDirection direction;
    theme::DynamicTypeSize dynamic_type_size{theme::DynamicTypeSize::large};
    theme::DynamicTypeRange dynamic_type_range{theme::DynamicTypeRange::all()};
    std::optional<theme::Font> font{};
    composition::TextAttributes text{};
    theme::Edges insets{};
    theme::Edges safe{};
    bool open_width{false};
    bool open_height{false};
    bool fixed_width{false};
    bool fixed_height{false};
    bool scrolled_width{false};
    bool scrolled_height{false};
    double priority{0.0};
    float shrink_weight{1.0F};
    bool grow_blocked{false};
    bool ranked{false};
    std::uint64_t cell{0};
    std::optional<theme::Insets> cell_padding{};
    std::optional<theme::Insets> cell_margin{};
    std::optional<composition::FrameSpec> cell_frame{};
    std::optional<composition::ContainerCorners::Enclosure> enclosure{};
    bool hoisted_width{false};
    bool hoisted_height{false};
    std::optional<std::reference_wrapper<const composition::Modifiers>>
        carried{};
    std::optional<std::reference_wrapper<const composition::KeyedValues>>
        carried_values{};
    bool unoriented{false};
  };

  enum class Fit : std::uint8_t {
    box,
    ratio,
    open_width,
    open_height,
    ideal,
  };

  enum class Flow : std::uint8_t {
    own,
    consume,
    neutral,
    column,
    row,
    all,
    base,
  };

  struct Bar {
    std::uint64_t provider;
    composition::NodeId node;
    theme::Edge edge;
  };

  struct Geometry {
    theme::Theme theme;
    std::uint64_t key{0};
  };

  struct Entry {
    std::reference_wrapper<const composition::Node> node;
    Scope scope;
  };

  static constexpr std::uint32_t relayout_rounds = 2;
  static constexpr std::size_t priority_levels = 4;
  static constexpr float priority_shrink_step = 1.0e-4F;
  static constexpr std::size_t geometry_limit = 16;
  static constexpr std::size_t guide_depth = 3;
  static constexpr std::size_t separator_depth = 8;
  inline static const composition::ElementKey separator_key{
      "arisa.list.row-separator"};

  static auto next_id() noexcept -> std::uint64_t;

  static void observe();

  [[nodiscard]] auto pass(composition::Tree& tree, layout::Viewport viewport,
                          TypesetterRef typesetter, bool force)
      -> std::expected<bool, layout::LayoutError>;

  static auto lazy_root(composition::Tree& tree, const composition::Node& node)
      -> bool;

  static auto transparent(const composition::Node& node) -> bool;

  static auto departing(const composition::Node& node) noexcept -> bool;

  auto scoped(const composition::Node& node, Scope scope) -> Scope;

  auto geometry(const theme::Theme& theme) -> std::uint64_t;

  static auto direction(components::LayoutDirection direction) noexcept
      -> YGDirection;

  static auto justify(components::Distribution distribution) noexcept
      -> YGJustify;

  static auto align(components::HorizontalAlignment alignment) noexcept
      -> YGAlign;

  static auto align(components::VerticalAlignment alignment) noexcept
      -> YGAlign;

  static auto justify(components::VerticalAlignment alignment) noexcept
      -> YGJustify;

  static auto justify(components::HorizontalAlignment alignment) noexcept
      -> YGJustify;

  static auto length(const std::optional<composition::Length>& length,
                     const theme::Theme& theme) noexcept -> YogaSlot::Length;

  static auto points(const std::optional<composition::Length>& length,
                     const theme::Theme& theme) noexcept
      -> std::optional<float>;

  static auto ideal_of(const std::optional<composition::Length>& fixed,
                       const std::optional<composition::Length>& ideal,
                       const theme::Theme& theme) noexcept
      -> std::optional<float>;

  static auto layered(const composition::Node& node) noexcept -> bool;

  static auto live(const composition::Node& node) noexcept -> bool;

  static auto flow_of(const composition::Node& node) -> Flow;

  static void distribute(std::span<Entry> entries, Flow flow);

  static void rank(std::span<Entry> entries, bool horizontal);

  static auto priority_of(const Entry& entry) noexcept -> double;

  static auto fixes(const composition::Node& node,
                    composition::Axis axis) noexcept -> bool;

  static auto proposes(const composition::Node& node) -> bool;

  auto container_length(composition::Tree& tree, const composition::Node& node,
                        composition::Axis axis) -> std::optional<float>;

  auto window_length(const Scope& scope, composition::Axis axis) const -> float;

  auto relate(composition::Tree& tree, const composition::Node& node,
              composition::FrameSpec frame, const Scope& scope)
      -> composition::FrameSpec;

  auto settle_relative(composition::Tree& tree, const composition::Node& node,
                       YGNodeRef laid) -> bool;

  static auto settle_position(const composition::Node& node, YGNodeRef laid)
      -> bool;

  static auto corner_radii(const theme::Theme& theme,
                           const theme::Shape& shape) noexcept
      -> theme::RectangleCornerRadii;

  static auto surveys(const composition::Node& node) -> bool;

  auto settle_corner(composition::Tree& tree, const composition::Node& node,
                     YogaSlot& slot) -> bool;

  static auto ignoring(
      theme::Edges edges,
      const composition::Scrolling::IgnoredSafeArea& ignored) noexcept
      -> theme::Edges;

  static auto plus(theme::Edges first, theme::Edges second) noexcept
      -> theme::Edges;

  auto provided(const composition::Node& node) -> std::optional<theme::Edges>;

  auto settle_bars(composition::Tree& tree) -> bool;

  struct Range {
    std::size_t begin{0};
    std::size_t end{0};
  };

  auto gather(const composition::Node& node, Scope scope) -> std::size_t;

  void admit(const composition::Node& child, Scope scope);

  void release(std::size_t first);

  auto produced(Range range) const noexcept -> std::span<const YGNodeRef>;

  auto sizing(const composition::FrameSpec& frame, Role role,
              YogaSlot::Style style, const Scope& scope) const
      -> YogaSlot::Style;

  static auto fills(const std::optional<composition::Length>& limit) noexcept
      -> bool;

  static auto sized(const composition::FrameSpec& frame) noexcept -> bool;

  static auto pinned(const composition::FrameSpec& frame) noexcept -> bool;

  static auto fit_of(const composition::FrameSpec& frame,
                     const composition::AspectRatio& aspect, bool open_width,
                     bool open_height) noexcept -> Fit;

  static auto unbounded(YogaSlot::Style style, float ratio, Role role,
                        Fit fit) noexcept -> YogaSlot::Style;

  static auto stretches(const composition::Node& node, bool horizontal) -> bool;

  static auto content_stretches(const composition::Node& node, bool horizontal)
      -> bool;

  static auto aligned_kind(const composition::Node& node) -> bool;

  static auto framed_cross(const composition::Node& node,
                           composition::HorizontalAlignment alignment)
      -> YGAlign;

  static auto exterior(YogaSlot::Style style, composition::Alignment alignment)
      -> YogaSlot::Style;

  static auto interior(YogaSlot::Style style, const composition::Node& node)
      -> YogaSlot::Style;

  static auto opened(const composition::Node& node, Scope scope) -> Scope;

  static auto inherited_frame(const composition::Node& node)
      -> composition::FrameSpec;

  static auto compressible(const composition::Node& node, bool horizontal)
      -> bool;

  static auto stacked(const composition::Node& node)
      -> std::optional<std::reference_wrapper<const composition::Node>>;

  static auto flexed(const composition::Node& node,
                     const composition::FrameSpec& frame, Role role,
                     bool open_width, bool open_height, YogaSlot::Style style)
      -> YogaSlot::Style;

  static auto flexible(const composition::Node& node, bool horizontal) -> bool;

  static auto flexible_child(const composition::Node& node, bool horizontal)
      -> bool;

  static auto decorated_frame(const composition::Node& node, std::size_t base)
      -> composition::FrameSpec;

  static auto absolute(YGNodeRef node) -> composition::Rect;

  auto slot(composition::Tree& tree, const composition::Node& node)
      -> std::expected<std::reference_wrapper<YogaSlot>, layout::LayoutError>;

  auto measured(std::string_view text, const text::TextStyle& style, bool wraps,
                float width, YGMeasureMode mode, std::optional<float> height,
                const text::TextFitting& fitting,
                std::optional<std::size_t> reserved) -> YGSize;

  auto reserve(const text::TextStyle& style, std::optional<std::size_t> lines,
               float height) -> float;

  auto line_height(const text::TextStyle& style) -> float;

  auto settle_row(composition::Tree& tree, const composition::Node& node,
                  YGNodeRef laid) -> bool;

  auto natural(std::string_view text, const text::TextStyle& style)
      -> text::TextSize;

  auto wrap_width(std::string_view text, const text::TextStyle& style,
                  bool wraps, float width) -> text::LayoutWidth;

  struct NodeAccess {
    using Node = const composition::Node;

    static auto children(const composition::Node& node)
        -> decltype(std::declval<const composition::Node&>().children()) {
      return node.children();
    }
  };

  enum class Gathering : std::uint8_t {
    none,
    children,
    decorated,
    members,
    table,
    cells,
  };

  struct Spacing {
    bool active{false};
    bool vertical{false};
    bool automatic{false};
    bool guided{false};
    float overlap{0.0F};
  };

  struct Gatherer {
    Gathering mode{Gathering::none};
    Role role{Role::column_item};
    bool overlay{false};
    bool leading{true};
    bool vertical{false};
    bool based{false};
    bool admitted{false};
    bool row{false};
    std::size_t first{0};
    std::size_t last{0};
    std::size_t at{0};
    std::size_t begin{0};
    std::size_t base{0};
    std::size_t position{0};
    std::size_t index{0};
    std::size_t tables{0};
    GridArrangement::Row line{};
  };

  struct Tail {
    YogaSlot::Style style{};
    composition::FrameSpec frame{};
    std::optional<YogaSlot::Measurement> measured{};
    std::optional<GridArrangement::Layout> gridded{};
    std::vector<GridArrangement::Cell> grid_cells{};
    std::vector<GridArrangement::Row> grid_rows{};
    std::shared_ptr<const components::AnyLayout::Box> arranged{};
    std::vector<CustomArrangement::Member> members{};
    Range children{};
    Spacing spacing{};
    std::size_t portals{0};
    std::size_t bars{0};
    float shrink_weight{1.0F};
    bool grow_blocked{false};
    bool decorated{false};
    bool button{false};
    bool framed{false};
    bool open_width{false};
    bool open_height{false};
    std::optional<YogaSlot::Visit> visit{};
    std::optional<Scope> scope{};
    std::optional<Scope> basis{};
  };

  struct Sync {
    YogaSlot* target{nullptr};
    YGNodeRef result{nullptr};
    Role role{Role::column_item};
    bool tailed{false};
    std::optional<Scope> handed{};
    Role handed_role{Role::column_item};
    Gatherer gatherer{};
  };

  struct TableFrame {
    const composition::Node* node;
    std::size_t position{0};
    Scope scope;
  };

  using SyncWalk = support::FullOrderWalk<NodeAccess, Sync>;

  class Syncer {
   public:
    Syncer(YogaLayout& layout, composition::Tree& tree) noexcept
        : layout_(layout), tree_(tree) {}

    auto enter(const composition::Node& node, const Sync& above)
        -> SyncWalk::Step;

    auto next(const composition::Node& node, Sync& sync)
        -> const composition::Node*;

    void leave(const composition::Node& node, Sync& sync, Sync* above);

   private:
    std::reference_wrapper<YogaLayout> layout_;
    std::reference_wrapper<composition::Tree> tree_;
  };

  struct Reading {
    YogaSlot* held{nullptr};
    std::optional<composition::Rect> placed{};
    std::optional<composition::Rect> bounds{};
    std::optional<composition::Rect> result{};
    bool finished{false};
    std::size_t position{0};
  };

  using ReadWalk = support::FullOrderWalk<NodeAccess, Reading>;

  class Reader {
   public:
    Reader(YogaLayout& layout, composition::Tree& tree) noexcept
        : layout_(layout), tree_(tree) {}

    auto enter(const composition::Node& node, const Reading& above)
        -> ReadWalk::Step;

    auto next(const composition::Node& node, Reading& reading)
        -> const composition::Node*;

    void leave(const composition::Node& node, Reading& reading, Reading* above);

   private:
    std::reference_wrapper<YogaLayout> layout_;
    std::reference_wrapper<composition::Tree> tree_;
  };

  auto sync(composition::Tree& tree, const composition::Node& node,
            const Scope& parent, Role role)
      -> std::expected<YGNodeRef, layout::LayoutError>;

  auto open_sync(composition::Tree& tree, const composition::Node& node,
                 const Sync& above) -> SyncWalk::Step;

  auto advance_sync(composition::Tree& tree, const composition::Node& node,
                    Sync& sync) -> const composition::Node*;

  void close_sync(composition::Tree& tree, const composition::Node& node,
                  Sync& sync, Sync* above);

  void finish_sync(composition::Tree& tree, const composition::Node& node,
                   Sync& sync);

  void deliver(composition::Tree& tree, const composition::Node& child,
               YGNodeRef result, Sync* above);

  void abandon(Sync& sync);

  auto prepare(const composition::Node& node, const Scope& scope, Role role,
               bool overlay) -> std::size_t;

  void begin_children(const composition::Node& node, Sync& sync,
                      const Scope& scope, Role role, bool overlay);

  void begin_decorated(Sync& sync, const Scope& scope, std::size_t base);

  void begin_members(const composition::Node& node, Sync& sync,
                     const Scope& scope, Role role);

  void begin_table(const composition::Node& node, Sync& sync,
                   const Scope& scope);

  void begin_cells(const composition::Node& node, Sync& sync,
                   const Scope& scope, bool vertical);

  auto next_decorated(const composition::Node& node, Sync& sync)
      -> const composition::Node*;

  auto next_table(composition::Tree& tree, Sync& sync)
      -> const composition::Node*;

  void push_cell(const Entry& entry, YGNodeRef result, bool vertical,
                 std::vector<GridArrangement::Cell>& cells);

  auto open_read(composition::Tree& tree, const composition::Node& node)
      -> ReadWalk::Step;

  void close_read(composition::Tree& tree, const composition::Node& node,
                  Reading& reading, Reading* above);

  void finish_read(composition::Tree& tree, const composition::Node& node,
                   Reading& reading);

  auto sync_children(composition::Tree& tree, const composition::Node& node,
                     Scope scope, Role role, bool overlay)
      -> std::expected<Range, layout::LayoutError>;

  auto space_children(composition::Tree& tree, const composition::Node& node,
                      Scope scope, bool vertical, bool automatic, bool guided,
                      float overlap)
      -> std::expected<void, layout::LayoutError>;

  auto automatic_gap(composition::Tree& tree, const composition::Node& before,
                     const composition::Node& after, bool vertical,
                     const theme::Theme& theme) -> float;

  auto view_spacing(composition::Tree& tree, const composition::Node& node,
                    const theme::Theme& theme) -> components::ViewSpacing;

  static auto grid_cell(const composition::Node& cell, YGNodeRef node,
                        bool vertical) -> GridArrangement::Cell;

  static auto table_stretches(const composition::Node& node) -> bool;

  static auto arrangement_of(composition::Tree& tree,
                             const composition::Node& node)
      -> std::optional<std::reference_wrapper<CustomArrangement>>;

  static auto spacing_of(const composition::Node& node,
                         const theme::Theme& theme) -> components::ViewSpacing;

  static void withhold(composition::Tree& tree, const YogaSlot& slot);

  static auto custom(const composition::Node& node) -> bool;

  static auto ranks(const composition::Node& node) -> bool;

  static auto shares(const composition::Node& node) -> bool;

  auto settle_shares(composition::Tree& tree, const composition::Node& node,
                     YogaSlot& slot) -> bool;

  struct Guided {
    std::reference_wrapper<const composition::Node> node;
    std::optional<std::reference_wrapper<const composition::Modifiers>>
        carried{};
  };

  static auto stack_guide(const composition::Node& node)
      -> std::optional<composition::AlignmentKey>;

  static auto declares(const composition::Node& node,
                       const composition::AlignmentKey& key, std::size_t depth)
      -> bool;

  static auto guides_of(const Guided& entry)
      -> std::span<const composition::AlignmentGuide>;

  static auto container(const composition::Node& node) -> bool;

  static void entries_of(
      const composition::Node& node,
      std::optional<std::reference_wrapper<const composition::Modifiers>>
          carried,
      std::vector<Guided>& out);

  auto settle_guides(composition::Tree& tree, const composition::Node& node)
      -> bool;

  struct LayeredGuides {
    std::optional<composition::AlignmentKey> horizontal;
    std::optional<composition::AlignmentKey> vertical;
  };

  static auto layered_guides(const composition::Node& node) -> LayeredGuides;

  static auto guided(const composition::Node& node,
                     const composition::AlignmentKey& key) -> bool;

  auto settle_layered(composition::Tree& tree, const composition::Node& node,
                      YogaSlot& slot) -> bool;

  auto settle_table_guides(composition::Tree& tree,
                           const composition::Node& node) -> bool;

  struct TableRow {
    std::optional<composition::VerticalAlignment> alignment{};
    std::vector<Guided> cells{};
  };

  static void table_cells(const composition::Node& node,
                          std::vector<TableRow>& rows);

  auto shift_cells(composition::Tree& tree, std::span<const Guided> cells,
                   const composition::AlignmentKey& key) -> bool;

  auto leading_offset(YGNodeRef parent, YGNodeRef child) const -> float;

  auto settle_separator(composition::Tree& tree, const composition::Node& node)
      -> bool;

  auto guide_value(composition::Tree& tree, const Guided& entry,
                   const composition::AlignmentKey& key) -> float;

  auto explicit_guide(composition::Tree& tree, const Guided& entry,
                      const composition::AlignmentKey& key,
                      std::size_t depth = guide_depth) -> std::optional<float>;

  auto baselines_of(composition::Tree& tree, const composition::Node& node)
      -> composition::TextBaselines;

  static auto laid_node(composition::Tree& tree, const composition::Node& node)
      -> std::optional<YGNodeRef>;

  auto sync_portals(composition::Tree& tree, bool force)
      -> std::expected<bool, layout::LayoutError>;

  auto read_back(composition::Tree& tree, const composition::Node& node)
      -> std::optional<composition::Rect>;

  void publish(composition::Tree& tree, const composition::Node& node,
               YogaSlot& slot, composition::Rect frame);

  void place(composition::Tree& tree);

  static auto visit_of(const composition::Node& node, const Scope& scope,
                       Role role) noexcept -> YogaSlot::Visit;

  struct RowText {
    std::reference_wrapper<YogaSlot> slot;
    float natural{0.0F};
  };

  std::shared_ptr<YogaSlot::ConfigHandle> config_;
  std::uint64_t id_;
  std::vector<Guided> row_entries_;
  std::vector<RowText> row_texts_;
  TypesetterRef typesetter_;
  provider::SurfaceScale scale_{provider::SurfaceScale::unit()};
  std::optional<layout::Viewport> viewport_;
  std::vector<Entry> portals_;
  std::vector<Entry> entries_;
  std::vector<YGNodeRef> refs_;
  std::vector<TableFrame> tables_;
  std::vector<Tail> tails_;
  SyncWalk sync_walk_;
  ReadWalk read_walk_;
  std::optional<layout::LayoutError> failure_;
  YGNodeRef returned_{nullptr};
  std::optional<composition::Rect> read_;
  std::vector<Bar> bars_;
  std::unordered_map<std::uint64_t, theme::Edges> insets_;
  std::deque<theme::Theme> sized_;
  std::vector<Geometry> geometries_;
  std::uint64_t geometry_keys_{0};
  std::uint64_t calculations_{0};
  std::uint64_t measurements_{0};
  std::uint64_t visits_{0};
  std::uint64_t placements_{0};
  std::uint64_t computations_{0};
  inline static std::atomic<std::uint64_t> computed_{0};
  const text::Typesetter* shaper_{nullptr};
  std::uint64_t settled_{0};
  bool placing_{false};
  bool force_{false};
  bool again_{false};
};

}
