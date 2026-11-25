#include <bridge/yoga/grid_arrangement.hpp>

#include <arisa/components/grid_tracks.hpp>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <utility>

namespace arisa::bridge::yoga {

GridArrangement::GridArrangement(std::shared_ptr<YogaSlot::ConfigHandle> config,
                                 YogaSlot& owner, YGDirection direction)
    : config_(std::move(config)),
      host_(YGNodeNewWithConfig(config_->get())),
      direction_(direction) {
  YGNodeSetContext(host_.get(), &owner);
}

GridArrangement::~GridArrangement() {
  wrappers_.clear();
  if (host_.valid()) {
    YGNodeRemoveAllChildren(host_.get());
  }
  host_.reset();
}

auto GridArrangement::vertical() const noexcept -> bool {
  if (const auto* flow = std::get_if<Flow>(&layout_)) {
    return flow->vertical;
  }
  return true;
}

auto GridArrangement::fraction(
    composition::HorizontalAlignment alignment) noexcept -> float {
  switch (alignment) {
    case composition::HorizontalAlignment::leading:
      return 0.0F;
    case composition::HorizontalAlignment::center:
      return 0.5F;
    case composition::HorizontalAlignment::trailing:
      return 1.0F;
  }
  return 0.5F;
}

auto GridArrangement::fraction(
    composition::VerticalAlignment alignment) noexcept -> float {
  switch (alignment) {
    case composition::VerticalAlignment::top:
    case composition::VerticalAlignment::first_text_baseline:
      return 0.0F;
    case composition::VerticalAlignment::last_text_baseline:
      return 1.0F;
    case composition::VerticalAlignment::center:
      return 0.5F;
    case composition::VerticalAlignment::bottom:
      return 1.0F;
  }
  return 0.5F;
}

auto GridArrangement::update(Layout layout, std::span<const Cell> cells,
                             std::span<const Row> rows, YGDirection direction)
    -> bool {
  const bool same = laid_ && layout_ == layout && direction_ == direction &&
                    std::ranges::equal(cells_, cells) &&
                    std::ranges::equal(rows_, rows);
  if (same && !YGNodeIsDirty(host_.get())) {
    return false;
  }
  if (!same) {
    layout_ = std::move(layout);
    direction_ = direction;
    cells_.assign(cells.begin(), cells.end());
    rows_.assign(rows.begin(), rows.end());
    bounds_.clear();
    for (const Cell& cell : cells_) {
      if (cell.bounds.has_value()) {
        bounds_.insert_or_assign(cell.node, *cell.bounds);
      }
    }
    adopt();
  }
  if (std::holds_alternative<Table>(layout_)) {
    measure_naturals();
    columns();
  }
  natural_ = natural_cross();
  lay(crossed_ ? cross_ : natural_);
  return true;
}

void GridArrangement::adopt() {
  const YGNodeRef host = host_.get();
  YGNodeRemoveAllChildren(host);
  if (wrappers_.size() > cells_.size()) {
    wrappers_.resize(cells_.size());
  }
  for (std::size_t index = 0; index < wrappers_.size(); ++index) {
    const YGNodeRef wrapper = wrappers_[index].get();
    if (YGNodeGetChildCount(wrapper) > 0 &&
        YGNodeGetChild(wrapper, 0) != cells_[index].node) {
      YGNodeRemoveAllChildren(wrapper);
    }
  }
  while (wrappers_.size() < cells_.size()) {
    YogaSlot::NodeHandle wrapper{YGNodeNewWithConfig(config_->get())};
    YGNodeStyleSetPositionType(wrapper.get(), YGPositionTypeAbsolute);
    YGNodeStyleSetFlexDirection(wrapper.get(), YGFlexDirectionColumn);
    YGNodeStyleSetAlignItems(wrapper.get(), YGAlignFlexStart);
    YGNodeStyleSetJustifyContent(wrapper.get(), YGJustifyFlexStart);
    wrappers_.push_back(std::move(wrapper));
  }
  std::size_t placed = 0;
  for (const Row& row : rows_) {
    if (row.node == nullptr) {
      continue;
    }
    if (const YGNodeRef owner = YGNodeGetOwner(row.node)) {
      YGNodeRemoveChild(owner, row.node);
    }
    YGNodeStyleSetPositionType(row.node, YGPositionTypeAbsolute);
    YGNodeInsertChild(host, row.node, placed++);
  }
  for (std::size_t index = 0; index < cells_.size(); ++index) {
    const YGNodeRef wrapper = wrappers_[index].get();
    const YGNodeRef cell = cells_[index].node;
    if (YGNodeGetChildCount(wrapper) == 0) {
      if (const YGNodeRef owner = YGNodeGetOwner(cell)) {
        YGNodeRemoveChild(owner, cell);
      }
      YGNodeInsertChild(wrapper, cell, 0);
    }
    YGNodeInsertChild(host, wrapper, placed++);
  }
  std::erase_if(naturals_, [this](const auto& entry) {
    return std::ranges::none_of(cells_, [&entry](const Cell& cell) {
      return cell.node == entry.first;
    });
  });
  laid_ = false;
}

void GridArrangement::measure_naturals() {
  for (std::size_t index = 0; index < cells_.size(); ++index) {
    const YGNodeRef cell = cells_[index].node;
    const bool known = naturals_.contains(cell) && !YGNodeIsDirty(cell);
    if (known) {
      continue;
    }
    const YGNodeRef wrapper = wrappers_[index].get();
    YGNodeRemoveChild(wrapper, cell);
    YGNodeCalculateLayout(cell, YGUndefined, YGUndefined, direction_);
    const float width = YGNodeLayoutGetWidth(cell) +
                        YGNodeLayoutGetMargin(cell, YGEdgeLeft) +
                        YGNodeLayoutGetMargin(cell, YGEdgeRight);
    const float height = YGNodeLayoutGetHeight(cell) +
                         YGNodeLayoutGetMargin(cell, YGEdgeTop) +
                         YGNodeLayoutGetMargin(cell, YGEdgeBottom);
    naturals_.insert_or_assign(cell, std::array{width, height});
    YGNodeInsertChild(wrapper, cell, 0);
  }
}

auto GridArrangement::natural(YGNodeRef cell) const -> std::array<float, 2> {
  const auto found = naturals_.find(cell);
  if (found == naturals_.end()) {
    return {0.0F, 0.0F};
  }
  return {bounded(cell, found->second[0], true),
          bounded(cell, found->second[1], false)};
}

auto GridArrangement::bounded(YGNodeRef cell, float length,
                              bool horizontal) const -> float {
  const auto found = bounds_.find(cell);
  return found == bounds_.end() ? length
                                : found->second.apply(length, horizontal);
}

auto GridArrangement::Bounds::apply(float natural,
                                    bool horizontal) const noexcept -> float {
  const std::optional<float>& fixed = horizontal ? width : height;
  const std::optional<float>& lower = horizontal ? min_width : min_height;
  const std::optional<float>& upper = horizontal ? max_width : max_height;
  float length = fixed.value_or(natural);
  if (upper.has_value()) {
    length = std::min(length, *upper);
  }
  if (lower.has_value()) {
    length = std::max(length, *lower);
  }
  return length;
}

auto GridArrangement::definite(float cross) const noexcept -> bool {
  return std::isfinite(cross) && cross >= 0.0F;
}

auto GridArrangement::natural_cross() const -> float {
  if (const auto* flow = std::get_if<Flow>(&layout_)) {
    return components::GridTracks::used(flow->items, flow->spacings,
                                        YGUndefined);
  }
  return natural_total_;
}

auto GridArrangement::flexible() const -> bool {
  if (std::holds_alternative<Flow>(layout_)) {
    return true;
  }
  return std::ranges::contains(flexes_, true);
}

void GridArrangement::columns() {
  const auto& table = std::get<Table>(layout_);
  const float spacing = table.column_spacing;
  std::size_t count = 1;
  for (const Row& row : rows_) {
    if (row.spans) {
      continue;
    }
    std::size_t sum = 0;
    for (std::size_t index = row.begin; index < row.end; ++index) {
      sum += cells_[index].span;
    }
    count = std::max(count, sum);
  }
  widths_.assign(count, 0.0F);
  flexes_.assign(count, false);
  aligns_.assign(count, fraction(table.alignment.horizontal));
  first_.assign(cells_.size(), 0);
  const auto clamp_span = [count](std::size_t first, std::uint32_t span) {
    return std::min<std::size_t>(span, count - std::min(first, count - 1));
  };
  for (const Row& row : rows_) {
    if (row.spans) {
      continue;
    }
    std::size_t column = 0;
    for (std::size_t index = row.begin; index < row.end; ++index) {
      first_[index] = std::min(column, count - 1);
      column += cells_[index].span;
    }
  }
  for (const Row& row : rows_) {
    if (row.spans) {
      continue;
    }
    for (std::size_t index = row.begin; index < row.end; ++index) {
      const Cell& cell = cells_[index];
      const std::size_t first = first_[index];
      if (cell.column_alignment.has_value()) {
        aligns_[first] = fraction(*cell.column_alignment);
      }
      const bool sized = !cell.unsized.contains(composition::Axis::horizontal);
      const std::size_t span = clamp_span(first, cell.span);
      if (cell.flexible && sized) {
        for (std::size_t at = first; at < first + span; ++at) {
          flexes_[at] = true;
        }
      }
      if (span == 1 && sized) {
        widths_[first] = std::max(widths_[first], natural(cell.node)[0]);
      }
    }
  }
  for (const Row& row : rows_) {
    if (row.spans) {
      continue;
    }
    for (std::size_t index = row.begin; index < row.end; ++index) {
      const Cell& cell = cells_[index];
      const std::size_t first = first_[index];
      const std::size_t span = clamp_span(first, cell.span);
      if (span < 2 || cell.unsized.contains(composition::Axis::horizontal)) {
        continue;
      }
      float covered = spacing * static_cast<float>(span - 1);
      for (std::size_t at = first; at < first + span; ++at) {
        covered += widths_[at];
      }
      const float need = natural(cell.node)[0] - covered;
      if (need > 0.0F) {
        for (std::size_t at = first; at < first + span; ++at) {
          widths_[at] += need / static_cast<float>(span);
        }
      }
    }
  }
  const auto total = [this, spacing] {
    float sum = spacing * static_cast<float>(widths_.size() - 1);
    for (const float width : widths_) {
      sum += width;
    }
    return sum;
  };
  bool spanning_flexible = false;
  for (const Row& row : rows_) {
    if (!row.spans) {
      continue;
    }
    for (std::size_t index = row.begin; index < row.end; ++index) {
      const Cell& cell = cells_[index];
      if (cell.unsized.contains(composition::Axis::horizontal)) {
        continue;
      }
      spanning_flexible = spanning_flexible || cell.flexible;
      const float need = natural(cell.node)[0] - total();
      if (need > 0.0F) {
        for (float& width : widths_) {
          width += need / static_cast<float>(widths_.size());
        }
      }
    }
  }
  if (spanning_flexible && !std::ranges::contains(flexes_, true)) {
    flexes_.assign(count, true);
  }
  natural_total_ = total();
}

void GridArrangement::place_table(float cross) {
  const auto& table = std::get<Table>(layout_);
  const float spacing = table.column_spacing;
  scratch_.assign(widths_.begin(), widths_.end());
  const auto flexible_count =
      static_cast<std::size_t>(std::ranges::count(flexes_, true));
  if (definite(cross) && cross > natural_total_ && flexible_count > 0) {
    const float extra =
        (cross - natural_total_) / static_cast<float>(flexible_count);
    for (std::size_t column = 0; column < scratch_.size(); ++column) {
      if (flexes_[column]) {
        scratch_[column] += extra;
      }
    }
  }
  float used = spacing * static_cast<float>(scratch_.size() - 1);
  for (const float width : scratch_) {
    used += width;
  }
  extent_ = definite(cross) ? std::max(cross, used) : used;
  const float start =
      definite(cross)
          ? std::max((cross - used) * fraction(table.placement.horizontal),
                     0.0F)
          : 0.0F;
  placed_.assign(cells_.size(), Placed{});
  line_of_.assign(cells_.size(), 0);
  lines_.assign(rows_.size(), Line{});
  const float vertical_default = fraction(table.alignment.vertical);
  for (std::size_t line = 0; line < rows_.size(); ++line) {
    const Row& row = rows_[line];
    const float ay =
        row.alignment.has_value() ? fraction(*row.alignment) : vertical_default;
    for (std::size_t index = row.begin; index < row.end; ++index) {
      const Cell& cell = cells_[index];
      line_of_[index] = line;
      Placed placed{.offset = start,
                    .length = used,
                    .anchor_x = fraction(table.alignment.horizontal),
                    .anchor_y = ay};
      if (!row.spans) {
        const std::size_t first = first_[index];
        const std::size_t span =
            std::min<std::size_t>(cell.span, scratch_.size() - first);
        float offset = start;
        for (std::size_t at = 0; at < first; ++at) {
          offset += scratch_[at] + spacing;
        }
        float length = spacing * static_cast<float>(span - 1);
        for (std::size_t at = first; at < first + span; ++at) {
          length += scratch_[at];
        }
        placed.offset = offset;
        placed.length = length;
        placed.anchor_x = aligns_[first];
      }
      if (cell.anchor.has_value()) {
        placed.anchor_x = cell.anchor->x;
        placed.anchor_y = cell.anchor->y;
      }
      placed_[index] = placed;
    }
  }
}

void GridArrangement::place_flow(float cross) {
  const auto& flow = std::get<Flow>(layout_);
  const components::GridTracks tracks(flow.items, flow.spacings,
                                      definite(cross) ? cross : YGUndefined);
  const std::span<const components::GridTracks::Track> resolved =
      tracks.tracks();
  const std::size_t lanes = std::max<std::size_t>(resolved.size(), 1);
  extent_ = tracks.extent();
  placed_.assign(cells_.size(), Placed{});
  line_of_.assign(cells_.size(), 0);
  lines_.assign((cells_.size() + lanes - 1) / lanes, Line{});
  for (std::size_t index = 0; index < cells_.size(); ++index) {
    const std::size_t lane = index % lanes;
    const components::GridTracks::Track track =
        lane < resolved.size() ? resolved[lane]
                               : components::GridTracks::Track{};
    const composition::Alignment alignment =
        track.alignment.value_or(flow.alignment);
    placed_[index] = Placed{.offset = track.offset,
                            .length = track.length,
                            .anchor_x = fraction(alignment.horizontal),
                            .anchor_y = fraction(alignment.vertical)};
    line_of_[index] = index / lanes;
  }
}

auto GridArrangement::row_gap(std::size_t row) const -> float {
  if (const auto* flow = std::get_if<Flow>(&layout_)) {
    return flow->spacing;
  }
  const auto& table = std::get<Table>(layout_);
  if (table.row_spacing.has_value()) {
    return *table.row_spacing;
  }
  const auto texts = [this](const Row& each) {
    return each.end > each.begin &&
           std::ranges::all_of(
               std::span{cells_}.subspan(each.begin, each.end - each.begin),
               &Cell::text);
  };
  return texts(rows_[row - 1]) && texts(rows_[row]) ? table.text_spacing
                                                    : table.spacing;
}

void GridArrangement::position(YGNodeRef node, float cross_offset,
                               float cross_length,
                               std::optional<float> main_offset,
                               std::optional<float> main_length) const {
  const bool down = vertical();
  const YGEdge across = down ? YGEdgeStart : YGEdgeTop;
  const YGEdge along = down ? YGEdgeTop : YGEdgeStart;
  YGNodeStyleSetPosition(node, across, cross_offset);
  YGNodeStyleSetPosition(node, along, main_offset.value_or(0.0F));
  if (down) {
    YGNodeStyleSetWidth(node, cross_length);
    if (main_length.has_value()) {
      YGNodeStyleSetHeight(node, *main_length);
    } else {
      YGNodeStyleSetHeightAuto(node);
    }
  } else {
    YGNodeStyleSetHeight(node, cross_length);
    if (main_length.has_value()) {
      YGNodeStyleSetWidth(node, *main_length);
    } else {
      YGNodeStyleSetWidthAuto(node);
    }
  }
}

void GridArrangement::lay(float cross) {
  if (std::holds_alternative<Table>(layout_)) {
    place_table(cross);
  } else {
    place_flow(cross);
  }
  const bool down = vertical();
  const YGNodeRef host = host_.get();
  for (std::size_t index = 0; index < cells_.size(); ++index) {
    const YGNodeRef wrapper = wrappers_[index].get();
    const Placed& placed = placed_[index];
    YGNodeStyleSetPadding(wrapper, YGEdgeAll, 0.0F);
    position(wrapper, placed.offset, std::max(placed.length, 0.0F), 0.0F,
             std::nullopt);
  }
  for (const Row& row : rows_) {
    if (row.node != nullptr) {
      position(row.node, 0.0F, 0.0F, 0.0F, 0.0F);
    }
  }
  if (down) {
    YGNodeStyleSetWidth(host, extent_);
    YGNodeStyleSetHeightAuto(host);
    YGNodeCalculateLayout(host, extent_, YGUndefined, direction_);
  } else {
    YGNodeStyleSetHeight(host, extent_);
    YGNodeStyleSetWidthAuto(host);
    YGNodeCalculateLayout(host, YGUndefined, extent_, direction_);
  }
  const auto main_of = [down](YGNodeRef cell) {
    return down ? YGNodeLayoutGetHeight(cell) +
                      YGNodeLayoutGetMargin(cell, YGEdgeTop) +
                      YGNodeLayoutGetMargin(cell, YGEdgeBottom)
                : YGNodeLayoutGetWidth(cell) +
                      YGNodeLayoutGetMargin(cell, YGEdgeLeft) +
                      YGNodeLayoutGetMargin(cell, YGEdgeRight);
  };
  const auto cross_of = [down](YGNodeRef cell) {
    return down ? YGNodeLayoutGetWidth(cell) +
                      YGNodeLayoutGetMargin(cell, YGEdgeLeft) +
                      YGNodeLayoutGetMargin(cell, YGEdgeRight)
                : YGNodeLayoutGetHeight(cell) +
                      YGNodeLayoutGetMargin(cell, YGEdgeTop) +
                      YGNodeLayoutGetMargin(cell, YGEdgeBottom);
  };
  const composition::Axis main_axis =
      down ? composition::Axis::vertical : composition::Axis::horizontal;
  for (std::size_t index = 0; index < cells_.size(); ++index) {
    if (cells_[index].unsized.contains(main_axis)) {
      continue;
    }
    Line& line = lines_[line_of_[index]];
    const YGNodeRef cell = cells_[index].node;
    line.extent = std::max(line.extent, bounded(cell, main_of(cell), !down));
  }
  float cursor = 0.0F;
  if (const auto* flow = std::get_if<Flow>(&layout_)) {
    cursor = flow->leading;
  }
  float natural_main = cursor;
  for (std::size_t line = 0; line < lines_.size(); ++line) {
    if (line > 0) {
      natural_main += row_gap(line);
    }
    natural_main += lines_[line].extent;
  }
  main_ = natural_main;
  cursor += lead();
  for (std::size_t line = 0; line < lines_.size(); ++line) {
    if (line > 0) {
      cursor += row_gap(line);
    }
    lines_[line].offset = cursor;
    cursor += lines_[line].extent;
  }
  if (const auto* flow = std::get_if<Flow>(&layout_)) {
    main_ += flow->trailing;
  }
  for (std::size_t index = 0; index < cells_.size(); ++index) {
    const YGNodeRef wrapper = wrappers_[index].get();
    const YGNodeRef cell = cells_[index].node;
    const Placed& placed = placed_[index];
    const Line& line = lines_[line_of_[index]];
    const float length = std::max(placed.length, 0.0F);
    const float across_anchor = down ? placed.anchor_x : placed.anchor_y;
    const float along_anchor = down ? placed.anchor_y : placed.anchor_x;
    const float across_pad =
        std::max((length - cross_of(cell)) * across_anchor, 0.0F);
    const float along_pad =
        cells_[index].fills
            ? 0.0F
            : std::max((line.extent - main_of(cell)) * along_anchor, 0.0F);
    YGNodeStyleSetPadding(wrapper, down ? YGEdgeStart : YGEdgeTop, across_pad);
    YGNodeStyleSetPadding(wrapper, down ? YGEdgeTop : YGEdgeStart, along_pad);
    position(wrapper, placed.offset, length, line.offset, line.extent);
  }
  for (std::size_t line = 0; line < rows_.size() && line < lines_.size();
       ++line) {
    if (rows_[line].node != nullptr) {
      position(rows_[line].node, 0.0F, extent_, lines_[line].offset,
               lines_[line].extent);
    }
  }
  const float hosted = main_ + lead();
  if (down) {
    YGNodeStyleSetHeight(host, hosted);
    YGNodeCalculateLayout(host, extent_, hosted, direction_);
  } else {
    YGNodeStyleSetWidth(host, hosted);
    YGNodeCalculateLayout(host, hosted, extent_, direction_);
  }
  cross_ = cross;
  crossed_ = true;
  laid_ = true;
}

auto GridArrangement::lead() const -> float {
  const auto* table = std::get_if<Table>(&layout_);
  if (table == nullptr || !definite(along_) || along_ <= main_) {
    return 0.0F;
  }
  return (along_ - main_) * fraction(table->placement.vertical);
}

auto GridArrangement::settle(float cross, float along) -> bool {
  if (laid_ && cross == cross_ && along == along_ &&
      !YGNodeIsDirty(host_.get())) {
    return false;
  }
  along_ = along;
  const float before = main_;
  const float wide = extent_;
  lay(cross);
  return main_ != before || extent_ != wide;
}

auto GridArrangement::measure(float width, YGMeasureMode width_mode,
                              float height, YGMeasureMode height_mode) const
    -> YGSize {
  const bool down = vertical();
  const float offered = down ? width : height;
  const YGMeasureMode mode = down ? width_mode : height_mode;
  float cross = natural_;
  if (mode == YGMeasureModeExactly) {
    cross = offered;
  } else if (mode == YGMeasureModeAtMost) {
    if (const auto* flow = std::get_if<Flow>(&layout_)) {
      cross = std::max(offered, components::GridTracks::used(
                                    flow->items, flow->spacings, offered));
    } else {
      cross = flexible() ? offered : std::min(offered, natural_);
    }
  }
  if (down) {
    return YGSize{.width = cross, .height = main_};
  }
  return YGSize{.width = main_, .height = cross};
}

}
