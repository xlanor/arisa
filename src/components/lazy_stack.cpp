#include <arisa/components/environment.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/pinned_view.hpp>
#include <arisa/components/reorder_cell.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/node_lifecycle_phase.hpp>
#include <arisa/composition/retained.hpp>
#include <arisa/theme/theme.hpp>

#include <algorithm>
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>

namespace arisa::components {

const composition::StateKey<LazyStack::Content::Window>
    LazyStack::Content::window_key{"arisa.lazy-window"};

auto LazyStack::arrangement(const LazyVStackOptions& options) noexcept
    -> Arrangement {
  return Arrangement{
      .axis = composition::Axis::vertical,
      .horizontal = options.alignment,
      .vertical = VerticalAlignment::center,
      .spacing = options.spacing,
      .overscan = std::max(options.overscan, 0.0F),
      .retention = options.retention,
      .estimated_extent = std::max(options.estimated_extent, 1.0F)};
}

auto LazyStack::arrangement(const LazyHStackOptions& options) noexcept
    -> Arrangement {
  return Arrangement{
      .axis = composition::Axis::horizontal,
      .horizontal = HorizontalAlignment::center,
      .vertical = options.alignment,
      .spacing = options.spacing,
      .overscan = std::max(options.overscan, 0.0F),
      .retention = options.retention,
      .estimated_extent = std::max(options.estimated_extent, 1.0F)};
}

auto LazyStack::source_of(const composition::Element& each)
    -> std::shared_ptr<const ForEach::Source> {
  const composition::ElementContent& content = each.content();
  if (content.type() != composition::ElementType::of<ForEach::Content>()) {
    return nullptr;
  }
  const auto& found = static_cast<const ForEach::Content&>(content);
  return ReorderCell::wrap(found.source(), found.edits().move);
}

auto LazyStack::repeated(const Sources& now, const Sources& before,
                         std::size_t from, std::size_t to) -> std::vector<Run> {
  std::vector<Run> runs;
  if (now.size() != before.size()) {
    return runs;
  }
  std::size_t offset = 0;
  for (const auto& [fresh, held] : std::views::zip(now, before)) {
    const std::size_t count = fresh != nullptr ? fresh->count() : 0;
    if (count != (held != nullptr ? held->count() : 0)) {
      return {};
    }
    const std::size_t lo = std::clamp(from, offset, offset + count);
    const std::size_t hi = std::clamp(to, lo, offset + count);
    for (std::size_t index = lo; index < hi; ++index) {
      const std::size_t local = index - offset;
      if (fresh != held && (fresh->key(local) != held->key(local) ||
                            !fresh->same_item(local, *held, local))) {
        continue;
      }
      if (!runs.empty() && runs.back().hi == index) {
        runs.back().hi = index + 1;
      } else {
        runs.push_back(Run{.lo = index, .hi = index + 1});
      }
    }
    offset += count;
  }
  return runs;
}

auto LazyStack::sections(
    std::vector<GridSection>& sections, PinnedScrollableViews pinned,
    composition::Axis axis, theme::Space spacing,
    const std::function<composition::Element(Sources)>& build)
    -> std::vector<composition::Element> {
  std::vector<composition::Element> column;
  for (std::size_t index = 0; index < sections.size(); ++index) {
    place(sections[index], index, pinned, axis, spacing, build, column);
  }
  return column;
}

auto LazyStack::composed(Parts& parts, PinnedScrollableViews pinned,
                         composition::Axis axis, theme::Space spacing,
                         const Build& build)
    -> std::vector<composition::Element> {
  std::vector<composition::Element> column;
  Sources run;
  std::size_t runs = 0;
  std::size_t grouped = 0;
  const auto flush = [&run, &runs, &column, &build] {
    const bool filled = std::ranges::any_of(
        run, [](const std::shared_ptr<const ForEach::Source>& source) {
          return source != nullptr && source->count() > 0;
        });
    if (filled) {
      column.push_back(build(std::exchange(run, {}))
                           .id(composition::ElementKey{
                               std::format("arisa.lazy.run.{}", runs)}));
      ++runs;
    }
    run.clear();
  };
  for (Part& part : parts) {
    if (auto* source =
            std::get_if<std::shared_ptr<const ForEach::Source>>(&part)) {
      run.push_back(std::move(*source));
    } else if (auto* section = std::get_if<GridSection>(&part)) {
      flush();
      place(*section, grouped++, pinned, axis, spacing, build, column);
    } else {
      auto& element = std::get<composition::Element>(part);
      if (auto found = source_of(element); found != nullptr) {
        run.push_back(std::move(found));
        continue;
      }
      flush();
      column.push_back(std::move(element));
    }
  }
  flush();
  return column;
}

void LazyStack::place(GridSection& section, std::size_t index,
                      PinnedScrollableViews pinned, composition::Axis axis,
                      theme::Space spacing, const Build& build,
                      std::vector<composition::Element>& column) {
  const bool vertical = axis == composition::Axis::vertical;
  const auto keyed = [](composition::Element element, std::string_view kind,
                        std::size_t at) {
    if (element.key().has_value()) {
      return element;
    }
    return std::move(element).id(
        composition::ElementKey{std::format("arisa.grid.{}.{}", kind, at)});
  };
  std::optional<composition::Element> header = section.take_header();
  std::optional<composition::Element> footer = section.take_footer();
  composition::Element body =
      build(Sources{section.source()}).id(static_cast<std::int64_t>(index));
  const bool pins_header =
      header.has_value() &&
      pinned.contains(PinnedScrollableViews::section_headers);
  const bool pins_footer =
      footer.has_value() &&
      pinned.contains(PinnedScrollableViews::section_footers);
  if (!pins_header && !pins_footer) {
    if (header.has_value()) {
      column.push_back(keyed(std::move(*header), "header", index));
    }
    column.push_back(std::move(body));
    if (footer.has_value()) {
      column.push_back(keyed(std::move(*footer), "footer", index));
    }
    return;
  }
  std::vector<composition::Element> parts;
  if (header.has_value()) {
    composition::Element shown = keyed(std::move(*header), "header", index);
    parts.push_back(pins_header ? composition::Element{PinnedView(
                                      std::move(shown), axis, false)}
                                : std::move(shown));
  }
  parts.push_back(std::move(body));
  if (footer.has_value()) {
    composition::Element shown = keyed(std::move(*footer), "footer", index);
    parts.push_back(pins_footer ? composition::Element{PinnedView(
                                      std::move(shown), axis, true)}
                                : std::move(shown));
  }
  composition::Element grouped =
      vertical ? composition::Element{VStack(VStackOptions{.spacing = spacing},
                                             std::move(parts))
                                          .frame({.max_width =
                                                      composition::infinity})}
               : composition::Element{
                     HStack(HStackOptions{.spacing = spacing}, std::move(parts))
                         .frame({.max_height = composition::infinity})};
  column.push_back(std::move(grouped).id(
      composition::ElementKey{std::format("arisa.grid.section.{}", index)}));
}

auto LazyStack::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto LazyStack::Content::composes() const noexcept -> bool { return true; }

auto LazyStack::Content::reveals() const noexcept -> bool { return true; }

auto LazyStack::Content::count() const noexcept -> std::size_t {
  std::size_t total = 0;
  for (const auto& source : sources_) {
    if (source != nullptr) {
      total += source->count();
    }
  }
  return total;
}

auto LazyStack::Content::key(std::size_t index) const
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

auto LazyStack::Content::element(std::size_t index) const
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
  return placeholder(0.0F);
}

auto LazyStack::Content::retained(std::size_t from, std::size_t to) const
    -> composition::Element {
  return composition::Retained{
      to - from,
      [this, from](std::size_t offset) { return key(from + offset); },
      [this, from](std::size_t offset) { return element(from + offset); }};
}

auto LazyStack::Content::estimate(const Window& window) const noexcept
    -> float {
  if (window.known == 0) {
    return arrangement_.estimated_extent;
  }
  return window.known_total / static_cast<float>(window.known);
}

auto LazyStack::Content::extent(const Window& window,
                                std::size_t index) const noexcept -> float {
  if (index < window.extents.size() && window.extents[index] >= 0.0F) {
    return window.extents[index];
  }
  return estimate(window);
}

auto LazyStack::Content::span(const Window& window, std::size_t from,
                              std::size_t to) const noexcept -> float {
  float total = 0.0F;
  for (std::size_t index = from; index < to; ++index) {
    total += extent(window, index) + window.spacing;
  }
  return total;
}

auto LazyStack::Content::position(const Window& window,
                                  std::size_t index) const noexcept -> float {
  return span(window, 0, index);
}

auto LazyStack::Content::placeholder(float length) const
    -> composition::Element {
  const float kept = std::max(length, 0.0F);
  if (vertical()) {
    return VStack().frame({.height = kept});
  }
  return HStack().frame({.width = kept});
}

auto LazyStack::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  Window& window = context.retained(window_key);
  const theme::Theme& theme = context.environment(Environment::theme);
  window.spacing = std::max(theme.space(arrangement_.spacing), 0.0F);
  const std::size_t total = count();
  if (window.extents.size() != total) {
    window.extents.resize(total, -1.0F);
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
    window.hi = std::min(total, initial_count);
    window.ready = true;
  }
  window.hi = std::min(window.hi, total);
  window.lo = std::min(window.lo, window.hi);
  if (window.lo == window.hi && total > 0) {
    window.lo = std::min(window.lo, total - 1);
    window.hi = window.lo + 1;
  }
  const std::vector<Run> runs =
      repeated(sources_, window.built_from, window.built_lo, window.built_hi);
  window.built_lo = window.lo;
  window.built_hi = window.hi;
  window.built_from = sources_;
  std::vector<composition::Element> children;
  children.reserve(window.hi - window.lo + 2);
  if (window.lo > 0) {
    children.push_back(
        placeholder(span(window, 0, window.lo) - window.spacing));
  }
  std::size_t index = window.lo;
  for (const Run& run : runs) {
    const std::size_t from = std::clamp(run.lo, index, window.hi);
    const std::size_t to = std::clamp(run.hi, from, window.hi);
    if (to == from) {
      continue;
    }
    for (; index < from; ++index) {
      children.push_back(element(index));
    }
    children.push_back(retained(from, to));
    index = to;
  }
  for (; index < window.hi; ++index) {
    children.push_back(element(index));
  }
  if (window.hi < total) {
    children.push_back(
        placeholder(span(window, window.hi, total) - window.spacing));
  }
  std::vector<composition::Element> produced;
  if (vertical()) {
    produced.push_back(
        VStack(VStackOptions{.alignment = arrangement_.horizontal,
                             .spacing = arrangement_.spacing},
               std::move(children))
            .frame({.max_width = composition::infinity}));
  } else {
    produced.push_back(
        HStack(HStackOptions{.alignment = arrangement_.vertical,
                             .spacing = arrangement_.spacing},
               std::move(children))
            .frame({.max_height = composition::infinity})
            .environment(
                ReorderCell::lanes,
                ReorderCell::Lanes{.axis = composition::Axis::horizontal,
                                   .count = 1}));
  }
  return produced;
}

auto LazyStack::Content::stack_of(const composition::Node& node)
    -> std::optional<std::reference_wrapper<const composition::Node>> {
  for (const composition::Node& child : node.children()) {
    if (child.phase() == composition::NodeLifecyclePhase::live ||
        child.phase() == composition::NodeLifecyclePhase::mounting) {
      return std::cref(child);
    }
  }
  return std::nullopt;
}

void LazyStack::Content::measure(Window& window,
                                 const composition::Node& node) const {
  const auto stack = stack_of(node);
  if (!stack.has_value()) {
    return;
  }
  std::size_t index = window.built_lo;
  for (const composition::Node& child : stack->get().children()) {
    if (child.phase() == composition::NodeLifecyclePhase::departing ||
        !child.key().has_value()) {
      continue;
    }
    if (index >= window.extents.size()) {
      break;
    }
    const composition::Rect& frame = child.frame();
    const float length = vertical() ? frame.height : frame.width;
    float& slot = window.extents[index];
    if (slot < 0.0F) {
      ++window.known;
    } else {
      window.known_total -= slot;
    }
    slot = length;
    window.known_total += length;
    ++index;
  }
}

auto LazyStack::Content::needed(const Window& window, float from,
                                float to) const -> Range {
  const std::size_t total = window.extents.size();
  Range range{.lo = total, .hi = 0};
  float cursor = 0.0F;
  for (std::size_t index = 0; index < total; ++index) {
    const float length = extent(window, index);
    if (cursor + length > from && cursor < to) {
      range.lo = std::min(range.lo, index);
      range.hi = std::max(range.hi, index + 1);
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

auto LazyStack::Content::reveal(composition::Context& context,
                                const composition::Node& node,
                                const composition::Rect& visible) -> bool {
  Window& window = context.retained(window_key);
  if (!window.ready) {
    return false;
  }
  measure(window, node);
  const auto stack = stack_of(node);
  const composition::Rect origin =
      stack.has_value() ? stack->get().frame() : node.frame();
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
  const std::size_t batch = arrangement_.retention;
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
    if (*window.kept >= need.lo && *window.kept < need.hi) {
      window.kept.reset();
    } else {
      lo = std::min(lo, *window.kept);
      hi = std::max(hi, *window.kept + 1);
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

auto LazyStack::Content::materialize(
    composition::Context& context, const composition::Node&,
    composition::MoveCommandDirection direction) -> bool {
  Window& window = context.retained(window_key);
  const bool forward =
      vertical() ? direction == composition::MoveCommandDirection::down
                 : direction == composition::MoveCommandDirection::right;
  const bool backward =
      vertical() ? direction == composition::MoveCommandDirection::up
                 : direction == composition::MoveCommandDirection::left;
  if (forward && window.hi < count()) {
    window.kept = window.hi;
    ++window.hi;
    context.request_frame();
    return true;
  }
  if (backward && window.lo > 0) {
    --window.lo;
    window.kept = window.lo;
    context.request_frame();
    return true;
  }
  return false;
}

auto LazyStack::Content::locate(composition::Context& context,
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
  if (!found.has_value() || window.extents.size() != total) {
    return std::nullopt;
  }
  const std::size_t index = *found;
  window.kept = index;
  if (index < window.lo || index >= window.hi) {
    window.lo =
        index > arrangement_.retention ? index - arrangement_.retention : 0;
    window.hi = std::min(total, index + located_span);
    context.request_frame();
  }
  const auto stack = stack_of(node);
  const composition::Rect origin =
      stack.has_value() ? stack->get().frame() : node.frame();
  const float along = position(window, index);
  const float length = extent(window, index);
  if (vertical()) {
    return composition::Rect{.x = origin.x,
                             .y = origin.y + along,
                             .width = origin.width,
                             .height = length};
  }
  return composition::Rect{.x = origin.x + along,
                           .y = origin.y,
                           .width = length,
                           .height = origin.height};
}

}
