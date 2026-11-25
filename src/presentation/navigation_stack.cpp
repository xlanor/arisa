#include <arisa/components/button_style.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/theme/metrics.hpp>

#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/safe_area_inset.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/dismiss_action.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/navigation_destination.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/preference_observer.hpp>
#include <arisa/composition/scroll_geometry_observer.hpp>
#include <arisa/composition/text_input_environment.hpp>
#include <arisa/composition/toolbar_environment.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <ranges>
#include <utility>

namespace arisa::presentation {

class NavigationStack::Screen : public composition::Element {
 public:
  Screen(std::vector<composition::Element> children, bool covered,
         std::shared_ptr<composition::ScrollGeometryObserver> collapse,
         std::optional<composition::MatchedGeometry::Key> zoom)
      : Element(std::make_unique<components::ZStack::Content>(
                    components::Alignment::top_leading),
                std::move(children)) {
    animate_transitions(composition::Layer::default_animation());
    declared().covered = covered;
    declared().scrolling.nested_geometry = std::move(collapse);
    if (zoom.has_value()) {
      zoom_from(std::move(*zoom));
    }
  }
};

const composition::EnvironmentKey<NavigationStack::Navigator>
    NavigationStack::navigator{"navigator", Navigator{}};

const composition::EnvironmentKey<NavigationStack::Navigator>
    NavigationStack::detail_navigator{"detail_navigator", Navigator{}};

const composition::StateKey<NavigationStack::History>
    NavigationStack::Content::history{"navigation-history", History{}};

const composition::StateKey<std::vector<NavigationStack::Content::Declared>>
    NavigationStack::Content::resolved{"navigation-resolved", {}};

const composition::StateKey<std::vector<NavigationStack::Content::Declared>>
    NavigationStack::Content::displayed_key{"navigation-shown", {}};

const composition::StateKey<composition::NavigationDeclarations::Destinations>
    NavigationStack::Content::found{"navigation-found", {}};

const composition::StateKey<std::vector<std::uint64_t>>
    NavigationStack::Content::collapsed{"navigation-collapsed", {}};

const composition::StateKey<std::map<std::uint64_t, std::shared_ptr<float>>>
    NavigationStack::Content::title_offsets{"navigation-title-offsets", {}};

const composition::StateKey<std::vector<std::uint64_t>>
    NavigationStack::Content::minimized_pages{"navigation-minimized", {}};

const composition::StateKey<composition::ToolbarCustomization>
    NavigationStack::Content::customized{"navigation-toolbar-customization",
                                         composition::ToolbarCustomization{}};

auto NavigationStack::Content::Placement::layout() const -> ToolbarLayout {
  return ToolbarLayout{toolbar, chrome.role(),
                       ToolbarLayout::Options{.customization = customization,
                                              .vertical = vertical}};
}

auto NavigationStack::Content::minimizes(
    const composition::NavigationChrome& chrome,
    const composition::NavigationDeclarations::Toolbar& toolbar)
    -> composition::ToolbarMinimizationBehavior {
  using Behavior = composition::ToolbarMinimizationBehavior;
  const Behavior declared =
      chrome.bar(composition::ToolbarPlacement::navigation_bar)
          .minimization.value_or(Behavior::automatic);
  if (declared != Behavior::automatic) {
    return declared;
  }
  const bool principal_search =
      std::ranges::any_of(toolbar, [](const auto& item) {
        return item->system() == composition::ToolbarDefaultItemKind::search &&
               item->placement() ==
                   composition::ToolbarItemPlacement::principal;
      });
  return principal_search ? Behavior::on_scroll_down : Behavior::never;
}

NavigationStack::NavigationStack(Root root)
    : NavigationStack(std::shared_ptr<const Path>{}, std::move(root)) {}

NavigationStack::NavigationStack(
    composition::Binding<composition::NavigationPath> path, Root root)
    : NavigationStack(std::make_shared<const ErasedPath>(std::move(path)),
                      std::move(root)) {}

NavigationStack::NavigationStack(std::shared_ptr<const Path> path, Root root)
    : Element(std::make_unique<Content>(std::move(path), std::move(root))) {}

auto NavigationStack::top_chrome(const composition::Node& node)
    -> std::optional<composition::NavigationChrome> {
  for (const composition::Node& child : node.children()) {
    if (child.phase() == composition::NodeLifecyclePhase::departing) {
      continue;
    }
    if (child.content_as<Content>().has_value()) {
      if (child.child_count() == 0) {
        continue;
      }
      const composition::Node& stacked = *child.children().begin();
      const composition::Node* top = nullptr;
      for (const composition::Node& page : stacked.children()) {
        if (page.phase() != composition::NodeLifecyclePhase::departing) {
          top = &page;
        }
      }
      if (top == nullptr) {
        continue;
      }
      return top->preference(composition::NavigationDeclarations::chrome_key);
    }
    if (std::optional<composition::NavigationChrome> found =
            top_chrome(child)) {
      return found;
    }
  }
  return std::nullopt;
}

auto NavigationStack::ErasedPath::entries() const
    -> std::vector<composition::NavigationPath::Entry> {
  return path_.get().entries() |
         std::ranges::to<std::vector<composition::NavigationPath::Entry>>();
}

void NavigationStack::ErasedPath::truncate(std::size_t count) const {
  const std::size_t current = path_.get().count();
  if (count >= current) {
    return;
  }
  path_.update([removed = current - count](composition::NavigationPath& path) {
    path.remove_last(removed);
  });
}

auto NavigationStack::ErasedPath::push(
    const composition::NavigationPath::Entry& entry) const -> bool {
  path_.update(
      [&entry](composition::NavigationPath& path) { path.append(entry); });
  return true;
}

auto NavigationStack::Navigator::contains(
    const composition::NavigationPath::Entry& entry) const -> bool {
  return path_ != nullptr && std::ranges::contains(path_->entries(), entry);
}

auto NavigationStack::Navigator::remove(
    const composition::NavigationPath::Entry& entry) const -> bool {
  if (path_ == nullptr) {
    return false;
  }
  const std::vector<composition::NavigationPath::Entry> entries =
      path_->entries();
  const auto found = std::ranges::find(entries, entry);
  if (found == entries.end()) {
    return false;
  }
  path_->truncate(static_cast<std::size_t>(found - entries.begin()));
  return true;
}

auto NavigationStack::Navigator::contains(composition::Identity view) const
    -> bool {
  if (path_ == nullptr) {
    return false;
  }
  return std::ranges::any_of(
      path_->entries(),
      [view](const composition::NavigationPath::Entry& entry) {
        const ViewDestination* shown = entry.get<ViewDestination>();
        return shown != nullptr && shown->identity() == view;
      });
}

auto NavigationStack::Navigator::remove(composition::Identity view) const
    -> bool {
  if (path_ == nullptr) {
    return false;
  }
  const std::vector<composition::NavigationPath::Entry> entries =
      path_->entries();
  const auto found = std::ranges::find_if(
      entries, [view](const composition::NavigationPath::Entry& entry) {
        const ViewDestination* shown = entry.get<ViewDestination>();
        return shown != nullptr && shown->identity() == view;
      });
  if (found == entries.end()) {
    return false;
  }
  path_->truncate(static_cast<std::size_t>(found - entries.begin()));
  return true;
}

auto NavigationStack::StackedPath::entries() const
    -> std::vector<composition::NavigationPath::Entry> {
  std::vector<composition::NavigationPath::Entry> found = base_->entries();
  std::ranges::copy(views_.get().entries(), std::back_inserter(found));
  return found;
}

void NavigationStack::StackedPath::truncate(std::size_t count) const {
  const std::size_t based = base_->entries().size();
  if (count >= based) {
    const std::size_t kept = count - based;
    const std::size_t current = views_.get().count();
    if (kept < current) {
      views_.update(
          [removed = current - kept](composition::NavigationPath& path) {
            path.remove_last(removed);
          });
    }
    return;
  }
  base_->truncate(count);
  if (!views_.get().empty()) {
    views_.update([](composition::NavigationPath& path) { path.clear(); });
  }
}

auto NavigationStack::StackedPath::push(
    const composition::NavigationPath::Entry& entry) const -> bool {
  if (entry.get<ViewDestination>() == nullptr && views_.get().empty() &&
      base_->push(entry)) {
    return true;
  }
  if (entry.get<ViewDestination>() == nullptr) {
    return false;
  }
  views_.update(
      [&entry](composition::NavigationPath& path) { path.append(entry); });
  return true;
}

auto NavigationStack::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto NavigationStack::Content::composes() const noexcept -> bool {
  return true;
}

auto NavigationStack::Content::identify(
    History& recorded,
    const std::vector<composition::NavigationPath::Entry>& entries) -> bool {
  if (recorded.entries == entries) {
    return false;
  }
  std::size_t common = 0;
  while (common < recorded.entries.size() && common < entries.size() &&
         recorded.entries[common] == entries[common]) {
    ++common;
  }
  recorded.ids.resize(common);
  for (std::size_t index = common; index < entries.size(); ++index) {
    recorded.ids.push_back(recorded.next++);
  }
  recorded.entries = entries;
  return true;
}

auto NavigationStack::Content::pop(std::shared_ptr<const Path> path,
                                   std::size_t level) -> std::function<void()> {
  return [path = std::move(path), level] {
    if (level > 0) {
      path->truncate(level - 1);
    }
  };
}

auto NavigationStack::Content::destination(
    const std::vector<std::shared_ptr<
        const composition::NavigationDestination>>& destinations,
    const composition::NavigationPath::Entry& entry)
    -> std::optional<composition::Element> {
  if (const ViewDestination* view = entry.get<ViewDestination>()) {
    return view->build();
  }
  for (const auto& candidate : destinations | std::views::reverse) {
    if (candidate->accepts(entry)) {
      return candidate->build(entry);
    }
  }
  return std::nullopt;
}

auto NavigationStack::Content::page_key(std::size_t level,
                                        const History& recorded)
    -> composition::ElementKey {
  if (level == 0) {
    return composition::ElementKey{"navigation-root"};
  }
  return composition::ElementKey{
      static_cast<std::int64_t>(recorded.ids[level - 1])};
}

auto NavigationStack::Content::isolates(
    composition::Identity preference) const noexcept -> bool {
  return composition::NavigationDeclarations::isolates(preference);
}

void NavigationStack::Content::resolve(composition::Context& context,
                                       const composition::Node& node) {
  using Declarations = composition::NavigationDeclarations;
  const History& recorded = context.retained(history);
  std::vector<Declared> pages;
  if (node.child_count() > 0) {
    const auto top = node.children();
    const composition::Node& stacked = *top.begin();
    for (std::size_t level = 0; level <= recorded.ids.size(); ++level) {
      const composition::ElementKey key = page_key(level, recorded);
      for (const composition::Node& page : stacked.children()) {
        if (page.key() != key ||
            page.phase() == composition::NodeLifecyclePhase::departing) {
          continue;
        }
        pages.push_back(Declared{
            .page = level == 0 ? 0 : recorded.ids[level - 1],
            .title =
                page.preference(Declarations::title_key).value_or(std::nullopt),
            .toolbar = page.preference(Declarations::toolbar_key)
                           .value_or(Declarations::Toolbar{}),
            .background = page.preference(Declarations::background_key)
                              .value_or(std::nullopt),
            .chrome = page.preference(Declarations::chrome_key)
                          .value_or(composition::NavigationChrome{})});
        break;
      }
    }
  }
  const std::vector<Declared>& displayed = context.retained(displayed_key);
  const bool differs =
      std::ranges::any_of(pages, [&displayed](const Declared& page) {
        const auto match =
            std::ranges::find(displayed, page.page, &Declared::page);
        return match == displayed.end() || *match != page;
      });
  context.retained(resolved) = std::move(pages);
  if (differs) {
    const composition::StateHandle<std::uint64_t> bump =
        context.state(revision);
    bump.set(bump.get() + 1);
  }
  Declarations::Destinations& kept = context.retained(found);
  kept = node.preference(Declarations::destinations_key)
             .value_or(Declarations::Destinations{});
  const composition::StateHandle<bool> waiting = context.state(unresolved);
  if (waiting.get() && !kept.empty()) {
    waiting.set(false);
  }
}

auto NavigationStack::Content::bar(const Placement& placement)
    -> std::optional<composition::Element> {
  using composition::ToolbarItemPlacement;
  const composition::NavigationChrome& chrome = placement.chrome;
  const composition::NavigationChrome::Bar& style =
      chrome.bar(composition::ToolbarPlacement::navigation_bar);
  const bool back = placement.level > 0 && !chrome.back_button_hidden();
  const ToolbarLayout layout = placement.layout();
  const float gap = theme::metrics::navigation.group_gap;
  std::optional<composition::Element> trailing = layout.fitted(gap);
  const bool declared = placement.title.has_value() ||
                        chrome.subtitle().has_value() ||
                        layout.has(ToolbarLayout::Side::leading) ||
                        layout.has(ToolbarLayout::Side::principal) ||
                        layout.has(ToolbarLayout::Side::title) ||
                        layout.has(ToolbarLayout::Side::subtitle) ||
                        layout.title_menu() != nullptr || trailing.has_value();
  if (style.hidden() || (!back && !declared)) {
    return std::nullopt;
  }
  const auto back_button = [&placement] {
    return components::Text(text::LocalizedStringKey{"\u2039 Back"})
        .foreground_style(theme::ColourRole::control_text)
        .padding(theme::Insets::symmetric(theme::space::sm, theme::space::xs))
        .content_shape(theme::Rectangle())
        .on_tap_gesture(pop(placement.path, placement.level));
  };
  std::vector<composition::Element> items;
  if (back) {
    items.push_back(back_button());
  }
  const composition::ToolbarTitleDisplayMode mode = chrome.title_display_mode();
  const bool large = mode == composition::ToolbarTitleDisplayMode::large;
  std::optional<composition::Element> compact;
  if (placement.minimized) {
    std::vector<composition::Element> shrunk;
    if (back) {
      shrunk.push_back(back_button());
    }
    std::optional<composition::Element> heading = layout.titled(
        placement.title, std::nullopt, theme::Font::subheadline, false);
    shrunk.push_back(components::Spacer());
    if (heading.has_value()) {
      shrunk.push_back(std::move(*heading));
    }
    shrunk.push_back(components::Spacer());
    compact = components::HStack(
                  components::HStackOptions{
                      .alignment = components::VerticalAlignment::center,
                      .spacing = theme::space::md},
                  std::move(shrunk))
                  .frame({.max_width = composition::infinity,
                          .min_height = minimized_height});
    if (placement.keeps_slot) {
      compact = std::move(*compact).frame(
          {.max_width = composition::infinity,
           .min_height = theme::metric::navigation_bar_height,
           .alignment = composition::Alignment::top});
    }
  }
  for (composition::Element& leading :
       layout.flattened(ToolbarLayout::Side::leading, gap)) {
    items.push_back(std::move(leading));
  }
  std::optional<composition::Element> heading;
  std::vector<composition::Element> principal =
      layout.flattened(ToolbarLayout::Side::principal, gap);
  const std::optional<ToolbarItemPlacement> moved =
      layout.relocated(composition::ToolbarDefaultItemKind::title);
  if ((principal.empty() || moved.has_value()) &&
      (!large || placement.collapsed)) {
    heading = layout.titled(
        placement.title, chrome.subtitle(),
        ToolbarLayout::title_font(
            large ? composition::ToolbarTitleDisplayMode::inline_title : mode,
            theme::Font::headline),
        false);
    if (heading.has_value() && large) {
      heading = std::move(*heading).transition(motion::Transition::opacity());
    }
  }
  const composition::ToolbarTitleAlignment alignment = chrome.title_alignment();
  bool centred = alignment == composition::ToolbarTitleAlignment::center ||
                 (alignment == composition::ToolbarTitleAlignment::automatic &&
                  chrome.role() == composition::ToolbarRole::navigation_stack);
  bool heading_trailing = false;
  if (moved.has_value()) {
    const composition::ToolbarItem probe{*moved, components::ZStack()};
    centred = *moved == ToolbarItemPlacement::principal;
    heading_trailing = probe.trailing();
  }
  if (!centred) {
    for (composition::Element& each : principal) {
      items.push_back(std::move(each));
    }
    if (heading.has_value() && !heading_trailing) {
      items.push_back(std::move(*heading));
      heading.reset();
    }
  }
  if (!trailing.has_value() || heading_trailing) {
    items.push_back(components::Spacer());
  }
  if (heading.has_value() && heading_trailing) {
    items.push_back(std::move(*heading));
    heading.reset();
  }
  if (trailing.has_value()) {
    items.push_back(std::move(*trailing));
  }
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::md},
          std::move(items))
          .frame({.max_width = composition::infinity,
                  .min_height = theme::metric::navigation_bar_height});
  if (centred && (heading.has_value() || !principal.empty())) {
    std::vector<composition::Element> middle = std::move(principal);
    if (heading.has_value()) {
      middle.push_back(std::move(*heading));
    }
    row = std::move(row).overlay(
        composition::Alignment::center,
        components::HStack(
            components::HStackOptions{
                .alignment = components::VerticalAlignment::center,
                .spacing = theme::space::md},
            std::move(middle)));
  }
  if (placement.minimization !=
      composition::ToolbarMinimizationBehavior::never) {
    if (compact.has_value()) {
      row = std::move(row).hidden().disabled(true).frame(
          {.height = 0.0F, .max_width = composition::infinity});
    }
    row = components::VStack(
        components::VStackOptions{.spacing = theme::space::none},
        std::move(row), std::move(compact));
  } else if (compact.has_value()) {
    row = std::move(*compact);
  }
  return ToolbarLayout::styled(
      std::move(row)
          .button_style(components::ButtonStyle::toolbar())
          .environment(composition::ToolbarEnvironment::title_alignment,
                       alignment)
          .id(std::string{bar_id}),
      style);
}

auto NavigationStack::Content::large_title(const Placement& placement)
    -> std::optional<composition::Element> {
  const composition::NavigationChrome& chrome = placement.chrome;
  if (chrome.title_display_mode() !=
          composition::ToolbarTitleDisplayMode::large ||
      (!placement.title.has_value() &&
       !placement.layout().has(ToolbarLayout::Side::large_title)) ||
      chrome.bar(composition::ToolbarPlacement::navigation_bar).hidden() ||
      placement.minimized) {
    return std::nullopt;
  }
  const ToolbarLayout layout = placement.layout();
  std::optional<composition::Element> heading = layout.titled(
      placement.title, chrome.subtitle(), theme::Font::large_title, true);
  if (!heading.has_value()) {
    return std::nullopt;
  }
  if (placement.collapsed) {
    heading = std::move(*heading).hidden();
  }
  return std::move(*heading)
      .visual_effect(
          [travelled = placement.travelled](composition::VisualEffect shown,
                                            composition::GeometryProxy) {
            const float offset = travelled == nullptr ? 0.0F : *travelled;
            if (offset < 0.0F) {
              const float stretch =
                  std::min(-offset, large_title_stretch) / large_title_stretch;
              return shown.scale_effect(1.0F + (large_title_growth * stretch),
                                        motion::UnitPoint::bottom_leading);
            }
            const float travel =
                std::min(offset, large_title_travel) / large_title_travel;
            return shown.offset(0.0F, -offset).opacity(1.0F - travel);
          })
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .transition(motion::Transition::opacity())
      .id(large_title_id);
}

auto NavigationStack::Content::keyboard_bar(const Placement& placement)
    -> std::optional<composition::Element> {
  const ToolbarLayout layout = placement.layout();
  if (!placement.keyboard || !layout.has(ToolbarLayout::Side::keyboard)) {
    return std::nullopt;
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::md},
             layout.flattened(ToolbarLayout::Side::keyboard,
                              theme::metrics::navigation.group_gap))
      .frame({.max_width = composition::infinity,
              .min_height = theme::metric::navigation_bar_height})
      .button_style(components::ButtonStyle::toolbar())
      .default_glass(theme::GlassSlot::toolbar, theme::Shape::rectangle())
      .id(keyboard_bar_id);
}

auto NavigationStack::Content::bottom_bar(const Placement& placement)
    -> std::optional<composition::Element> {
  const composition::NavigationChrome& chrome = placement.chrome;
  const composition::NavigationChrome::Bar& style =
      chrome.bar(composition::ToolbarPlacement::bottom_bar);
  const ToolbarLayout layout = placement.layout();
  if (style.hidden() || !layout.has(ToolbarLayout::Side::bottom)) {
    return std::nullopt;
  }
  composition::Element row =
      placement.floating
          ? composition::Element{components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::Space{placement.metrics.gap}},
                layout.grouped(ToolbarLayout::Side::bottom, placement.metrics))}
          : composition::Element{components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::space::md},
                layout.flattened(ToolbarLayout::Side::bottom,
                                 theme::metrics::navigation.group_gap))};
  row = std::move(row)
            .frame({.max_width = composition::infinity,
                    .min_height = theme::metric::navigation_bar_height})
            .button_style(components::ButtonStyle::toolbar())
            .environment(composition::ToolbarEnvironment::title_alignment,
                         chrome.title_alignment());
  if (!placement.floating &&
      style.background_visibility != composition::Visibility::hidden) {
    row = std::move(row).default_glass(theme::GlassSlot::toolbar,
                                       theme::Shape::rectangle());
  }
  return ToolbarLayout::styled(std::move(row).id(bottom_bar_id), style);
}

auto NavigationStack::Content::vertical_bar(const Placement& placement)
    -> std::optional<composition::Element> {
  if (!placement.vertical) {
    return std::nullopt;
  }
  const ToolbarLayout layout = placement.layout();
  if (!layout.has(ToolbarLayout::Side::vertical)) {
    return std::nullopt;
  }
  std::vector<composition::Element> items =
      layout.flattened(ToolbarLayout::Side::vertical, placement.metrics.gap);
  ToolbarLayout::capsules(items);
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::Space{placement.metrics.spacing}},
             std::move(items))
      .padding(theme::Insets::symmetric(
          theme::space::none, theme::Space{placement.metrics.padding}))
      .frame({.min_width = placement.metrics.height})
      .default_glass(theme::GlassSlot::toolbar, theme::Shape::capsule())
      .button_style(components::ButtonStyle::toolbar())
      .padding(theme::Edge::trailing, theme::space::md)
      .id(std::string{ToolbarLayout::vertical_key});
}

class NavigationStack::Content::TitleTracker final
    : public composition::ScrollGeometryObserver {
 public:
  TitleTracker(const Placement& placement, bool large) noexcept
      : collapsing_(placement.collapsing),
        minimizing_(placement.minimizing),
        page_(placement.id),
        travelled_(placement.travelled),
        large_(large),
        behavior_(placement.minimization),
        restoration_(placement.restoration) {}

  auto differs(const composition::ScrollGeometry& before,
               const composition::ScrollGeometry& after) const
      -> bool override {
    bool changed = false;
    if (large_) {
      *travelled_ =
          std::round(
              std::clamp(after.content_offset.y + after.content_insets.top,
                         -large_title_stretch, large_title_travel) *
              2.0F) *
          0.5F;
      changed = collapsed(before) != collapsed(after);
    }
    if (behavior_ != composition::ToolbarMinimizationBehavior::never) {
      changed = changed || wanted(before, after) != minimized();
    }
    return changed;
  }

  void notify(const composition::ScrollGeometry& before,
              const composition::ScrollGeometry& after) override {
    if (large_) {
      const bool now = collapsed(after);
      if (std::ranges::contains(collapsing_.get(), page_) != now) {
        motion::with_animation(composition::Layer::default_animation(), [this,
                                                                         now] {
          collapsing_.update([this, now](std::vector<std::uint64_t>& pages) {
            std::erase(pages, page_);
            if (now) {
              pages.push_back(page_);
            }
            if (pages.size() > collapsed_limit) {
              pages.erase(pages.begin());
            }
          });
        });
      }
    }
    if (behavior_ != composition::ToolbarMinimizationBehavior::never) {
      const bool want = wanted(before, after);
      if (want != minimized()) {
        motion::with_animation(composition::Layer::default_animation(), [this,
                                                                         want] {
          minimizing_.update([this, want](std::vector<std::uint64_t>& pages) {
            std::erase(pages, page_);
            if (want) {
              pages.push_back(page_);
            }
            if (pages.size() > collapsed_limit) {
              pages.erase(pages.begin());
            }
          });
        });
      }
    }
  }

 private:
  static auto collapsed(const composition::ScrollGeometry& geometry) -> bool {
    return geometry.content_offset.y + geometry.content_insets.top >
           large_title_travel;
  }

  auto minimized() const -> bool {
    return std::ranges::contains(minimizing_.get(), page_);
  }

  auto wanted(const composition::ScrollGeometry& before,
              const composition::ScrollGeometry& after) const -> bool {
    using Behavior = composition::ToolbarMinimizationBehavior;
    const float moved = (after.content_offset.y + after.content_insets.top) -
                        (before.content_offset.y + before.content_insets.top);
    const bool now = minimized();
    if (after.content_offset.y + after.content_insets.top <= scroll_edge) {
      return false;
    }
    if (behavior_ == Behavior::on_scroll_down) {
      if (moved > minimize_threshold) {
        return true;
      }
      if (moved < -minimize_threshold) {
        return restoration_ == composition::ToolbarMinimizationRestoration::
                                   at_scroll_edge
                   ? now
                   : false;
      }
      return now;
    }
    if (moved < -minimize_threshold) {
      return true;
    }
    if (moved > minimize_threshold) {
      return false;
    }
    return now;
  }

  composition::StateHandle<std::vector<std::uint64_t>> collapsing_;
  composition::StateHandle<std::vector<std::uint64_t>> minimizing_;
  std::uint64_t page_;
  std::shared_ptr<float> travelled_;
  bool large_;
  composition::ToolbarMinimizationBehavior behavior_;
  composition::ToolbarMinimizationRestoration restoration_;
};

auto NavigationStack::Content::collapse(const Placement& placement)
    -> std::shared_ptr<composition::ScrollGeometryObserver> {
  const bool large = placement.chrome.title_display_mode() ==
                     composition::ToolbarTitleDisplayMode::large;
  if (!large && placement.minimization ==
                    composition::ToolbarMinimizationBehavior::never) {
    return nullptr;
  }
  return std::make_shared<TitleTracker>(placement, large);
}

auto NavigationStack::Content::edge(const Placement& placement)
    -> composition::Element {
  const std::shared_ptr<const Path> path = placement.path;
  const composition::StateHandle<Drag> drag = placement.dragging;
  const std::uint64_t id = placement.id;
  const std::size_t level = placement.level;
  const float sign = placement.mirrored ? -1.0F : 1.0F;
  return components::ZStack()
      .frame({.width = edge_width, .max_height = composition::infinity})
      .content_shape(theme::Rectangle())
      .gesture(
          composition::DragGesture(composition::DragGesture::default_distance,
                                   composition::CoordinateSpace::global)
              .on_changed([drag, id,
                           sign](const composition::DragGesture::Value& value) {
                drag.set(Drag{
                    .offset = std::max(value.translation.width * sign, 0.0F),
                    .screen = id});
              })
              .on_ended([drag, id, path, level,
                         sign](const composition::DragGesture::Value& value) {
                const bool commit =
                    value.translation.width * sign > commit_distance ||
                    value.predicted_end_translation.width * sign >
                        commit_prediction;
                motion::with_animation(
                    composition::Layer::default_animation(),
                    [&drag, &path, commit, id, level] {
                      if (commit) {
                        path->truncate(level - 1);
                      }
                      drag.set(Drag{.offset = 0.0F, .screen = id});
                    });
              }));
}

auto NavigationStack::Content::screen(composition::Element content,
                                      const Placement& placement)
    -> composition::Element {
  const bool covered = placement.level < placement.depth;
  const bool hides_back = placement.chrome.back_button_hidden();
  const bool top = !covered && placement.level > 0 && !hides_back;
  std::vector<composition::Element> column;
  if (auto shown = bar(placement)) {
    column.push_back(std::move(*shown));
  }
  std::optional<composition::Element> heading = large_title(placement);
  std::vector<composition::Element> area;
  area.push_back(components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::leading},
                     std::move(content))
                     .frame({.max_width = composition::infinity,
                             .max_height = composition::infinity,
                             .alignment = composition::Alignment::leading}));
  if (auto upright = vertical_bar(placement)) {
    area.push_back(components::ZStack(std::move(*upright))
                       .frame({.max_width = composition::infinity,
                               .max_height = composition::infinity,
                               .alignment = composition::Alignment::trailing}));
  }
  if (top) {
    area.push_back(edge(placement));
  }
  composition::Element body =
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        components::Alignment::top_leading},
          std::move(area))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading});
  body = std::move(body).safe_area_inset(
      composition::VerticalEdge::top, composition::HorizontalAlignment::leading,
      0.0F,
      heading.has_value()
          ? std::move(*heading).padding(theme::Edge::bottom, theme::space::sm)
          : composition::Element{components::ZStack()});
  column.push_back(std::move(body));
  if (auto typing = keyboard_bar(placement)) {
    column.push_back(std::move(*typing));
  }
  if (auto below = bottom_bar(placement)) {
    column.push_back(std::move(*below));
  }
  std::vector<composition::Element> layers;
  layers.push_back(
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::sm},
          std::move(column))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading}));
  std::optional<composition::MatchedGeometry::Key> zoom;
  if (placement.level > 0 && placement.transition.has_value()) {
    zoom = placement.transition->zoom_source();
  }
  const bool zooms = zoom.has_value() ||
                     (placement.level > 0 && placement.transition.has_value() &&
                      placement.transition->fades());
  composition::Element built =
      Screen(std::move(layers), covered, collapse(placement), std::move(zoom))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity});
  if (placement.background.has_value()) {
    built = placement.background->clad(std::move(built), theme::Rectangle());
  } else if (!placement.floating) {
    built = std::move(built).background(theme::ColourRole::background);
  }
  if (placement.level == 0) {
    built = std::move(built).id("navigation-root");
  } else {
    built =
        std::move(built)
            .id(static_cast<std::int64_t>(placement.id))
            .transition(zooms ? motion::Transition::opacity()
                              : motion::Transition::move(theme::Edge::trailing))
            .focus_scope(placement.scope,
                         composition::FocusScopeBehavior::confined)
            .environment(components::Environment::dismiss,
                         composition::DismissAction{
                             placement.path->identity(), placement.level,
                             pop(placement.path, placement.level)});
    if (!hides_back) {
      built = std::move(built).on_exit_command(
          std::string{back_label}, pop(placement.path, placement.level));
    }
  }
  if (covered) {
    const bool revealed =
        placement.drag > 0.0F && placement.level + 1 == placement.depth;
    if (placement.floating) {
      built = std::move(built).opacity(
          revealed ? std::min(placement.drag / commit_distance, 1.0F) : 0.0F);
    }
    const float sign = placement.mirrored ? -1.0F : 1.0F;
    built = std::move(built).offset(
        sign * (revealed ? std::min((placement.drag * 0.25F) - parallax, 0.0F)
                         : -parallax));
  } else if (top && placement.drag > 0.0F) {
    built = std::move(built).offset(placement.mirrored ? -placement.drag
                                                       : placement.drag);
  }
  if (placement.changed) {
    built = std::move(built).transaction([](motion::Transaction& transaction) {
      if (!transaction.animation().has_value() &&
          !transaction.disables_animations()) {
        transaction.set_animation(composition::Layer::default_animation());
      }
    });
  }
  return built;
}

auto NavigationStack::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::shared_ptr<const Path> base = path_;
  if (base == nullptr) {
    base = std::make_shared<const ErasedPath>(context.state(internal_path));
  }
  std::shared_ptr<const Path> path = std::make_shared<const StackedPath>(
      std::move(base), context.state(views));
  const std::vector<composition::NavigationPath::Entry> entries =
      path->entries();
  History& recorded = context.retained(history);
  const composition::StateHandle<bool> typing = context.state(keyboard_showing);
  const bool changed = identify(recorded, entries);
  const composition::StateHandle<Drag> drag = context.state(dragging);
  const std::size_t depth = entries.size();
  const std::uint64_t top = depth == 0 ? 0 : recorded.ids.back();
  const float offset =
      drag.get().screen == top && depth > 0 ? drag.get().offset : 0.0F;
  const composition::Namespace::ID scope = context.namespace_id(screens);
  const theme::Theme& current =
      context.environment(components::Environment::theme);
  const bool floating = current.glass().toolbar.has_value();
  const ToolbarLayout::Metrics metrics = ToolbarLayout::metrics(current);
  const composition::StateHandle<std::vector<std::uint64_t>> collapsing =
      context.state(collapsed);
  std::map<std::uint64_t, std::shared_ptr<float>>& offsets =
      context.retained(title_offsets);
  std::erase_if(offsets, [&recorded](const auto& entry) {
    return entry.first != 0 &&
           !std::ranges::contains(recorded.ids, entry.first);
  });
  const auto travelled = [&offsets](std::uint64_t page) {
    std::shared_ptr<float>& slot = offsets[page];
    if (slot == nullptr) {
      slot = std::make_shared<float>(0.0F);
    }
    return slot;
  };
  const std::optional<composition::PresentationBackground>& surrounding =
      context.environment(
          composition::ContainerBackgroundPlacement::navigation.key());
  composition::Element root = root_ ? root_() : components::ZStack();
  if (root_ && !root.navigation().toolbar.empty()) {
    const std::weak_ptr<const composition::NodeAnchor> declarer =
        context.node_anchor();
    for (const auto& item : root.navigation().toolbar) {
      if (item->content() != nullptr) {
        item->content()->declared_by(declarer);
      }
    }
  }
  static_cast<void>(context.state(revision));
  const std::vector<Declared>& declared_pages = context.retained(resolved);
  std::vector<Declared>& displayed = context.retained(displayed_key);
  displayed.clear();
  std::vector<std::shared_ptr<const composition::NavigationDestination>>
      destinations = context.retained(found);
  std::ranges::copy(root.navigation().destinations,
                    std::back_inserter(destinations));
  const bool mirrored =
      context.environment(components::Environment::layout_direction) ==
      composition::LayoutDirection::right_to_left;
  const bool compressed =
      context.environment(composition::ToolbarEnvironment::compressed);
  const composition::StateHandle<std::vector<std::uint64_t>> minimizing =
      context.state(minimized_pages);
  composition::ToolbarTitleDisplayMode inherited =
      composition::ToolbarTitleDisplayMode::automatic;
  const auto place = [&](std::size_t level,
                         const composition::Element& content) {
    const std::uint64_t id = level == 0 ? 0 : recorded.ids[level - 1];
    const auto declared_page =
        std::ranges::find(declared_pages, id, &Declared::page);
    const Declared* page =
        declared_page == declared_pages.end() ? nullptr : &*declared_page;
    const composition::NavigationDeclarations& own = content.navigation();
    Declared used{
        .page = id,
        .title =
            own.title.has_value() || page == nullptr ? own.title : page->title,
        .toolbar = page == nullptr ? own.toolbar : page->toolbar,
        .background = own.container_background.has_value() || page == nullptr
                          ? own.container_background
                          : page->background,
        .chrome = page == nullptr ? own.chrome : page->chrome};
    std::optional<ToolbarLayout::Customization> customization;
    if (const auto named = ToolbarLayout::customization_id(used.toolbar)) {
      customization =
          context.app_storage(ToolbarLayout::storage_key(*named), customized);
    }
    const composition::ToolbarMinimizationBehavior behavior =
        minimizes(used.chrome, used.toolbar);
    const composition::NavigationChrome::Bar& navigation_bar =
        used.chrome.bar(composition::ToolbarPlacement::navigation_bar);
    Placement placed{
        .level = level,
        .depth = depth,
        .id = id,
        .drag = offset,
        .changed = changed,
        .floating = floating,
        .background = used.background.or_else([&] { return surrounding; }),
        .title = used.title,
        .toolbar = used.toolbar,
        .chrome = used.chrome,
        .collapsed = std::ranges::contains(collapsing.get(), id),
        .metrics = metrics,
        .scope = scope,
        .path = path,
        .dragging = drag,
        .collapsing = collapsing,
        .transition = own.transition,
        .travelled = travelled(id),
        .customization = std::move(customization),
        .vertical = used.chrome.vertical_behavior() !=
                        composition::ToolbarVerticalBehavior::disabled &&
                    ToolbarLayout::prefers_vertical(used.toolbar),
        .keyboard = typing.get(),
        .minimized =
            compressed ||
            (behavior != composition::ToolbarMinimizationBehavior::never &&
             std::ranges::contains(minimizing.get(), id)),
        .keeps_slot =
            navigation_bar.safe_area_adjustment ==
            composition::ToolbarMinimizationSafeAreaAdjustment::disabled,
        .mirrored = mirrored,
        .minimization = behavior,
        .restoration = navigation_bar.restoration.value_or(
            composition::ToolbarMinimizationRestoration::automatic),
        .minimizing = minimizing};
    if (level > 0 && !placed.chrome.declares_title_display_mode()) {
      placed.chrome.set_title_display_mode(inherited);
    }
    inherited = placed.chrome.title_display_mode();
    displayed.push_back(std::move(used));
    return placed;
  };
  std::vector<composition::Element> screens_built;
  screens_built.reserve(depth + 1);
  const Placement first = place(0, root);
  screens_built.push_back(screen(std::move(root), first));
  bool& missing = context.retained(unresolved);
  missing = false;
  for (std::size_t index = 0; index < depth; ++index) {
    std::optional<composition::Element> built =
        destination(destinations, entries[index]);
    if (!built.has_value()) {
      missing = true;
    }
    composition::Element content =
        built.has_value()
            ? std::move(*built)
            : composition::Element{
                  components::Text("No destination for this value")
                      .foreground_style(theme::ColourRole::text_secondary)
                      .padding(theme::space::xl)};
    std::ranges::copy(content.navigation().destinations,
                      std::back_inserter(destinations));
    const Placement placement = place(index + 1, content);
    screens_built.push_back(screen(std::move(content), placement));
  }
  std::vector<composition::Element> produced;
  produced.push_back(
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        components::Alignment::top_leading},
          std::move(screens_built))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading})
          .clip_shape(theme::Rectangle())
          .environment(navigator, Navigator{path})
          .on_preference_change(composition::TextInputEnvironment::keyboard,
                                [typing](bool showing) {
                                  if (typing.get() != showing) {
                                    typing.set(showing);
                                  }
                                }));
  return produced;
}

}
