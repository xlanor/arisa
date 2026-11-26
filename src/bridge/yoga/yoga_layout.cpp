#include <bridge/yoga/yoga_layout.hpp>

#include <arisa/components/any_layout.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/divider.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/grid.hpp>
#include <arisa/components/grid_row.hpp>
#include <arisa/components/grid_tracks.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/lazy_grid.hpp>
#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/text_layout.hpp>
#include <arisa/components/text_layout_key.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/decoration.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/list_row_separator_alignment.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/position.hpp>
#include <arisa/composition/preference.hpp>
#include <arisa/composition/safe_area.hpp>
#include <arisa/support/scoped_callback.hpp>
#include <arisa/theme/metrics.hpp>

#include <yoga/event/event.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <ranges>
#include <string>
#include <utility>

namespace arisa::bridge::yoga {

YogaLayout::YogaLayout(Token, std::shared_ptr<YogaSlot::ConfigHandle> config,
                       std::uint64_t id) noexcept
    : config_(std::move(config)), id_(id) {}

auto YogaLayout::next_id() noexcept -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{0};
  return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

void YogaLayout::observe() {
  static const bool observed = [] {
    facebook::yoga::Event::subscribe([](YGNodeConstRef,
                                        facebook::yoga::Event::Type type,
                                        facebook::yoga::Event::Data data) {
      if (type != facebook::yoga::Event::LayoutPassEnd) {
        return;
      }
      const facebook::yoga::LayoutData& pass =
          *data.get<facebook::yoga::Event::LayoutPassEnd>().layoutData;
      computed_.fetch_add(
          static_cast<std::uint64_t>(pass.layouts + pass.measures),
          std::memory_order_relaxed);
    });
    return true;
  }();
  static_cast<void>(observed);
}

auto YogaLayout::create()
    -> std::expected<std::unique_ptr<YogaLayout>, layout::LayoutError> {
  observe();
  auto config = std::make_shared<YogaSlot::ConfigHandle>(YGConfigNew());
  if (!config->valid()) {
    return std::unexpected(
        layout::LayoutError{layout::LayoutErrorCode::engine_failure});
  }
  YGConfigSetPointScaleFactor(config->get(), 0.0F);
  return std::make_unique<YogaLayout>(Token{}, std::move(config), next_id());
}

auto YogaLayout::transparent(const composition::Node& node) -> bool {
  const composition::ElementType type = node.type();
  const bool known =
      type == composition::ElementType::of<components::Text::Content>() ||
      type == composition::ElementType::of<components::Button::Content>() ||
      type == composition::ElementType::of<components::Spacer::Content>() ||
      type == composition::ElementType::of<components::VStack::Content>() ||
      type == composition::ElementType::of<components::HStack::Content>() ||
      type == composition::ElementType::of<components::ZStack::Content>() ||
      type == composition::ElementType::of<components::Grid::Content>() ||
      type == composition::ElementType::of<components::LazyGrid::Flow>() ||
      type == composition::ElementType::of<components::ScrollView::Content>() ||
      type == composition::ElementType::of<composition::Layer::Content>() ||
      type == composition::ElementType::of<composition::Decoration>() ||
      type == composition::ElementType::of<components::Divider::Content>() ||
      type ==
          composition::ElementType::of<components::Image::Raster::Content>() ||
      type ==
          composition::ElementType::of<components::GeometryReader::Content>() ||
      type == composition::ElementType::of<composition::Position>() ||
      type == composition::ElementType::of<components::AnyLayout::Content>() ||
      node.content().framed();
  composition::Modifiers layout = node.modifiers();
  layout.effects = composition::GeometryEffects{};
  layout.z_index = 0.0;
  layout.layout_priority = 0.0;
  layout.layout_values = composition::KeyedValues{};
  layout.alignment_guides.clear();
  layout.content_transition.reset();
  layout.hover.reset();
  layout.path.reset();
  return !known && layout == composition::Modifiers{};
}

auto YogaLayout::lazy_root(composition::Tree& tree,
                           const composition::Node& node) -> bool {
  const std::optional<composition::NodeId> above = node.parent();
  if (!above.has_value()) {
    return false;
  }
  const auto parent = tree.node(*above);
  return parent.has_value() &&
         parent->get().content_as<components::LazyStack::Content>().has_value();
}

auto YogaLayout::departing(const composition::Node& node) noexcept -> bool {
  return node.phase() == composition::NodeLifecyclePhase::departing;
}

auto YogaLayout::scoped(const composition::Node& node, Scope scope) -> Scope {
  bool rescale = false;
  if (const auto theme = node.override_of(components::Environment::theme)) {
    scope.theme = *theme;
    scope.geometry = geometry(theme->get());
    rescale = true;
  }
  if (const auto range =
          node.override_of(components::Environment::dynamic_type_range)) {
    scope.dynamic_type_range = scope.dynamic_type_range.intersect(range->get());
    scope.dynamic_type_size =
        scope.dynamic_type_range.clamp(scope.dynamic_type_size);
    rescale = true;
  }
  if (const auto size =
          node.override_of(components::Environment::dynamic_type_size)) {
    scope.dynamic_type_size = scope.dynamic_type_range.clamp(size->get());
    rescale = true;
  }
  if (const auto font = node.override_of(components::Environment::font)) {
    if (font->get().has_value()) {
      scope.font = font->get();
    }
  }
  scope.text = components::Text::inherited(node, scope.text);
  if (rescale &&
      scope.theme.get().dynamic_type_size() != scope.dynamic_type_size) {
    scope.theme =
        sized_.emplace_back(scope.theme.get().sized(scope.dynamic_type_size));
  }
  if (const auto insets = node.override_of(composition::SafeArea::insets)) {
    scope.insets = insets->get();
    scope.safe = insets->get();
  }
  if (const auto& ignored = node.interactions().scrolling.ignores_safe_area;
      ignored.has_value() &&
      ignored->regions.contains(composition::SafeAreaRegions::container)) {
    scope.safe = ignoring(scope.safe, *ignored);
  }
  if (const auto locale = node.override_of(components::Environment::locale)) {
    scope.locale = *locale;
  }
  if (const auto direction =
          node.override_of(components::Environment::layout_direction)) {
    scope.direction = direction->get();
  }
  if (const auto& behavior = node.modifiers().layout_direction_behavior;
      behavior.has_value() && behavior->mirrored_in(scope.direction)) {
    scope.direction = components::LayoutDirection::left_to_right;
  }
  return scope;
}

auto YogaLayout::geometry(const theme::Theme& theme) -> std::uint64_t {
  const auto found =
      std::ranges::find_if(geometries_, [&theme](const Geometry& known) {
        return known.theme.lays_out_like(theme);
      });
  if (found != geometries_.end()) {
    return found->key;
  }
  if (geometries_.size() >= geometry_limit) {
    geometries_.clear();
  }
  return geometries_
      .emplace_back(Geometry{.theme = theme, .key = ++geometry_keys_})
      .key;
}

auto YogaLayout::direction(components::LayoutDirection direction) noexcept
    -> YGDirection {
  return direction == components::LayoutDirection::right_to_left
             ? YGDirectionRTL
             : YGDirectionLTR;
}

auto YogaLayout::justify(components::Distribution distribution) noexcept
    -> YGJustify {
  switch (distribution) {
    case components::Distribution::start:
      return YGJustifyFlexStart;
    case components::Distribution::center:
      return YGJustifyCenter;
    case components::Distribution::end:
      return YGJustifyFlexEnd;
    case components::Distribution::space_between:
      return YGJustifySpaceBetween;
    case components::Distribution::space_around:
      return YGJustifySpaceAround;
    case components::Distribution::space_evenly:
      return YGJustifySpaceEvenly;
  }
  return YGJustifyFlexStart;
}

auto YogaLayout::align(components::HorizontalAlignment alignment) noexcept
    -> YGAlign {
  switch (alignment) {
    case components::HorizontalAlignment::leading:
      return YGAlignFlexStart;
    case components::HorizontalAlignment::center:
      return YGAlignCenter;
    case components::HorizontalAlignment::trailing:
      return YGAlignFlexEnd;
  }
  return YGAlignCenter;
}

auto YogaLayout::align(components::VerticalAlignment alignment) noexcept
    -> YGAlign {
  switch (alignment) {
    case components::VerticalAlignment::top:
      return YGAlignFlexStart;
    case components::VerticalAlignment::center:
      return YGAlignCenter;
    case components::VerticalAlignment::bottom:
      return YGAlignFlexEnd;
    case components::VerticalAlignment::first_text_baseline:
      return YGAlignBaseline;
    case components::VerticalAlignment::last_text_baseline:
      return YGAlignFlexStart;
  }
  return YGAlignCenter;
}

auto YogaLayout::justify(components::HorizontalAlignment alignment) noexcept
    -> YGJustify {
  switch (alignment) {
    case components::HorizontalAlignment::leading:
      return YGJustifyFlexStart;
    case components::HorizontalAlignment::center:
      return YGJustifyCenter;
    case components::HorizontalAlignment::trailing:
      return YGJustifyFlexEnd;
  }
  return YGJustifyCenter;
}

auto YogaLayout::justify(components::VerticalAlignment alignment) noexcept
    -> YGJustify {
  switch (alignment) {
    case components::VerticalAlignment::top:
    case components::VerticalAlignment::first_text_baseline:
    case components::VerticalAlignment::last_text_baseline:
      return YGJustifyFlexStart;
    case components::VerticalAlignment::center:
      return YGJustifyCenter;
    case components::VerticalAlignment::bottom:
      return YGJustifyFlexEnd;
  }
  return YGJustifyCenter;
}

auto YogaLayout::length(const std::optional<composition::Length>& length,
                        const theme::Theme& theme) noexcept
    -> YogaSlot::Length {
  if (!length.has_value()) {
    return YogaSlot::Length{};
  }
  if (length->is_fraction()) {
    return YogaSlot::Length{.unit = YogaSlot::Length::Unit::percent,
                            .value = length->value() * 100.0F};
  }
  return YogaSlot::Length{.unit = YogaSlot::Length::Unit::points,
                          .value = std::max(*points(length, theme), 0.0F)};
}

auto YogaLayout::points(const std::optional<composition::Length>& length,
                        const theme::Theme& theme) noexcept
    -> std::optional<float> {
  if (!length.has_value()) {
    return std::nullopt;
  }
  if (const auto& metric = length->scaled()) {
    return theme.scaled(*metric);
  }
  return length->value();
}

auto YogaLayout::ideal_of(const std::optional<composition::Length>& fixed,
                          const std::optional<composition::Length>& ideal,
                          const theme::Theme& theme) noexcept
    -> std::optional<float> {
  if (fixed.has_value() || !ideal.has_value() || ideal->is_fraction()) {
    return std::nullopt;
  }
  return std::max(*points(ideal, theme), 0.0F);
}

auto YogaLayout::layered(const composition::Node& node) noexcept -> bool {
  return node.content_as<composition::Layer::Content>().has_value();
}

auto YogaLayout::live(const composition::Node& node) noexcept -> bool {
  return std::ranges::any_of(node.children(),
                             [](const composition::Node& child) {
                               return !departing(child) && !layered(child);
                             });
}

auto YogaLayout::flow_of(const composition::Node& node) -> Flow {
  if (node.content_as<components::Spacer::Content>().has_value() ||
      node.content_as<components::Divider::Content>().has_value()) {
    return Flow::neutral;
  }
  if (node.content_as<components::ScrollView::Content>().has_value()) {
    return Flow::consume;
  }
  const composition::Modifiers& modifiers = node.modifiers();
  const bool decorated =
      modifiers.background.has_value() || modifiers.border.has_value() ||
      modifiers.shadow != theme::Elevation::none ||
      modifiers.drop_shadow.has_value() || modifiers.glass.has_value();
  const bool stack =
      node.content_as<components::VStack::Content>().has_value() ||
      node.content_as<components::HStack::Content>().has_value() ||
      node.content_as<components::ZStack::Content>().has_value();
  if (stack && !live(node)) {
    return decorated ? Flow::own : Flow::neutral;
  }
  if (decorated) {
    return Flow::own;
  }
  if (node.content_as<components::VStack::Content>().has_value()) {
    return Flow::column;
  }
  if (node.content_as<components::HStack::Content>().has_value()) {
    return Flow::row;
  }
  if (node.content_as<components::ZStack::Content>().has_value() ||
      node.content_as<composition::Layer::Content>().has_value()) {
    return Flow::all;
  }
  if (node.content_as<composition::Decoration>().has_value()) {
    return Flow::base;
  }
  const bool leaf =
      node.content_as<components::Text::Content>().has_value() ||
      node.content_as<components::Button::Content>().has_value() ||
      custom(node) ||
      node.content_as<components::Grid::Content>().has_value() ||
      node.content_as<components::LazyGrid::Flow>().has_value() ||
      node.content().framed() || !live(node);
  return leaf ? Flow::own : Flow::all;
}

void YogaLayout::distribute(std::span<Entry> entries, Flow flow) {
  const bool passes = flow == Flow::column || flow == Flow::row ||
                      flow == Flow::all || flow == Flow::base;
  if (!passes) {
    for (Entry& entry : entries) {
      entry.scope.safe = theme::Edges{};
    }
    return;
  }
  if (flow == Flow::all) {
    return;
  }
  if (flow == Flow::base) {
    for (std::size_t index = 1; index < entries.size(); ++index) {
      entries[index].scope.safe = theme::Edges{};
    }
    return;
  }
  const auto counts = [](const Entry& entry) {
    return flow_of(entry.node.get()) != Flow::neutral;
  };
  const auto first = std::ranges::find_if(entries, counts);
  const auto last = std::ranges::find_if(entries | std::views::reverse, counts);
  for (auto at = entries.begin(); at != entries.end(); ++at) {
    const bool leads = at == first;
    const bool trails =
        last != (entries | std::views::reverse).end() && &*at == &*last;
    theme::Edges& safe = at->scope.safe;
    if (flow == Flow::column) {
      safe.top = leads ? safe.top : 0.0F;
      safe.bottom = trails ? safe.bottom : 0.0F;
    } else {
      safe.leading = leads ? safe.leading : 0.0F;
      safe.trailing = trails ? safe.trailing : 0.0F;
      safe.top = 0.0F;
      safe.bottom = 0.0F;
    }
  }
}

auto YogaLayout::ignoring(
    theme::Edges edges,
    const composition::Scrolling::IgnoredSafeArea& ignored) noexcept
    -> theme::Edges {
  const auto chosen = [&ignored](theme::Edge edge) {
    return (static_cast<unsigned>(ignored.edges) &
            static_cast<unsigned>(edge)) != 0U;
  };
  float across = 0.0F;
  float down = 0.0F;
  if (ignored.alignment.has_value()) {
    switch (ignored.alignment->horizontal) {
      case components::HorizontalAlignment::leading:
        break;
      case components::HorizontalAlignment::center:
        across = 0.5F;
        break;
      case components::HorizontalAlignment::trailing:
        across = 1.0F;
        break;
    }
    switch (ignored.alignment->vertical) {
      case components::VerticalAlignment::center:
        down = 0.5F;
        break;
      case components::VerticalAlignment::bottom:
        down = 1.0F;
        break;
      case components::VerticalAlignment::top:
      case components::VerticalAlignment::first_text_baseline:
      case components::VerticalAlignment::last_text_baseline:
        break;
    }
  }
  const bool aligned = ignored.alignment.has_value();
  return theme::Edges{
      .top = chosen(theme::Edge::top) ? edges.top * down : edges.top,
      .leading =
          chosen(theme::Edge::leading) ? edges.leading * across : edges.leading,
      .bottom = chosen(theme::Edge::bottom)
                    ? edges.bottom * (aligned ? 1.0F - down : 0.0F)
                    : edges.bottom,
      .trailing = chosen(theme::Edge::trailing)
                      ? edges.trailing * (aligned ? 1.0F - across : 0.0F)
                      : edges.trailing};
}

auto YogaLayout::corner_radii(const theme::Theme& theme,
                              const theme::Shape& shape) noexcept
    -> theme::RectangleCornerRadii {
  switch (shape.form) {
    case theme::ShapeForm::uneven:
      return shape.corner_radii;
    case theme::ShapeForm::ellipse:
    case theme::ShapeForm::circle: {
      const float round = std::numeric_limits<float>::infinity();
      return theme::RectangleCornerRadii{.top_leading = round,
                                         .bottom_leading = round,
                                         .bottom_trailing = round,
                                         .top_trailing = round};
    }
    case theme::ShapeForm::rounded: {
      const float round = theme.radius(shape.radius);
      return theme::RectangleCornerRadii{.top_leading = round,
                                         .bottom_leading = round,
                                         .bottom_trailing = round,
                                         .top_trailing = round};
    }
    case theme::ShapeForm::concentric:
    case theme::ShapeForm::container_relative:
      break;
  }
  return theme::RectangleCornerRadii{};
}

auto YogaLayout::surveys(const composition::Node& node) -> bool {
  return !node.interactions().geometry.empty() ||
         node.content().reads_geometry() ||
         node.modifiers().corner_offset.has_value();
}

auto YogaLayout::settle_corner(composition::Tree& tree,
                               const composition::Node& node, YogaSlot& slot)
    -> bool {
  const composition::CornerOffset& offset = *node.modifiers().corner_offset;
  theme::Edges wanted{};
  const auto& enclosure = node.enclosure();
  const auto container =
      enclosure.has_value() ? tree.node(enclosure->container) : std::nullopt;
  const auto held =
      container.has_value() ? tree.layout_slot(container->get()) : std::nullopt;
  if (held.has_value()) {
    const composition::Rect outer =
        absolute(static_cast<YogaSlot&>(held->get()).node());
    const composition::LayoutDirection direction = slot.layout_direction();
    const bool mirrored =
        direction == composition::LayoutDirection::right_to_left;
    const theme::Edges applied = slot.corner();
    composition::Rect frame = absolute(slot.node());
    frame.x -= mirrored ? applied.trailing : applied.leading;
    frame.y -= applied.top;
    if (offset.size_to_fit) {
      frame.width += applied.leading + applied.trailing;
      frame.height += applied.top + applied.bottom;
    }
    const theme::RectangleCornerInsets corners =
        composition::ContainerCorners{outer, enclosure->radii, direction}
            .insets(frame);
    const auto chosen = [&offset](theme::Edge edge) {
      return (static_cast<unsigned>(offset.edges) &
              static_cast<unsigned>(edge)) != 0U;
    };
    const float top =
        chosen(theme::Edge::top)
            ? std::max(corners.top_leading.height, corners.top_trailing.height)
            : 0.0F;
    const float bottom = chosen(theme::Edge::bottom)
                             ? std::max(corners.bottom_leading.height,
                                        corners.bottom_trailing.height)
                             : 0.0F;
    const float leading =
        chosen(theme::Edge::leading)
            ? std::max(corners.top_leading.width, corners.bottom_leading.width)
            : 0.0F;
    const float trailing = chosen(theme::Edge::trailing)
                               ? std::max(corners.top_trailing.width,
                                          corners.bottom_trailing.width)
                               : 0.0F;
    wanted = offset.size_to_fit ? theme::Edges{.top = top,
                                               .leading = leading,
                                               .bottom = bottom,
                                               .trailing = trailing}
                                : theme::Edges{.top = top - bottom,
                                               .leading = leading - trailing,
                                               .bottom = bottom - top,
                                               .trailing = trailing - leading};
  }
  return slot.set_corner(wanted);
}

auto YogaLayout::plus(theme::Edges first, theme::Edges second) noexcept
    -> theme::Edges {
  return theme::Edges{.top = first.top + second.top,
                      .leading = first.leading + second.leading,
                      .bottom = first.bottom + second.bottom,
                      .trailing = first.trailing + second.trailing};
}

auto YogaLayout::provided(const composition::Node& node)
    -> std::optional<theme::Edges> {
  bool found = false;
  for (const composition::Node& child : node.children()) {
    if (departing(child)) {
      continue;
    }
    if (const auto& edge = child.interactions().scrolling.safe_area_bar) {
      bars_.push_back(Bar{
          .provider = node.id().value(), .node = child.id(), .edge = *edge});
      found = true;
    }
  }
  if (!found) {
    return std::nullopt;
  }
  const auto known = insets_.find(node.id().value());
  return known == insets_.end() ? theme::Edges{} : known->second;
}

auto YogaLayout::settle_bars(composition::Tree& tree) -> bool {
  std::unordered_map<std::uint64_t, theme::Edges> measured;
  for (const Bar& bar : bars_) {
    theme::Edges& edges = measured[bar.provider];
    const auto node = tree.node(bar.node);
    if (!node.has_value()) {
      continue;
    }
    const auto found = tree.layout_slot(node->get());
    if (!found) {
      continue;
    }
    const YGNodeRef laid = static_cast<YogaSlot&>(found->get()).node();
    const float height = YGNodeLayoutGetHeight(laid);
    const float width = YGNodeLayoutGetWidth(laid);
    switch (bar.edge) {
      case theme::Edge::top:
        edges.top += height;
        break;
      case theme::Edge::bottom:
        edges.bottom += height;
        break;
      case theme::Edge::leading:
        edges.leading += width;
        break;
      case theme::Edge::trailing:
        edges.trailing += width;
        break;
      default:
        break;
    }
  }
  bars_.clear();
  const bool changed = measured != insets_;
  insets_ = std::move(measured);
  return changed;
}

void YogaLayout::admit(const composition::Node& child, Scope scope) {
  if (departing(child)) {
    return;
  }
  if (layered(child)) {
    portals_.push_back(Entry{.node = child, .scope = scope});
    return;
  }
  if (transparent(child)) {
    Scope inner = scoped(child, scope);
    if (child.modifiers().layout_priority != 0.0) {
      inner.priority = child.modifiers().layout_priority;
    }
    if (!child.modifiers().layout_values.empty() ||
        !child.modifiers().alignment_guides.empty() ||
        child.modifiers().layout_priority != 0.0) {
      inner.carried = std::cref(child.modifiers());
    }
    if (!child.container_values().empty()) {
      inner.carried_values = std::cref(child.container_values());
    }
    static_cast<void>(gather(child, inner));
    return;
  }
  entries_.push_back(Entry{.node = child, .scope = scope});
}

auto YogaLayout::priority_of(const Entry& entry) noexcept -> double {
  const double own = entry.node.get().modifiers().layout_priority;
  return own != 0.0 ? own : entry.scope.priority;
}

void YogaLayout::rank(std::span<Entry> entries, bool horizontal) {
  if (entries.size() < 2) {
    return;
  }
  const double first = priority_of(entries.front());
  if (std::ranges::all_of(entries, [first](const Entry& entry) {
        return priority_of(entry) == first;
      })) {
    return;
  }
  std::array<double, priority_levels> levels{};
  std::size_t count = 0;
  for (const Entry& entry : entries) {
    const double priority = priority_of(entry);
    const auto known =
        std::ranges::find(std::span{levels}.first(count), priority);
    if (known != std::span{levels}.first(count).end()) {
      continue;
    }
    if (count < priority_levels) {
      levels.at(count++) = priority;
    } else {
      levels.back() = std::min(levels.back(), priority);
    }
  }
  std::ranges::sort(std::span{levels}.first(count), std::greater<>{});
  std::optional<double> growing;
  for (const Entry& entry : entries) {
    if (stretches(entry.node.get(), horizontal)) {
      growing =
          std::max(growing.value_or(priority_of(entry)), priority_of(entry));
    }
  }
  for (Entry& entry : entries) {
    const double priority = priority_of(entry);
    const auto above = static_cast<std::size_t>(std::ranges::count_if(
        std::span{levels}.first(count),
        [priority](double level) { return level > priority; }));
    const std::size_t below = count - 1 - std::min(above, count - 1);
    entry.scope.shrink_weight =
        std::pow(priority_shrink_step, static_cast<float>(below));
    entry.scope.ranked = true;
    entry.scope.grow_blocked = growing.has_value() && priority < *growing &&
                               stretches(entry.node.get(), horizontal);
  }
}

auto YogaLayout::fixes(const composition::Node& node,
                       composition::Axis axis) noexcept -> bool {
  return node.modifiers().fixed_size.contains(axis);
}

auto YogaLayout::proposes(const composition::Node& node) -> bool {
  return node.content_as<components::GeometryReader::Content>().has_value() ||
         node.content_as<composition::Position>().has_value();
}

auto YogaLayout::gather(const composition::Node& node, Scope scope)
    -> std::size_t {
  const std::size_t first = entries_.size();
  for (const composition::Node& child : node.children()) {
    admit(child, scope);
  }
  return first;
}

void YogaLayout::release(std::size_t first) {
  entries_.erase(entries_.begin() + static_cast<std::ptrdiff_t>(first),
                 entries_.end());
}

auto YogaLayout::produced(Range range) const noexcept
    -> std::span<const YGNodeRef> {
  return std::span{refs_}.subspan(range.begin, range.end - range.begin);
}

auto YogaLayout::decorated_frame(const composition::Node& node,
                                 std::size_t base) -> composition::FrameSpec {
  composition::FrameSpec frame = node.modifiers().frame;
  std::size_t index = 0;
  for (const composition::Node& child : node.children()) {
    if (departing(child)) {
      continue;
    }
    if (index++ != base) {
      continue;
    }
    const auto nested = child.content_as<composition::Decoration>();
    const composition::FrameSpec inner =
        nested.has_value() ? decorated_frame(child, nested->get().base())
                           : child.modifiers().frame;
    if (!frame.max_width.has_value() && inner.max_width.has_value() &&
        std::isinf(inner.max_width->value())) {
      frame.max_width = inner.max_width;
    }
    if (!frame.max_height.has_value() && inner.max_height.has_value() &&
        std::isinf(inner.max_height->value())) {
      frame.max_height = inner.max_height;
    }
    if (!frame.width.has_value() && inner.width.has_value() &&
        inner.width->is_fraction()) {
      frame.width = inner.width;
    }
    if (!frame.height.has_value() && inner.height.has_value() &&
        inner.height->is_fraction()) {
      frame.height = inner.height;
    }
  }
  return frame;
}

auto YogaLayout::fills(const std::optional<composition::Length>& limit) noexcept
    -> bool {
  return limit.has_value() && std::isinf(limit->value());
}

auto YogaLayout::sized(const composition::FrameSpec& frame) noexcept -> bool {
  return frame.constrains();
}

auto YogaLayout::pinned(const composition::FrameSpec& frame) noexcept -> bool {
  const bool spans_width = fills(frame.max_width) && !frame.max_height;
  const bool spans_height = fills(frame.max_height) && !frame.max_width;
  const bool loose = !frame.width && !frame.height && !frame.min_width &&
                     !frame.min_height && !frame.ideal_width &&
                     !frame.ideal_height;
  return sized(frame) && !(loose && (spans_width || spans_height));
}

auto YogaLayout::fit_of(const composition::FrameSpec& frame,
                        const composition::AspectRatio& aspect, bool open_width,
                        bool open_height) noexcept -> Fit {
  const auto bounded = [](const std::optional<composition::Length>& fixed,
                          const std::optional<composition::Length>& limit) {
    return fixed.has_value() || (limit.has_value() && !fills(limit));
  };
  const bool across = open_width && !bounded(frame.width, frame.max_width);
  const bool down = open_height && !bounded(frame.height, frame.max_height);
  if (across && down && frame.ideal_width.has_value() &&
      frame.ideal_height.has_value()) {
    return Fit::ideal;
  }
  if (across != down && aspect.ratio.has_value()) {
    return across ? Fit::open_width : Fit::open_height;
  }
  return pinned(frame) ? Fit::box : Fit::ratio;
}

auto YogaLayout::unbounded(YogaSlot::Style style, float ratio, Role role,
                           Fit fit) noexcept -> YogaSlot::Style {
  const bool across = fit == Fit::open_width;
  style.aspect_ratio = ratio;
  if (across) {
    style.width = YogaSlot::Length{};
    style.min_width.reset();
    style.max_width.reset();
  } else {
    style.height = YogaSlot::Length{};
    style.min_height.reset();
    style.max_height.reset();
  }
  if (role != Role::column_item && role != Role::row_item) {
    return style;
  }
  const bool along = (role == Role::row_item) == across;
  style.grow = along ? 0.0F : 1.0F;
  style.shrink = along ? 0.0F : 1.0F;
  style.align_self = along ? YGAlignStretch : YGAlignAuto;
  return style;
}

auto YogaLayout::stretches(const composition::Node& node, bool horizontal)
    -> bool {
  const composition::Axis axis =
      horizontal ? composition::Axis::horizontal : composition::Axis::vertical;
  if (fixes(node, axis)) {
    return false;
  }
  if (const auto& relative = node.modifiers().relative_frame;
      relative.has_value() && relative->axes().contains(axis)) {
    return false;
  }
  const composition::FrameSpec& frame = node.modifiers().frame;
  if (const auto& fixed = horizontal ? frame.width : frame.height) {
    return fixed->is_fraction();
  }
  if (fills(horizontal ? frame.max_width : frame.max_height)) {
    return true;
  }
  if (node.modifiers().aspect.has_value()) {
    return true;
  }
  if (node.content_as<components::Spacer::Content>().has_value() ||
      node.content_as<components::ScrollView::Content>().has_value() ||
      proposes(node)) {
    return true;
  }
  return content_stretches(node, horizontal);
}

auto YogaLayout::content_stretches(const composition::Node& node,
                                   bool horizontal) -> bool {
  if (node.content_as<components::Text::Content>().has_value() ||
      node.content_as<components::Divider::Content>().has_value() ||
      node.content_as<components::Spacer::Content>().has_value() ||
      node.content_as<components::Image::Raster::Content>().has_value() ||
      custom(node)) {
    return false;
  }
  if (node.content_as<components::ScrollView::Content>().has_value()) {
    return true;
  }
  if (node.content_as<components::Grid::Content>().has_value()) {
    return horizontal && table_stretches(node);
  }
  if (const auto flow = node.content_as<components::LazyGrid::Flow>()) {
    const bool down =
        flow->get().arrangement().axis == composition::Axis::vertical;
    return down == horizontal;
  }
  if (const auto lazy = node.content_as<components::LazyStack::Content>();
      lazy.has_value() && horizontal &&
      lazy->get().arrangement().axis == composition::Axis::vertical) {
    return true;
  }
  const auto vertical = node.content_as<components::VStack::Content>();
  const auto sideways = node.content_as<components::HStack::Content>();
  const bool column = vertical.has_value();
  const bool row = sideways.has_value();
  if (column || row) {
    const bool along = column ? !horizontal : horizontal;
    const bool spreads = column ? vertical->get().options().distribution !=
                                          components::Distribution::start ||
                                      vertical->get().options().wrap
                                : sideways->get().options().distribution !=
                                          components::Distribution::start ||
                                      sideways->get().options().wrap;
    if (along && spreads) {
      return true;
    }
  }
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    if ((column || row) && (column ? !horizontal : horizontal) &&
        child.modifiers().grow.value_or(0.0F) > 0.0F) {
      return true;
    }
    const bool spacer =
        child.content_as<components::Spacer::Content>().has_value();
    const bool divider =
        child.content_as<components::Divider::Content>().has_value();
    if ((column || row) && (spacer || divider)) {
      const bool along = column ? !horizontal : horizontal;
      if (spacer ? along : !along) {
        return true;
      }
      continue;
    }
    if (stretches(child, horizontal)) {
      return true;
    }
  }
  return false;
}

auto YogaLayout::aligned_kind(const composition::Node& node) -> bool {
  return node.content_as<components::Text::Content>().has_value() ||
         node.content_as<components::VStack::Content>().has_value() ||
         node.content_as<components::HStack::Content>().has_value() ||
         node.content_as<components::ZStack::Content>().has_value();
}

auto YogaLayout::framed_cross(const composition::Node& node,
                              composition::HorizontalAlignment alignment)
    -> YGAlign {
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    if (stretches(child, true)) {
      return YGAlignStretch;
    }
  }
  return node.child_count() == 0 ? YGAlignStretch : align(alignment);
}

auto YogaLayout::exterior(YogaSlot::Style style,
                          composition::Alignment alignment) -> YogaSlot::Style {
  style.flex_direction = YGFlexDirectionRow;
  style.justify = justify(alignment.horizontal);
  style.align_items = align(alignment.vertical);
  style.align_content = YGAlignFlexStart;
  style.wrap = YGWrapNoWrap;
  style.gap_row = 0.0F;
  style.gap_column = 0.0F;
  return style;
}

auto YogaLayout::interior(YogaSlot::Style style, const composition::Node& node)
    -> YogaSlot::Style {
  const bool across = content_stretches(node, true);
  const bool along = content_stretches(node, false);
  style.align_self = along ? YGAlignStretch : YGAlignAuto;
  style.grow = across ? 1.0F : 0.0F;
  style.shrink = 1.0F;
  style.basis.reset();
  style.position = YGPositionTypeRelative;
  style.overflow = YGOverflowVisible;
  style.padding = theme::Edges{};
  style.margin = theme::Edges{};
  style.width = YogaSlot::Length{};
  style.height = YogaSlot::Length{};
  style.min_width.reset();
  style.max_width.reset();
  style.min_height.reset();
  style.max_height.reset();
  return style;
}

auto YogaLayout::opened(const composition::Node& node, Scope scope) -> Scope {
  if (const auto scroll = node.content_as<components::ScrollView::Content>()) {
    const composition::Axes axes = scroll->get().options().axes;
    scope.open_width = axes.contains(composition::Axis::horizontal);
    scope.open_height = axes.contains(composition::Axis::vertical);
    scope.scrolled_width = scope.open_width;
    scope.scrolled_height = scope.open_height;
    scope.fixed_width = false;
    scope.fixed_height = false;
    return scope;
  }
  scope.scrolled_width = false;
  scope.scrolled_height = false;
  const bool carries =
      node.content_as<components::VStack::Content>().has_value() ||
      node.content_as<components::HStack::Content>().has_value() ||
      node.content_as<components::ZStack::Content>().has_value() ||
      node.content_as<composition::Decoration>().has_value() ||
      node.content_as<components::Image::Content>().has_value() ||
      !(node.content_as<components::Text::Content>().has_value() ||
        node.content_as<components::Button::Content>().has_value() ||
        node.content_as<components::Grid::Content>().has_value() ||
        node.content_as<components::LazyGrid::Flow>().has_value() ||
        node.content_as<composition::Layer::Content>().has_value() ||
        custom(node) || node.content().framed());
  if (!carries) {
    scope.open_width = false;
    scope.open_height = false;
    scope.fixed_width = false;
    scope.fixed_height = false;
    return scope;
  }
  const composition::FrameSpec& frame = node.modifiers().frame;
  const auto& relative = node.modifiers().relative_frame;
  const auto bounds = [&relative](
                          const std::optional<composition::Length>& fixed,
                          const std::optional<composition::Length>& limit,
                          composition::Axis axis) {
    return fixed.has_value() || (limit.has_value() && !fills(limit)) ||
           (relative.has_value() && relative->axes().contains(axis));
  };
  if (bounds(frame.width, frame.max_width, composition::Axis::horizontal)) {
    scope.open_width = false;
    scope.fixed_width = false;
  }
  if (bounds(frame.height, frame.max_height, composition::Axis::vertical)) {
    scope.open_height = false;
    scope.fixed_height = false;
  }
  return scope;
}

auto YogaLayout::container_length(composition::Tree& tree,
                                  const composition::Node& node,
                                  composition::Axis axis)
    -> std::optional<float> {
  std::optional<composition::NodeId> current = node.parent();
  while (current.has_value()) {
    const auto found = tree.node(*current);
    if (!found.has_value()) {
      break;
    }
    const composition::Node& at = found->get();
    const auto scroll = at.content_as<components::ScrollView::Content>();
    if ((scroll.has_value() && scroll->get().options().axes.contains(axis)) ||
        at.interactions().relative_container) {
      const auto held = tree.layout_slot(at);
      if (!held.has_value()) {
        return std::nullopt;
      }
      const YGNodeRef laid = static_cast<YogaSlot&>(held->get()).node();
      const bool across = axis == composition::Axis::horizontal;
      const float total =
          across ? YGNodeLayoutGetWidth(laid) : YGNodeLayoutGetHeight(laid);
      const float inset = across
                              ? YGNodeLayoutGetPadding(laid, YGEdgeLeft) +
                                    YGNodeLayoutGetPadding(laid, YGEdgeRight)
                              : YGNodeLayoutGetPadding(laid, YGEdgeTop) +
                                    YGNodeLayoutGetPadding(laid, YGEdgeBottom);
      const float length = total - inset;
      if (!(length > 0.0F)) {
        return std::nullopt;
      }
      return length;
    }
    current = at.parent();
  }
  return std::nullopt;
}

auto YogaLayout::window_length(const Scope& scope, composition::Axis axis) const
    -> float {
  if (!viewport_.has_value()) {
    return 0.0F;
  }
  if (axis == composition::Axis::horizontal) {
    return std::max(
        viewport_->width() - scope.insets.leading - scope.insets.trailing,
        0.0F);
  }
  return std::max(viewport_->height() - scope.insets.top - scope.insets.bottom,
                  0.0F);
}

auto YogaLayout::relate(composition::Tree& tree, const composition::Node& node,
                        composition::FrameSpec frame, const Scope& scope)
    -> composition::FrameSpec {
  const composition::ContainerRelativeFrame& relative =
      *node.modifiers().relative_frame;
  const float spacing =
      std::max(scope.theme.get().space(relative.spacing()), 0.0F);
  for (const composition::Axis axis :
       {composition::Axis::horizontal, composition::Axis::vertical}) {
    if (!relative.axes().contains(axis)) {
      continue;
    }
    const float container =
        container_length(tree, node, axis).value_or(window_length(scope, axis));
    const composition::Length length{relative.length(container, spacing, axis)};
    if (axis == composition::Axis::horizontal) {
      frame.width = length;
    } else {
      frame.height = length;
    }
  }
  frame.alignment = relative.alignment();
  return frame;
}

auto YogaLayout::settle_relative(composition::Tree& tree,
                                 const composition::Node& node, YGNodeRef laid)
    -> bool {
  const auto& relative = node.modifiers().relative_frame;
  const auto held = tree.layout_slot(node);
  if (!relative.has_value() || !held.has_value()) {
    return false;
  }
  const std::optional<float> spacing =
      static_cast<YogaSlot&>(held->get()).relative_spacing();
  bool changed = false;
  for (const composition::Axis axis :
       {composition::Axis::horizontal, composition::Axis::vertical}) {
    if (!relative->axes().contains(axis)) {
      continue;
    }
    const std::optional<float> container = container_length(tree, node, axis);
    if (!container.has_value()) {
      continue;
    }
    const float wanted =
        relative->length(*container, spacing.value_or(0.0F), axis);
    const bool across = axis == composition::Axis::horizontal;
    const YGValue current =
        across ? YGNodeStyleGetWidth(laid) : YGNodeStyleGetHeight(laid);
    if (current.unit == YGUnitPoint &&
        std::abs(current.value - wanted) < 0.01F) {
      continue;
    }
    if (across) {
      YGNodeStyleSetWidth(laid, wanted);
    } else {
      YGNodeStyleSetHeight(laid, wanted);
    }
    changed = true;
  }
  return changed;
}

auto YogaLayout::settle_position(const composition::Node& node, YGNodeRef laid)
    -> bool {
  const auto positioned = node.content_as<composition::Position>();
  if (!positioned.has_value()) {
    return false;
  }
  const composition::Point centre = positioned->get().centre();
  const float left = YGNodeLayoutGetPadding(laid, YGEdgeLeft);
  const float top = YGNodeLayoutGetPadding(laid, YGEdgeTop);
  bool changed = false;
  const std::size_t count = YGNodeGetChildCount(laid);
  for (std::size_t index = 0; index < count; ++index) {
    const YGNodeRef child = YGNodeGetChild(laid, index);
    const float x = left + centre.x - (YGNodeLayoutGetWidth(child) * 0.5F);
    const float y = top + centre.y - (YGNodeLayoutGetHeight(child) * 0.5F);
    const YGValue at_x = YGNodeStyleGetPosition(child, YGEdgeLeft);
    const YGValue at_y = YGNodeStyleGetPosition(child, YGEdgeTop);
    if (at_x.unit != YGUnitPoint || std::abs(at_x.value - x) >= 0.01F) {
      YGNodeStyleSetPosition(child, YGEdgeLeft, x);
      changed = true;
    }
    if (at_y.unit != YGUnitPoint || std::abs(at_y.value - y) >= 0.01F) {
      YGNodeStyleSetPosition(child, YGEdgeTop, y);
      changed = true;
    }
  }
  return changed;
}

auto YogaLayout::sizing(const composition::FrameSpec& frame, Role role,
                        YogaSlot::Style style, const Scope& scope) const
    -> YogaSlot::Style {
  const theme::Theme& theme = scope.theme;
  style.width = length(frame.width, theme);
  style.height = length(frame.height, theme);
  if (!frame.width.has_value() && frame.ideal_width.has_value() &&
      scope.open_width) {
    style.width = length(frame.ideal_width, theme);
  }
  if (!frame.height.has_value() && frame.ideal_height.has_value() &&
      scope.open_height) {
    style.height = length(frame.ideal_height, theme);
  }
  const bool infinite_width =
      frame.max_width.has_value() && std::isinf(frame.max_width->value());
  const bool infinite_height =
      frame.max_height.has_value() && std::isinf(frame.max_height->value());
  const bool fill_width =
      frame.max_width.has_value() && !frame.max_width->is_fraction();
  const bool fill_height =
      frame.max_height.has_value() && !frame.max_height->is_fraction();
  style.min_width = points(frame.min_width, theme);
  style.max_width =
      infinite_width ? std::nullopt : points(frame.max_width, theme);
  style.min_height = points(frame.min_height, theme);
  style.max_height =
      infinite_height ? std::nullopt : points(frame.max_height, theme);
  if (role == Role::overlay) {
    style.position = YGPositionTypeAbsolute;
  }
  if (role == Role::layer && viewport_.has_value()) {
    style.width = YogaSlot::Length{.unit = YogaSlot::Length::Unit::points,
                                   .value = viewport_->width()};
    style.height = YogaSlot::Length{.unit = YogaSlot::Length::Unit::points,
                                    .value = viewport_->height()};
    return style;
  }
  const bool ideal_width = scope.scrolled_width && infinite_width;
  const bool ideal_height = scope.scrolled_height && infinite_height;
  if (fill_width && !frame.width.has_value() && !ideal_width) {
    switch (role) {
      case Role::row_item:
        style.grow = 1.0F;
        style.shrink = 1.0F;
        break;
      case Role::column_item:
        style.align_self = YGAlignStretch;
        break;
      case Role::overlay:
        style.width = YogaSlot::Length{.unit = YogaSlot::Length::Unit::percent,
                                       .value = 100.0F};
        break;
      case Role::layer:
        break;
    }
  }
  if (fill_height && !frame.height.has_value() && !ideal_height) {
    switch (role) {
      case Role::row_item:
        style.align_self = YGAlignStretch;
        break;
      case Role::column_item:
        style.grow = 1.0F;
        style.shrink = 1.0F;
        break;
      case Role::overlay:
        style.height = YogaSlot::Length{.unit = YogaSlot::Length::Unit::percent,
                                        .value = 100.0F};
        break;
      case Role::layer:
        break;
    }
  }
  return style;
}

auto YogaLayout::slot(composition::Tree& tree, const composition::Node& node)
    -> std::expected<std::reference_wrapper<YogaSlot>, layout::LayoutError> {
  if (const auto existing = tree.layout_slot(node)) {
    auto& found = static_cast<YogaSlot&>(existing->get());
    if (found.engine() == id_) {
      return std::ref(found);
    }
  }
  auto created = YogaSlot::create(config_, id_);
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  return std::ref(static_cast<YogaSlot&>(
      tree.attach_layout_slot(node, std::move(*created))));
}

auto YogaLayout::natural(std::string_view text, const text::TextStyle& style)
    -> text::TextSize {
  if (!typesetter_.has_value()) {
    return text::TextSize{};
  }
  return typesetter_->get()
      .measure(text, style, text::LayoutWidth::unbounded(), scale_)
      .value_or(text::TextSize{});
}

auto YogaLayout::wrap_width(std::string_view text, const text::TextStyle& style,
                            bool wraps, float width) -> text::LayoutWidth {
  if (!wraps || std::isnan(width) || std::isinf(width)) {
    return text::LayoutWidth::unbounded();
  }
  if (width + 0.01F >= natural(text, style).width) {
    return text::LayoutWidth::unbounded();
  }
  return text::LayoutWidth::points(std::max(width, 0.0F))
      .value_or(text::LayoutWidth::unbounded());
}

auto YogaLayout::measure(std::string_view text, const text::TextStyle& style,
                         bool wraps, float width, YGMeasureMode mode,
                         std::optional<float> height,
                         const text::TextFitting& fitting,
                         std::optional<std::size_t> reserved, CostRef cost)
    -> YGSize {
  ++measurements_;
  if (!typesetter_.has_value()) {
    return YGSize{.width = 0.0F, .height = 0.0F};
  }
  const std::size_t shaped = typesetter_->get().statistics().shaping_misses;
  const YGSize size =
      measured(text, style, wraps, width, mode, height, fitting, reserved);
  if (cost.has_value()) {
    cost->get().shaped(typesetter_->get().statistics().shaping_misses - shaped);
  }
  return size;
}

auto YogaLayout::measured(std::string_view text, const text::TextStyle& style,
                          bool wraps, float width, YGMeasureMode mode,
                          std::optional<float> height,
                          const text::TextFitting& fitting,
                          std::optional<std::size_t> reserved) -> YGSize {
  const text::LayoutWidth bound = mode == YGMeasureModeUndefined
                                      ? text::LayoutWidth::unbounded()
                                      : wrap_width(text, style, wraps, width);
  if (!bound.bounded()) {
    const text::TextSize size = natural(text, style);
    return YGSize{.width = size.width,
                  .height = reserve(style, reserved, size.height)};
  }
  text::TextFitting wanted = fitting;
  if (!wanted.lines.has_value() && wanted.minimum_scale_factor < 1.0F &&
      height.has_value() && std::isfinite(*height)) {
    wanted.height = *height;
  }
  if (wanted.lines.has_value() || wanted.height.has_value()) {
    const auto shown =
        typesetter_->get().fitted(text, style, bound, scale_, wanted);
    const text::TextSize size =
        shown.has_value()
            ? typesetter_->get()
                  .measure((*shown)->text, (*shown)->style, bound, scale_)
                  .value_or(text::TextSize{})
            : text::TextSize{};
    return YGSize{.width = std::min(bound.value(), size.width),
                  .height = reserve(style, reserved, size.height)};
  }
  const text::TextSize size = typesetter_->get()
                                  .measure(text, style, bound, scale_)
                                  .value_or(text::TextSize{});
  return YGSize{.width = std::min(bound.value(), size.width),
                .height = reserve(style, reserved, size.height)};
}

auto YogaLayout::reserve(const text::TextStyle& style,
                         std::optional<std::size_t> lines, float height)
    -> float {
  if (!lines.has_value() || *lines == 0) {
    return height;
  }
  return std::max(height, line_height(style) * static_cast<float>(*lines));
}

auto YogaLayout::line_height(const text::TextStyle& style) -> float {
  text::TextStyle single = style;
  single.line_spacing = 0.0F;
  return natural("A", single).height;
}

auto YogaLayout::settle_row(composition::Tree& tree,
                            const composition::Node& node, YGNodeRef laid)
    -> bool {
  const auto row = node.content_as<components::HStack::Content>();
  if (!row.has_value() || row->get().options().wrap) {
    return false;
  }
  row_entries_.clear();
  entries_of(node, std::nullopt, row_entries_);
  if (row_entries_.size() < 2) {
    return false;
  }
  const bool even = std::ranges::all_of(row_entries_, [](const Guided& entry) {
    return entry.node.get().modifiers().layout_priority == 0.0 &&
           (!entry.carried.has_value() ||
            entry.carried->get().layout_priority == 0.0);
  });
  float available = YGNodeLayoutGetWidth(laid) -
                    YGNodeLayoutGetPadding(laid, YGEdgeLeft) -
                    YGNodeLayoutGetPadding(laid, YGEdgeRight) -
                    (YGNodeStyleGetGap(laid, YGGutterColumn) *
                     static_cast<float>(row_entries_.size() - 1));
  row_texts_.clear();
  for (const Guided& entry : row_entries_) {
    const auto held = tree.layout_slot(entry.node.get());
    if (!held.has_value()) {
      return false;
    }
    auto& slot = static_cast<YogaSlot&>(held->get());
    const YGNodeRef child = slot.node();
    available -= YGNodeLayoutGetMargin(child, YGEdgeLeft) +
                 YGNodeLayoutGetMargin(child, YGEdgeRight);
    const auto text = slot.text();
    const bool shrinks =
        even && text.has_value() &&
        entry.node.get().content_as<components::Text::Content>().has_value() &&
        !fixes(entry.node.get(), composition::Axis::horizontal) &&
        !entry.node.get().modifiers().frame.width.has_value();
    if (!shrinks) {
      if (text.has_value()) {
        static_cast<void>(slot.set_cap(std::nullopt));
      }
      available -= YGNodeLayoutGetWidth(child);
      continue;
    }
    const float padding = YGNodeLayoutGetPadding(child, YGEdgeLeft) +
                          YGNodeLayoutGetPadding(child, YGEdgeRight);
    row_texts_.push_back(RowText{
        .slot = slot,
        .natural =
            natural(text->get().text, text->get().style).width + padding});
  }
  std::optional<float> level;
  float total = 0.0F;
  for (const RowText& text : row_texts_) {
    total += text.natural;
  }
  if (row_texts_.size() >= 2 && total > available + 0.5F && available > 0.0F) {
    std::ranges::sort(row_texts_, {}, &RowText::natural);
    float remaining = available;
    std::size_t count = row_texts_.size();
    for (const RowText& text : row_texts_) {
      const float share = remaining / static_cast<float>(count);
      if (text.natural > share) {
        level = share;
        break;
      }
      remaining -= text.natural;
      --count;
    }
  }
  bool changed = false;
  for (const RowText& text : row_texts_) {
    const std::optional<float> cap =
        level.has_value() && text.natural > *level ? level : std::nullopt;
    changed = text.slot.get().set_cap(cap) || changed;
  }
  return changed;
}

auto YogaLayout::baseline(std::string_view text, const text::TextStyle& style,
                          bool wraps, float width) -> float {
  if (!typesetter_.has_value()) {
    return 0.0F;
  }
  const auto laid = typesetter_->get().layout(
      text, style, wrap_width(text, style, wraps, width), scale_);
  return laid.has_value() ? (*laid)->first_baseline() : 0.0F;
}

auto YogaLayout::baselines(std::string_view text, const text::TextStyle& style,
                           bool wraps, float width)
    -> composition::TextBaselines {
  if (!typesetter_.has_value()) {
    return composition::TextBaselines{};
  }
  const auto laid = typesetter_->get().layout(
      text, style, wrap_width(text, style, wraps, width), scale_);
  if (!laid.has_value()) {
    return composition::TextBaselines{};
  }
  return composition::TextBaselines{.first = (*laid)->first_baseline(),
                                    .last = (*laid)->last_baseline()};
}

auto YogaLayout::prepare(const composition::Node& node, const Scope& scope,
                         Role role, bool overlay) -> std::size_t {
  const std::size_t first = gather(node, scope);
  const std::size_t last = entries_.size();
  distribute(std::span{entries_}.subspan(first, last - first), flow_of(node));
  if (!overlay && (role == Role::row_item || role == Role::column_item)) {
    rank(std::span{entries_}.subspan(first, last - first),
         role == Role::row_item);
    if (role == Role::column_item && shares(node)) {
      for (Entry& entry : std::span{entries_}.subspan(first, last - first)) {
        entry.scope.ranked = true;
      }
    }
  }
  return first;
}

auto YogaLayout::sync_children(composition::Tree& tree,
                               const composition::Node& node, Scope scope,
                               Role role, bool overlay)
    -> std::expected<Range, layout::LayoutError> {
  const std::size_t first = prepare(node, scope, role, overlay);
  const std::size_t last = entries_.size();
  const std::size_t begin = refs_.size();
  bool leading = true;
  for (std::size_t index = first; index < last; ++index) {
    const Entry entry = entries_[index];
    const Role placed = overlay && !leading ? Role::overlay : role;
    leading = false;
    auto child = sync(tree, entry.node.get(), entry.scope, placed);
    if (!child.has_value()) {
      release(first);
      return std::unexpected(child.error());
    }
    refs_.push_back(*child);
  }
  release(first);
  return Range{.begin = begin, .end = refs_.size()};
}

auto YogaLayout::space_children(composition::Tree& tree,
                                const composition::Node& node, Scope scope,
                                bool vertical, bool automatic, bool guided,
                                float overlap)
    -> std::expected<void, layout::LayoutError> {
  const std::size_t first = gather(node, scope);
  const std::size_t last = entries_.size();
  for (std::size_t index = first; index < last; ++index) {
    auto found = slot(tree, entries_[index].node.get());
    if (!found.has_value()) {
      release(first);
      return std::unexpected(found.error());
    }
    float gap = 0.0F;
    if (index > first) {
      gap = automatic ? automatic_gap(tree, entries_[index - 1].node.get(),
                                      entries_[index].node.get(), vertical,
                                      entries_[index].scope.theme)
                      : overlap;
    }
    found->get().set_gap(vertical ? theme::Edges{.top = gap}
                                  : theme::Edges{.leading = gap});
    if (!guided) {
      found->get().set_guide(theme::Edges{});
    }
  }
  release(first);
  return {};
}

auto YogaLayout::automatic_gap(composition::Tree& tree,
                               const composition::Node& before,
                               const composition::Node& after, bool vertical,
                               const theme::Theme& theme) -> float {
  return view_spacing(tree, before, theme)
      .distance(view_spacing(tree, after, theme),
                vertical ? composition::Axis::vertical
                         : composition::Axis::horizontal);
}

auto YogaLayout::view_spacing(composition::Tree& tree,
                              const composition::Node& node,
                              const theme::Theme& theme)
    -> components::ViewSpacing {
  if (custom(node)) {
    if (const auto arrangement = arrangement_of(tree, node)) {
      return arrangement->get().layout_spacing();
    }
  }
  return spacing_of(node, theme);
}

auto YogaLayout::arrangement_of(composition::Tree& tree,
                                const composition::Node& node)
    -> std::optional<std::reference_wrapper<CustomArrangement>> {
  const auto held = tree.layout_slot(node);
  if (!held.has_value()) {
    return std::nullopt;
  }
  return static_cast<YogaSlot&>(held->get()).arrangement();
}

void YogaLayout::begin_children(const composition::Node& node, Sync& sync,
                                const Scope& scope, Role role, bool overlay) {
  Gatherer& gatherer = sync.gatherer;
  gatherer.mode = Gathering::children;
  gatherer.role = role;
  gatherer.overlay = overlay;
  gatherer.first = prepare(node, scope, role, overlay);
  gatherer.last = entries_.size();
  gatherer.at = gatherer.first;
  gatherer.begin = refs_.size();
}

void YogaLayout::begin_decorated(Sync& sync, const Scope& scope,
                                 std::size_t base) {
  Gatherer& gatherer = sync.gatherer;
  gatherer.mode = Gathering::decorated;
  gatherer.base = base;
  gatherer.begin = refs_.size();
  tails_.back().basis = scope;
}

void YogaLayout::begin_members(const composition::Node& node, Sync& sync,
                               const Scope& scope, Role role) {
  Gatherer& gatherer = sync.gatherer;
  gatherer.mode = Gathering::members;
  gatherer.role = role;
  gatherer.first = gather(node, scope);
  gatherer.last = entries_.size();
  gatherer.at = gatherer.first;
}

void YogaLayout::begin_table(const composition::Node& node, Sync& sync,
                             const Scope& scope) {
  Gatherer& gatherer = sync.gatherer;
  gatherer.mode = Gathering::table;
  gatherer.tables = tables_.size();
  tables_.push_back(TableFrame{.node = &node, .position = 0, .scope = scope});
}

void YogaLayout::begin_cells(const composition::Node& node, Sync& sync,
                             const Scope& scope, bool vertical) {
  Gatherer& gatherer = sync.gatherer;
  gatherer.mode = Gathering::cells;
  gatherer.role = Role::column_item;
  gatherer.vertical = vertical;
  gatherer.first = gather(node, scope);
  gatherer.last = entries_.size();
  gatherer.at = gatherer.first;
}

auto YogaLayout::advance_sync(composition::Tree& tree,
                              const composition::Node& node, Sync& sync)
    -> const composition::Node* {
  Gatherer& gatherer = sync.gatherer;
  if (failure_.has_value()) {
    abandon(sync);
    return nullptr;
  }
  switch (gatherer.mode) {
    case Gathering::children:
    case Gathering::members:
    case Gathering::cells: {
      if (gatherer.at < gatherer.last) {
        const Entry& entry = entries_[gatherer.at++];
        sync.handed = entry.scope;
        sync.handed_role = gatherer.mode == Gathering::children &&
                                   gatherer.overlay && !gatherer.leading
                               ? Role::overlay
                               : gatherer.role;
        gatherer.leading = false;
        return &entry.node.get();
      }
      release(gatherer.first);
      if (gatherer.mode == Gathering::children) {
        tails_.back().children =
            Range{.begin = gatherer.begin, .end = refs_.size()};
      }
      gatherer.mode = Gathering::none;
      return nullptr;
    }
    case Gathering::decorated:
      return next_decorated(node, sync);
    case Gathering::table:
      return next_table(tree, sync);
    case Gathering::none:
      return nullptr;
  }
  return nullptr;
}

auto YogaLayout::next_decorated(const composition::Node& node, Sync& sync)
    -> const composition::Node* {
  Gatherer& gatherer = sync.gatherer;
  const auto children = node.children();
  const std::size_t count = std::ranges::size(children);
  while (true) {
    if (gatherer.admitted && gatherer.at < gatherer.last) {
      Entry entry = entries_[gatherer.at++];
      if (!gatherer.based) {
        entry.scope.hoisted_width = false;
        entry.scope.hoisted_height = false;
        entry.scope.open_width = false;
        entry.scope.open_height = false;
        entry.scope.fixed_width = false;
        entry.scope.fixed_height = false;
        const auto& bar =
            entry.node.get().interactions().scrolling.safe_area_bar;
        const bool beside = bar.has_value() && (*bar == theme::Edge::leading ||
                                                *bar == theme::Edge::trailing);
        entry.scope.safe = beside
                               ? theme::Edges{.top = entry.scope.safe.top,
                                              .bottom = entry.scope.safe.bottom}
                               : theme::Edges{};
      }
      sync.handed = entry.scope;
      sync.handed_role = gatherer.based ? Role::column_item : Role::overlay;
      return &entry.node.get();
    }
    if (gatherer.admitted) {
      release(gatherer.first);
      gatherer.admitted = false;
    }
    while (gatherer.position < count &&
           departing(children[gatherer.position])) {
      ++gatherer.position;
    }
    if (gatherer.position >= count) {
      tails_.back().children =
          Range{.begin = gatherer.begin, .end = refs_.size()};
      gatherer.mode = Gathering::none;
      return nullptr;
    }
    const composition::Node& child = children[gatherer.position++];
    gatherer.based = gatherer.index++ == gatherer.base;
    gatherer.first = entries_.size();
    admit(child, *tails_.back().basis);
    gatherer.last = entries_.size();
    gatherer.at = gatherer.first;
    gatherer.admitted = true;
  }
}

auto YogaLayout::next_table(composition::Tree& tree, Sync& sync)
    -> const composition::Node* {
  Gatherer& gatherer = sync.gatherer;
  while (true) {
    if (gatherer.row) {
      if (gatherer.at < gatherer.last) {
        const Entry& entry = entries_[gatherer.at++];
        sync.handed = entry.scope;
        sync.handed_role = Role::column_item;
        return &entry.node.get();
      }
      release(gatherer.first);
      gatherer.row = false;
      gatherer.line.end = tails_.back().grid_cells.size();
      tails_.back().grid_rows.push_back(gatherer.line);
      continue;
    }
    if (tables_.size() == gatherer.tables) {
      gatherer.mode = Gathering::none;
      return nullptr;
    }
    TableFrame& frame = tables_.back();
    const auto children = frame.node->children();
    if (frame.position >= std::ranges::size(children)) {
      tables_.pop_back();
      continue;
    }
    const composition::Node& child = children[frame.position++];
    if (departing(child)) {
      continue;
    }
    if (layered(child)) {
      portals_.push_back(Entry{.node = child, .scope = frame.scope});
      continue;
    }
    if (const auto row = child.content_as<components::GridRow::Content>()) {
      GridArrangement::Row line{.alignment = row->get().options().alignment,
                                .begin = tails_.back().grid_cells.size()};
      if (!transparent(child)) {
        auto found = slot(tree, child);
        if (!found.has_value()) {
          failure_ = found.error();
          abandon(sync);
          return nullptr;
        }
        found->get().set_children({});
        line.node = found->get().node();
      }
      Scope cell_scope = scoped(child, frame.scope);
      const composition::Modifiers& row_modifiers = child.modifiers();
      cell_scope.cell = child.paint_stamp();
      cell_scope.cell_padding = row_modifiers.padding;
      cell_scope.cell_margin = row_modifiers.margin;
      if (sized(row_modifiers.frame)) {
        cell_scope.cell_frame = row_modifiers.frame;
      }
      gatherer.first = gather(child, cell_scope);
      gatherer.last = entries_.size();
      gatherer.at = gatherer.first;
      gatherer.line = line;
      gatherer.row = true;
      continue;
    }
    if (transparent(child)) {
      Scope inner = scoped(child, frame.scope);
      tables_.push_back(
          TableFrame{.node = &child, .position = 0, .scope = std::move(inner)});
      continue;
    }
    sync.handed = frame.scope;
    sync.handed_role = Role::column_item;
    return &child;
  }
}

void YogaLayout::abandon(Sync& sync) {
  Gatherer& gatherer = sync.gatherer;
  switch (gatherer.mode) {
    case Gathering::children:
    case Gathering::members:
    case Gathering::cells:
      release(gatherer.first);
      break;
    case Gathering::decorated:
      if (gatherer.admitted) {
        release(gatherer.first);
      }
      break;
    case Gathering::table:
      if (gatherer.row) {
        release(gatherer.first);
      }
      tables_.erase(
          tables_.begin() + static_cast<std::ptrdiff_t>(gatherer.tables),
          tables_.end());
      break;
    case Gathering::none:
      break;
  }
  gatherer.mode = Gathering::none;
}

void YogaLayout::deliver(composition::Tree& tree,
                         const composition::Node& child, YGNodeRef result,
                         Sync* above) {
  if (above == nullptr) {
    returned_ = result;
    return;
  }
  Gatherer& gatherer = above->gatherer;
  Tail& tail = tails_.back();
  switch (gatherer.mode) {
    case Gathering::children:
    case Gathering::decorated:
      refs_.push_back(result);
      break;
    case Gathering::members: {
      const Entry& entry = entries_[gatherer.at - 1];
      const auto held = slot(tree, entry.node.get());
      tail.members.push_back(CustomArrangement::Member{
          .node = result,
          .source = entry.node,
          .theme = entry.scope.theme,
          .carried = entry.scope.carried,
          .carried_values = entry.scope.carried_values,
          .spacing = view_spacing(tree, entry.node.get(), entry.scope.theme),
          .ideal_width =
              held.has_value() ? held->get().ideal_width() : std::nullopt,
          .ideal_height =
              held.has_value() ? held->get().ideal_height() : std::nullopt});
      break;
    }
    case Gathering::cells:
      push_cell(entries_[gatherer.at - 1], result, gatherer.vertical,
                tail.grid_cells);
      break;
    case Gathering::table:
      if (gatherer.row) {
        push_cell(entries_[gatherer.at - 1], result, true, tail.grid_cells);
      } else {
        GridArrangement::Row line{.spans = true,
                                  .begin = tail.grid_cells.size()};
        tail.grid_cells.push_back(grid_cell(child, result, true));
        line.end = tail.grid_cells.size();
        tail.grid_rows.push_back(line);
      }
      break;
    case Gathering::none:
      break;
  }
}

void YogaLayout::push_cell(const Entry& entry, YGNodeRef result, bool vertical,
                           std::vector<GridArrangement::Cell>& cells) {
  cells.push_back(grid_cell(entry.node.get(), result, vertical));
  if (entry.scope.cell_frame.has_value()) {
    const composition::FrameSpec& outer = *entry.scope.cell_frame;
    const theme::Theme& theme = entry.scope.theme;
    cells.back().bounds = GridArrangement::Bounds{
        .width = points(outer.width, theme),
        .height = points(outer.height, theme),
        .min_width = points(outer.min_width, theme),
        .min_height = points(outer.min_height, theme),
        .max_width = fills(outer.max_width) ? std::nullopt
                                            : points(outer.max_width, theme),
        .max_height = fills(outer.max_height)
                          ? std::nullopt
                          : points(outer.max_height, theme)};
  }
}

auto YogaLayout::container(const composition::Node& node) -> bool {
  return node.content_as<components::VStack::Content>().has_value() ||
         node.content_as<components::HStack::Content>().has_value() ||
         node.content_as<components::ZStack::Content>().has_value() ||
         node.content_as<composition::Decoration>().has_value();
}

auto YogaLayout::declares(const composition::Node& node,
                          const composition::AlignmentKey& key,
                          std::size_t depth) -> bool {
  if (std::ranges::contains(node.modifiers().alignment_guides, key,
                            &composition::AlignmentGuide::key)) {
    return true;
  }
  if (const auto laid = node.content_as<components::AnyLayout::Content>()) {
    return laid->get().box().aligns();
  }
  if (depth == 0 || (!container(node) && !transparent(node))) {
    return false;
  }
  return std::ranges::any_of(node.children(),
                             [&key, depth](const composition::Node& child) {
                               return !departing(child) && !layered(child) &&
                                      declares(child, key, depth - 1);
                             });
}

auto YogaLayout::stack_guide(const composition::Node& node)
    -> std::optional<composition::AlignmentKey> {
  std::optional<composition::AlignmentKey> key;
  bool forced = false;
  if (const auto column = node.content_as<components::VStack::Content>()) {
    const components::VStackOptions& options = column->get().options();
    if (options.wrap) {
      return std::nullopt;
    }
    key = options.guide.value_or(composition::AlignmentKey{options.alignment});
    forced = options.guide.has_value();
  } else if (const auto row = node.content_as<components::HStack::Content>()) {
    const components::HStackOptions& options = row->get().options();
    if (options.wrap) {
      return std::nullopt;
    }
    key = options.guide.value_or(composition::AlignmentKey{options.alignment});
    forced =
        options.guide.has_value() ||
        options.alignment == composition::VerticalAlignment::last_text_baseline;
  } else {
    return std::nullopt;
  }
  if (forced) {
    return key;
  }
  const bool explicit_child = std::ranges::any_of(
      node.children(), [&key](const composition::Node& child) {
        return !departing(child) && !layered(child) &&
               declares(child, *key, guide_depth);
      });
  return explicit_child ? key : std::nullopt;
}

auto YogaLayout::guides_of(const Guided& entry)
    -> std::span<const composition::AlignmentGuide> {
  const std::vector<composition::AlignmentGuide>& own =
      entry.node.get().modifiers().alignment_guides;
  if (own.empty() && entry.carried.has_value()) {
    return entry.carried->get().alignment_guides;
  }
  return own;
}

void YogaLayout::entries_of(
    const composition::Node& node,
    std::optional<std::reference_wrapper<const composition::Modifiers>> carried,
    std::vector<Guided>& out) {
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    if (transparent(child)) {
      entries_of(child,
                 child.modifiers().alignment_guides.empty()
                     ? carried
                     : std::optional{std::cref(child.modifiers())},
                 out);
      continue;
    }
    out.push_back(Guided{.node = child, .carried = carried});
  }
}

auto YogaLayout::laid_node(composition::Tree& tree,
                           const composition::Node& node)
    -> std::optional<YGNodeRef> {
  const auto held = tree.layout_slot(node);
  if (!held.has_value()) {
    return std::nullopt;
  }
  return static_cast<YogaSlot&>(held->get()).node();
}

auto YogaLayout::baselines_of(composition::Tree& tree,
                              const composition::Node& node)
    -> composition::TextBaselines {
  const auto held = tree.layout_slot(node);
  if (!held.has_value()) {
    return composition::TextBaselines{};
  }
  const auto& slot = static_cast<YogaSlot&>(held->get());
  const YGNodeRef laid = slot.node();
  if (const auto text = slot.text()) {
    const float inset = YGNodeLayoutGetPadding(laid, YGEdgeTop);
    const float width = YGNodeLayoutGetWidth(laid) -
                        YGNodeLayoutGetPadding(laid, YGEdgeLeft) -
                        YGNodeLayoutGetPadding(laid, YGEdgeRight);
    const composition::TextBaselines found = baselines(
        text->get().text, text->get().style, text->get().wraps, width);
    return composition::TextBaselines{
        .first = found.first.transform(
            [inset](float value) { return value + inset; }),
        .last = found.last.transform(
            [inset](float value) { return value + inset; })};
  }
  if (!container(node)) {
    return composition::TextBaselines{};
  }
  std::vector<Guided> inner;
  entries_of(node, std::nullopt, inner);
  if (inner.empty()) {
    return composition::TextBaselines{};
  }
  const composition::Rect origin = absolute(laid);
  const auto shifted = [this, &tree, origin](
                           const composition::Node& child,
                           bool last) -> std::optional<float> {
    const auto child_laid = laid_node(tree, child);
    if (!child_laid.has_value()) {
      return std::nullopt;
    }
    const composition::TextBaselines found = baselines_of(tree, child);
    const std::optional<float>& value = last ? found.last : found.first;
    if (!value.has_value()) {
      return std::nullopt;
    }
    return *value + absolute(*child_laid).y - origin.y;
  };
  return composition::TextBaselines{
      .first = shifted(inner.front().node.get(), false),
      .last = shifted(inner.back().node.get(), true)};
}

auto YogaLayout::explicit_guide(composition::Tree& tree, const Guided& entry,
                                const composition::AlignmentKey& key,
                                std::size_t depth) -> std::optional<float> {
  const composition::Node& node = entry.node.get();
  const auto laid = laid_node(tree, node);
  if (!laid.has_value()) {
    return std::nullopt;
  }
  const std::span<const composition::AlignmentGuide> own = guides_of(entry);
  if (std::ranges::contains(own, key, &composition::AlignmentGuide::key)) {
    const composition::ViewDimensions dimensions{YGNodeLayoutGetWidth(*laid),
                                                 YGNodeLayoutGetHeight(*laid),
                                                 baselines_of(tree, node), own};
    return dimensions.explicit_value(key);
  }
  if (custom(node)) {
    const auto arrangement = arrangement_of(tree, node);
    if (!arrangement.has_value()) {
      return std::nullopt;
    }
    return arrangement->get().explicit_alignment(
        key, YGNodeLayoutGetWidth(*laid), YGNodeLayoutGetHeight(*laid));
  }
  if (!container(node)) {
    return std::nullopt;
  }
  std::vector<Guided> inner;
  entries_of(node, std::nullopt, inner);
  const composition::Rect origin = absolute(*laid);
  const bool across = key.axis() == composition::Axis::horizontal;
  std::vector<std::optional<float>> found;
  for (const Guided& child : inner) {
    if (!declares(child.node.get(), key, depth) &&
        !(child.carried.has_value() &&
          std::ranges::contains(child.carried->get().alignment_guides, key,
                                &composition::AlignmentGuide::key))) {
      continue;
    }
    const std::optional<float> value = explicit_guide(tree, child, key, depth);
    const auto child_laid = laid_node(tree, child.node.get());
    if (!value.has_value() || !child_laid.has_value()) {
      continue;
    }
    found.emplace_back(*value + (across ? leading_offset(*laid, *child_laid)
                                        : absolute(*child_laid).y - origin.y));
  }
  return key.combine_explicit(found);
}

auto YogaLayout::leading_offset(YGNodeRef parent, YGNodeRef child) const
    -> float {
  const composition::Rect outer = absolute(parent);
  const composition::Rect inner = absolute(child);
  if (YGNodeLayoutGetDirection(parent) == YGDirectionRTL) {
    return (outer.x + outer.width) - (inner.x + inner.width);
  }
  return inner.x - outer.x;
}

auto YogaLayout::guide_value(composition::Tree& tree, const Guided& entry,
                             const composition::AlignmentKey& key) -> float {
  if (const std::optional<float> found = explicit_guide(tree, entry, key)) {
    return *found;
  }
  const auto laid = laid_node(tree, entry.node.get());
  if (!laid.has_value()) {
    return 0.0F;
  }
  return composition::ViewDimensions{YGNodeLayoutGetWidth(*laid),
                                     YGNodeLayoutGetHeight(*laid),
                                     baselines_of(tree, entry.node.get())}
      .default_value(key);
}

auto YogaLayout::settle_guides(composition::Tree& tree,
                               const composition::Node& node) -> bool {
  const std::optional<composition::AlignmentKey> key = stack_guide(node);
  if (!key.has_value()) {
    return false;
  }
  std::vector<Guided> inner;
  entries_of(node, std::nullopt, inner);
  if (inner.empty()) {
    return false;
  }
  std::vector<float> values;
  values.reserve(inner.size());
  float line = 0.0F;
  for (const Guided& entry : inner) {
    values.push_back(guide_value(tree, entry, *key));
    line = std::max(line, values.back());
  }
  const bool across = key->axis() == composition::Axis::horizontal;
  bool changed = false;
  for (std::size_t index = 0; index < inner.size(); ++index) {
    const auto held = tree.layout_slot(inner[index].node.get());
    if (!held.has_value()) {
      continue;
    }
    auto& slot = static_cast<YogaSlot&>(held->get());
    const float offset = std::round((line - values[index]) * 64.0F) / 64.0F;
    const theme::Edges wanted =
        across ? theme::Edges{.leading = offset} : theme::Edges{.top = offset};
    const theme::Edges& current = slot.guide();
    if (std::abs(current.leading - wanted.leading) < 0.01F &&
        std::abs(current.top - wanted.top) < 0.01F) {
      continue;
    }
    slot.set_guide(wanted);
    changed = true;
  }
  return changed;
}

auto YogaLayout::settle_separator(composition::Tree& tree,
                                  const composition::Node& node) -> bool {
  if (!node.key().has_value() || *node.key() != separator_key ||
      !node.parent().has_value()) {
    return false;
  }
  const auto host = tree.node(*node.parent());
  if (!host.has_value()) {
    return false;
  }
  const auto decoration = host->get().content_as<composition::Decoration>();
  if (!decoration.has_value()) {
    return false;
  }
  std::optional<std::reference_wrapper<const composition::Node>> base;
  std::size_t index = 0;
  for (const composition::Node& child : host->get().children()) {
    if (index++ == decoration->get().base()) {
      base = std::cref(child);
      break;
    }
  }
  const auto held = tree.layout_slot(node);
  const auto laid = laid_node(tree, node);
  if (!base.has_value() || !held.has_value() || !laid.has_value()) {
    return false;
  }
  const auto base_laid = laid_node(tree, base->get());
  if (!base_laid.has_value()) {
    return false;
  }
  const Guided entry{.node = base->get(), .carried = std::nullopt};
  const std::optional<float> leading = explicit_guide(
      tree, entry,
      composition::AlignmentKey{composition::list_row_separator_leading},
      separator_depth);
  const std::optional<float> trailing = explicit_guide(
      tree, entry,
      composition::AlignmentKey{composition::list_row_separator_trailing},
      separator_depth);
  auto& slot = static_cast<YogaSlot&>(held->get());
  std::optional<theme::Edges> wanted;
  if (leading.has_value() || trailing.has_value()) {
    const composition::Rect placed = absolute(*laid);
    const composition::Rect based = absolute(*base_laid);
    const float start = YGNodeLayoutGetPadding(*laid, YGEdgeLeft);
    const float end = YGNodeLayoutGetPadding(*laid, YGEdgeRight);
    const auto snapped = [](float value) {
      return std::round(std::max(value, 0.0F) * 64.0F) / 64.0F;
    };
    wanted = theme::Edges{
        .leading =
            leading.has_value()
                ? snapped(based.x + *leading - placed.x)
                : (slot.inset().has_value() ? slot.inset()->leading : start),
        .trailing =
            trailing.has_value()
                ? snapped(placed.x + placed.width - (based.x + *trailing))
                : (slot.inset().has_value() ? slot.inset()->trailing : end)};
  }
  const std::optional<theme::Edges>& current = slot.inset();
  if (current.has_value() == wanted.has_value() &&
      (!wanted.has_value() ||
       (std::abs(current->leading - wanted->leading) < 0.01F &&
        std::abs(current->trailing - wanted->trailing) < 0.01F))) {
    return false;
  }
  slot.set_inset(wanted);
  return true;
}

auto YogaLayout::ranks(const composition::Node& node) -> bool {
  if (!node.content_as<components::VStack::Content>().has_value() &&
      !node.content_as<components::HStack::Content>().has_value()) {
    return false;
  }
  std::optional<double> first;
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    const double priority = child.modifiers().layout_priority;
    if (!first.has_value()) {
      first = priority;
    } else if (*first != priority) {
      return true;
    }
  }
  return false;
}

auto YogaLayout::shares(const composition::Node& node) -> bool {
  const auto column = node.content_as<components::VStack::Content>();
  if (!column.has_value() || column->get().options().wrap) {
    return false;
  }
  std::size_t texts = 0;
  std::optional<double> first;
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    if (!child.content_as<components::Text::Content>().has_value()) {
      return false;
    }
    const double priority = child.modifiers().layout_priority;
    if (first.has_value() && *first != priority) {
      return false;
    }
    first = priority;
    ++texts;
  }
  return texts >= 2;
}

auto YogaLayout::settle_shares(composition::Tree& tree,
                               const composition::Node& node, YogaSlot& slot)
    -> bool {
  const bool sharing = shares(node) && slot.aligns();
  if (!sharing && !slot.distributes()) {
    return false;
  }
  slot.set_distributes(sharing);
  struct Share {
    std::reference_wrapper<YogaSlot> slot;
    float natural{0.0F};
    float line{0.0F};
    float padding{0.0F};
  };
  std::vector<Share> texts;
  bool changed = false;
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    const auto held = tree.layout_slot(child);
    if (!held.has_value()) {
      continue;
    }
    auto& text_slot = static_cast<YogaSlot&>(held->get());
    if (!sharing) {
      changed = text_slot.set_share(std::nullopt) || changed;
      continue;
    }
    const auto measured_text = text_slot.text();
    if (!measured_text.has_value()) {
      continue;
    }
    const YGNodeRef laid = text_slot.node();
    const float padding = YGNodeLayoutGetPadding(laid, YGEdgeTop) +
                          YGNodeLayoutGetPadding(laid, YGEdgeBottom);
    const float width = YGNodeLayoutGetWidth(laid) -
                        YGNodeLayoutGetPadding(laid, YGEdgeLeft) -
                        YGNodeLayoutGetPadding(laid, YGEdgeRight);
    const YGSize natural_size = measured(
        measured_text->get().text, measured_text->get().style,
        measured_text->get().wraps, width, YGMeasureModeAtMost, std::nullopt,
        measured_text->get().fitting, measured_text->get().reserved);
    texts.push_back(Share{.slot = text_slot,
                          .natural = natural_size.height + padding,
                          .line = line_height(measured_text->get().style),
                          .padding = padding});
  }
  if (!sharing) {
    return changed;
  }
  const YGNodeRef outer = slot.node();
  const YGNodeRef content = slot.content();
  float room = YGNodeLayoutGetHeight(outer) -
               YGNodeLayoutGetPadding(outer, YGEdgeTop) -
               YGNodeLayoutGetPadding(outer, YGEdgeBottom) -
               YGNodeLayoutGetPadding(content, YGEdgeTop) -
               YGNodeLayoutGetPadding(content, YGEdgeBottom);
  float wanted = 0.0F;
  for (std::size_t index = 0; index < texts.size(); ++index) {
    const YGNodeRef laid = texts[index].slot.get().node();
    room -= YGNodeLayoutGetMargin(laid, YGEdgeTop) +
            YGNodeLayoutGetMargin(laid, YGEdgeBottom);
    if (index > 0) {
      room -= YGNodeStyleGetGap(content, YGGutterRow);
    }
    wanted += texts[index].natural;
  }
  if (wanted <= room + 0.5F) {
    for (Share& text : texts) {
      changed = text.slot.get().set_share(std::nullopt) || changed;
    }
    return changed;
  }
  std::vector<std::size_t> order(texts.size());
  for (std::size_t index = 0; index < order.size(); ++index) {
    order[index] = index;
  }
  std::ranges::stable_sort(order, {}, [&texts](std::size_t index) {
    return texts[index].natural - texts[index].line - texts[index].padding;
  });
  for (std::size_t step = 0; step < order.size(); ++step) {
    Share& text = texts[order[step]];
    const float offered = room / static_cast<float>(order.size() - step);
    float given = text.natural;
    if (given > offered) {
      const float lines = std::max(
          std::floor((offered - text.padding + 0.5F) / text.line), 1.0F);
      given = (lines * text.line) + text.padding;
    }
    room -= given;
    changed = text.slot.get().set_share(given) || changed;
  }
  return changed;
}

auto YogaLayout::layered_guides(const composition::Node& node)
    -> LayeredGuides {
  const auto layered_content = node.content_as<components::ZStack::Content>();
  if (!layered_content.has_value()) {
    return LayeredGuides{};
  }
  const components::ZStackOptions& options = layered_content->get().options();
  const composition::AlignmentKey across = options.horizontal_guide.value_or(
      composition::AlignmentKey{options.alignment.horizontal});
  const composition::AlignmentKey down = options.vertical_guide.value_or(
      composition::AlignmentKey{options.alignment.vertical});
  return LayeredGuides{
      .horizontal = options.horizontal_guide.has_value() || guided(node, across)
                        ? std::optional{across}
                        : std::nullopt,
      .vertical = options.vertical_guide.has_value() || guided(node, down)
                      ? std::optional{down}
                      : std::nullopt};
}

auto YogaLayout::guided(const composition::Node& node,
                        const composition::AlignmentKey& key) -> bool {
  return std::ranges::any_of(node.children(),
                             [&key](const composition::Node& child) {
                               return !departing(child) && !layered(child) &&
                                      declares(child, key, guide_depth);
                             });
}

auto YogaLayout::settle_layered(composition::Tree& tree,
                                const composition::Node& node, YogaSlot& slot)
    -> bool {
  if (!node.content_as<components::ZStack::Content>().has_value()) {
    return false;
  }
  const LayeredGuides keys = layered_guides(node);
  std::vector<Guided> inner;
  entries_of(node, std::nullopt, inner);
  bool changed = false;
  if (!keys.horizontal.has_value() && !keys.vertical.has_value()) {
    for (const Guided& entry : inner) {
      if (const auto held = tree.layout_slot(entry.node.get())) {
        auto& child = static_cast<YogaSlot&>(held->get());
        if (child.guide() != theme::Edges{}) {
          child.set_guide(theme::Edges{});
          changed = true;
        }
      }
    }
    return slot.set_extent(std::nullopt) || changed;
  }
  std::vector<composition::Point> values;
  std::vector<composition::Size> sizes;
  values.reserve(inner.size());
  sizes.reserve(inner.size());
  composition::Point line{};
  for (const Guided& entry : inner) {
    const auto laid = laid_node(tree, entry.node.get());
    sizes.push_back(
        laid.has_value()
            ? composition::Size{.width = YGNodeLayoutGetWidth(*laid),
                                .height = YGNodeLayoutGetHeight(*laid)}
            : composition::Size{});
    values.push_back(
        composition::Point{.x = keys.horizontal.has_value()
                                    ? guide_value(tree, entry, *keys.horizontal)
                                    : 0.0F,
                           .y = keys.vertical.has_value()
                                    ? guide_value(tree, entry, *keys.vertical)
                                    : 0.0F});
    line.x = std::max(line.x, values.back().x);
    line.y = std::max(line.y, values.back().y);
  }
  composition::Size extent{};
  for (std::size_t index = 0; index < inner.size(); ++index) {
    const auto held = tree.layout_slot(inner[index].node.get());
    if (!held.has_value()) {
      continue;
    }
    auto& child = static_cast<YogaSlot&>(held->get());
    const auto snapped = [](float value) {
      return std::round(value * 64.0F) / 64.0F;
    };
    const theme::Edges wanted{.top = keys.vertical.has_value()
                                         ? snapped(line.y - values[index].y)
                                         : 0.0F,
                              .leading = keys.horizontal.has_value()
                                             ? snapped(line.x - values[index].x)
                                             : 0.0F};
    extent.width = std::max(extent.width, wanted.leading + sizes[index].width);
    extent.height = std::max(extent.height, wanted.top + sizes[index].height);
    const theme::Edges& current = child.guide();
    if (std::abs(current.leading - wanted.leading) >= 0.01F ||
        std::abs(current.top - wanted.top) >= 0.01F) {
      child.set_guide(wanted);
      changed = true;
    }
  }
  const YGNodeRef laid = slot.content();
  return slot.set_extent(std::array{
             keys.horizontal.has_value()
                 ? extent.width + YGNodeLayoutGetPadding(laid, YGEdgeLeft) +
                       YGNodeLayoutGetPadding(laid, YGEdgeRight)
                 : 0.0F,
             keys.vertical.has_value()
                 ? extent.height + YGNodeLayoutGetPadding(laid, YGEdgeTop) +
                       YGNodeLayoutGetPadding(laid, YGEdgeBottom)
                 : 0.0F}) ||
         changed;
}

void YogaLayout::table_cells(const composition::Node& node,
                             std::vector<TableRow>& rows) {
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    if (const auto row = child.content_as<components::GridRow::Content>()) {
      rows.push_back(TableRow{.alignment = row->get().options().alignment});
      entries_of(child, std::nullopt, rows.back().cells);
      continue;
    }
    if (transparent(child)) {
      table_cells(child, rows);
    }
  }
}

auto YogaLayout::settle_table_guides(composition::Tree& tree,
                                     const composition::Node& node) -> bool {
  const auto table = node.content_as<components::Grid::Content>();
  if (!table.has_value()) {
    return false;
  }
  const components::GridOptions& options = table->get().options();
  std::vector<TableRow> rows;
  table_cells(node, rows);
  bool changed = false;
  std::vector<std::vector<Guided>> columns;
  std::vector<std::optional<composition::HorizontalAlignment>> aligned;
  for (TableRow& row : rows) {
    std::size_t column = 0;
    std::vector<Guided> free;
    for (const Guided& cell : row.cells) {
      const std::optional<composition::GridCell>& spec =
          cell.node.get().modifiers().grid_cell;
      const std::size_t span = spec.has_value() ? spec->columns : 1U;
      const bool anchored = spec.has_value() && spec->anchor.has_value();
      if (columns.size() < column + span) {
        columns.resize(column + span);
        aligned.resize(column + span);
      }
      if (spec.has_value() && spec->column_alignment.has_value()) {
        aligned[column] = spec->column_alignment;
      }
      if (span == 1 && !anchored) {
        columns[column].push_back(cell);
      }
      if (!anchored) {
        free.push_back(cell);
      }
      column += span;
    }
    changed = shift_cells(tree, free,
                          composition::AlignmentKey{row.alignment.value_or(
                              options.alignment.vertical)}) ||
              changed;
  }
  for (std::size_t column = 0; column < columns.size(); ++column) {
    changed = shift_cells(tree, columns[column],
                          composition::AlignmentKey{aligned[column].value_or(
                              options.alignment.horizontal)}) ||
              changed;
  }
  if (changed) {
    if (const auto held = tree.layout_slot(node)) {
      static_cast<YogaSlot&>(held->get()).relay_grid();
    }
  }
  return changed;
}

auto YogaLayout::shift_cells(composition::Tree& tree,
                             std::span<const Guided> cells,
                             const composition::AlignmentKey& key) -> bool {
  const bool across = key.axis() == composition::Axis::horizontal;
  const bool guided_cells =
      std::ranges::any_of(cells, [&key](const Guided& cell) {
        return declares(cell.node.get(), key, guide_depth) ||
               (cell.carried.has_value() &&
                std::ranges::contains(cell.carried->get().alignment_guides, key,
                                      &composition::AlignmentGuide::key));
      });
  std::vector<float> defaults;
  std::vector<float> values;
  float line = 0.0F;
  float plain = 0.0F;
  if (guided_cells) {
    for (const Guided& cell : cells) {
      const auto laid = laid_node(tree, cell.node.get());
      const float fallback =
          laid.has_value()
              ? composition::ViewDimensions{YGNodeLayoutGetWidth(*laid),
                                            YGNodeLayoutGetHeight(*laid),
                                            baselines_of(tree, cell.node.get())}
                    .default_value(key)
              : 0.0F;
      defaults.push_back(fallback);
      values.push_back(guide_value(tree, cell, key));
      line = std::max(line, values.back());
      plain = std::max(plain, fallback);
    }
  }
  bool changed = false;
  for (std::size_t index = 0; index < cells.size(); ++index) {
    const auto held = tree.layout_slot(cells[index].node.get());
    if (!held.has_value()) {
      continue;
    }
    auto& slot = static_cast<YogaSlot&>(held->get());
    const float offset =
        guided_cells
            ? std::round((defaults[index] - values[index] + line - plain) *
                         64.0F) /
                  64.0F
            : 0.0F;
    theme::Edges wanted = slot.shift();
    if (across) {
      wanted.leading = offset;
    } else {
      wanted.top = offset;
    }
    changed = slot.set_shift(wanted) || changed;
  }
  return changed;
}

auto YogaLayout::custom(const composition::Node& node) -> bool {
  return node.content_as<components::AnyLayout::Content>().has_value();
}

auto YogaLayout::spacing_of(const composition::Node& node,
                            const theme::Theme& theme)
    -> components::ViewSpacing {
  if (node.content_as<components::Spacer::Content>().has_value()) {
    return components::ViewSpacing::zero;
  }
  return components::ViewSpacing{
      theme.scaled(theme::metric::stack_spacing),
      theme::metrics::stack.text_spacing,
      node.content_as<components::Text::Content>().has_value()};
}

void YogaLayout::withhold(composition::Tree& tree, const YogaSlot& slot) {
  const auto held = slot.arrangement();
  if (!held.has_value()) {
    return;
  }
  const CustomArrangement& arrangement = held->get();
  const std::span<const CustomArrangement::Member> members =
      arrangement.members();
  for (std::size_t index = 0; index < members.size(); ++index) {
    const composition::Node& member = members[index].source.get();
    if (tree.presentation(member).withhold(arrangement.hidden(index))) {
      tree.repaint(member.id());
    }
  }
}

auto YogaLayout::grid_cell(const composition::Node& cell, YGNodeRef node,
                           bool vertical) -> GridArrangement::Cell {
  const std::optional<composition::GridCell>& spec = cell.modifiers().grid_cell;
  const bool divider =
      cell.content_as<components::Divider::Content>().has_value();
  return GridArrangement::Cell{
      .node = node,
      .span = spec.has_value() ? spec->columns : 1U,
      .column_alignment =
          spec.has_value() ? spec->column_alignment : std::nullopt,
      .anchor = spec.has_value() ? spec->anchor : std::nullopt,
      .flexible = stretches(cell, vertical) || (divider && vertical),
      .fills = stretches(cell, !vertical) || (divider && !vertical),
      .unsized = spec.has_value() ? spec->unsized_axes : composition::Axes{},
      .text = cell.content_as<components::Text::Content>().has_value()};
}

auto YogaLayout::table_stretches(const composition::Node& node) -> bool {
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    const bool row =
        child.content_as<components::GridRow::Content>().has_value();
    if (row || transparent(child)) {
      if (table_stretches(child)) {
        return true;
      }
      continue;
    }
    const std::optional<composition::GridCell>& spec =
        child.modifiers().grid_cell;
    if (spec.has_value() &&
        spec->unsized_axes.contains(composition::Axis::horizontal)) {
      continue;
    }
    if (stretches(child, true) ||
        child.content_as<components::Divider::Content>().has_value()) {
      return true;
    }
  }
  return false;
}

auto YogaLayout::sync_portals(composition::Tree& tree, bool force)
    -> std::expected<bool, layout::LayoutError> {
  bool calculated = false;
  for (std::size_t index = 0; index < portals_.size(); ++index) {
    const Entry entry = portals_[index];
    auto synced = sync(tree, entry.node.get(), entry.scope, Role::layer);
    if (!synced.has_value()) {
      return std::unexpected(synced.error());
    }
    if (force || YGNodeIsDirty(*synced)) {
      YGNodeCalculateLayout(*synced, YGUndefined, YGUndefined,
                            direction(entry.scope.direction));
      ++calculations_;
      place(tree);
      static_cast<void>(read_back(tree, entry.node.get()));
      for (std::uint32_t round = 0;
           round < relayout_rounds && std::exchange(again_, false); ++round) {
        YGNodeCalculateLayout(*synced, YGUndefined, YGUndefined,
                              direction(entry.scope.direction));
        ++calculations_;
        static_cast<void>(read_back(tree, entry.node.get()));
      }
      again_ = false;
      calculated = true;
    }
  }
  return calculated;
}

void YogaLayout::place(composition::Tree& tree) {
  if (placing_) {
    return;
  }
  tree.seal_paint();
  settled_ = tree.paint_epoch();
  placing_ = true;
}

auto YogaLayout::visit_of(const composition::Node& node, const Scope& scope,
                          Role role) noexcept -> YogaSlot::Visit {
  return YogaSlot::Visit{
      .stamp = node.paint_stamp(),
      .role = static_cast<std::uint8_t>(role),
      .geometry = scope.geometry,
      .dynamic_type_size = scope.dynamic_type_size,
      .dynamic_type_range = scope.dynamic_type_range,
      .locale = reinterpret_cast<std::uintptr_t>(&scope.locale.get()),
      .direction = static_cast<std::uint8_t>(scope.direction),
      .font = scope.font,
      .text = scope.text,
      .insets = scope.insets,
      .safe = scope.safe,
      .open_width = scope.open_width,
      .open_height = scope.open_height,
      .fixed_width = scope.fixed_width,
      .fixed_height = scope.fixed_height,
      .scrolled_width = scope.scrolled_width,
      .scrolled_height = scope.scrolled_height,
      .shrink_weight = scope.shrink_weight,
      .grow_blocked = scope.grow_blocked,
      .ranked = scope.ranked,
      .cell = scope.cell,
      .enclosure = scope.enclosure,
      .unoriented = scope.unoriented};
}

auto YogaLayout::sync(composition::Tree& tree, const composition::Node& node,
                      const Scope& parent, Role role)
    -> std::expected<YGNodeRef, layout::LayoutError> {
  failure_.reset();
  returned_ = nullptr;
  Syncer syncer{*this, tree};
  sync_walk_.run(node, Sync{.handed = parent, .handed_role = role}, syncer);
  if (failure_.has_value()) {
    return std::unexpected(*std::exchange(failure_, std::nullopt));
  }
  return returned_;
}

auto YogaLayout::Syncer::enter(const composition::Node& node, const Sync& above)
    -> SyncWalk::Step {
  return layout_.get().open_sync(tree_, node, above);
}

auto YogaLayout::Syncer::next(const composition::Node& node, Sync& sync)
    -> const composition::Node* {
  return layout_.get().advance_sync(tree_, node, sync);
}

void YogaLayout::Syncer::leave(const composition::Node& node, Sync& sync,
                               Sync* above) {
  layout_.get().close_sync(tree_, node, sync, above);
}

auto YogaLayout::open_sync(composition::Tree& tree,
                           const composition::Node& node, const Sync& above)
    -> SyncWalk::Step {
  SyncWalk::Step step{.state = Sync{}};
  Sync& sync = step.state;
  const Scope& parent = *above.handed;
  const Role role = above.handed_role;
  auto found = slot(tree, node);
  if (!found.has_value()) {
    failure_ = found.error();
    step.descend = false;
    return step;
  }
  YogaSlot& target = found->get();
  const YogaSlot::Visit visit = visit_of(node, parent, role);
  if (!force_ && target.current(visit, settled_)) {
    sync.result = target.node();
    step.descend = false;
    return step;
  }
  ++visits_;
  node.cost().laid_out();
  const std::size_t portals = portals_.size();
  const std::size_t bars = bars_.size();
  Scope scope = scoped(node, parent);
  scope.priority = 0.0;
  scope.shrink_weight = 1.0F;
  scope.grow_blocked = false;
  scope.ranked = false;
  scope.cell = 0;
  scope.cell_padding.reset();
  scope.cell_margin.reset();
  scope.cell_frame.reset();
  scope.hoisted_width = false;
  scope.hoisted_height = false;
  scope.unoriented = false;
  scope.carried_values.reset();
  const theme::Theme& theme = scope.theme;
  const composition::Modifiers& modifiers = node.modifiers();
  if (surveys(node)) {
    tree.assign_enclosure(node.id(), parent.enclosure);
    tree.assign_safe_area(node.id(), scope.safe);
  }
  if (modifiers.container_shape.has_value() &&
      !modifiers.container_shape->relative()) {
    scope.enclosure = composition::ContainerCorners::Enclosure{
        .container = node.id(),
        .radii = corner_radii(theme, *modifiers.container_shape)};
  }
  if (fixes(node, composition::Axis::horizontal)) {
    scope.open_width = true;
    scope.fixed_width = true;
  }
  if (fixes(node, composition::Axis::vertical)) {
    scope.open_height = true;
    scope.fixed_height = true;
  }
  const bool unwrapped = scope.fixed_width;
  const auto button = node.content_as<components::Button::Content>();
  const bool automatic = button.has_value() && button->get().automatic();
  const bool framed =
      node.content().framed() || (button.has_value() && !automatic);
  YogaSlot::Style style;
  style.direction = direction(scope.direction);
  const theme::Edges padded = theme.edges(
      automatic ? modifiers.padding.value_or(components::Button::label_insets)
                : modifiers.padding.value_or(theme::Insets{}));
  style.padding = padded.positive();
  style.margin = plus(theme.edges(modifiers.margin.value_or(theme::Insets{})),
                      padded.negative());
  if (parent.cell_padding.has_value()) {
    style.margin = plus(style.margin, theme.edges(*parent.cell_padding));
  }
  if (parent.cell_margin.has_value()) {
    style.margin = plus(style.margin, theme.edges(*parent.cell_margin));
  }
  style.overflow = modifiers.clip ? YGOverflowHidden : YGOverflowVisible;
  const auto decoration = node.content_as<composition::Decoration>();
  const auto reader = node.content_as<components::GeometryReader::Content>();
  const auto positioned = node.content_as<composition::Position>();
  composition::FrameSpec frame = modifiers.frame;
  if (decoration.has_value()) {
    frame = decorated_frame(node, decoration->get().base());
  } else if (framed) {
    frame = inherited_frame(node);
  }
  if (parent.hoisted_width && frame.width.has_value() &&
      frame.width->is_fraction()) {
    frame.width = composition::Length::percent(1.0F);
  }
  if (parent.hoisted_height && frame.height.has_value() &&
      frame.height->is_fraction()) {
    frame.height = composition::Length::percent(1.0F);
  }
  if (reader.has_value() || positioned.has_value()) {
    if (!frame.width.has_value() && !frame.max_width.has_value() &&
        !scope.open_width) {
      frame.max_width = composition::infinity;
    }
    if (!frame.height.has_value() && !frame.max_height.has_value() &&
        !scope.open_height) {
      frame.max_height = composition::infinity;
    }
  }
  if (reader.has_value()) {
    if (!frame.ideal_width.has_value()) {
      frame.ideal_width = components::GeometryReader::ideal_length;
    }
    if (!frame.ideal_height.has_value()) {
      frame.ideal_height = components::GeometryReader::ideal_length;
    }
  }
  target.relate(std::nullopt);
  if (modifiers.relative_frame.has_value()) {
    frame = relate(tree, node, frame, scope);
    target.relate(
        std::max(theme.space(modifiers.relative_frame->spacing()), 0.0F));
  }
  style = sizing(frame, role, std::move(style), scope);
  target.idealize(ideal_of(frame.width, frame.ideal_width, theme),
                  ideal_of(frame.height, frame.ideal_height, theme));
  const bool open_width = scope.open_width;
  const bool open_height = scope.open_height;
  if (!lazy_root(tree, node)) {
    style =
        flexed(node, frame, role, open_width, open_height, std::move(style));
  }
  scope = opened(node, scope);
  sync.target = &target;
  sync.role = role;
  sync.tailed = true;
  Tail& tail = tails_.emplace_back(Tail{});
  tail.visit = visit;
  std::optional<YogaSlot::Measurement>& measured = tail.measured;
  std::optional<GridArrangement::Layout>& gridded = tail.gridded;
  std::shared_ptr<const components::AnyLayout::Box>& arranged = tail.arranged;
  const std::uintptr_t typesetter =
      typesetter_.has_value()
          ? reinterpret_cast<std::uintptr_t>(&typesetter_->get())
          : 0U;
  const Role inner =
      role == Role::row_item ? Role::row_item : Role::column_item;
  tail.children = Range{.begin = refs_.size(), .end = refs_.size()};
  if (const auto spacer = node.content_as<components::Spacer::Content>()) {
    const float minimum = spacer->get().minimum_length();
    style.grow = 1.0F;
    style.shrink = 1.0F;
    if (parent.unoriented) {
      style.min_width = std::max(style.min_width.value_or(0.0F), minimum);
      style.min_height = std::max(style.min_height.value_or(0.0F), minimum);
      style.align_self = YGAlignStretch;
    } else if (role == Role::row_item) {
      style.min_width = std::max(style.min_width.value_or(0.0F), minimum);
    } else if (role == Role::column_item) {
      style.min_height = std::max(style.min_height.value_or(0.0F), minimum);
    }
  } else if (node.content_as<components::Divider::Content>().has_value()) {
    const YogaSlot::Length hairline{.unit = YogaSlot::Length::Unit::points,
                                    .value = 1.0F / scale_.value()};
    const bool across = role == Role::row_item;
    if (across) {
      style.width = hairline;
    } else {
      style.height = hairline;
    }
    if (!(across ? frame.height : frame.width).has_value()) {
      style.align_self = YGAlignStretch;
    }
  } else if (framed) {
    style.flex_direction = YGFlexDirectionColumn;
    const bool imaged =
        node.content_as<components::Image::Content>().has_value();
    style.justify =
        imaged ? justify(frame.alignment.vertical) : YGJustifyCenter;
    style.align_items = imaged ? framed_cross(node, frame.alignment.horizontal)
                               : YGAlignStretch;
    begin_children(node, sync, scope, Role::column_item, false);
  } else if (button.has_value() && node.child_count() > 0) {
    style.flex_direction = YGFlexDirectionColumn;
    style.justify = justify(frame.alignment.vertical);
    style.align_items = align(frame.alignment.horizontal);
    begin_children(node, sync, scope, Role::column_item, false);
  } else if (button.has_value()) {
    measured = YogaSlot::Measurement{
        .text = button->get().localized_label(scope.text.catalog, scope.locale),
        .style = components::Text::text_style(
            theme, theme::Font{components::Button::label_role}, scope.locale),
        .wraps = false,
        .scale = scale_,
        .typesetter = typesetter};
  } else if (const auto text = node.content_as<components::Text::Content>()) {
    const composition::TextAttributes attributes =
        text->get().attributes_in(scope.text);
    const components::AttributedString& shown =
        text->get().resolved(attributes, scope.locale);
    measured = YogaSlot::Measurement{
        .text = shown.text(),
        .style = components::Text::text_style(
            theme, text->get().font_in(scope.font), scope.locale,
            text->get().weight(), attributes, scope.direction),
        .wraps = !unwrapped,
        .scale = scale_,
        .typesetter = typesetter,
        .fitting = components::Text::fitting(attributes, text->get().marquee()),
        .reserved = attributes.line_limit.minimum.has_value()
                        ? attributes.line_limit.minimum
                        : (attributes.line_limit.reserves_space
                               ? attributes.line_limit.lines
                               : std::nullopt),
        .variants = text->get().variants(attributes, scope.locale)};
    measured->style.runs =
        components::Text::styled_runs(theme, measured->style, shown);
    if (attributes.publishes_layout) {
      const std::optional<YogaSlot::Publication>& held = target.publication();
      if (!held.has_value() || held->text == nullptr || *held->text != shown ||
          held->custom != text->get().custom() ||
          held->foreground != text->get().foreground()) {
        target.set_publication(YogaSlot::Publication{
            .text = std::make_shared<const components::AttributedString>(shown),
            .custom = text->get().custom(),
            .foreground = text->get().foreground()});
      }
    } else {
      target.set_publication(std::nullopt);
    }
    if (role == Role::row_item) {
      style.shrink = 1.0F;
    } else if (role == Role::column_item && parent.ranked && !unwrapped &&
               !fixes(node, composition::Axis::vertical)) {
      style.shrink = 1.0F;
      style.min_height = std::max(style.min_height.value_or(0.0F),
                                  line_height(measured->style));
    }
  } else if (const auto vertical =
                 node.content_as<components::VStack::Content>()) {
    const components::VStackOptions& options = vertical->get().options();
    const bool automatic_spacing = !options.spacing.has_value();
    const float spacing = automatic_spacing
                              ? theme.scaled(theme::metric::stack_spacing)
                              : theme.space(*options.spacing);
    const bool guided = stack_guide(node).has_value();
    style.flex_direction = YGFlexDirectionColumn;
    const float overlap =
        automatic_spacing || options.wrap ? 0.0F : std::min(spacing, 0.0F);
    style.gap_row = automatic_spacing ? 0.0F : std::max(spacing, 0.0F);
    style.align_items = guided ? YGAlignFlexStart : align(options.alignment);
    style.justify = justify(options.distribution);
    if (options.wrap) {
      style.wrap = YGWrapWrap;
      style.gap_column = std::max(spacing, 0.0F);
    }
    begin_children(node, sync, scope, Role::column_item, false);
    tail.spacing = Spacing{.active = true,
                           .vertical = true,
                           .automatic = automatic_spacing && !options.wrap,
                           .guided = guided,
                           .overlap = overlap};
  } else if (const auto horizontal =
                 node.content_as<components::HStack::Content>()) {
    const components::HStackOptions& options = horizontal->get().options();
    const bool automatic_spacing = !options.spacing.has_value();
    const float spacing = automatic_spacing
                              ? theme.scaled(theme::metric::stack_spacing)
                              : theme.space(*options.spacing);
    const bool guided = stack_guide(node).has_value();
    style.flex_direction = YGFlexDirectionRow;
    const float overlap =
        automatic_spacing || options.wrap ? 0.0F : std::min(spacing, 0.0F);
    style.gap_column = automatic_spacing ? 0.0F : std::max(spacing, 0.0F);
    style.align_items = guided ? YGAlignFlexStart : align(options.alignment);
    style.justify = justify(options.distribution);
    if (options.wrap) {
      style.wrap = YGWrapWrap;
      style.gap_row = std::max(spacing, 0.0F);
      style.gap_column = std::max(spacing, 0.0F);
    }
    begin_children(node, sync, scope, Role::row_item, false);
    tail.spacing = Spacing{.active = true,
                           .vertical = false,
                           .automatic = automatic_spacing && !options.wrap,
                           .guided = guided,
                           .overlap = overlap};
  } else if (const auto layered =
                 node.content_as<components::ZStack::Content>()) {
    const components::Alignment alignment = layered->get().alignment();
    const LayeredGuides guides = layered_guides(node);
    style.flex_direction = YGFlexDirectionColumn;
    style.justify = guides.vertical.has_value() ? YGJustifyFlexStart
                                                : justify(alignment.vertical);
    style.align_items = guides.horizontal.has_value()
                            ? YGAlignFlexStart
                            : align(alignment.horizontal);
    begin_children(node, sync, scope, Role::column_item, true);
  } else if (reader.has_value()) {
    style.flex_direction = YGFlexDirectionColumn;
    style.justify = YGJustifyFlexStart;
    style.align_items = YGAlignFlexStart;
    begin_children(node, sync, scope, Role::column_item, true);
  } else if (positioned.has_value()) {
    style.flex_direction = YGFlexDirectionColumn;
    style.justify = YGJustifyFlexStart;
    style.align_items = YGAlignFlexStart;
    begin_children(node, sync, scope, Role::overlay, false);
  } else if (decoration.has_value()) {
    const components::Alignment alignment = decoration->get().alignment();
    style.flex_direction = YGFlexDirectionColumn;
    style.justify = justify(alignment.vertical);
    style.align_items = align(alignment.horizontal);
    Scope based = scope;
    based.hoisted_width = frame.width.has_value() && frame.width->is_fraction();
    based.hoisted_height =
        frame.height.has_value() && frame.height->is_fraction();
    if (const auto insets = provided(node)) {
      based.insets = plus(scope.insets, *insets);
      based.safe = plus(scope.safe, *insets);
    }
    begin_decorated(sync, based, decoration->get().base());
  } else if (const auto layer =
                 node.content_as<composition::Layer::Content>()) {
    const components::Alignment alignment = layer->get().alignment();
    style.flex_direction = YGFlexDirectionColumn;
    style.justify = justify(alignment.vertical);
    style.align_items = align(alignment.horizontal);
    Scope layered_scope = scope;
    layered_scope.safe =
        layer->get().layer() == composition::OverlayLayer::toast
            ? scope.insets
            : theme::Edges{};
    begin_children(node, sync, layered_scope, Role::column_item, true);
  } else if (const auto scroll =
                 node.content_as<components::ScrollView::Content>()) {
    const composition::Axes axes = scroll->get().options().axes;
    const bool free = axes == composition::Axes::all;
    const bool sideways = axes == composition::Axes::horizontal || free;
    style.flex_direction =
        sideways ? YGFlexDirectionRow : YGFlexDirectionColumn;
    style.overflow = YGOverflowScroll;
    style.justify = YGJustifyFlexStart;
    style.align_items = sideways ? YGAlignFlexStart : YGAlignCenter;
    const theme::Edges margined =
        node.interactions().scrolling.content_margins.has_value()
            ? theme.edges(*node.interactions().scrolling.content_margins)
                  .positive()
            : theme::Edges{};
    style.padding = plus(style.padding, margined);
    const bool downward = axes.contains(composition::Axis::vertical);
    const theme::Edges consumed{.top = downward ? scope.safe.top : 0.0F,
                                .leading = scope.safe.leading,
                                .bottom = downward ? scope.safe.bottom : 0.0F,
                                .trailing = scope.safe.trailing};
    style.padding = plus(style.padding, consumed);
    style.margin =
        plus(style.margin,
             theme::Edges{.top = downward ? 0.0F : scope.safe.top,
                          .bottom = downward ? 0.0F : scope.safe.bottom});
    tree.assign_safe_area(node.id(), consumed);
    tree.assign_content_insets(node.id(), plus(margined, consumed));
    if (role == Role::column_item || role == Role::row_item) {
      const bool along = free || !sideways || role == Role::row_item;
      const bool sized = role == Role::column_item
                             ? modifiers.frame.height.has_value()
                             : modifiers.frame.width.has_value();
      if (along && !sized && !modifiers.grow.has_value()) {
        style.grow = 1.0F;
        style.shrink = 1.0F;
      }
      if (style.align_self == YGAlignAuto) {
        style.align_self = YGAlignStretch;
      }
    }
    const Role content = sideways ? Role::row_item : Role::column_item;
    begin_children(node, sync, scope, free ? Role::overlay : content, false);
  } else if (const auto laid =
                 node.content_as<components::AnyLayout::Content>()) {
    arranged = laid->get().shared_box();
    const std::optional<composition::Axis> orientation =
        arranged->properties().stack_orientation;
    Scope members_scope = scope;
    members_scope.unoriented = !orientation.has_value();
    begin_members(node, sync, members_scope,
                  orientation == composition::Axis::horizontal
                      ? Role::row_item
                      : Role::column_item);
  } else if (const auto table = node.content_as<components::Grid::Content>()) {
    const components::GridOptions& options = table->get().options();
    const float stack = theme.scaled(theme::metric::stack_spacing);
    gridded = GridArrangement::Table{
        .alignment = options.alignment,
        .placement = frame.alignment,
        .column_spacing =
            options.horizontal_spacing.has_value()
                ? std::max(theme.space(*options.horizontal_spacing), 0.0F)
                : stack,
        .row_spacing = options.vertical_spacing.has_value()
                           ? std::optional<float>{std::max(
                                 theme.space(*options.vertical_spacing), 0.0F)}
                           : std::nullopt,
        .spacing = stack,
        .text_spacing = theme::metrics::stack.text_spacing};
    begin_table(node, sync, scope);
  } else if (const auto flow = node.content_as<components::LazyGrid::Flow>()) {
    const components::LazyGrid::Arrangement& arrangement =
        flow->get().arrangement();
    const bool down = arrangement.axis == composition::Axis::vertical;
    gridded = GridArrangement::Flow{
        .items = arrangement.items,
        .spacings = components::GridTracks::spacings(arrangement.items, theme),
        .alignment = arrangement.alignment,
        .spacing = components::LazyGrid::spacing(arrangement, theme),
        .leading = flow->get().leading(),
        .trailing = flow->get().trailing(),
        .vertical = down};
    begin_cells(node, sync, scope, down);
  } else {
    const bool across = inner == Role::row_item;
    style.flex_direction = across ? YGFlexDirectionRow : YGFlexDirectionColumn;
    style.justify = across ? justify(frame.alignment.horizontal)
                           : justify(frame.alignment.vertical);
    style.align_items = across ? align(frame.alignment.vertical)
                               : align(frame.alignment.horizontal);
    begin_children(node, sync, scope, inner, false);
  }
  tail.style = std::move(style);
  tail.frame = frame;
  tail.portals = portals;
  tail.bars = bars;
  tail.shrink_weight = parent.shrink_weight;
  tail.grow_blocked = parent.grow_blocked;
  tail.decorated = decoration.has_value();
  tail.button = button.has_value();
  tail.framed = framed;
  tail.open_width = open_width;
  tail.open_height = open_height;
  tail.scope = std::move(scope);
  return step;
}

void YogaLayout::close_sync(composition::Tree& tree,
                            const composition::Node& node, Sync& sync,
                            Sync* above) {
  if (sync.tailed) {
    if (!failure_.has_value()) {
      finish_sync(tree, node, sync);
    }
    tails_.pop_back();
  }
  if (failure_.has_value()) {
    return;
  }
  deliver(tree, node, sync.result, above);
}

void YogaLayout::finish_sync(composition::Tree& tree,
                             const composition::Node& node, Sync& sync) {
  Tail& tail = tails_.back();
  const Scope& scope = *tail.scope;
  if (tail.spacing.active) {
    const Spacing& spacing = tail.spacing;
    if (auto spaced =
            space_children(tree, node, scope, spacing.vertical,
                           spacing.automatic, spacing.guided, spacing.overlap);
        !spaced.has_value()) {
      failure_ = spaced.error();
      return;
    }
  }
  YogaSlot& target = *sync.target;
  const Role role = sync.role;
  const composition::Modifiers& modifiers = node.modifiers();
  YogaSlot::Style& style = tail.style;
  const composition::FrameSpec& frame = tail.frame;
  const Range children = tail.children;
  const bool open_width = tail.open_width;
  const bool open_height = tail.open_height;
  const bool framed = tail.framed;
  std::optional<GridArrangement::Layout>& gridded = tail.gridded;
  std::shared_ptr<const components::AnyLayout::Box>& arranged = tail.arranged;
  const Flow flow = flow_of(node);
  if (flow == Flow::own) {
    style.margin = plus(style.margin, scope.safe);
  } else if (flow == Flow::row) {
    style.padding =
        plus(style.padding,
             theme::Edges{.top = scope.safe.top, .bottom = scope.safe.bottom});
  }
  if (modifiers.grow.has_value()) {
    style.grow = std::max(*modifiers.grow, 0.0F);
  }
  if (modifiers.shrink.has_value()) {
    style.shrink = std::max(*modifiers.shrink, 0.0F);
  }
  if (tail.grow_blocked) {
    style.grow = 0.0F;
  }
  style.shrink *= tail.shrink_weight;
  if (role == Role::row_item || role == Role::column_item) {
    const composition::Axis along = role == Role::row_item
                                        ? composition::Axis::horizontal
                                        : composition::Axis::vertical;
    const composition::Axis across = role == Role::row_item
                                         ? composition::Axis::vertical
                                         : composition::Axis::horizontal;
    if (fixes(node, along)) {
      style.grow = 0.0F;
      style.shrink = 0.0F;
    }
    if (fixes(node, across) && style.align_self == YGAlignStretch) {
      style.align_self = YGAlignAuto;
    }
  }
  std::optional<YogaSlot::Style> contents;
  const std::optional<composition::AspectRatio>& aspect = modifiers.aspect;
  const bool fitting = aspect.has_value() && !tail.decorated && !tail.button &&
                       !gridded.has_value() && !lazy_root(tree, node);
  const Fit fit =
      fitting ? fit_of(frame, *aspect, open_width, open_height) : Fit::box;
  const bool boxed = fitting && fit != Fit::ideal;
  if (boxed) {
    contents = interior(style, node);
    style = exterior(std::move(style), frame.alignment);
    if (fit == Fit::ratio) {
      style.aspect_ratio = aspect->ratio;
      style.within_parent = true;
      style.shrink = 1.0F;
      style.grow = 1.0F;
      if (role == Role::column_item || role == Role::row_item) {
        style.align_self = YGAlignStretch;
      }
    } else if (fit == Fit::open_width || fit == Fit::open_height) {
      style = unbounded(std::move(style), *aspect->ratio, role, fit);
    }
  } else if (fitting) {
    style.grow = 0.0F;
    style.shrink = 0.0F;
    if (style.align_self == YGAlignStretch) {
      style.align_self = YGAlignAuto;
    }
  } else if (!framed && !tail.decorated && !tail.button &&
             !gridded.has_value() && aligned_kind(node) && sized(frame) &&
             !lazy_root(tree, node)) {
    contents = interior(style, node);
    contents->within_parent = ranks(node) || shares(node);
    style = exterior(std::move(style), frame.alignment);
  }
  target.apply(style);
  target.align(contents);
  target.aspect(boxed ? aspect : std::nullopt);
  target.measure(tail.measured, *this, node.cost());
  if (gridded.has_value()) {
    const YGDirection flowing = direction(scope.direction);
    if (target.grid(flowing).update(std::move(*gridded), tail.grid_cells,
                                    tail.grid_rows, flowing)) {
      YGNodeMarkDirty(target.node());
    }
  } else if (arranged != nullptr) {
    const YGDirection flowing = direction(scope.direction);
    if (target.custom(flowing).update(
            std::move(arranged), tail.members, flowing,
            [this, &tree](const composition::Node& member) {
              return baselines_of(tree, member);
            })) {
      YGNodeMarkDirty(target.node());
    }
  } else if (const bool along = style.flex_direction == YGFlexDirectionColumn;
             (node.content_as<components::VStack::Content>().has_value() ||
              node.content_as<components::HStack::Content>().has_value()) &&
             lazy_root(tree, node)) {
    target.bound(
        produced(children),
        YogaSlot::Bound{.vertical = along,
                        .align = style.align_items,
                        .gap = along ? style.gap_row : style.gap_column,
                        .direction = direction(scope.direction)});
  } else {
    target.set_children(produced(children));
  }
  refs_.resize(children.begin);
  target.remember(*tail.visit,
                  portals_.size() > tail.portals || bars_.size() > tail.bars);
  sync.result = target.node();
}

auto YogaLayout::inherited_frame(const composition::Node& node)
    -> composition::FrameSpec {
  composition::FrameSpec frame = node.modifiers().frame;
  for (const composition::Node& child : node.children()) {
    if (departing(child) || layered(child)) {
      continue;
    }
    const auto decorated = child.content_as<composition::Decoration>();
    const composition::FrameSpec inner =
        transparent(child) ? inherited_frame(child)
        : decorated.has_value()
            ? decorated_frame(child, decorated->get().base())
            : child.modifiers().frame;
    const auto fills = [](const std::optional<composition::Length>& limit) {
      return limit.has_value() && std::isinf(limit->value());
    };
    if (!frame.width.has_value() && !frame.max_width.has_value() &&
        fills(inner.max_width)) {
      frame.max_width = composition::infinity;
    }
    if (!frame.height.has_value() && !frame.max_height.has_value() &&
        fills(inner.max_height)) {
      frame.max_height = composition::infinity;
    }
    break;
  }
  return frame;
}

auto YogaLayout::stacked(const composition::Node& node)
    -> std::optional<std::reference_wrapper<const composition::Node>> {
  if (node.content_as<components::VStack::Content>().has_value() ||
      node.content_as<components::HStack::Content>().has_value() ||
      node.content_as<components::ZStack::Content>().has_value()) {
    return std::cref(node);
  }
  const auto decoration = node.content_as<composition::Decoration>();
  if (!decoration.has_value()) {
    if (!node.content().composes() || node.content().passes_through()) {
      return std::nullopt;
    }
    std::optional<std::reference_wrapper<const composition::Node>> only;
    for (const composition::Node& child : node.children()) {
      if (departing(child)) {
        continue;
      }
      if (only.has_value()) {
        return std::nullopt;
      }
      only = std::cref(child);
    }
    return only.has_value() ? stacked(only->get()) : std::nullopt;
  }
  std::size_t index = 0;
  for (const composition::Node& child : node.children()) {
    if (departing(child)) {
      continue;
    }
    if (index++ == decoration->get().base()) {
      return stacked(child);
    }
  }
  return std::nullopt;
}

auto YogaLayout::compressible(const composition::Node& node, bool horizontal)
    -> bool {
  const composition::Axis axis =
      horizontal ? composition::Axis::horizontal : composition::Axis::vertical;
  const composition::FrameSpec& frame = node.modifiers().frame;
  if (fixes(node, axis) || (horizontal ? frame.width : frame.height)) {
    return false;
  }
  if (const auto scroll = node.content_as<components::ScrollView::Content>()) {
    return scroll->get().options().axes.contains(axis);
  }
  return std::ranges::any_of(node.children(),
                             [horizontal](const composition::Node& child) {
                               return !departing(child) && !layered(child) &&
                                      compressible(child, horizontal);
                             });
}

auto YogaLayout::flexed(const composition::Node& node,
                        const composition::FrameSpec& frame, Role role,
                        bool open_width, bool open_height,
                        YogaSlot::Style style) -> YogaSlot::Style {
  const auto subject = stacked(node);
  const auto lazy = node.content_as<components::LazyStack::Content>();
  if ((!subject.has_value() && !lazy.has_value()) ||
      (role != Role::row_item && role != Role::column_item)) {
    return style;
  }
  const bool across =
      !open_width && !frame.width.has_value() &&
      !frame.max_width.has_value() &&
      !fixes(node, composition::Axis::horizontal) &&
      (subject.has_value()
           ? flexible_child(subject->get(), true)
           : lazy->get().arrangement().axis == composition::Axis::vertical);
  const bool down = subject.has_value() && !open_height &&
                    !frame.height.has_value() &&
                    !frame.max_height.has_value() &&
                    !fixes(node, composition::Axis::vertical) &&
                    flexible_child(subject->get(), false);
  const bool row = role == Role::row_item;
  if (row ? across : down) {
    style.grow = 1.0F;
    if (compressible(subject.has_value() ? subject->get() : node, row)) {
      style.shrink = 1.0F;
    }
  }
  if ((row ? down : across) && style.align_self == YGAlignAuto) {
    style.align_self = YGAlignStretch;
  }
  return style;
}

auto YogaLayout::flexible_child(const composition::Node& node, bool horizontal)
    -> bool {
  const bool row = node.content_as<components::HStack::Content>().has_value();
  const bool column =
      node.content_as<components::VStack::Content>().has_value();
  return std::ranges::any_of(
      node.children(),
      [horizontal, row, column](const composition::Node& child) {
        if (departing(child) || layered(child)) {
          return false;
        }
        if (child.content_as<components::Spacer::Content>().has_value()) {
          return (row && horizontal) || (column && !horizontal);
        }
        if (child.content_as<components::Divider::Content>().has_value()) {
          return (row && !horizontal) || (column && horizontal);
        }
        return flexible(child, horizontal);
      });
}

auto YogaLayout::flexible(const composition::Node& node, bool horizontal)
    -> bool {
  const composition::Axis axis =
      horizontal ? composition::Axis::horizontal : composition::Axis::vertical;
  if (fixes(node, axis)) {
    return false;
  }
  if (const auto& relative = node.modifiers().relative_frame;
      relative.has_value() && relative->axes().contains(axis)) {
    return false;
  }
  const composition::FrameSpec& frame = node.modifiers().frame;
  if (const auto& fixed = horizontal ? frame.width : frame.height) {
    return fixed->is_fraction();
  }
  if (fills(horizontal ? frame.max_width : frame.max_height) ||
      node.modifiers().aspect.has_value() ||
      node.content_as<components::ScrollView::Content>().has_value() ||
      proposes(node)) {
    return true;
  }
  if (node.content_as<components::Button::Content>().has_value()) {
    return node.child_count() > 0 && flexible_child(node, horizontal);
  }
  if (const auto decoration = node.content_as<composition::Decoration>()) {
    std::size_t index = 0;
    for (const composition::Node& child : node.children()) {
      if (departing(child)) {
        continue;
      }
      if (index++ == decoration->get().base()) {
        return !layered(child) && flexible(child, horizontal);
      }
    }
    return false;
  }
  if (node.content_as<components::Spacer::Content>().has_value() ||
      node.content_as<components::Divider::Content>().has_value() ||
      node.content_as<components::Text::Content>().has_value() ||
      node.content_as<components::Image::Raster::Content>().has_value() ||
      custom(node)) {
    return false;
  }
  if (node.content().framed()) {
    const composition::FrameSpec inherited = inherited_frame(node);
    return fills(horizontal ? inherited.max_width : inherited.max_height);
  }
  if (node.content_as<components::Grid::Content>().has_value() ||
      node.content_as<components::LazyGrid::Flow>().has_value()) {
    return content_stretches(node, horizontal);
  }
  return flexible_child(node, horizontal);
}

auto YogaLayout::absolute(YGNodeRef node) -> composition::Rect {
  composition::Rect frame{.x = 0.0F,
                          .y = 0.0F,
                          .width = YGNodeLayoutGetWidth(node),
                          .height = YGNodeLayoutGetHeight(node)};
  YGNodeRef current = node;
  while (current != nullptr) {
    frame.x += YGNodeLayoutGetLeft(current);
    frame.y += YGNodeLayoutGetTop(current);
    if (const YGNodeRef parent = YGNodeGetParent(current)) {
      current = parent;
      continue;
    }
    current = YogaSlot::anchor(current);
    if (current != nullptr) {
      frame.x += YGNodeLayoutGetPadding(current, YGEdgeLeft) +
                 YGNodeLayoutGetBorder(current, YGEdgeLeft);
      frame.y += YGNodeLayoutGetPadding(current, YGEdgeTop) +
                 YGNodeLayoutGetBorder(current, YGEdgeTop);
    }
  }
  return frame;
}

void YogaLayout::publish(composition::Tree& tree, const composition::Node& node,
                         YogaSlot& slot, composition::Rect frame) {
  std::optional<YogaSlot::Published>& published = slot.published();
  const std::optional<YogaSlot::Publication>& publication = slot.publication();
  const auto measured = slot.text();
  if (!publication.has_value() || !measured.has_value() ||
      !typesetter_.has_value()) {
    if (published.has_value()) {
      published.reset();
      tree.assign_published(node.id(), std::nullopt);
    }
    return;
  }
  const YogaSlot::Measured& held = measured->get();
  const composition::Rect inside = absolute(slot.content());
  const composition::Rect content{.x = inside.x - frame.x,
                                  .y = inside.y - frame.y,
                                  .width = inside.width,
                                  .height = inside.height};
  text::Typesetter& typesetter = typesetter_->get();
  std::string shown = held.text;
  text::TextStyle style = held.style;
  const auto lay = [&]() -> std::shared_ptr<const text::TextLayout> {
    const text::TextSize natural =
        typesetter
            .measure(shown, style, text::LayoutWidth::unbounded(), held.scale)
            .value_or(text::TextSize{});
    text::LayoutWidth width = text::LayoutWidth::unbounded();
    if (held.wraps && content.width + 0.01F < natural.width) {
      width = text::LayoutWidth::points(std::max(content.width, 0.0F))
                  .value_or(width);
    }
    auto made = typesetter.layout(shown, style, width, held.scale);
    return made.has_value() ? std::move(*made) : nullptr;
  };
  std::shared_ptr<const text::TextLayout> laid = lay();
  if (laid == nullptr) {
    return;
  }
  bool truncated = false;
  if (held.fitting.lines.has_value() ||
      held.fitting.minimum_scale_factor < 1.0F ||
      laid->size().height > content.height + 0.5F) {
    text::TextFitting wanted = held.fitting;
    if (!wanted.lines.has_value()) {
      wanted.height = std::max(content.height, 0.0F) + 0.5F;
    }
    const text::LayoutWidth width =
        text::LayoutWidth::points(std::max(content.width, 0.0F))
            .value_or(text::LayoutWidth::unbounded());
    if (auto fit = typesetter.fitted(shown, style, width, held.scale, wanted);
        fit.has_value() && *fit != nullptr &&
        ((*fit)->text != shown || (*fit)->style != style)) {
      truncated = (*fit)->text != shown;
      shown = (*fit)->text;
      style = (*fit)->style;
      if (std::shared_ptr<const text::TextLayout> refit = lay()) {
        laid = std::move(refit);
      }
    }
  }
  if (published.has_value() && published->layout == laid &&
      published->content == content && published->truncated == truncated &&
      published->publication == publication) {
    return;
  }
  published = YogaSlot::Published{.layout = laid,
                                  .content = content,
                                  .truncated = truncated,
                                  .publication = publication};
  const components::Text::Layout layout{
      std::make_shared<const components::Text::Layout::Source>(
          components::Text::Layout::Source{
              .layout = laid,
              .text = std::move(shown),
              .attributed = *publication->text,
              .custom = publication->custom,
              .foreground = publication->foreground,
              .truncated = truncated})};
  tree.assign_published(
      node.id(),
      composition::Preference::anchored(
          components::Text::LayoutKey::key,
          composition::AnchorSource<composition::Point>::point(
              composition::Point{.x = content.x, .y = content.y}),
          [layout](const composition::Anchor<composition::Point>& origin) {
            return components::Text::LayoutKey::Value{
                components::Text::LayoutKey::AnchoredLayout{.origin = origin,
                                                            .layout = layout}};
          }));
}

auto YogaLayout::read_back(composition::Tree& tree,
                           const composition::Node& node)
    -> std::optional<composition::Rect> {
  read_.reset();
  Reader reader{*this, tree};
  read_walk_.run(node, Reading{}, reader);
  return std::exchange(read_, std::nullopt);
}

auto YogaLayout::Reader::enter(const composition::Node& node, const Reading&)
    -> ReadWalk::Step {
  return layout_.get().open_read(tree_, node);
}

auto YogaLayout::Reader::next(const composition::Node& node, Reading& reading)
    -> const composition::Node* {
  const auto children = node.children();
  while (reading.position < std::ranges::size(children)) {
    const composition::Node& child = children[reading.position++];
    if (!departing(child) && !layered(child)) {
      return &child;
    }
  }
  return nullptr;
}

void YogaLayout::Reader::leave(const composition::Node& node, Reading& reading,
                               Reading* above) {
  layout_.get().close_read(tree_, node, reading, above);
}

auto YogaLayout::open_read(composition::Tree& tree,
                           const composition::Node& node) -> ReadWalk::Step {
  Reading reading;
  std::optional<std::reference_wrapper<composition::LayoutSlot>> held;
  if (!transparent(node)) {
    held = tree.layout_slot(node);
  }
  std::optional<composition::Rect>& placed = reading.placed;
  if (held.has_value()) {
    auto& found = static_cast<YogaSlot&>(held->get());
    reading.held = &found;
    const std::optional<YGNodeRef> laid = found.node();
    if (found.fit()) {
      again_ = true;
    }
    if (settle_position(node, *laid)) {
      again_ = true;
    }
    if (settle_guides(tree, node)) {
      again_ = true;
    }
    if (settle_layered(tree, node, found)) {
      again_ = true;
    }
    if (settle_table_guides(tree, node)) {
      again_ = true;
    }
    if (settle_shares(tree, node, found)) {
      again_ = true;
    }
    if (settle_separator(tree, node)) {
      again_ = true;
    }
    if (settle_row(tree, node, *laid)) {
      again_ = true;
    }
    if (found.relative_spacing().has_value() &&
        settle_relative(tree, node, *laid)) {
      again_ = true;
    }
    if (node.modifiers().corner_offset.has_value() &&
        settle_corner(tree, node, found)) {
      again_ = true;
    }
    placed = absolute(found.fits() ? found.content() : *laid);
    if (!force_ && !YGNodeGetHasNewLayout(*laid) && *placed == node.frame()) {
      reading.result = placed;
      reading.finished = true;
      return ReadWalk::Step{.state = std::move(reading), .descend = false};
    }
    if (found.bounds() && found.settle()) {
      again_ = true;
    }
    withhold(tree, found);
  }
  ++placements_;
  return ReadWalk::Step{.state = std::move(reading)};
}

void YogaLayout::close_read(composition::Tree& tree,
                            const composition::Node& node, Reading& reading,
                            Reading* above) {
  if (!reading.finished) {
    finish_read(tree, node, reading);
  }
  const std::optional<composition::Rect>& frame = reading.result;
  if (above == nullptr) {
    read_ = frame;
    return;
  }
  if (!frame.has_value()) {
    return;
  }
  std::optional<composition::Rect>& bounds = above->bounds;
  if (!bounds.has_value()) {
    bounds = frame;
    return;
  }
  const float left = std::min(bounds->x, frame->x);
  const float top = std::min(bounds->y, frame->y);
  const float right =
      std::max(bounds->x + bounds->width, frame->x + frame->width);
  const float bottom =
      std::max(bounds->y + bounds->height, frame->y + frame->height);
  bounds = composition::Rect{
      .x = left, .y = top, .width = right - left, .height = bottom - top};
}

void YogaLayout::finish_read(composition::Tree& tree,
                             const composition::Node& node, Reading& reading) {
  const std::optional<composition::Rect>& bounds = reading.bounds;
  if (reading.held != nullptr) {
    YogaSlot& found = *reading.held;
    const std::optional<YGNodeRef> laid = found.node();
    const composition::Rect frame = *reading.placed;
    tree.assign_frame(node.id(), frame);
    std::optional<composition::Rect> aligned;
    if (found.aligns() &&
        node.content_as<components::Text::Content>().has_value()) {
      const composition::Rect inside = absolute(found.content());
      aligned = composition::Rect{.x = inside.x - frame.x,
                                  .y = inside.y - frame.y,
                                  .width = inside.width,
                                  .height = inside.height};
    }
    tree.assign_content_frame(node.id(), aligned);
    publish(tree, node, found, frame);
    if (node.interactions().scrolling.container.has_value()) {
      const float right =
          bounds.has_value() ? bounds->x + bounds->width : frame.x;
      const float bottom =
          bounds.has_value() ? bounds->y + bounds->height : frame.y;
      tree.assign_content_size(
          node.id(),
          composition::Size{
              .width = std::max(
                  right - frame.x + YGNodeLayoutGetPadding(*laid, YGEdgeRight),
                  0.0F),
              .height =
                  std::max(bottom - frame.y +
                               YGNodeLayoutGetPadding(*laid, YGEdgeBottom),
                           0.0F)});
    }
    YGNodeSetHasNewLayout(*laid, false);
    reading.result = frame;
    return;
  }
  tree.assign_frame(node.id(), bounds.value_or(composition::Rect{}));
  reading.result = bounds;
}

auto YogaLayout::run(composition::Tree& tree, layout::Viewport viewport,
                     TypesetterRef typesetter, bool force)
    -> std::expected<bool, layout::LayoutError> {
  const std::uint64_t computed = computed_.load(std::memory_order_relaxed);
  auto ran = pass(tree, viewport, typesetter, force);
  computations_ += computed_.load(std::memory_order_relaxed) - computed;
  return ran;
}

auto YogaLayout::pass(composition::Tree& tree, layout::Viewport viewport,
                      TypesetterRef typesetter, bool force)
    -> std::expected<bool, layout::LayoutError> {
  const text::Typesetter* shaper =
      typesetter.has_value() ? &typesetter->get() : nullptr;
  force_ = force || !viewport_.has_value() || *viewport_ != viewport ||
           shaper_ != shaper;
  shaper_ = shaper;
  const support::ScopedCallback deferred = tree.defer_geometry();
  tree.seal_paint();
  typesetter_ = typesetter;
  scale_ = viewport.scale;
  viewport_ = viewport;
  portals_.clear();
  entries_.clear();
  refs_.clear();
  sized_.clear();
  const composition::Node& root = tree.root();
  const Scope scope = scoped(
      root,
      Scope{.theme = components::Environment::theme.fallback(),
            .locale = components::Environment::locale.fallback(),
            .direction = components::Environment::layout_direction.fallback()});
  auto found = slot(tree, root);
  if (!found.has_value()) {
    typesetter_.reset();
    return std::unexpected(found.error());
  }
  bars_.clear();
  auto children = sync_children(tree, root, scope, Role::column_item, false);
  if (!children.has_value()) {
    typesetter_.reset();
    return std::unexpected(children.error());
  }
  YogaSlot& target = found->get();
  YogaSlot::Style style;
  style.direction = direction(scope.direction);
  style.flex_direction = YGFlexDirectionColumn;
  style.justify = YGJustifyCenter;
  style.align_items = YGAlignCenter;
  style.width = YogaSlot::Length{.unit = YogaSlot::Length::Unit::points,
                                 .value = viewport.width()};
  style.height = YogaSlot::Length{.unit = YogaSlot::Length::Unit::points,
                                  .value = viewport.height()};
  target.apply(style);
  target.measure(std::nullopt, *this, root.cost());
  target.set_children(produced(*children));
  refs_.clear();
  const YGNodeRef node = target.node();
  const bool calculate = force || YGNodeIsDirty(node);
  if (calculate) {
    YGNodeCalculateLayout(node, YGUndefined, YGUndefined,
                          direction(scope.direction));
    ++calculations_;
    if (settle_bars(tree)) {
      portals_.clear();
      children = sync_children(tree, root, scope, Role::column_item, false);
      if (!children.has_value()) {
        typesetter_.reset();
        return std::unexpected(children.error());
      }
      target.set_children(produced(*children));
      refs_.clear();
      bars_.clear();
      YGNodeCalculateLayout(node, YGUndefined, YGUndefined,
                            direction(scope.direction));
      ++calculations_;
    }
    place(tree);
    tree.assign_frame(root.id(), absolute(node));
    for (std::uint32_t round = 0; round <= relayout_rounds; ++round) {
      if (round > 0) {
        if (!std::exchange(again_, false)) {
          break;
        }
        YGNodeCalculateLayout(node, YGUndefined, YGUndefined,
                              direction(scope.direction));
        ++calculations_;
      }
      for (const composition::Node& child : root.children()) {
        if (!departing(child) && !layered(child)) {
          static_cast<void>(read_back(tree, child));
        }
      }
    }
    again_ = false;
  }
  auto portals = sync_portals(tree, force || calculate);
  if (std::exchange(placing_, false)) {
    tree.seal_paint();
  }
  typesetter_.reset();
  portals_.clear();
  if (!portals.has_value()) {
    return std::unexpected(portals.error());
  }
  return calculate || *portals;
}

}
