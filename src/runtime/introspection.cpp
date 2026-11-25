#include <arisa/composition/layer.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/runtime/introspection.hpp>
#include <arisa/runtime/root.hpp>

#include <interaction/input_router.hpp>
#include <runtime/painter_renderer.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

namespace arisa::runtime {

Introspection::Introspection(const Root& root,
                             const interaction::InputRouter& router,
                             const PainterRenderer& renderer,
                             provider::GpuProvider& gpu) noexcept
    : root_(root), router_(router), renderer_(renderer), gpu_(gpu) {}

auto Introspection::tree() const -> const composition::Tree& {
  return root_.get().tree();
}

auto Introspection::shown(const composition::Node& node) const -> bool {
  if (!node.parent().has_value()) {
    return true;
  }
  const std::optional<composition::NodeId> debug = tree().debug_layer();
  return node.phase() == composition::NodeLifecyclePhase::live &&
         (!debug.has_value() || node.id() != *debug);
}

auto Introspection::walk() const
    -> std::vector<std::reference_wrapper<const composition::Node>> {
  std::vector<std::reference_wrapper<const composition::Node>> found;
  found.reserve(tree().size());
  std::vector<std::reference_wrapper<const composition::Node>> pending{
      std::cref(tree().root())};
  while (!pending.empty()) {
    const composition::Node& node = pending.back();
    pending.pop_back();
    found.emplace_back(node);
    for (const composition::Node& child :
         node.children() | std::views::reverse) {
      if (shown(child)) {
        pending.emplace_back(child);
      }
    }
  }
  return found;
}

auto Introspection::bounds_of(const composition::Node& node)
    -> composition::Rect {
  const std::optional<composition::PresentedFrame>& presented =
      node.presented();
  return presented.has_value() ? presented->bounds : node.frame();
}

auto Introspection::scoping(const composition::Node& node) -> bool {
  const composition::Interactions& declared = node.interactions();
  if (declared.focus_scope.has_value() || declared.focus_section) {
    return true;
  }
  const auto layer = node.content_as<composition::Layer::Content>();
  return layer.has_value() &&
         layer->get().input() == composition::OverlayInput::modal;
}

auto Introspection::readable(std::string_view spelled) -> std::string {
  constexpr std::string_view component = "ComponentContent<";
  if (const std::size_t at = spelled.find(component);
      at != std::string_view::npos) {
    const std::size_t from = at + component.size();
    const std::size_t to = spelled.rfind('>');
    spelled = spelled.substr(from, to == std::string_view::npos || to < from
                                       ? std::string_view::npos
                                       : to - from);
  }
  if (const std::size_t open = spelled.find('<');
      open != std::string_view::npos) {
    spelled = spelled.substr(0, open);
  }
  constexpr std::string_view separator = "::";
  std::size_t last = spelled.rfind(separator);
  std::string_view tail = last == std::string_view::npos
                              ? spelled
                              : spelled.substr(last + separator.size());
  if (tail == "Content" && last != std::string_view::npos) {
    const std::string_view head = spelled.substr(0, last);
    const std::size_t before = head.rfind(separator);
    tail = before == std::string_view::npos
               ? head
               : head.substr(before + separator.size());
  }
  return std::string{tail};
}

auto Introspection::record(const composition::Node& node) const -> NodeRecord {
  const composition::Modifiers& modifiers = node.modifiers();
  const composition::Interactions& declared = node.interactions();
  return NodeRecord{
      .id = node.id(),
      .parent = node.parent(),
      .depth = node.depth(),
      .key = node.key().has_value() ? node.key()->description() : std::string{},
      .name = node.parent().has_value() ? readable(node.type().name())
                                        : std::string{"Root"},
      .frame = node.frame(),
      .bounds = bounds_of(node),
      .padding = modifiers.padding,
      .flags =
          Flags{.focusable =
                    declared.focusable.value_or(node.content().activatable()),
                .clips =
                    modifiers.clip || declared.scrolling.container.has_value(),
                .glass = modifiers.glass.has_value(),
                .promoted = renderer_.get().painter().promoted(node.id()),
                .layer =
                    node.content_as<composition::Layer::Content>().has_value(),
                .animating = node.presentation().animating()},
      .cost = node.cost().counts(),
      .children = node.child_count(),
      .notes = node.content().notes()};
}

auto Introspection::nodes() const -> std::vector<NodeRecord> {
  const auto walked = walk();
  std::vector<NodeRecord> found;
  found.reserve(walked.size());
  for (const composition::Node& node : walked) {
    found.push_back(record(node));
  }
  return found;
}

auto Introspection::node(composition::NodeId id) const
    -> std::optional<NodeRecord> {
  const auto found = tree().node(id);
  if (!found.has_value() || !shown(found->get())) {
    return std::nullopt;
  }
  return record(found->get());
}

auto Introspection::accessibility(composition::NodeId id) const
    -> std::optional<components::AccessibilityElement> {
  return components::AccessibilityTree{tree()}.element(id);
}

auto Introspection::costs() const -> std::vector<CostRecord> {
  const auto walked = walk();
  std::vector<CostRecord> found;
  found.reserve(walked.size());
  for (const composition::Node& node : walked) {
    found.push_back(CostRecord{.id = node.id(), .cost = node.cost().counts()});
  }
  return found;
}

auto Introspection::deepest(const composition::Node& node,
                            composition::Point point) const
    -> std::optional<composition::NodeId> {
  for (const composition::Node& child : node.children() | std::views::reverse) {
    if (!shown(child)) {
      continue;
    }
    if (const auto inner = deepest(child, point)) {
      return inner;
    }
  }
  if (!node.parent().has_value()) {
    return std::nullopt;
  }
  const std::optional<composition::PresentedFrame>& presented =
      node.presented();
  const bool inside = presented.has_value()
                          ? presented->hits(point)
                          : node.frame().contains(point.x, point.y);
  if (!inside || (presented.has_value() && !presented->visible)) {
    return std::nullopt;
  }
  return node.id();
}

auto Introspection::hit(composition::Point point) const
    -> std::optional<composition::NodeId> {
  return deepest(tree().root(), point);
}

auto Introspection::points(provider::PixelPoint pixel) const
    -> composition::Point {
  const float scale = renderer_.get().scale().value();
  return composition::Point{.x = pixel.x / scale, .y = pixel.y / scale};
}

auto Introspection::viewport() const -> composition::Size {
  const composition::Rect& whole = tree().root().frame();
  return composition::Size{.width = whole.width, .height = whole.height};
}

auto Introspection::focus() const -> FocusRecord {
  FocusRecord found;
  const interaction::InputRouter& router = router_;
  found.focused = router.focused();
  if (!found.focused.has_value()) {
    return found;
  }
  for (std::optional<composition::NodeId> at = found.focused; at.has_value();) {
    const auto node = tree().node(*at);
    if (!node.has_value() || !node->get().parent().has_value()) {
      break;
    }
    found.chain.push_back(*at);
    if (scoping(node->get())) {
      found.scopes.push_back(*at);
    }
    at = node->get().parent();
  }
  std::ranges::reverse(found.chain);
  std::ranges::reverse(found.scopes);
  for (std::size_t index = 0; index < directions.size(); ++index) {
    found.next.at(index) = router.engine().next(tree(), directions.at(index));
  }
  return found;
}

auto Introspection::gpu() const -> GpuRecord {
  provider::GpuProvider& gpu = gpu_;
  GpuRecord found{.inventory = gpu.inventory(),
                  .hooks = {},
                  .external = {},
                  .surfaces = {},
                  .atlas_bytes = 0,
                  .atlas_pages = 0,
                  .targets = renderer_.get().painter().targets()};
  if (const auto schedule = gpu.frame_schedule()) {
    found.hooks = schedule->get().listing();
    std::erase_if(found.hooks,
                  [](const provider::FrameSchedule::Listing& listing) {
                    return listing.surface;
                  });
  }
  if (const auto publications = gpu.publications()) {
    found.external = publications->get().listing();
  }
  if (const auto surfaces = gpu.surfaces()) {
    found.surfaces = surfaces->get().listing();
  }
  const text::GlyphAtlas& atlas = renderer_.get().painter().atlas();
  const std::uint64_t side = atlas.options().page_size;
  found.atlas_pages = atlas.statistics().pages;
  found.atlas_bytes = side * side * found.atlas_pages;
  return found;
}

}
