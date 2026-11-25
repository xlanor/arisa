#include <arisa/components/environment.hpp>
#include <arisa/components/grid_tracks.hpp>
#include <arisa/components/lazy_grid.hpp>
#include <arisa/components/reorder_cell.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/retained.hpp>

#include <algorithm>
#include <memory>
#include <utility>

namespace arisa::components {

const composition::StateKey<LazyGrid::Content::Window>
    LazyGrid::Content::window_key{"arisa.lazy-grid-window"};

auto LazyGrid::arrangement(const LazyVGridOptions& options) -> Arrangement {
  return Arrangement{
      .axis = composition::Axis::vertical,
      .items = options.columns.empty() ? std::vector<GridItem>{GridItem{}}
                                       : options.columns,
      .alignment = Alignment{.horizontal = options.alignment,
                             .vertical = VerticalAlignment::center},
      .spacing = options.spacing,
      .overscan = std::max(options.overscan, 0.0F),
      .retention = options.retention,
      .estimated_extent = std::max(options.estimated_extent, 1.0F)};
}

auto LazyGrid::arrangement(const LazyHGridOptions& options) -> Arrangement {
  return Arrangement{
      .axis = composition::Axis::horizontal,
      .items = options.rows.empty() ? std::vector<GridItem>{GridItem{}}
                                    : options.rows,
      .alignment = Alignment{.horizontal = HorizontalAlignment::center,
                             .vertical = options.alignment},
      .spacing = options.spacing,
      .overscan = std::max(options.overscan, 0.0F),
      .retention = options.retention,
      .estimated_extent = std::max(options.estimated_extent, 1.0F)};
}

auto LazyGrid::spacing(const Arrangement& arrangement,
                       const theme::Theme& theme) noexcept -> float {
  if (arrangement.spacing.has_value()) {
    return std::max(theme.space(*arrangement.spacing), 0.0F);
  }
  return theme.scaled(theme::metric::stack_spacing);
}

LazyGrid::Cells::Cells(Arrangement arrangement,
                       std::vector<composition::Element> cells, float leading,
                       float trailing)
    : Element(std::make_unique<Flow>(std::move(arrangement), leading, trailing),
              std::move(cells)) {}

auto LazyGrid::Flow::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Flow>();
}

auto LazyGrid::Flow::paints_like(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Flow&>(other);
  return arrangement_ == theirs.arrangement_ && leading_ == theirs.leading_ &&
         trailing_ == theirs.trailing_;
}

auto LazyGrid::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto LazyGrid::Content::composes() const noexcept -> bool { return true; }

auto LazyGrid::Content::reveals() const noexcept -> bool { return true; }

auto LazyGrid::Content::count() const noexcept -> std::size_t {
  std::size_t total = 0;
  for (const auto& source : sources_) {
    if (source != nullptr) {
      total += source->count();
    }
  }
  return total;
}

auto LazyGrid::Content::key(std::size_t index) const
    -> composition::ElementKey {
  for (const auto& source : sources_) {
    if (source == nullptr) {
      continue;
    }
    if (index < source->count()) {
      return source->key(index);
    }
    index -= source->count();
  }
  return composition::ElementKey{0};
}

auto LazyGrid::Content::element(std::size_t index) const
    -> composition::Element {
  for (const auto& source : sources_) {
    if (source == nullptr) {
      continue;
    }
    if (index < source->count()) {
      return source->element(index);
    }
    index -= source->count();
  }
  return Cells(arrangement_, {});
}

auto LazyGrid::Content::retained(std::size_t from, std::size_t to) const
    -> composition::Element {
  return composition::Retained{
      to - from,
      [this, from](std::size_t offset) { return key(from + offset); },
      [this, from](std::size_t offset) { return element(from + offset); }};
}

auto LazyGrid::Content::guess() const noexcept -> std::size_t {
  return std::max<std::size_t>(arrangement_.items.size(), 1);
}

auto LazyGrid::Content::lines(const Window& window) const noexcept
    -> std::size_t {
  const std::size_t lanes = std::max<std::size_t>(window.lanes, 1);
  return (count() + lanes - 1) / lanes;
}

auto LazyGrid::Content::estimate(const Window& window) const noexcept -> float {
  if (window.known > 0) {
    return window.known_total / static_cast<float>(window.known);
  }
  return window.seed > 0.0F ? window.seed : arrangement_.estimated_extent;
}

auto LazyGrid::Content::extent(const Window& window,
                               std::size_t line) const noexcept -> float {
  if (line < window.extents.size() && window.extents[line] >= 0.0F) {
    return window.extents[line];
  }
  return estimate(window);
}

auto LazyGrid::Content::span(const Window& window, std::size_t from,
                             std::size_t to) const noexcept -> float {
  float total = 0.0F;
  for (std::size_t line = from; line < to; ++line) {
    total += extent(window, line) + window.spacing;
  }
  return total;
}

void LazyGrid::Content::reset(Window& window, std::size_t lines) {
  if (window.known > 0) {
    window.seed = window.known_total / static_cast<float>(window.known);
  }
  window.extents.assign(lines, -1.0F);
  window.known = 0;
  window.known_total = 0.0F;
}

auto LazyGrid::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  Window& window = context.retained(window_key);
  const theme::Theme& theme = context.environment(Environment::theme);
  window.spacing = LazyGrid::spacing(arrangement_, theme);
  if (window.lanes == 0) {
    window.lanes = guess();
  }
  const std::size_t total = count();
  const std::size_t lanes = window.lanes;
  const std::size_t rows = lines(window);
  if (window.extents.size() != rows) {
    window.extents.resize(rows, -1.0F);
    window.known = 0;
    window.known_total = 0.0F;
    for (const float each : window.extents) {
      if (each >= 0.0F) {
        ++window.known;
        window.known_total += each;
      }
    }
  }
  if (!window.ready) {
    window.lo = 0;
    window.hi = std::min(rows, initial_lines);
    window.ready = true;
  }
  window.hi = std::min(window.hi, rows);
  window.lo = std::min(window.lo, window.hi);
  if (window.lo == window.hi && rows > 0) {
    window.lo = std::min(window.lo, rows - 1);
    window.hi = window.lo + 1;
  }
  const std::vector<LazyStack::Run> runs =
      window.built_lanes == lanes
          ? LazyStack::repeated(sources_, window.built_from,
                                window.built_lo * lanes,
                                window.built_hi * lanes)
          : std::vector<LazyStack::Run>{};
  window.built_lo = window.lo;
  window.built_hi = window.hi;
  window.built_lanes = lanes;
  window.built_from = sources_;
  const std::size_t first = window.lo * lanes;
  const std::size_t last = std::min(window.hi * lanes, total);
  std::vector<composition::Element> cells;
  cells.reserve(last - first + 1);
  std::size_t index = first;
  for (const LazyStack::Run& run : runs) {
    const std::size_t from = std::clamp(run.lo, index, last);
    const std::size_t to = std::clamp(run.hi, from, last);
    if (to == from) {
      continue;
    }
    for (; index < from; ++index) {
      cells.push_back(element(index));
    }
    cells.push_back(retained(from, to));
    index = to;
  }
  for (; index < last; ++index) {
    cells.push_back(element(index));
  }
  const float leading = window.lo > 0 ? span(window, 0, window.lo) : 0.0F;
  const float trailing =
      window.hi < rows ? span(window, window.hi, rows) : 0.0F;
  std::vector<composition::Element> produced;
  produced.push_back(
      Cells(arrangement_, std::move(cells), leading, trailing)
          .environment(
              ReorderCell::lanes,
              ReorderCell::Lanes{.axis = arrangement_.axis, .count = lanes}));
  return produced;
}

auto LazyGrid::Content::flow_of(const composition::Node& node)
    -> std::optional<std::reference_wrapper<const composition::Node>> {
  for (const composition::Node& child : node.children()) {
    if (child.phase() == composition::NodeLifecyclePhase::live ||
        child.phase() == composition::NodeLifecyclePhase::mounting) {
      return std::cref(child);
    }
  }
  return std::nullopt;
}

void LazyGrid::Content::measure(Window& window,
                                const composition::Node& node) const {
  const auto flow = flow_of(node);
  if (!flow.has_value() || window.built_lanes != window.lanes ||
      window.lanes == 0) {
    return;
  }
  const std::size_t lanes = window.lanes;
  std::size_t index = window.built_lo * lanes;
  std::optional<std::size_t> line;
  float tallest = 0.0F;
  const auto commit = [&window, &line, &tallest] {
    if (!line.has_value() || *line >= window.extents.size()) {
      return;
    }
    float& slot = window.extents[*line];
    if (slot < 0.0F) {
      ++window.known;
    } else {
      window.known_total -= slot;
    }
    slot = tallest;
    window.known_total += tallest;
  };
  for (const composition::Node& child : flow->get().children()) {
    if (child.phase() == composition::NodeLifecyclePhase::departing ||
        !child.key().has_value()) {
      continue;
    }
    const std::size_t at = index / lanes;
    if (!line.has_value() || at != *line) {
      commit();
      line = at;
      tallest = 0.0F;
    }
    const composition::Rect& frame = child.frame();
    tallest = std::max(tallest, vertical() ? frame.height : frame.width);
    ++index;
  }
  commit();
}

auto LazyGrid::Content::relane(composition::Context& context, Window& window,
                               const composition::Node& node) const -> bool {
  const auto flow = flow_of(node);
  if (!flow.has_value()) {
    return false;
  }
  const composition::Rect& frame = flow->get().frame();
  const float across = vertical() ? frame.width : frame.height;
  if (across <= 0.0F || across == window.across) {
    return false;
  }
  window.across = across;
  const theme::Theme& theme = context.environment(Environment::theme);
  const std::size_t lanes = std::max<std::size_t>(
      GridTracks::count(arrangement_.items, across, theme), 1);
  if (lanes == window.lanes) {
    return false;
  }
  const std::size_t old = std::max<std::size_t>(window.lanes, 1);
  const std::size_t first = window.lo * old;
  const std::size_t shown = std::max<std::size_t>(window.hi - window.lo, 1);
  window.lanes = lanes;
  const std::size_t rows = lines(window);
  reset(window, rows);
  window.lo = std::min(first / lanes, rows > 0 ? rows - 1 : 0);
  window.hi =
      std::min(rows, window.lo + std::max(shown * old / lanes, std::size_t{1}));
  if (window.kept.has_value()) {
    const std::size_t line = *window.kept / lanes;
    window.lo = std::min(window.lo, line);
    window.hi = std::max(window.hi, std::min(rows, line + 1));
  }
  context.request_frame();
  return true;
}

auto LazyGrid::Content::needed(const Window& window, float from, float to) const
    -> Range {
  const std::size_t total = window.extents.size();
  Range range{.lo = total, .hi = 0};
  float cursor = 0.0F;
  for (std::size_t line = 0; line < total; ++line) {
    const float length = extent(window, line);
    if (cursor + length > from && cursor < to) {
      range.lo = std::min(range.lo, line);
      range.hi = std::max(range.hi, line + 1);
    }
    if (cursor >= to) {
      break;
    }
    cursor += length + window.spacing;
  }
  if (range.lo >= range.hi) {
    if (total == 0) {
      return Range{};
    }
    const std::size_t edge = from > cursor ? total - 1 : 0;
    return Range{.lo = edge, .hi = edge + 1};
  }
  return range;
}

auto LazyGrid::Content::reveal(composition::Context& context,
                               const composition::Node& node,
                               const composition::Rect& visible) -> bool {
  Window& window = context.retained(window_key);
  if (!window.ready) {
    return false;
  }
  if (relane(context, window, node)) {
    return true;
  }
  measure(window, node);
  const auto flow = flow_of(node);
  const composition::Rect origin =
      flow.has_value() ? flow->get().frame() : node.frame();
  const float from =
      (vertical() ? visible.y - origin.y : visible.x - origin.x) -
      arrangement_.overscan;
  const float to = (vertical() ? visible.y + visible.height - origin.y
                               : visible.x + visible.width - origin.x) +
                   arrangement_.overscan;
  const Range need = needed(window, from, to);
  if (need.lo >= need.hi) {
    return false;
  }
  const std::size_t total = window.extents.size();
  const std::size_t batch = std::max<std::size_t>(arrangement_.retention, 1);
  std::size_t lo = window.lo;
  std::size_t hi = window.hi;
  if (need.lo < lo) {
    lo = need.lo > batch ? need.lo - batch : 0;
  }
  if (need.hi > hi) {
    hi = std::min(total, need.hi + batch);
  }
  if (need.lo > lo + (2 * batch)) {
    lo = need.lo - batch;
  }
  if (hi > need.hi + (2 * batch)) {
    hi = need.hi + batch;
  }
  if (window.kept.has_value()) {
    const std::size_t line =
        *window.kept / std::max<std::size_t>(window.lanes, 1);
    if (line >= need.lo && line < need.hi) {
      window.kept.reset();
    } else {
      lo = std::min(lo, line);
      hi = std::max(hi, std::min(total, line + 1));
    }
  }
  if (lo == window.lo && hi == window.hi) {
    return false;
  }
  window.lo = lo;
  window.hi = hi;
  context.request_frame();
  return true;
}

auto LazyGrid::Content::materialize(composition::Context& context,
                                    const composition::Node&,
                                    composition::MoveCommandDirection direction)
    -> bool {
  Window& window = context.retained(window_key);
  const bool forward =
      vertical() ? direction == composition::MoveCommandDirection::down
                 : direction == composition::MoveCommandDirection::right;
  const bool backward =
      vertical() ? direction == composition::MoveCommandDirection::up
                 : direction == composition::MoveCommandDirection::left;
  const std::size_t lanes = std::max<std::size_t>(window.lanes, 1);
  if (forward && window.hi < lines(window)) {
    window.kept = window.hi * lanes;
    ++window.hi;
    context.request_frame();
    return true;
  }
  if (backward && window.lo > 0) {
    --window.lo;
    window.kept = window.lo * lanes;
    context.request_frame();
    return true;
  }
  return false;
}

auto LazyGrid::Content::locate(composition::Context& context,
                               const composition::Node& node,
                               const composition::ElementKey& wanted)
    -> std::optional<composition::Rect> {
  Window& window = context.retained(window_key);
  const std::size_t total = count();
  std::optional<std::size_t> found;
  for (std::size_t index = 0; index < total; ++index) {
    if (key(index) == wanted) {
      found = index;
      break;
    }
  }
  const std::size_t rows = lines(window);
  if (!found.has_value() || window.extents.size() != rows) {
    return std::nullopt;
  }
  const std::size_t lanes = std::max<std::size_t>(window.lanes, 1);
  const std::size_t index = *found;
  const std::size_t line = index / lanes;
  window.kept = index;
  if (line < window.lo || line >= window.hi) {
    window.lo =
        line > arrangement_.retention ? line - arrangement_.retention : 0;
    window.hi = std::min(rows, line + located_lines);
    context.request_frame();
  }
  const auto flow = flow_of(node);
  const composition::Rect origin =
      flow.has_value() ? flow->get().frame() : node.frame();
  const float along = span(window, 0, line);
  const float length = extent(window, line);
  const theme::Theme& theme = context.environment(Environment::theme);
  const float across = vertical() ? origin.width : origin.height;
  const GridTracks tracks(arrangement_.items,
                          GridTracks::spacings(arrangement_.items, theme),
                          across);
  const std::size_t lane = index % lanes;
  const bool matched = tracks.count() == lanes;
  const float offset = matched ? tracks.tracks()[lane].offset : 0.0F;
  const float breadth = matched ? tracks.tracks()[lane].length : across;
  if (vertical()) {
    return composition::Rect{.x = origin.x + offset,
                             .y = origin.y + along,
                             .width = breadth,
                             .height = length};
  }
  return composition::Rect{.x = origin.x + along,
                           .y = origin.y + offset,
                           .width = length,
                           .height = breadth};
}

auto LazyGrid::Content::page_stop(composition::Context& context,
                                  const composition::Node& node, float position)
    -> std::optional<float> {
  Window& window = context.retained(window_key);
  const auto flow = flow_of(node);
  if (!window.ready || !flow.has_value()) {
    return std::nullopt;
  }
  const composition::Rect& origin = flow->get().frame();
  const float start = vertical() ? origin.y : origin.x;
  const float local = position - start;
  if (local < 0.0F) {
    return std::nullopt;
  }
  float cursor = 0.0F;
  for (std::size_t line = 0; line < window.extents.size(); ++line) {
    const float end = cursor + extent(window, line);
    if (local < end) {
      return start + cursor;
    }
    cursor = end + window.spacing;
    if (local < cursor) {
      return start + cursor;
    }
  }
  return std::nullopt;
}

}
