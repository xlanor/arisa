#include <arisa/async/awaitables.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/index_set.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/reorder_cell.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/focus_follow.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/toolbar_environment.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/index_view_style.hpp>
#include <arisa/structure/swipe_row.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/theme.hpp>
#include <arisa/theme/typography.hpp>
#include <chrono>

#include <algorithm>
#include <cstdint>
#include <format>
#include <type_traits>
#include <utility>

namespace arisa::structure {

TabView::TabView(std::optional<Selection> selection,
                 std::vector<composition::Element> tabs)
    : Element(std::make_unique<Content>(std::move(selection),
                                        entries(std::move(tabs)))) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

const composition::StateKey<std::vector<composition::Rect>>
    TabView::item_frames{"tab-view-item-frames", {}};

const composition::EnvironmentKey<TabView::SidebarContent>
    TabView::sidebar_header{"tab_view_sidebar_header", nullptr};

const composition::StateKey<composition::NavigationChrome::Bar>
    TabView::Content::tab_bar_style{"tab-view-bar-style", {}};

const composition::EnvironmentKey<TabView::SidebarContent>
    TabView::sidebar_footer{"tab_view_sidebar_footer", nullptr};

const composition::EnvironmentKey<TabView::SidebarContent>
    TabView::sidebar_bottom_bar{"tab_view_sidebar_bottom_bar", nullptr};

const composition::EnvironmentKey<TabView::SidebarContent>
    TabView::sidebar_toolbar{"tab_view_sidebar_toolbar", nullptr};

const composition::EnvironmentKey<TabView::SidebarContent>
    TabView::bottom_accessory{"tab_view_bottom_accessory", nullptr};

const composition::EnvironmentKey<TabBarMinimizeBehavior>
    TabView::minimize_behavior{"tab_bar_minimize_behavior",
                               TabBarMinimizeBehavior::automatic};

const composition::EnvironmentKey<TabSearchActivation>
    TabView::search_activation{"tab_view_search_activation",
                               TabSearchActivation::automatic};

const composition::EnvironmentKey<std::optional<AdaptableTabBarPlacement>>
    TabView::default_placement{"default_tab_bar_placement", std::nullopt};

const composition::EnvironmentKey<std::optional<AdaptableTabBarPlacement>>
    TabView::adaptable_placement{"default_adaptable_tab_bar_placement",
                                 std::nullopt};

const composition::EnvironmentKey<std::optional<TabBarPlacement>>
    TabView::bar_placement{"tab_bar_placement", std::nullopt};

const composition::EnvironmentKey<bool> TabView::sidebar_available{
    "is_tab_view_sidebar_available", false};

const composition::EnvironmentKey<bool> TabView::showing_sections{
    "is_tab_bar_showing_sections", false};

const composition::EnvironmentKey<
    std::optional<TabViewBottomAccessoryPlacement>>
    TabView::accessory_placement{"tab_view_bottom_accessory_placement",
                                 std::nullopt};

auto TabView::Minimizer::wanted(TabBarMinimizeBehavior behavior, bool now,
                                float before, float after) noexcept -> bool {
  if (after <= edge) {
    return false;
  }
  const float moved = after - before;
  const bool down = behavior == TabBarMinimizeBehavior::on_scroll_down;
  if (moved > threshold) {
    return down;
  }
  if (moved < -threshold) {
    return !down;
  }
  return now;
}

auto TabView::Minimizer::differs(const composition::ScrollGeometry& before,
                                 const composition::ScrollGeometry& after) const
    -> bool {
  return wanted(behavior_, minimized_.get(),
                before.content_offset.y + before.content_insets.top,
                after.content_offset.y + after.content_insets.top) !=
         minimized_.get();
}

void TabView::Minimizer::notify(const composition::ScrollGeometry& before,
                                const composition::ScrollGeometry& after) {
  const bool want = wanted(behavior_, minimized_.get(),
                           before.content_offset.y + before.content_insets.top,
                           after.content_offset.y + after.content_insets.top);
  if (want != minimized_.get()) {
    motion::with_animation(motion::Animation::snappy(0.3),
                           [this, want] { minimized_.set(want); });
  }
}

TabView::Pane::Pane(
    std::shared_ptr<const Tab::Builder> builder, bool frozen, bool covered,
    std::shared_ptr<composition::ScrollGeometryObserver> observer)
    : Element(std::make_unique<Content>(std::move(builder), frozen)) {
  declared().covered = covered;
  declared().scrolling.nested_geometry = std::move(observer);
}

auto TabView::Pane::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TabView::Pane::Content::same_as(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return theirs.frozen_ || theirs.builder_ == builder_;
}

auto TabView::Pane::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  produced.push_back(
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          builder_ != nullptr && *builder_
              ? (*builder_)()
              : composition::Element{components::ZStack()})
          .frame({.max_width = composition::infinity,
                  .min_height = 0.0F,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .flex_shrink(1.0F));
  return produced;
}

auto TabView::item_key(std::size_t index) -> composition::ElementKey {
  return composition::ElementKey{std::format("arisa.tab.{}", index)};
}

auto TabView::tab_view_customization(
    composition::Binding<TabViewCustomization> customization) && -> TabView {
  edit<Content>().bind_customization(std::move(customization));
  return std::move(*this);
}

auto TabView::column_visibility(
    composition::Binding<composition::NavigationSplitViewVisibility>
        visibility) && -> TabView {
  edit<Content>().bind_visibility(NavigationSplitView::Binding{
      .get = [visibility] { return visibility.get(); },
      .set =
          [visibility](composition::NavigationSplitViewVisibility value) {
            visibility.set(value);
          }});
  return std::move(*this);
}

void TabView::add(Entries& found, const composition::Element& tab,
                  std::optional<std::size_t> section) {
  const auto content = Tab::content(tab);
  if (!content.has_value()) {
    return;
  }
  const Tab::Content& each = content->get();
  const std::shared_ptr<const composition::ListRow> row = tab.list_row();
  found.tabs.push_back(Entry{
      .title = each.title(),
      .system_image = each.system_image(),
      .value = each.value(),
      .content = std::make_shared<const Tab::Builder>(each.builder()),
      .role = each.role(),
      .badge = row != nullptr ? row->badge : std::nullopt,
      .customization = each.customization(),
      .behavior = each.customization_behavior(),
      .section = section,
      .traits = each.traits(),
      .label = each.label() ? std::make_shared<const Tab::Builder>(each.label())
                            : nullptr,
      .decorations = each.decorations(),
      .leading = each.swipe(composition::HorizontalEdge::leading),
      .trailing = each.swipe(composition::HorizontalEdge::trailing)});
}

auto TabView::entries(std::vector<composition::Element> tabs) -> Entries {
  Entries found;
  for (composition::Element& tab : tabs) {
    if (const auto section = TabSection::content(tab)) {
      const std::size_t index = found.sections.size();
      found.sections.push_back(section->get().title());
      found.actions.push_back(section->get().actions());
      found.headings.push_back(
          SectionHeading{.header = section->get().header(),
                         .customization = section->get().customization(),
                         .expansion = section->get().expansion()});
      for (const composition::Element& member : section->get().tabs()) {
        add(found, member, index);
      }
      continue;
    }
    add(found, tab, std::nullopt);
  }
  return found;
}

auto TabView::ItemStyle::make_body(
    components::ButtonStyle::Configuration configuration) const
    -> composition::Element {
  std::vector<composition::Element> parts;
  parts.push_back(std::move(configuration.label));
  if (badge.has_value() && sidebar && !rail) {
    parts.push_back(components::Spacer());
    parts.push_back(
        components::Text(*badge)
            .foreground_style(
                selected && emphasized
                    ? theme::
                          ColourValue{theme::ColourRole::selected_control_text}
                    : theme::ColourValue{theme::ColourRole::text_secondary})
            .line_limit(1)
            .id(badge_id));
  }
  composition::Element body =
      components::HStack(std::move(parts))
          .padding(sidebar ? theme::Insets::symmetric(
                                 theme::metric::sidebar_selection_inset,
                                 theme::metric::sidebar_row_vertical_inset)
                           : theme::Insets::symmetric(theme::space::md, 6.0F))
          .foreground_style(prominent ? theme::ColourRole::light_text
                            : selected && emphasized
                                ? theme::ColourRole::selected_control_text
                                : theme::ColourRole::label);
  if (prominent) {
    body = std::move(body).background(theme::ColourRole::accent,
                                      theme::Shape::capsule());
  }
  if (sidebar) {
    body = std::move(body).frame(
        {.max_width = composition::infinity,
         .alignment = rail ? composition::Alignment::center
                           : composition::Alignment::leading});
  }
  if (badge.has_value() && (!sidebar || rail)) {
    body = std::move(body).overlay(
        composition::Alignment::top_trailing,
        components::Text(*badge)
            .font(theme::Font::caption2)
            .foreground_style(theme::ColourRole::light_text)
            .line_limit(1)
            .padding(theme::Insets::symmetric(5.0F, 1.0F))
            .background(theme::ColourRole::red, theme::Shape::capsule())
            .offset(4.0F, -4.0F)
            .allows_hit_testing(false)
            .id(badge_id));
  }
  if (configuration.is_pressed) {
    body = std::move(body).opacity(components::ButtonStyle::pressed_opacity);
  }
  return std::move(body).animation(motion::Animation::snappy(0.2),
                                   configuration.is_pressed);
}

auto TabView::RailLabel::linger(composition::StateHandle<bool> shown)
    -> async::Task<void> {
  shown.set(true);
  co_await async::sleep(std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(
          theme::metrics::list.sidebar_rail_label_linger)));
  motion::with_animation(motion::Animation::default_animation(),
                         [shown] { shown.set(false); });
}

auto TabView::RailLabel::body(composition::Context& context) const
    -> composition::Element {
  const auto shown = context.state(visible);
  return components::Text(text::LocalizedStringKey{title_})
      .font(theme::Font::callout)
      .foreground_style(theme::ColourRole::label)
      .padding(theme::Insets::symmetric(theme::space::md, theme::space::xs))
      .frame({.width = theme::metric::sidebar_width,
              .alignment = composition::Alignment::center})
      .background(theme::ColourRole::surface_raised, theme::Shape::capsule())
      .border(theme::ColourRole::separator, theme::Stroke::hairline())
      .shadow(theme::Elevation::low)
      .opacity(shown.get() ? 1.0F : 0.0F)
      .allows_hit_testing(false)
      .task(composition::ElementKey{static_cast<std::int64_t>(index_)},
            [shown] { return linger(shown); });
}

void TabView::Content::resolve(composition::Context& context,
                               const composition::Node& node) {
  composition::NavigationChrome declared =
      node.preference(composition::NavigationDeclarations::chrome_key)
          .value_or(composition::NavigationChrome{});
  if (const std::optional<composition::NavigationChrome> stacked =
          presentation::NavigationStack::top_chrome(node)) {
    composition::NavigationChrome merged = *stacked;
    merged.override_with(declared);
    declared = std::move(merged);
  }
  const composition::NavigationChrome::Bar& found =
      declared.bar(composition::ToolbarPlacement::tab_bar);
  const composition::StateHandle<composition::NavigationChrome::Bar> held =
      context.state(tab_bar_style);
  if (held.get() != found) {
    held.set(found);
  }
  const composition::StateHandle<
      composition::ToolbarVerticalCompressionBehavior>
      squeezed = context.state(compression);
  if (squeezed.get() != declared.vertical_compression()) {
    squeezed.set(declared.vertical_compression());
  }
}

auto TabView::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto TabView::Content::visibility(composition::Context& context) const
    -> NavigationSplitView::Binding {
  if (visibility_.has_value()) {
    return *visibility_;
  }
  const auto state = context.state(NavigationSplitView::chosen);
  const composition::NavigationSplitViewVisibility fallback =
      context.environment(adaptable_placement) ==
              AdaptableTabBarPlacement::tab_bar
          ? composition::NavigationSplitViewVisibility::detail_only
          : context.environment(components::Environment::sidebar_visibility);
  return NavigationSplitView::Binding{
      .get = [state, fallback] { return state.get().value_or(fallback); },
      .set =
          [state](composition::NavigationSplitViewVisibility value) {
            state.set(value);
          }};
}

auto TabView::Content::selected(composition::Context& context) const
    -> std::size_t {
  if (entries_.empty()) {
    return 0;
  }
  if (selection_.has_value()) {
    const auto found = std::ranges::find_if(entries_, [this](const Entry& e) {
      return e.value != nullptr && selection_->is_selected(*e.value);
    });
    if (found != entries_.end()) {
      return static_cast<std::size_t>(found - entries_.begin());
    }
    return 0;
  }
  return std::min(context.state(chosen).get(), entries_.size() - 1);
}

auto TabView::Content::pane(
    const DetailPresenter::Slot& slot,
    const std::shared_ptr<composition::ScrollGeometryObserver>& observer) const
    -> composition::Element {
  const std::size_t* index = slot.key.get<std::size_t>();
  return composition::Element{Pane{index != nullptr && *index < entries_.size()
                                       ? entries_[*index].content
                                       : nullptr,
                                   slot.frozen, false, observer}};
}

auto TabView::Content::visible(std::size_t index, bool sidebar) const -> bool {
  const Entry& entry = entries_[index];
  if (entry.traits.hidden) {
    return false;
  }
  if (!sidebar && entry.traits.placement == TabPlacement::sidebar_only) {
    return false;
  }
  if (!customization_.has_value()) {
    return true;
  }
  const composition::Visibility fallback =
      sidebar ? entry.traits.sidebar_default : entry.traits.tab_bar_default;
  if (!customizable(index)) {
    return true;
  }
  const TabViewCustomization current = customization_->get();
  const composition::Visibility picked =
      sidebar ? current.sidebar_visibility(entry.customization)
              : current.tab_bar_visibility(entry.customization);
  return picked == composition::Visibility::visible ||
         (picked == composition::Visibility::automatic &&
          fallback != composition::Visibility::hidden);
}

auto TabView::Content::order(bool sidebar) const -> std::vector<std::size_t> {
  std::vector<std::size_t> arranged;
  if (customization_.has_value()) {
    std::vector<std::string> ids;
    ids.reserve(entries_.size());
    for (const Entry& entry : entries_) {
      ids.push_back(entry.role.has_value() ||
                            entry.behavior == TabCustomizationBehavior::disabled
                        ? std::string{}
                        : entry.customization);
    }
    arranged = customization_->get().ordered(ids);
  } else {
    arranged.reserve(entries_.size());
    for (std::size_t index = 0; index < entries_.size(); ++index) {
      arranged.push_back(index);
    }
  }
  arrange_sections(arranged);
  std::erase_if(arranged, [this, sidebar](std::size_t index) {
    return !visible(index, sidebar);
  });
  std::vector<std::size_t> searching;
  std::vector<std::size_t> rest;
  for (const std::size_t index : arranged) {
    (entries_[index].role == TabRole::search ? searching : rest)
        .push_back(index);
  }
  std::vector<std::size_t> ordered = sidebar ? searching : rest;
  for (const std::size_t index : sidebar ? rest : searching) {
    ordered.push_back(index);
  }
  return ordered;
}

auto TabView::Content::items(
    std::span<const std::size_t> order, std::size_t shown, const Choose& choose,
    bool sidebar, bool rail,
    composition::StateHandle<std::vector<composition::Rect>> frames,
    std::optional<composition::FocusBinding<std::size_t>> focus) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> built;
  built.reserve(order.size());
  for (const std::size_t index : order) {
    const Entry& entry = entries_[index];
    built.emplace_back(Item{
        Item::Inputs{
            .index = index,
            .shown = shown,
            .value = entry.value,
            .style = ItemStyle{.selected = index == shown,
                               .sidebar = sidebar,
                               .emphasized =
                                   index == shown &&
                                   (!sidebar || (focus.has_value() &&
                                                 focus->get().has_value())),
                               .rail = rail,
                               .title = entry.title,
                               .badge = entry.badge},
            .system_image = entry.system_image,
            .traits = entry.traits,
            .label = entry.label,
            .decorations = entry.decorations,
            .frames = frames,
            .focus = focus},
        choose});
  }
  return built;
}

auto TabView::Item::follows(const Item& other) const noexcept -> bool {
  const bool values =
      inputs_.value == other.inputs_.value ||
      (inputs_.value != nullptr && other.inputs_.value != nullptr &&
       inputs_.value->equals(*other.inputs_.value));
  const bool focused =
      inputs_.focus.has_value() == other.inputs_.focus.has_value() &&
      (!inputs_.focus.has_value() ||
       inputs_.focus->identity() == other.inputs_.focus->identity());
  return values && focused &&
         inputs_.frames.identity() == other.inputs_.frames.identity();
}

auto TabView::Item::operator==(const Item& other) const -> bool {
  return inputs_.index == other.inputs_.index &&
         inputs_.style == other.inputs_.style &&
         inputs_.system_image == other.inputs_.system_image &&
         inputs_.traits == other.inputs_.traits &&
         inputs_.label == other.inputs_.label &&
         inputs_.decorations == other.inputs_.decorations && follows(other);
}

auto TabView::Item::body(composition::Context&) const -> composition::Element {
  const std::size_t index = inputs_.index;
  const std::size_t shown = inputs_.shown;
  const Choose& choose = choose_;
  const bool sidebar = inputs_.style.sidebar;
  composition::Element item =
      components::Button([choose, index] { (*choose)(index, true); }, face())
          .button_style(inputs_.style)
          .frame(sidebar ? composition::FrameSpec{.max_width =
                                                      composition::infinity}
                         : composition::FrameSpec{})
          .on_geometry_change(
              std::type_identity<composition::Rect>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.frame(
                    composition::CoordinateSpace::named(bar_space));
              },
              [frames = inputs_.frames, index](composition::Rect frame) {
                const std::vector<composition::Rect>& known = frames.get();
                if (index < known.size() && known[index] == frame) {
                  return;
                }
                frames.update(
                    [index, frame](std::vector<composition::Rect>& kept) {
                      if (kept.size() <= index) {
                        kept.resize(index + 1);
                      }
                      kept[index] = frame;
                    });
              })
          .id(item_key(index));
  if (inputs_.focus.has_value()) {
    item = std::move(item).focused(std::make_unique<composition::FocusFollow>(
        composition::FocusRequest::of(*inputs_.focus, index),
        [choose, index, shown] {
          if (index != shown) {
            (*choose)(index, false);
          }
        }));
  }
  return dressed(std::move(item));
}

auto TabView::Item::face() const -> composition::Element {
  if (inputs_.label != nullptr && *inputs_.label) {
    return (*inputs_.label)();
  }
  switch (inputs_.traits.icon) {
    case TabTraits::Icon::asset:
      return components::Label(inputs_.style.title,
                               components::Image(inputs_.system_image));
    case TabTraits::Icon::none:
      if (inputs_.style.title.empty()) {
        return components::ZStack()
            .frame({.width = dot_extent, .height = dot_extent})
            .background(theme::ColourRole::label, theme::Shape::capsule())
            .padding(theme::space::xs);
      }
      return components::Text(text::LocalizedStringKey{inputs_.style.title});
    case TabTraits::Icon::system:
      break;
  }
  return components::Label(inputs_.style.title,
                           components::Image::system_name(inputs_.system_image)
                               .font(theme::Font::body));
}

auto TabView::Item::dressed(composition::Element item) const
    -> composition::Element {
  const TabTraits& traits = inputs_.traits;
  if (traits.disabled) {
    item = std::move(item).disabled(true);
  }
  if (traits.help.has_value()) {
    item = std::move(item).help(*traits.help);
  }
  if (traits.spring_loading.has_value()) {
    item = std::move(item).spring_loading_behavior(*traits.spring_loading);
  }
  if (traits.accessibility_label.has_value()) {
    item = std::move(item).accessibility_label(*traits.accessibility_label);
  }
  if (traits.accessibility_hint.has_value()) {
    item = std::move(item).accessibility_hint(*traits.accessibility_hint);
  }
  if (traits.accessibility_value.has_value()) {
    item = std::move(item).accessibility_value(*traits.accessibility_value);
  }
  if (traits.accessibility_identifier.has_value()) {
    item = std::move(item).accessibility_identifier(
        *traits.accessibility_identifier);
  }
  if (!traits.accessibility_input_labels.empty()) {
    item = std::move(item).accessibility_input_labels(
        traits.accessibility_input_labels);
  }
  for (const Tab::Decoration& decoration : inputs_.decorations) {
    if (decoration != nullptr && *decoration) {
      item = (*decoration)(std::move(item));
    }
  }
  return item;
}

auto TabView::Content::bar(
    std::vector<composition::Element> items, std::size_t shown, bool sidebar,
    bool emphasized, std::optional<composition::Element> pill,
    composition::StateHandle<std::vector<composition::Rect>> frames,
    composition::StateHandle<composition::Rect> origin)
    -> composition::Element {
  const std::vector<composition::Rect>& known = frames.get();
  const composition::Rect home = origin.get();
  const composition::Rect target =
      shown < known.size() ? known[shown] : composition::Rect{};
  composition::Element capsule =
      components::ZStack()
          .frame({.width = target.width, .height = target.height})
          .background(
              emphasized ? theme::ColourRole::selected_content_background
                         : theme::ColourRole::selected_control,
              theme::Shape::rounded(
                  sidebar ? theme::metrics::list.sidebar_selection_radius
                          : theme::Theme::Radii{}.control,
                  true))
          .offset(target.x - home.x, target.y - home.y)
          .allows_hit_testing(false)
          .animation(motion::Animation::bouncy(0.45, 0.15), shown)
          .id(capsule_id);
  composition::Element row =
      sidebar ? composition::Element{components::VStack(
                    components::VStackOptions{
                        .alignment = components::HorizontalAlignment::leading,
                        .spacing = theme::space::none},
                    std::move(items))}
              : composition::Element{components::HStack(
                    components::HStackOptions{
                        .alignment = components::VerticalAlignment::center,
                        .spacing = theme::space::xs},
                    std::move(items))};
  if (sidebar) {
    row = std::move(row).frame({.max_width = composition::infinity});
  }
  std::optional<composition::Element> label;
  if (pill.has_value()) {
    label =
        std::move(*pill)
            .offset(
                target.x - home.x + theme::metrics::list.sidebar_rail_width -
                    theme::metrics::list.sidebar_selection_inset,
                target.y - home.y +
                    ((target.height - theme::metrics::list.sidebar_row_height) *
                     0.5F))
            .animation(motion::Animation::snappy(0.25), shown)
            .id(rail_label_id);
  }
  composition::Element stacked = std::move(row).background(
      composition::Alignment::top_leading, std::move(capsule));
  if (label.has_value()) {
    stacked = std::move(stacked).overlay(composition::Alignment::top_leading,
                                         std::move(*label));
  }
  return std::move(stacked)
      .frame(sidebar
                 ? composition::FrameSpec{.max_width = composition::infinity}
                 : composition::FrameSpec{})
      .on_geometry_change(
          std::type_identity<composition::Rect>{},
          [](const composition::GeometryProxy& proxy) {
            return proxy.frame(composition::CoordinateSpace::named(bar_space));
          },
          [origin](composition::Rect frame) {
            if (origin.get() != frame) {
              origin.set(frame);
            }
          })
      .id(bar_id);
}

auto TabView::Content::customizable(std::size_t index) const -> bool {
  const Entry& entry = entries_[index];
  return !entry.customization.empty() && !entry.role.has_value() &&
         entry.behavior != TabCustomizationBehavior::disabled &&
         entry.traits.sidebar_behavior != TabCustomizationBehavior::disabled &&
         entry.traits.placement != TabPlacement::pinned;
}

auto TabView::Content::visibility_rows() const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> rows;
  if (!customization_.has_value()) {
    return rows;
  }
  const composition::Binding<TabViewCustomization> binding = *customization_;
  const TabViewCustomization current = binding.get();
  for (std::size_t index = 0; index < entries_.size(); ++index) {
    if (!customizable(index)) {
      continue;
    }
    const Entry& entry = entries_[index];
    const bool hidden = current.sidebar_visibility(entry.customization) ==
                        composition::Visibility::hidden;
    rows.push_back(
        components::Button(
            [binding, id = entry.customization, hidden] {
              binding.update([&id, hidden](TabViewCustomization& held) {
                held.set_sidebar_visibility(
                    id, hidden ? composition::Visibility::visible
                               : composition::Visibility::hidden);
              });
            },
            components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::space::sm},
                components::Image::system_name(hidden ? "radio_button_unchecked"
                                                      : "check_circle")
                    .foreground_style(
                        hidden ? theme::ColourValue{theme::ColourRole::
                                                        tertiary_label}
                               : theme::ColourValue{theme::ColourRole::accent}),
                components::Label(entry.title, components::Image::system_name(
                                                   entry.system_image)),
                components::Spacer())
                .frame({.max_width = composition::infinity,
                        .min_height = theme::metric::sidebar_row_height})
                .padding(theme::Edge::horizontal,
                         theme::Space{theme::metric::sidebar_selection_inset})
                .opacity(hidden ? 0.6F : 1.0F))
            .button_style(components::ButtonStyle::plain())
            .frame({.max_width = composition::infinity})
            .id(composition::ElementKey{
                std::format("arisa.tab.visibility.{}", entry.customization)}));
  }
  return rows;
}

auto TabView::Content::edit_toggle(composition::StateHandle<bool> editing)
    -> composition::Element {
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center},
             components::Spacer(),
             components::Button(std::string{editing.get() ? sidebar_done_label
                                                          : sidebar_edit_label},
                                [editing] { editing.set(!editing.get()); })
                 .button_style(components::ButtonStyle::borderless())
                 .id("arisa.tab.sidebar-edit"))
      .frame({.max_width = composition::infinity})
      .padding(theme::Edge::top, theme::space::sm);
}

const composition::StateKey<std::vector<std::size_t>>
    TabView::Content::toggled_sections{"tab-view-toggled-sections", {}};

void TabView::Content::arrange_sections(
    std::vector<std::size_t>& arranged) const {
  if (!customization_.has_value()) {
    return;
  }
  const TabViewCustomization current = customization_->get();
  for (std::size_t section = 0; section < headings_.size(); ++section) {
    const std::string& id = headings_[section].customization;
    if (id.empty()) {
      continue;
    }
    const std::optional<std::vector<std::string>> wanted =
        current.section_order(id);
    if (!wanted.has_value()) {
      continue;
    }
    std::vector<std::size_t> slots;
    std::vector<std::size_t> members;
    for (std::size_t slot = 0; slot < arranged.size(); ++slot) {
      if (entries_[arranged[slot]].section == section) {
        slots.push_back(slot);
        members.push_back(arranged[slot]);
      }
    }
    std::vector<std::size_t> sorted;
    sorted.reserve(members.size());
    for (const std::string& tab : *wanted) {
      const auto found =
          std::ranges::find_if(members, [this, &tab](std::size_t index) {
            return entries_[index].customization == tab;
          });
      if (found != members.end() && !std::ranges::contains(sorted, *found)) {
        sorted.push_back(*found);
      }
    }
    for (const std::size_t index : members) {
      if (!std::ranges::contains(sorted, index)) {
        sorted.push_back(index);
      }
    }
    for (std::size_t at = 0; at < slots.size(); ++at) {
      arranged[slots[at]] = sorted[at];
    }
  }
}

auto TabView::Content::collapsible(std::size_t section) const noexcept -> bool {
  return section < headings_.size() &&
         headings_[section].expansion != TabSectionExpansion::automatic;
}

auto TabView::Content::section_expanded(
    std::size_t section, const std::vector<std::size_t>& toggled) const noexcept
    -> bool {
  if (!collapsible(section)) {
    return true;
  }
  const bool initially =
      headings_[section].expansion != TabSectionExpansion::collapsed;
  return std::ranges::contains(toggled, section) ? !initially : initially;
}

auto TabView::Content::swiped(composition::Element item, const Entry& entry)
    -> composition::Element {
  if (!entry.leading.content && !entry.trailing.content) {
    return item;
  }
  return SwipeRow(std::move(item), entry.leading, entry.trailing);
}

auto TabView::Content::section_heading(
    std::size_t section,
    composition::StateHandle<std::vector<std::size_t>> toggled) const
    -> composition::Element {
  const SectionHeading* heading =
      section < headings_.size() ? &headings_[section] : nullptr;
  composition::Element label =
      heading != nullptr && heading->header != nullptr
          ? composition::Element{composition::PlaceholderContentView(
                heading->header)}
          : composition::Element{components::Text(sections_[section])};
  std::vector<composition::Element> line;
  line.push_back(std::move(label)
                     .font(theme::Font::subheadline)
                     .font_weight(theme::Font::Weight::bold)
                     .foreground_style(theme::ColourRole::text_secondary));
  const bool toggles = collapsible(section);
  const bool open = section_expanded(section, toggled.get());
  if (toggles) {
    line.push_back(components::Spacer());
    line.push_back(
        components::Glyph::chevron_right(theme::ColourRole::text_secondary,
                                         section_chevron_extent)
            .rotation_effect(motion::Angle::degrees(open ? 90.0 : 0.0))
            .animation(motion::Animation::snappy(0.3), open)
            .id(composition::ElementKey{
                std::format("arisa.tab.section-chevron.{}", section)}));
  }
  composition::Element heading_row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::sm},
          std::move(line))
          .padding(theme::Insets{
              .top = theme::space::md,
              .leading = theme::Space{theme::metric::sidebar_selection_inset},
              .bottom = theme::space::xs,
              .trailing =
                  toggles ? theme::Space{theme::metric::sidebar_selection_inset}
                          : theme::space::none});
  if (toggles) {
    heading_row =
        components::Button(
            [toggled, section] {
              motion::with_animation(
                  motion::Animation::snappy(0.3), [&toggled, section] {
                    toggled.update([section](std::vector<std::size_t>& held) {
                      if (std::erase(held, section) == 0) {
                        held.push_back(section);
                      }
                    });
                  });
            },
            std::move(heading_row).frame({.max_width = composition::infinity}))
            .button_style(components::ButtonStyle::plain());
  }
  return std::move(heading_row)
      .id(composition::ElementKey{
          std::format("arisa.tab.section.{}", section)});
}

auto TabView::Content::sidebar_rows(
    std::span<const std::size_t> order, std::vector<composition::Element> items,
    bool rail, composition::StateHandle<std::vector<std::size_t>> toggled) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> rows;
  rows.reserve(items.size() + sections_.size());
  std::optional<std::size_t> current;
  const auto movable = [this](std::size_t index) {
    return customization_.has_value() && customizable(index);
  };
  std::vector<std::vector<std::size_t>> groups;
  for (const std::size_t index : order) {
    if (groups.empty() || entries_[index].section != current ||
        movable(index) != movable(groups.back().front())) {
      groups.emplace_back();
      current = entries_[index].section;
    }
    groups.back().push_back(index);
  }
  std::size_t at = 0;
  std::optional<std::size_t> titled;
  for (std::size_t position = 0; position < groups.size(); ++position) {
    const std::vector<std::size_t>& group = groups[position];
    const std::optional<std::size_t> section = entries_[group.front()].section;
    if (section.has_value() && section != titled && !rail &&
        *section < sections_.size()) {
      rows.push_back(section_heading(*section, toggled));
    }
    const bool hidden_section = section.has_value() && !rail &&
                                !section_expanded(*section, toggled.get());
    titled = section;
    const bool reorders = movable(group.front()) && group.size() > 1;
    std::vector<std::string> ids;
    ids.reserve(entries_.size());
    for (const Entry& entry : entries_) {
      ids.push_back(entry.customization);
    }
    const std::vector<std::string> customs = ids;
    const components::ForEach::MoveAction move =
        reorders
            ? components::ForEach::MoveAction{[binding = *customization_, group,
                                               shown = std::vector<std::size_t>(
                                                   order.begin(), order.end()),
                                               ids = std::move(ids)](
                                                  const components::IndexSet&
                                                      from,
                                                  std::size_t to) {
                std::vector<std::size_t> picked;
                std::vector<std::size_t> kept;
                std::size_t before = 0;
                for (std::size_t slot = 0; slot < group.size(); ++slot) {
                  if (from.contains(slot)) {
                    picked.push_back(group[slot]);
                  } else {
                    kept.push_back(group[slot]);
                    if (slot < to) {
                      ++before;
                    }
                  }
                }
                kept.insert(kept.begin() + static_cast<std::ptrdiff_t>(
                                               std::min(before, kept.size())),
                            picked.begin(), picked.end());
                std::vector<std::string> arranged;
                arranged.reserve(shown.size());
                bool placed = false;
                for (const std::size_t index : shown) {
                  if (std::ranges::contains(group, index)) {
                    if (!placed) {
                      for (const std::size_t each : kept) {
                        arranged.push_back(ids[each]);
                      }
                      placed = true;
                    }
                    continue;
                  }
                  if (!ids[index].empty()) {
                    arranged.push_back(ids[index]);
                  }
                }
                binding.update([&arranged](TabViewCustomization& held) {
                  held.reorder(std::move(arranged));
                });
              }}
            : components::ForEach::MoveAction{};
    const std::string section_id =
        section.has_value() && *section < headings_.size()
            ? headings_[*section].customization
            : std::string{};
    const components::ForEach::MoveAction placed =
        reorders && !section_id.empty()
            ? components::ForEach::MoveAction{[binding = *customization_, group,
                                               section_id, customs](
                                                  const components::IndexSet&
                                                      from,
                                                  std::size_t to) {
                std::vector<std::size_t> picked;
                std::vector<std::size_t> kept;
                std::size_t before = 0;
                for (std::size_t slot = 0; slot < group.size(); ++slot) {
                  if (from.contains(slot)) {
                    picked.push_back(group[slot]);
                  } else {
                    kept.push_back(group[slot]);
                    if (slot < to) {
                      ++before;
                    }
                  }
                }
                kept.insert(kept.begin() + static_cast<std::ptrdiff_t>(
                                               std::min(before, kept.size())),
                            picked.begin(), picked.end());
                std::vector<std::string> arranged;
                arranged.reserve(kept.size());
                for (const std::size_t index : kept) {
                  arranged.push_back(customs[index]);
                }
                binding.update(
                    [&arranged, &section_id](TabViewCustomization& held) {
                      held.reorder_section(section_id, std::move(arranged));
                    });
              }}
            : move;
    for (std::size_t offset = 0; offset < group.size(); ++offset) {
      composition::Element item = std::move(items[at++]);
      if (hidden_section) {
        continue;
      }
      if (reorders) {
        item = components::ReorderCell(std::move(item), offset, group.size(),
                                       placed);
      }
      if (!rail) {
        item = swiped(std::move(item), entries_[group[offset]]);
      }
      rows.push_back(std::move(item));
    }
    const bool closes =
        position + 1 == groups.size() ||
        entries_[groups[position + 1].front()].section != section;
    if (closes && !rail && section.has_value() && *section < actions_.size() &&
        actions_[*section] != nullptr && *actions_[*section]) {
      rows.push_back(
          (*actions_[*section])()
              .button_style(components::ButtonStyle::borderless())
              .padding(theme::Edge::horizontal,
                       theme::Space{theme::metric::sidebar_selection_inset})
              .id(composition::ElementKey{
                  std::format("arisa.tab.section-actions.{}", *section)}));
    }
  }
  return rows;
}

auto TabView::Content::dots(std::size_t count, std::size_t shown,
                            const Choose& choose, bool backed)
    -> composition::Element {
  std::vector<composition::Element> marks;
  marks.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    marks.push_back(
        components::ZStack()
            .frame({.width = dot_extent, .height = dot_extent})
            .background(
                index == shown
                    ? theme::ColourValue{theme::ColourRole::label}
                    : theme::ColourValue{theme::ColourRole::label}.opacity(
                          0.3F),
                theme::Shape::capsule())
            .content_shape(theme::Rectangle())
            .on_tap_gesture([choose, index] { (*choose)(index, true); })
            .id(composition::ElementKey{static_cast<std::int64_t>(index)}));
  }
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::Space{dot_spacing}},
          std::move(marks))
          .padding(
              theme::Insets::symmetric(theme::space::sm, theme::space::xs));
  if (backed) {
    row = std::move(row).background(
        theme::ColourValue{theme::ColourRole::label}.opacity(index_backing),
        theme::Shape::capsule());
  }
  return std::move(row)
      .animation(motion::Animation::snappy(0.25), shown)
      .id(dots_id);
}

auto TabView::Content::paged(composition::Context& context, std::size_t shown,
                             const Choose& choose) const
    -> composition::Element {
  const TabViewStyle& style = context.environment(TabViewStyle::key);
  const std::size_t total = entries_.size();
  const composition::StateHandle<float> drag = context.state(page_drag);
  const float dragged = drag.get();
  std::vector<std::shared_ptr<const Tab::Builder>> builders;
  builders.reserve(total);
  for (const Entry& entry : entries_) {
    builders.push_back(entry.content);
  }
  const composition::Namespace::ID scope = context.namespace_id(pager);
  composition::Element pages = components::GeometryReader(
      [builders = std::move(builders), shown, dragged,
       scope](const composition::GeometryProxy& proxy) {
        const float width = proxy.size().width;
        std::vector<composition::Element> shown_pages;
        const std::size_t first = shown == 0 ? 0 : shown - 1;
        const std::size_t last = std::min(shown + 1, builders.size() - 1);
        for (std::size_t index = first; index <= last; ++index) {
          const float place =
              (static_cast<float>(index) - static_cast<float>(shown)) * width;
          composition::Element placed{
              Pane{builders[index], false, index != shown}};
          if (index == shown) {
            placed = std::move(placed).focus_scope(
                scope, composition::FocusScopeBehavior::confined);
          }
          shown_pages.push_back(
              std::move(placed)
                  .frame({.width = width,
                          .max_height = composition::infinity,
                          .alignment = composition::Alignment::top_leading})
                  .offset(place + dragged, 0.0F)
                  .animation(motion::Animation::snappy(0.35), shown)
                  .id(composition::ElementKey{
                      static_cast<std::int64_t>(index)}));
        }
        return composition::Element{
            components::ZStack(
                components::ZStackOptions{
                    .alignment = components::Alignment::top_leading},
                std::move(shown_pages))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::top_leading})};
      });
  const auto step = [choose, shown, total](bool forward) {
    if (forward && shown + 1 < total) {
      (*choose)(shown + 1, true);
    } else if (!forward && shown > 0) {
      (*choose)(shown - 1, true);
    }
  };
  pages =
      std::move(pages)
          .clip_shape(theme::Rectangle())
          .content_shape(theme::Rectangle())
          .gesture(
              composition::DragGesture(
                  composition::DragGesture::default_distance,
                  composition::CoordinateSpace::local)
                  .on_changed(
                      [drag](const composition::DragGesture::Value& value) {
                        drag.set(value.translation.width);
                      })
                  .on_ended([drag, step](
                                const composition::DragGesture::Value& value) {
                    const float moved = value.predicted_end_translation.width;
                    const float edge = 80.0F;
                    if (moved < -edge || value.translation.width < -edge) {
                      step(true);
                    } else if (moved > edge || value.translation.width > edge) {
                      step(false);
                    }
                    motion::with_animation(motion::Animation::snappy(0.35),
                                           [&drag] { drag.set(0.0F); });
                  }))
          .on_focus_edge(composition::MoveCommandDirection::right,
                         [step] { step(true); })
          .on_focus_edge(composition::MoveCommandDirection::left,
                         [step] { step(false); })
          .focus_fallback()
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .id(page_id);
  const bool marked =
      style.index_display_mode() == TabViewStyle::IndexDisplayMode::always ||
      (style.index_display_mode() ==
           TabViewStyle::IndexDisplayMode::automatic &&
       total > 1);
  if (marked) {
    const IndexViewStyle& index_style =
        context.environment(IndexViewStyle::key);
    pages = std::move(pages).overlay(
        composition::Alignment::bottom,
        dots(total, shown, choose, index_style.backs(dragged != 0.0F))
            .padding(theme::Edge::bottom, theme::space::md));
  }
  return pages;
}

auto TabView::Content::accessory(composition::Context& context,
                                 bool minimized) const
    -> std::optional<composition::Element> {
  const SidebarContent& built = context.environment(bottom_accessory);
  if (built == nullptr || !*built) {
    return std::nullopt;
  }
  const TabViewBottomAccessoryPlacement placement =
      minimized ? TabViewBottomAccessoryPlacement::inline_placement
                : TabViewBottomAccessoryPlacement::expanded;
  composition::Element shown =
      (*built)()
          .padding(theme::Insets::symmetric(theme::space::md, theme::space::xs))
          .default_glass(theme::GlassSlot::toolbar, theme::Shape::capsule())
          .environment(
              accessory_placement,
              std::optional<TabViewBottomAccessoryPlacement>{placement})
          .focus_section()
          .id(accessory_id);
  if (!minimized) {
    shown = std::move(shown).frame({.max_width = composition::infinity});
  }
  return shown;
}

auto TabView::Content::top_bar(
    composition::Context& context, std::span<const std::size_t> arranged,
    std::size_t shown, const Choose& choose, bool minimized,
    std::optional<composition::Element> inline_accessory) const
    -> std::optional<composition::Element> {
  const auto frames = context.state(item_frames);
  const auto origin = context.state(bar_frame);
  std::vector<std::size_t> regular;
  std::vector<std::size_t> searching;
  std::optional<std::size_t> prominent;
  for (const std::size_t index : arranged) {
    if (entries_[index].role == TabRole::search) {
      searching.push_back(index);
    } else if (entries_[index].role == TabRole::prominent &&
               !prominent.has_value()) {
      prominent = index;
    } else if (!minimized || index == shown) {
      regular.push_back(index);
    }
  }
  std::ranges::stable_partition(regular, [this](std::size_t index) {
    return entries_[index].traits.placement == TabPlacement::pinned;
  });
  const composition::NavigationChrome::Bar bar_style =
      context.state(tab_bar_style).get();
  std::vector<composition::Element> row;
  row.push_back(
      components::HStack(
          bar(items(regular, shown, choose, false, false, frames, std::nullopt),
              shown, false, true, std::nullopt, frames, origin))
          .padding(theme::space::xs)
          .background(bar_style.background.value_or(
                          theme::ColourValue{theme::ColourRole::surface}),
                      theme::Shape::capsule())
          .focus_section()
          .id("arisa.tab.bar"));
  if (prominent.has_value() && !minimized) {
    std::vector<composition::Element> made =
        items(std::span<const std::size_t>{&*prominent, 1}, shown, choose,
              false, false, frames, std::nullopt);
    row.push_back(composition::Element{components::HStack(std::move(made))
                                           .padding(theme::space::xs)
                                           .focus_section()
                                           .id(prominent_id)});
  }
  if (!searching.empty() && !minimized) {
    row.push_back(
        components::HStack(
            items(searching, shown, choose, false, false, frames, std::nullopt))
            .padding(theme::space::xs)
            .background(theme::ColourRole::surface, theme::Shape::capsule())
            .focus_section()
            .id(search_id));
  }
  if (inline_accessory.has_value()) {
    row.push_back(std::move(*inline_accessory));
  }
  composition::Element tabs =
      row.size() == 1
          ? std::move(row.front())
          : composition::Element{components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::space::sm},
                std::move(row))};
  tabs = std::move(tabs).coordinate_space(bar_space);
  composition::NavigationChrome::Bar inked = bar_style;
  inked.background.reset();
  if (bar_style.hidden()) {
    return std::nullopt;
  }
  return presentation::ToolbarLayout::styled(std::move(tabs), inked);
}

auto TabView::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (entries_.empty()) {
    return produced;
  }
  const TabViewStyle& style = context.environment(TabViewStyle::key);
  const std::optional<AdaptableTabBarPlacement> fixed =
      context.environment(default_placement);
  const bool sidebar = style.kind() == TabViewStyle::Kind::sidebar &&
                       fixed != AdaptableTabBarPlacement::tab_bar;
  const bool paging = style.kind() == TabViewStyle::Kind::page;
  const std::size_t shown = selected(context);
  const auto index_state = context.state(chosen);
  const auto frames = context.state(item_frames);
  const auto origin = context.state(bar_frame);
  const auto earlier = context.state(previous);
  const std::optional<Selection> selection = selection_;
  const std::vector<std::shared_ptr<const composition::TagValue>> values =
      [this] {
        std::vector<std::shared_ptr<const composition::TagValue>> collected;
        for (const Entry& entry : entries_) {
          collected.push_back(entry.value);
        }
        return collected;
      }();
  std::vector<bool> searches;
  searches.reserve(entries_.size());
  for (const Entry& entry : entries_) {
    searches.push_back(entry.role == TabRole::search);
  }
  const composition::FocusBinding<std::size_t> focus =
      context.state(TabView::sidebar_focus);
  const bool sidebar_focused = sidebar && focus.get().has_value();
  const DetailPresenter presenter{
      context, DetailPresenter::Request{.shown = composition::TagKey::of(shown),
                                        .following = sidebar_focused,
                                        .delivered = true}};
  const DetailPresenter::Commit commit = presenter.commit();
  const Choose choose =
      std::make_shared<const std::function<void(std::size_t, bool)>>(
          [selection, values, index_state, commit, earlier, shown, searches](
              std::size_t index, bool now) {
            if (index != shown && shown < searches.size() && !searches[shown]) {
              earlier.set(shown);
            }
            motion::with_animation(motion::Animation::snappy(0.35), [&] {
              if (selection.has_value() && values[index] != nullptr) {
                selection->select(*values[index]);
              } else {
                index_state.set(index);
              }
            });
            if (now) {
              commit(composition::TagKey::of(index));
            }
          });
  const std::vector<std::size_t> arranged = order(sidebar);
  const bool searching_tab =
      shown < entries_.size() && entries_[shown].role == TabRole::search;
  const auto activated = [&](composition::Element page) {
    if (!searching_tab || context.environment(search_activation) !=
                              TabSearchActivation::search_tab_selection) {
      return page;
    }
    return std::move(page).environment(
        composition::SearchEnvironment::activation,
        composition::SearchEnvironment::Activation{
            earlier.identity(), [choose, earlier, total = entries_.size()] {
              const std::size_t back = earlier.get();
              if (back < total) {
                (*choose)(back, true);
              }
            }});
  };
  if (paging) {
    produced.push_back(
        paged(context, shown, choose)
            .environment(
                bar_placement,
                std::optional<TabBarPlacement>{TabBarPlacement::page_indicator})
            .on_command(provider::Button::l, "Previous",
                        [choose, shown] {
                          if (shown > 0) {
                            (*choose)(shown - 1, true);
                          }
                        })
            .on_command(provider::Button::r, "Next",
                        [choose, shown, total = entries_.size()] {
                          if (shown + 1 < total) {
                            (*choose)(shown + 1, true);
                          }
                        }));
    return produced;
  }
  const TabBarMinimizeBehavior minimizing =
      context.environment(minimize_behavior);
  const auto minimized_state = context.state(bar_minimized);
  const bool compact_height =
      context.environment(components::Environment::vertical_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const composition::ToolbarVerticalCompressionBehavior squeeze =
      context.state(compression).get();
  const bool squeezes_tabs =
      compact_height && squeeze ==
                            composition::ToolbarVerticalCompressionBehavior::
                                prefers_toolbar_items;
  const bool squeezes_toolbars = compact_height && !squeezes_tabs;
  const bool minimized =
      squeezes_tabs || ((minimizing == TabBarMinimizeBehavior::on_scroll_down ||
                         minimizing == TabBarMinimizeBehavior::on_scroll_up) &&
                        minimized_state.get());
  std::shared_ptr<composition::ScrollGeometryObserver> observer;
  if (minimizing == TabBarMinimizeBehavior::on_scroll_down ||
      minimizing == TabBarMinimizeBehavior::on_scroll_up) {
    observer = std::make_shared<Minimizer>(minimized_state, minimizing);
  }
  composition::Element page =
      presenter.stack([this, observer](const DetailPresenter::Slot& slot) {
        return pane(slot, observer);
      });
  page = components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading},
             std::move(page))
             .frame({.max_width = composition::infinity,
                     .min_height = 0.0F,
                     .max_height = composition::infinity,
                     .alignment = composition::Alignment::leading})
             .flex_shrink(1.0F)
             .focus_section()
             .id(page_id);
  page = activated(std::move(page));
  if (squeezes_toolbars) {
    page = std::move(page).environment(
        composition::ToolbarEnvironment::compressed, true);
  }
  composition::Element layout = components::ZStack();
  if (sidebar) {
    const NavigationSplitView::Binding shown_columns = visibility(context);
    const composition::NavigationSplitViewVisibility resolved_columns =
        NavigationSplitView::resolved(
            shown_columns.get(), context.environment(SidebarCollapse::key),
            context.environment(
                composition::ToolbarRemoval::sidebar_toggle_removed));
    const bool rail = resolved_columns ==
                      composition::NavigationSplitViewVisibility::icons_only;
    const bool collapsed =
        resolved_columns ==
            composition::NavigationSplitViewVisibility::detail_only &&
        context.environment(adaptable_placement) ==
            AdaptableTabBarPlacement::tab_bar;
    const std::size_t total = entries_.size();
    const auto editing = context.state(editing_sidebar);
    const bool customizing =
        customization_.has_value() && !rail && editing.get();
    composition::Element column = components::Group();
    if (customizing) {
      column = components::VStack(
                   components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::none},
                   visibility_rows())
                   .frame({.max_width = composition::infinity})
                   .id("arisa.tab.sidebar-visibility");
    } else {
      std::vector<composition::Element> built =
          items(arranged, shown, choose, true, rail, frames, focus);
      const bool plain = sections_.empty() && !customization_.has_value() &&
                         std::ranges::none_of(entries_, [](const Entry& entry) {
                           return static_cast<bool>(entry.leading.content) ||
                                  static_cast<bool>(entry.trailing.content);
                         });
      column = bar(plain ? std::move(built)
                         : sidebar_rows(arranged, std::move(built), rail,
                                        context.state(toggled_sections)),
                   shown, true, sidebar_focused,
                   rail && focus.get().has_value() && *focus.get() < total
                       ? std::optional<composition::Element>{RailLabel{
                             entries_[*focus.get()].title, *focus.get()}}
                       : std::nullopt,
                   frames, origin);
    }
    std::vector<composition::Element> stacked;
    const SidebarContent& header = context.environment(sidebar_header);
    const SidebarContent& footer = context.environment(sidebar_footer);
    const SidebarContent& tools = context.environment(sidebar_toolbar);
    if (tools != nullptr && *tools && !rail) {
      stacked.push_back(
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::center,
                  .spacing = theme::space::sm},
              components::Spacer(), (*tools)())
              .frame({.max_width = composition::infinity})
              .button_style(components::ButtonStyle::toolbar())
              .id(sidebar_toolbar_id));
    }
    if (header != nullptr && *header && !rail) {
      stacked.push_back((*header)().id("arisa.tab.sidebar-header"));
    }
    stacked.push_back(std::move(column));
    if (footer != nullptr && *footer && !rail) {
      stacked.push_back((*footer)().id("arisa.tab.sidebar-footer"));
    }
    if (customization_.has_value() && !rail) {
      stacked.push_back(edit_toggle(editing));
    }
    composition::Element scroller =
        components::ScrollView(
            components::VStack(
                components::VStackOptions{
                    .alignment = components::HorizontalAlignment::leading},
                std::move(stacked))
                .coordinate_space(bar_space)
                .padding(theme::Insets::symmetric(
                    theme::metric::sidebar_selection_inset, theme::space::sm))
                .frame({.max_width = composition::infinity}))
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity})
            .focus_section()
            .default_focus(
                focus, shown,
                composition::DefaultFocusEvaluationPriority::user_initiated)
            .on_change(focus.get(), [focus, choose, shown] {
              const std::optional<std::size_t> row = focus.get();
              if (row.has_value() && *row != shown) {
                (*choose)(*row, false);
              }
            });
    if (rail) {
      scroller = std::move(scroller).scroll_clip_disabled();
    }
    if (customization_.has_value()) {
      scroller = std::move(scroller).environment(
          components::ReorderCell::lanes,
          components::ReorderCell::Lanes{.axis = composition::Axis::vertical,
                                         .count = 1});
    }
    const SidebarContent& pinned_bar = context.environment(sidebar_bottom_bar);
    if (pinned_bar != nullptr && *pinned_bar && !rail) {
      scroller =
          components::VStack(
              components::VStackOptions{
                  .alignment = components::HorizontalAlignment::leading,
                  .spacing = theme::space::none},
              std::move(scroller),
              (*pinned_bar)()
                  .button_style(components::ButtonStyle::borderless())
                  .padding(theme::Insets::symmetric(
                      theme::metric::sidebar_selection_inset, theme::space::sm))
                  .frame({.max_width = composition::infinity})
                  .id(sidebar_bottom_id))
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity});
    }
    const bool sections = !sections_.empty() && !collapsed;
    page = std::move(page)
               .environment(bar_placement,
                            std::optional<TabBarPlacement>{
                                collapsed ? TabBarPlacement::top_bar
                                          : TabBarPlacement::sidebar})
               .environment(sidebar_available, true)
               .environment(showing_sections, sections);
    if (collapsed) {
      std::optional<composition::Element> expanded =
          minimized ? std::nullopt : accessory(context, false);
      std::optional<composition::Element> tabs =
          top_bar(context, order(false), shown, choose, minimized,
                  minimized ? accessory(context, true) : std::nullopt);
      if (expanded.has_value()) {
        page = std::move(page).overlay(
            composition::Alignment::bottom,
            std::move(*expanded).padding(theme::space::md));
      }
      page = components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::center,
                     .spacing = theme::space::md},
                 std::move(tabs), std::move(page))
                 .frame({.max_width = composition::infinity,
                         .max_height = composition::infinity});
    }
    layout = NavigationSplitView(
        shown_columns,
        std::move(scroller).focus_effect_disabled().id("arisa.tab.sidebar"),
        std::move(page));
  } else {
    std::optional<composition::Element> expanded =
        minimized ? std::nullopt : accessory(context, false);
    std::optional<composition::Element> shown_tabs =
        top_bar(context, arranged, shown, choose, minimized,
                minimized ? accessory(context, true) : std::nullopt);
    page = std::move(page)
               .environment(
                   bar_placement,
                   std::optional<TabBarPlacement>{TabBarPlacement::top_bar})
               .environment(showing_sections, false);
    if (expanded.has_value()) {
      page = std::move(page).overlay(
          composition::Alignment::bottom,
          std::move(*expanded).padding(theme::space::md));
    }
    layout = components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::center,
                     .spacing = theme::space::md},
                 std::move(shown_tabs), std::move(page))
                 .frame({.max_width = composition::infinity,
                         .max_height = composition::infinity});
  }
  const auto step = [choose, focus, shown, arranged](bool forward) {
    if (arranged.empty()) {
      return;
    }
    const auto at = std::ranges::find(arranged, shown);
    const std::size_t position =
        at == arranged.end() ? 0
                             : static_cast<std::size_t>(at - arranged.begin());
    const std::size_t count = arranged.size();
    const std::size_t next = arranged[forward ? (position + 1) % count
                                              : (position + count - 1) % count];
    (*choose)(next, true);
    if (focus.get().has_value()) {
      focus.set(next);
    }
  };
  layout =
      std::move(layout)
          .on_command(provider::Button::l, "Previous", [step] { step(false); })
          .on_command(provider::Button::r, "Next", [step] { step(true); });
  produced.push_back(presenter.settling(std::move(layout)));
  return produced;
}

}
