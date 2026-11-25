#include <arisa/components/column_resizer.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/sidebar_toggle.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/theme.hpp>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::structure {

const composition::EnvironmentKey<SidebarCollapse> SidebarCollapse::key{
    "sidebar_collapse", SidebarCollapse{}};

const composition::FocusState<composition::TagKey>
    NavigationSplitView::sidebar_focus{"navigation-split-view-sidebar-focus"};

const composition::EnvironmentKey<NavigationSplitView::CustomStyle>
    NavigationSplitView::custom_style{"navigation_split_view_custom_style",
                                      nullptr};

const composition::StateKey<std::shared_ptr<SidebarSelection::Reader>>
    NavigationSplitView::reader{"navigation-split-view-reader", nullptr};

NavigationSplitView::NavigationSplitView(composition::Element sidebar,
                                         composition::Element detail)
    : NavigationSplitView(std::optional<Binding>{}, std::move(sidebar),
                          std::move(detail)) {}

NavigationSplitView::NavigationSplitView(
    composition::Binding<Visibility> column_visibility,
    composition::Element sidebar, composition::Element detail)
    : NavigationSplitView(
          Binding{
              .get = [column_visibility] { return column_visibility.get(); },
              .set = [column_visibility](
                         Visibility value) { column_visibility.set(value); }},
          std::move(sidebar), std::move(detail)) {}

NavigationSplitView::NavigationSplitView(
    std::optional<Binding> column_visibility, composition::Element sidebar,
    composition::Element detail)
    : NavigationSplitView(std::move(column_visibility), std::move(sidebar),
                          std::optional<composition::Element>{},
                          std::move(detail)) {}

NavigationSplitView::NavigationSplitView(composition::Element sidebar,
                                         composition::Element content,
                                         composition::Element detail)
    : NavigationSplitView(
          std::optional<Binding>{}, std::move(sidebar),
          std::optional<composition::Element>{std::move(content)},
          std::move(detail)) {}

NavigationSplitView::NavigationSplitView(
    composition::Binding<Visibility> column_visibility,
    composition::Element sidebar, composition::Element content,
    composition::Element detail)
    : NavigationSplitView(
          Binding{
              .get = [column_visibility] { return column_visibility.get(); },
              .set = [column_visibility](
                         Visibility value) { column_visibility.set(value); }},
          std::move(sidebar),
          std::optional<composition::Element>{std::move(content)},
          std::move(detail)) {}

NavigationSplitView::NavigationSplitView(
    std::optional<Binding> column_visibility, composition::Element sidebar,
    std::optional<composition::Element> content, composition::Element detail)
    : Element(std::make_unique<Content>(
          std::move(column_visibility),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(sidebar)),
          content.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*content))
              : nullptr,
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(detail)))) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

NavigationSplitView::NavigationSplitView(
    composition::Binding<composition::NavigationSplitViewColumn>
        preferred_compact_column,
    composition::Element sidebar, composition::Element detail)
    : NavigationSplitView(std::move(sidebar), std::move(detail)) {
  edit<Content>().prefer(std::move(preferred_compact_column));
}

NavigationSplitView::NavigationSplitView(
    composition::Binding<Visibility> column_visibility,
    composition::Binding<composition::NavigationSplitViewColumn>
        preferred_compact_column,
    composition::Element sidebar, composition::Element detail)
    : NavigationSplitView(std::move(column_visibility), std::move(sidebar),
                          std::move(detail)) {
  edit<Content>().prefer(std::move(preferred_compact_column));
}

NavigationSplitView::NavigationSplitView(
    composition::Binding<composition::NavigationSplitViewColumn>
        preferred_compact_column,
    composition::Element sidebar, composition::Element content,
    composition::Element detail)
    : NavigationSplitView(std::move(sidebar), std::move(content),
                          std::move(detail)) {
  edit<Content>().prefer(std::move(preferred_compact_column));
}

NavigationSplitView::NavigationSplitView(
    composition::Binding<Visibility> column_visibility,
    composition::Binding<composition::NavigationSplitViewColumn>
        preferred_compact_column,
    composition::Element sidebar, composition::Element content,
    composition::Element detail)
    : NavigationSplitView(std::move(column_visibility), std::move(sidebar),
                          std::move(content), std::move(detail)) {
  edit<Content>().prefer(std::move(preferred_compact_column));
}

auto NavigationSplitView::compact_visibility(
    composition::NavigationSplitViewColumn column, bool three) noexcept
    -> Visibility {
  switch (column) {
    case composition::NavigationSplitViewColumn::sidebar:
      return Visibility::all;
    case composition::NavigationSplitViewColumn::content:
      return three ? Visibility::double_column : Visibility::detail_only;
    case composition::NavigationSplitViewColumn::detail:
      break;
  }
  return Visibility::detail_only;
}

auto NavigationSplitView::compact_column(Visibility visibility,
                                         bool three) noexcept
    -> composition::NavigationSplitViewColumn {
  switch (visibility) {
    case Visibility::automatic:
    case Visibility::all:
    case Visibility::icons_only:
      return composition::NavigationSplitViewColumn::sidebar;
    case Visibility::double_column:
      return three ? composition::NavigationSplitViewColumn::content
                   : composition::NavigationSplitViewColumn::sidebar;
    case Visibility::detail_only:
      break;
  }
  return composition::NavigationSplitViewColumn::detail;
}

NavigationSplitView::Column::Column(composition::Element content,
                                    composition::StateHandle<Width> width,
                                    composition::Alignment alignment)
    : Element(std::make_unique<Content>(std::move(width))) {
  declared().relative_container = true;
  edit<Content>().hold(
      std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(content)
              .frame({.max_width = composition::infinity,
                      .min_height = 0.0F,
                      .max_height = composition::infinity,
                      .alignment = alignment})
              .flex_shrink(1.0F)));
}

auto NavigationSplitView::Column::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto NavigationSplitView::Column::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  produced.push_back(composition::PlaceholderContentView(content_));
  return produced;
}

auto NavigationSplitView::Column::Content::isolates(
    composition::Identity preference) const noexcept -> bool {
  return preference ==
         composition::NavigationDeclarations::column_width_key.identity();
}

void NavigationSplitView::Column::Content::resolve(
    composition::Context&, const composition::Node& node) {
  const Width found =
      node.preference(composition::NavigationDeclarations::column_width_key)
          .value_or(std::nullopt);
  if (declared_.get() != found) {
    declared_.set(found);
  }
}

auto NavigationSplitView::columns(
    Visibility visibility, bool compact,
    composition::NavigationSplitViewStyle style) noexcept -> Visibility {
  if (visibility != Visibility::automatic) {
    return visibility;
  }
  if (style == composition::NavigationSplitViewStyle::prominent_detail) {
    return Visibility::detail_only;
  }
  return compact ? Visibility::double_column : Visibility::all;
}

auto NavigationSplitView::next_column(Visibility visibility) noexcept
    -> Visibility {
  switch (visibility) {
    case Visibility::automatic:
    case Visibility::all:
    case Visibility::icons_only:
      return Visibility::double_column;
    case Visibility::double_column:
      return Visibility::detail_only;
    case Visibility::detail_only:
      return Visibility::all;
  }
  return Visibility::all;
}

auto NavigationSplitView::resolved(Visibility visibility, SidebarCollapse forms,
                                   bool fixed) noexcept -> Visibility {
  if (fixed) {
    return Visibility::all;
  }
  switch (visibility) {
    case Visibility::automatic:
    case Visibility::all:
    case Visibility::double_column:
      return Visibility::all;
    case Visibility::detail_only:
      if (forms.detail_only) {
        return Visibility::detail_only;
      }
      return forms.icons_only ? Visibility::icons_only : Visibility::all;
    case Visibility::icons_only:
      if (forms.icons_only) {
        return Visibility::icons_only;
      }
      return forms.detail_only ? Visibility::detail_only : Visibility::all;
  }
  return Visibility::all;
}

auto NavigationSplitView::next(Visibility visibility,
                               SidebarCollapse forms) noexcept -> Visibility {
  switch (resolved(visibility, forms, false)) {
    case Visibility::icons_only:
      return forms.detail_only ? Visibility::detail_only : Visibility::all;
    case Visibility::detail_only:
      return Visibility::all;
    default:
      break;
  }
  if (forms.icons_only) {
    return Visibility::icons_only;
  }
  return forms.detail_only ? Visibility::detail_only : Visibility::all;
}

auto NavigationSplitView::toggle(const Binding& binding, SidebarCollapse forms)
    -> void {
  motion::with_animation(motion::Animation::smooth(0.35), [&binding, forms] {
    binding.set(next(binding.get(), forms));
  });
}

auto NavigationSplitView::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto NavigationSplitView::Content::binding(composition::Context& context) const
    -> Binding {
  if (binding_.has_value()) {
    return *binding_;
  }
  const auto state = context.state(chosen);
  const Visibility fallback =
      content_ != nullptr
          ? Visibility::automatic
          : context.environment(components::Environment::sidebar_visibility);
  return Binding{
      .get = [state, fallback] { return state.get().value_or(fallback); },
      .set = [state](Visibility value) { state.set(value); }};
}

auto NavigationSplitView::DetailPath::entries() const
    -> std::vector<composition::NavigationPath::Entry> {
  return shown_.get().entries() |
         std::ranges::to<std::vector<composition::NavigationPath::Entry>>();
}

void NavigationSplitView::DetailPath::truncate(std::size_t count) const {
  const std::size_t current = shown_.get().count();
  if (count >= current) {
    return;
  }
  shown_.update([removed = current - count](composition::NavigationPath& path) {
    path.remove_last(removed);
  });
}

auto NavigationSplitView::DetailPath::push(
    const composition::NavigationPath::Entry& entry) const -> bool {
  if (entry.get<presentation::NavigationStack::ViewDestination>() == nullptr) {
    return false;
  }
  shown_.update([&entry](composition::NavigationPath& path) {
    path.clear();
    path.append(entry);
  });
  return true;
}

auto NavigationSplitView::Content::pages(
    const DetailPresenter& presenter,
    const composition::NavigationPath& linked) const -> composition::Element {
  const auto spent =
      std::make_shared<composition::PlaceholderContentView::Source>();
  const presentation::NavigationStack::ViewDestination* shown =
      linked.empty()
          ? nullptr
          : linked.entries()
                .back()
                .get<presentation::NavigationStack::ViewDestination>();
  return presenter.stack(
      [this, &spent, shown](const DetailPresenter::Slot& slot) {
        if (shown != nullptr && !slot.frozen) {
          return shown->build().id(
              composition::ElementKey::identifying(shown->identity()));
        }
        return composition::Element{
            composition::PlaceholderContentView(slot.frozen ? spent : detail_)};
      });
}

auto NavigationSplitView::Content::below_header(composition::Element content,
                                                theme::Colour fade, float depth)
    -> composition::Element {
  composition::Element pocket =
      components::ShapeView(theme::Shape::rectangle())
          .fill(graphics::LinearGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  fade.with_alpha(
                      theme::metrics::navigation.scroll_edge_opacity),
                  fade.with_alpha(0.0F)}},
              .start_point = motion::UnitPoint::top,
              .end_point = motion::UnitPoint::bottom});
  composition::Element clipped =
      std::move(content)
          .ignores_safe_area(theme::Edge::top)
          .safe_area_padding(theme::Edge::top,
                             theme::Space{theme::metric::toolbar_group_inset})
          .overlay(composition::Alignment::top,
                   std::move(pocket)
                       .frame({.height = theme::metric::toolbar_group_inset,
                               .max_width = composition::infinity})
                       .allows_hit_testing(false))
          .clip_shape(theme::Rectangle());
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           components::Alignment::top_leading},
             std::move(clipped))
      .padding(theme::Insets{.top = theme::Space{depth}})
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

auto NavigationSplitView::Content::chosen_width(
    const Column::Width& declared, const std::optional<float>& dragged)
    -> std::optional<float> {
  if (!declared.has_value()) {
    return std::nullopt;
  }
  if (dragged.has_value() && declared->resizable()) {
    return declared->clamped(*dragged);
  }
  return declared->resolved();
}

auto NavigationSplitView::Content::handle(
    composition::Context& context, const Column::Width& declared,
    const composition::StateKey<std::optional<float>>& dragged,
    const composition::StateKey<std::optional<float>>& anchor, float current)
    -> composition::Element {
  const bool active = declared.has_value() && declared->resizable();
  return components::ColumnResizer{components::ColumnResizer::Resize{
      .width = context.state(dragged),
      .anchor = context.state(anchor),
      .current = current,
      .minimum = active ? declared->lower() : current,
      .maximum = active ? declared->upper() : current,
      .direction = 1.0F,
      .active = active}};
}

auto NavigationSplitView::Content::middle(composition::Context& context,
                                          float width, bool hidden,
                                          bool floating,
                                          composition::Element handle) const
    -> composition::Element {
  composition::Element column =
      Column(composition::PlaceholderContentView(content_).environment(
                 presentation::NavigationStack::detail_navigator,
                 presentation::NavigationStack::Navigator{
                     std::make_shared<const DetailPath>(
                         context.state(linked_detail))}),
             context.state(content_width), composition::Alignment::top_leading)
          .frame({.width = hidden ? composition::Length{0.0F}
                                  : composition::Length{width},
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading});
  if (!floating) {
    column = std::move(column).background(theme::ColourRole::background);
  }
  column = std::move(column).clip_shape(theme::Rectangle());
  column = std::move(column).overlay(composition::Alignment::trailing,
                                     std::move(handle));
  if (hidden) {
    return std::move(column).opacity(0.0F).disabled(true).id(content_id);
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::top,
                 .spacing = theme::space::none},
             std::move(column).id(content_id),
             components::ZStack()
                 .frame({.width = separator_width,
                         .max_height = composition::infinity})
                 .background(theme::ColourRole::separator)
                 .allows_hit_testing(false)
                 .id("arisa.split.separator"))
      .frame({.max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

auto NavigationSplitView::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const SidebarCollapse forms = context.environment(SidebarCollapse::key);
  const bool fixed =
      context.environment(composition::ToolbarRemoval::sidebar_toggle_removed);
  const bool three = content_ != nullptr;
  const bool collapsible =
      !fixed && (three || forms.detail_only || forms.icons_only);
  const bool compact =
      context.environment(components::Environment::horizontal_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const bool prefers = compact && compact_.has_value();
  const Binding bound =
      prefers ? Binding{.get =
                            [column = *compact_, three] {
                              return compact_visibility(column.get(), three);
                            },
                        .set =
                            [column = *compact_, three](Visibility value) {
                              column.set(compact_column(value, three));
                            }}
              : binding(context);
  const composition::NavigationSplitViewStyle style = context.environment(
      composition::NavigationDeclarations::split_view_style);
  const bool overlaid =
      three && style == composition::NavigationSplitViewStyle::prominent_detail;
  const Visibility arranged =
      three ? (fixed ? Visibility::all : columns(bound.get(), compact, style))
            : Visibility::all;
  Visibility shown = resolved(bound.get(), forms, fixed);
  if (three) {
    shown = arranged == Visibility::double_column ||
                    arranged == Visibility::detail_only
                ? Visibility::detail_only
                : (arranged == Visibility::icons_only ? Visibility::icons_only
                                                      : Visibility::all);
  }
  const bool middle_hidden = three && arranged == Visibility::detail_only;
  const bool rail = shown == Visibility::icons_only;
  const bool hidden = shown == Visibility::detail_only;
  const composition::StateHandle<Column::Width> sidebar_declared =
      context.state(sidebar_width);
  const std::optional<float> chosen_width = Content::chosen_width(
      sidebar_declared.get(), context.state(sidebar_dragged).get());
  const theme::Space width =
      hidden ? theme::Space{0.0F}
      : rail ? theme::Space{theme::metric::sidebar_rail_width}
      : chosen_width.has_value() ? theme::Space{*chosen_width}
                                 : theme::Space{theme::metric::sidebar_width};
  const composition::Length inner =
      rail ? composition::Length{theme::metric::sidebar_rail_width}
      : chosen_width.has_value()
          ? composition::Length{*chosen_width}
          : composition::Length{theme::metric::sidebar_width};
  const theme::Theme& current =
      context.environment(components::Environment::theme);
  const bool floating = current.glass().sidebar.has_value();
  const float inset =
      floating ? current.metric(theme::MetricToken::sidebar_panel_inset) : 0.0F;
  const std::function<void()> flip =
      three ? std::function<void()>{[bound, arranged] {
        motion::with_animation(
            motion::Animation::smooth(0.35),
            [&bound, arranged] { bound.set(next_column(arranged)); });
      }}
            : std::function<void()>{[bound, forms] { toggle(bound, forms); }};
  const composition::FocusBinding<composition::TagKey> focus =
      context.state(sidebar_focus);
  std::shared_ptr<SidebarSelection::Reader>& selected =
      context.retained(reader);
  if (selected == nullptr) {
    selected = std::make_shared<SidebarSelection::Reader>();
  }
  const composition::StateHandle<composition::NavigationPath> linked =
      context.state(linked_detail);
  const DetailPresenter presenter{
      context, DetailPresenter::Request{
                   .shown = selected->current().value_or(composition::TagKey{}),
                   .following = focus.get().has_value(),
                   .delivered = detail_->pending(),
                   .space = page_space}};
  const presentation::NavigationStack::Navigator detail_pushes{
      std::make_shared<const DetailPath>(linked)};
  composition::Element sidebar_column =
      composition::PlaceholderContentView(sidebar_)
          .environment(SidebarSelection::key,
                       std::optional<SidebarSelection>{SidebarSelection{
                           focus, presenter.commit(), selected}})
          .environment(presentation::NavigationStack::detail_navigator,
                       detail_pushes)
          .environment(composition::SearchEnvironment::sidebar_field, nullptr);
  if (const auto& field =
          context.environment(composition::SearchEnvironment::sidebar_field);
      field != nullptr && *field) {
    sidebar_column = components::VStack(
        components::VStackOptions{
            .alignment = components::HorizontalAlignment::leading,
            .spacing = theme::space::sm},
        (*field)()
            .padding(
                theme::Insets::symmetric(theme::space::sm, theme::space::xs))
            .id("arisa.split.search"),
        std::move(sidebar_column));
  }
  composition::Element listing =
      Column(std::move(sidebar_column), sidebar_declared,
             composition::Alignment::center)
          .frame({.width = inner, .max_height = composition::infinity});
  if (const std::optional<SidebarToggle>& docked =
          context.environment(SidebarToggle::key);
      floating && docked.has_value()) {
    const float reported = context.read(docked->header_depth());
    const float depth =
        reported > 0.0F
            ? reported
            : current.metric(theme::MetricToken::toolbar_group_height);
    const theme::ColorScheme scheme =
        context.environment(components::Environment::colour_scheme)
            .value_or(current.scheme());
    listing = below_header(
        std::move(listing),
        current.colour(theme::ColourRole::background, scheme), depth);
  }
  composition::Element column =
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        components::Alignment::top_leading},
          std::move(listing))
          .frame({.width = hidden ? composition::Length{0.0F}
                                  : composition::Length{inner},
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::top_leading});
  if (floating) {
    const theme::Shape panel = theme::Shape::rounded(
        theme::Radius{current.metric(theme::MetricToken::sidebar_panel_radius)},
        true);
    column =
        std::move(column)
            .background(components::ZStack()
                            .frame({.max_width = composition::infinity,
                                    .max_height = composition::infinity})
                            .default_glass(theme::GlassSlot::sidebar, panel))
            .clip_shape(panel);
  } else {
    const std::optional<composition::PresentationBackground>& placed =
        context.environment(
            composition::ContainerBackgroundPlacement::navigation_split_view
                .key());
    column = placed.has_value()
                 ? placed->clad(std::move(column), theme::Shape::rectangle())
                 : std::move(column).background(
                       theme::ColourRole::secondary_system_background);
    column =
        std::move(column)
            .default_glass(theme::GlassSlot::sidebar, theme::Shape::rectangle())
            .overlay(
                composition::Alignment::trailing,
                components::ZStack()
                    .frame({.width = 0.5F, .max_height = composition::infinity})
                    .background(theme::ColourRole::separator)
                    .allows_hit_testing(false));
    if (!rail) {
      column = std::move(column).clip_shape(theme::Rectangle());
    }
  }
  column = std::move(column).overlay(
      composition::Alignment::trailing,
      handle(context, rail || hidden ? Column::Width{} : sidebar_declared.get(),
             sidebar_dragged, sidebar_anchor, chosen_width.value_or(0.0F)));
  if (rail) {
    column = std::move(column).label_style(components::IconOnlyLabelStyle{});
  }
  if (hidden) {
    column = std::move(column).opacity(0.0F).disabled(true);
  }
  composition::Element detail =
      Column(components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading},
                 pages(presenter, linked.get()))
                 .environment(composition::SearchEnvironment::sidebar_field,
                              nullptr),
             context.state(detail_width), composition::Alignment::leading)
          .frame({.max_width = composition::infinity,
                  .min_height = 0.0F,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .flex_shrink(1.0F)
          .id(detail_id);
  const theme::Insets around =
      hidden ? theme::Insets{}
             : theme::Insets{.top = theme::Space{inset},
                             .leading = theme::Space{inset},
                             .bottom = theme::Space{inset},
                             .trailing = theme::Space{inset}};
  if (three) {
    const float extent =
        floating && !hidden && !overlaid
            ? std::max(current.space(width), 0.0F) + (2.0F * inset)
            : 0.0F;
    const composition::StateHandle<Column::Width> middle_declared =
        context.state(content_width);
    const float middle_width =
        Content::chosen_width(middle_declared.get(),
                              context.state(content_dragged).get())
            .value_or(NavigationSplitView::content_width);
    composition::Element side = middle(
        context, middle_width + extent, middle_hidden, floating,
        handle(context, middle_hidden ? Column::Width{} : middle_declared.get(),
               content_dragged, content_anchor, middle_width));
    if (overlaid) {
      composition::Element panel =
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::top,
                  .spacing = theme::space::none},
              std::move(column).padding(around).id(sidebar_id), std::move(side))
              .frame({.max_height = composition::infinity,
                      .alignment = composition::Alignment::top_leading});
      if (!middle_hidden) {
        panel = std::move(panel).shadow(theme::Elevation::high);
      }
      column = std::move(panel);
    } else {
      detail = components::HStack(
                   components::HStackOptions{
                       .alignment = components::VerticalAlignment::top,
                       .spacing = theme::space::none},
                   std::move(side), std::move(detail))
                   .frame({.max_width = composition::infinity,
                           .max_height = composition::infinity,
                           .alignment = composition::Alignment::top_leading});
    }
  }
  composition::Element layout =
      overlaid
          ? components::ZStack(
                components::ZStackOptions{
                    .alignment = components::Alignment::top_leading},
                components::VStack(
                    components::VStackOptions{
                        .alignment = components::HorizontalAlignment::leading},
                    std::move(detail))
                    .frame({.max_width = composition::infinity,
                            .max_height = composition::infinity,
                            .alignment = composition::Alignment::leading}),
                std::move(column))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::top_leading})
      : floating
          ? components::VStack(
                components::VStackOptions{
                    .alignment = components::HorizontalAlignment::leading},
                std::move(detail))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::leading})
                .overlay(
                    composition::Alignment::leading,
                    components::ZStack(
                        components::ZStackOptions{
                            .alignment = components::Alignment::top_leading},
                        std::move(column))
                        .padding(around)
                        .frame(
                            {.max_height = composition::infinity,
                             .alignment = composition::Alignment::top_leading})
                        .id(sidebar_id)
                        .safe_area_bar(theme::Edge::leading))
          : components::ZStack(
                components::ZStackOptions{
                    .alignment = components::Alignment::top_leading},
                components::VStack(
                    components::VStackOptions{
                        .alignment = components::HorizontalAlignment::leading},
                    std::move(detail))
                    .padding(theme::Insets{.leading = width})
                    .frame({.max_width = composition::infinity,
                            .max_height = composition::infinity,
                            .alignment = composition::Alignment::leading}),
                std::move(column).id(sidebar_id))
                .frame({.max_width = composition::infinity,
                        .max_height = composition::infinity,
                        .alignment = composition::Alignment::top_leading});
  if (collapsible && three && arranged != Visibility::all) {
    layout = std::move(layout).on_focus_edge(
        composition::MoveCommandDirection::left, [bound, arranged] {
          motion::with_animation(motion::Animation::smooth(0.35),
                                 [&bound, arranged] {
                                   bound.set(arranged == Visibility::detail_only
                                                 ? Visibility::double_column
                                                 : Visibility::all);
                                 });
        });
  } else if (collapsible && hidden) {
    layout = std::move(layout).on_focus_edge(
        composition::MoveCommandDirection::left,
        [bound, forms] { toggle(bound, forms); });
  }
  if (collapsible) {
    layout = std::move(layout)
                 .on_command(toggle_button, std::string{toggle_label}, flip)
                 .focus_fallback();
  }
  if (const std::optional<SidebarToggle>& control =
          context.environment(SidebarToggle::key)) {
    const SidebarToggle held = *control;
    const composition::Identity owner = context.state(chosen).identity();
    const Visibility panel = floating ? shown : Visibility::detail_only;
    layout = std::move(layout)
                 .on_appear([held, owner, flip, collapsible, panel] {
                   held.show_floating_panel(panel);
                   if (collapsible) {
                     held.attach(owner, flip);
                   }
                 })
                 .on_change(
                     panel,
                     [held, panel] {
                       motion::with_animation(
                           motion::Animation::smooth(0.35),
                           [&held, panel] { held.show_floating_panel(panel); });
                     })
                 .on_disappear([held, owner] {
                   held.show_floating_panel(Visibility::detail_only);
                   held.detach(owner);
                 });
  }
  if (prefers) {
    layout = std::move(layout).on_change(
        selected->current().value_or(composition::TagKey{}),
        [column = *compact_] {
          column.set(composition::NavigationSplitViewColumn::detail);
        });
  }
  composition::Element finished =
      presenter.settling(std::move(layout))
          .environment(presentation::NavigationStack::navigator, detail_pushes);
  if (const CustomStyle& styled = context.environment(custom_style);
      styled != nullptr && *styled) {
    finished =
        (*styled)(
            NavigationSplitViewStyleConfiguration{
                std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(finished).environment(custom_style, nullptr)),
                bound.get()})
            .environment(custom_style, nullptr);
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(finished));
  return produced;
}

}
