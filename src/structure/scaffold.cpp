#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hint_bar.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/status/battery.hpp>
#include <arisa/components/status/clock.hpp>
#include <arisa/components/status/wireless.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/theme.hpp>
#include <arisa/theme/typography.hpp>

#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

Scaffold::Scaffold(composition::Element content)
    : Scaffold(ScaffoldOptions{}, std::move(content)) {}

Scaffold::Scaffold(ScaffoldOptions options, composition::Element content)
    : Element([&options, &content] {
        std::optional<std::string> title = content.navigation().title;
        Content::Items toolbar = content.navigation().toolbar;
        composition::NavigationChrome chrome = content.navigation().chrome;
        return std::make_unique<Content>(
            options, std::move(title), std::move(toolbar), std::move(chrome),
            std::make_shared<composition::PlaceholderContentView::Source>(
                std::move(content)));
      }()) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

const composition::StateKey<std::optional<SidebarToggle>> Scaffold::toggle{
    "scaffold-sidebar-toggle", std::nullopt};

const composition::StateKey<std::optional<Scaffold::Content::Declared>>
    Scaffold::Content::resolved{"scaffold-resolved", std::nullopt};

const composition::StateKey<Scaffold::Content::Declared>
    Scaffold::Content::shown{"scaffold-shown", Declared{}};

auto Scaffold::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Scaffold::Content::rule() -> composition::Element {
  return components::ZStack()
      .frame({.height = 0.5F, .max_width = composition::infinity})
      .background(theme::ColourRole::separator);
}

auto Scaffold::Content::title(const Declared& chosen, bool floating)
    -> std::optional<composition::Element> {
  const Layout layout{chosen.toolbar, chosen.chrome.role()};
  const Layout::Builder menu = layout.title_menu();
  const composition::ToolbarTitleDisplayMode mode =
      chosen.chrome.title_display_mode();
  const bool plain = !chosen.chrome.subtitle().has_value() && menu == nullptr &&
                     mode == composition::ToolbarTitleDisplayMode::automatic &&
                     !layout.has(Layout::Side::title) &&
                     !layout.has(Layout::Side::subtitle);
  if (!plain) {
    std::optional<composition::Element> heading = layout.titled(
        chosen.title, chosen.chrome.subtitle(),
        Layout::title_font(
            mode, floating ? theme::Font::title3 : theme::Font::headline),
        false);
    if (heading.has_value() && floating) {
      heading = std::move(*heading).padding(theme::Insets::symmetric(
          theme::metric::toolbar_horizontal_padding, theme::space::none));
    }
    return heading;
  }
  if (!chosen.title.has_value()) {
    return std::nullopt;
  }
  return floating
             ? composition::Element{components::Text(*chosen.title)
                                        .font(theme::Font::title3)
                                        .font_weight(theme::Font::Weight::bold)}
                   .padding(theme::Insets::symmetric(
                       theme::metric::toolbar_horizontal_padding,
                       theme::space::none))
             : composition::Element{
                   components::Text(*chosen.title).font(theme::Font::headline)};
}

auto Scaffold::Content::trailing_groups(
    std::vector<Layout::Segment> segments,
    std::optional<composition::Element> last, const Chrome& metrics)
    -> std::vector<composition::Element> {
  std::vector<composition::Element> groups;
  std::vector<composition::Element> tail;
  if (!segments.empty()) {
    Layout::Segment final_segment = std::move(segments.back());
    segments.pop_back();
    for (Layout::Segment& segment : segments) {
      if (segment.flexible_before) {
        groups.push_back(components::Spacer());
      }
      groups.push_back(Layout::group(std::move(segment.items), metrics));
    }
    if (final_segment.flexible_before && !groups.empty()) {
      groups.push_back(components::Spacer());
    }
    tail = std::move(final_segment.items);
  }
  if (last.has_value()) {
    tail.push_back(std::move(*last));
  }
  if (!tail.empty()) {
    groups.push_back(
        Layout::group(std::move(tail), metrics).id(trailing_group_key));
  }
  return groups;
}

auto Scaffold::Content::header(const Declared& chosen,
                               const std::optional<SidebarToggle>& sidebar,
                               bool floating, Visibility panel,
                               const Chrome& metrics) const
    -> composition::Element {
  const bool docked = floating && panel != Visibility::detail_only;
  const bool rail = panel == Visibility::icons_only;
  const Layout layout{chosen.toolbar, chosen.chrome.role()};
  std::vector<composition::Element> leading;
  std::vector<composition::Element> beside;
  if (sidebar.has_value()) {
    leading.push_back(
        components::Button([held = *sidebar] { held.toggle(); },
                           components::Image::system_name("left_panel_open")
                               .font(theme::Font::title3))
            .id(sidebar_toggle_key));
  }
  std::vector<composition::Element>& named = docked && rail ? beside : leading;
  for (composition::Element& each :
       layout.flattened(Layout::Side::leading, metrics.gap)) {
    named.push_back(std::move(each));
  }
  const bool centred = chosen.chrome.title_alignment() ==
                       composition::ToolbarTitleAlignment::center;
  std::optional<composition::Element> centre_heading;
  if (std::optional<composition::Element> heading = title(chosen, floating)) {
    if (centred) {
      centre_heading = std::move(heading);
    } else {
      named.push_back(std::move(*heading));
    }
  }
  std::vector<composition::Element>& principal = docked ? beside : leading;
  for (composition::Element& each :
       layout.flattened(Layout::Side::principal, metrics.gap)) {
    principal.push_back(std::move(each));
  }
  std::vector<Layout::Segment> segments =
      layout.segments(Layout::Side::trailing);
  if (std::optional<composition::Element> more = layout.overflow()) {
    if (segments.empty()) {
      segments.emplace_back();
    }
    segments.back().items.push_back(std::move(*more));
  }
  std::optional<composition::Element> status_group;
  if (options_.shows_status &&
      !chosen.chrome.bar(composition::ToolbarPlacement::status_bar).hidden()) {
    std::vector<composition::Element> status =
        layout.flattened(Layout::Side::status, metrics.gap);
    if (status.empty()) {
      status.push_back(components::status::Wireless{});
      status.push_back(components::status::Battery{});
      status.push_back(components::status::Clock{});
    }
    status_group =
        components::HStack(
            components::HStackOptions{
                .alignment = components::VerticalAlignment::center,
                .spacing = theme::space::md},
            std::move(status))
            .padding(theme::Insets::symmetric(
                floating
                    ? theme::Space{theme::metric::toolbar_horizontal_padding}
                    : theme::space::none,
                theme::space::none))
            .foreground_style(theme::ColourRole::text_secondary)
            .id("arisa.scaffold.status");
  }
  std::vector<composition::Element> items;
  if (docked) {
    composition::Element dock =
        docked_group(std::move(leading), rail, metrics).id(leading_group_key);
    if (sidebar.has_value()) {
      dock = std::move(dock).on_geometry_change(
          std::type_identity<float>{},
          [](const composition::GeometryProxy& proxy) {
            return proxy.frame(composition::CoordinateSpace::global).height;
          },
          [held = *sidebar](float depth) { held.report_header_depth(depth); });
    }
    items.push_back(std::move(dock));
    for (composition::Element& each : beside) {
      items.push_back(
          std::move(each)
              .frame({.max_height = theme::metric::navigation_bar_height})
              .clip_shape(theme::Rectangle()));
    }
    items.push_back(components::Spacer());
    for (composition::Element& each : trailing_groups(
             std::move(segments), std::move(status_group), metrics)) {
      items.push_back(std::move(each));
    }
  } else if (floating) {
    if (!leading.empty()) {
      items.push_back(
          Layout::group(std::move(leading), metrics).id(leading_group_key));
    }
    items.push_back(components::Spacer());
    for (composition::Element& each : trailing_groups(
             std::move(segments), std::move(status_group), metrics)) {
      items.push_back(std::move(each));
    }
  } else {
    items = std::move(leading);
    items.push_back(components::Spacer());
    bool first = true;
    for (Layout::Segment& segment : segments) {
      if (segment.flexible_before) {
        items.push_back(components::Spacer());
      } else if (!first) {
        items.push_back(components::ZStack().frame({.width = metrics.gap}));
      }
      first = false;
      for (composition::Element& each : segment.items) {
        items.push_back(std::move(each));
      }
    }
    if (status_group.has_value()) {
      items.push_back(std::move(*status_group));
    }
  }
  const theme::Space edge{metrics.inset};
  const theme::Insets inset =
      floating ? theme::Insets{.top = edge,
                               .leading = edge,
                               .bottom = theme::space::none,
                               .trailing = edge}
               : theme::Insets::symmetric(theme::space::xl, theme::space::none);
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing =
                  floating ? theme::Space{metrics.gap} : theme::space::md},
          std::move(items))
          .padding(inset)
          .frame({.max_width = composition::infinity});
  if (centre_heading.has_value()) {
    row = std::move(row).overlay(composition::Alignment::center,
                                 std::move(*centre_heading));
  }
  if (floating) {
    row = components::ZStack(
        components::ZStackOptions{.alignment = components::Alignment::top},
        std::move(row));
  }
  return std::move(row)
      .frame({.max_width = composition::infinity,
              .min_height = theme::metric::navigation_bar_height,
              .alignment = floating ? composition::Alignment::top
                                    : composition::Alignment::center})
      .button_style(components::ButtonStyle::toolbar())
      .focus_section();
}

auto Scaffold::Content::docked_group(std::vector<composition::Element> items,
                                     bool rail, const Chrome& metrics)
    -> composition::Element {
  Layout::capsules(items);
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::Space{metrics.spacing}},
             std::move(items))
      .padding(theme::Insets::symmetric(theme::Space{metrics.padding},
                                        theme::space::none))
      .frame({.width = rail ? theme::metric::sidebar_rail_width
                            : theme::metric::sidebar_width,
              .min_height = metrics.height,
              .alignment = composition::Alignment::leading});
}

auto Scaffold::Content::edge_fade(composition::Context& context, bool top)
    -> composition::Element {
  const theme::Theme& current =
      context.environment(components::Environment::theme);
  const theme::ColorScheme scheme =
      context.environment(components::Environment::colour_scheme)
          .value_or(current.scheme());
  const theme::Colour base =
      current.colour(theme::ColourRole::background, scheme);
  const theme::Colour near =
      base.with_alpha(theme::metrics::navigation.scroll_edge_opacity);
  const theme::Colour far = base.with_alpha(0.0F);
  return composition::Element{
      components::ShapeView(theme::Shape::rectangle())
          .fill(graphics::LinearGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  top ? near : far, top ? far : near}},
              .start_point = motion::UnitPoint::top,
              .end_point = motion::UnitPoint::bottom})}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .allows_hit_testing(false);
}

auto Scaffold::Content::edge_effect(
    composition::Context& context, bool top,
    const composition::NavigationChrome::Bar& bar)
    -> std::optional<composition::Element> {
  if (bar.scroll_edge_effect_hidden.value_or(false)) {
    return std::nullopt;
  }
  if (bar.scroll_edge_effect_style !=
      composition::ScrollEdgeEffectStyle::hard) {
    return edge_fade(context, top);
  }
  return composition::Element{components::ZStack()
                                  .frame({.max_width = composition::infinity,
                                          .max_height = composition::infinity})
                                  .background(theme::ColourRole::background)
                                  .overlay(top ? composition::Alignment::bottom
                                               : composition::Alignment::top,
                                           rule())
                                  .allows_hit_testing(false)};
}

auto Scaffold::Content::footer(const Declared& chosen, bool floating,
                               const Chrome& metrics) const
    -> composition::Element {
  const Layout layout{chosen.toolbar, chosen.chrome.role()};
  const bool bottom =
      layout.has(Layout::Side::bottom) &&
      !chosen.chrome.bar(composition::ToolbarPlacement::bottom_bar).hidden();
  if (floating) {
    std::vector<composition::Element> row;
    if (bottom) {
      for (composition::Element& each :
           layout.grouped(Layout::Side::bottom, metrics)) {
        row.push_back(std::move(each));
      }
    }
    row.push_back(components::Spacer());
    std::vector<composition::Element> hints;
    hints.push_back(composition::Element{components::HintBar{}}.padding(
        theme::Insets::symmetric(theme::metric::toolbar_horizontal_padding,
                                 theme::space::none)));
    row.push_back(Layout::group(std::move(hints), metrics).id(hint_group_key));
    const theme::Space edge{metrics.inset};
    return components::HStack(
               components::HStackOptions{
                   .alignment = components::VerticalAlignment::center,
                   .spacing = theme::Space{metrics.gap}},
               std::move(row))
        .padding(theme::Insets{.top = theme::space::none,
                               .leading = edge,
                               .bottom = edge,
                               .trailing = edge})
        .frame({.max_width = composition::infinity});
  }
  std::vector<composition::Element> row;
  if (bottom) {
    row = layout.flattened(Layout::Side::bottom, metrics.gap);
  }
  row.push_back(components::Spacer());
  row.push_back(components::HintBar{});
  composition::Element bar =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::md},
          std::move(row))
          .padding(theme::Insets::symmetric(theme::space::xl, theme::space::sm))
          .frame({.max_width = composition::infinity});
  if (bottom) {
    bar = std::move(bar).button_style(components::ButtonStyle::toolbar());
  }
  return bar;
}

auto Scaffold::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::optional<SidebarToggle>& retained = context.retained(toggle);
  if (!retained.has_value()) {
    retained.emplace();
  }
  const SidebarToggle sidebar = *retained;
  const bool available = context.read(sidebar.available());
  const composition::BarPlacement above = placement(context, true);
  const composition::BarPlacement below = placement(context, false);
  const bool floats_above = above == composition::BarPlacement::overlay;
  const bool floats_below = below == composition::BarPlacement::overlay;
  const Visibility panel = context.read(sidebar.floating_panel());
  const Chrome metrics =
      Layout::metrics(context.environment(components::Environment::theme));
  static_cast<void>(context.state(revision));
  const std::optional<Declared>& found = context.retained(resolved);
  Declared& used = context.retained(shown);
  used = Declared{
      .title = title_.has_value() || !found.has_value() ? title_ : found->title,
      .toolbar = found.has_value() ? found->toolbar : toolbar_,
      .chrome = found.has_value() ? found->chrome : chrome_};
  const composition::NavigationChrome::Bar& above_style =
      used.chrome.bar(composition::ToolbarPlacement::navigation_bar);
  const composition::NavigationChrome::Bar& below_style =
      used.chrome.bar(composition::ToolbarPlacement::bottom_bar);
  composition::Element top = header(
      used, available ? std::optional<SidebarToggle>{sidebar} : std::nullopt,
      floats_above, panel, metrics);
  if (available) {
    top = std::move(top).on_command(
        NavigationSplitView::toggle_button,
        std::string{NavigationSplitView::toggle_label},
        [sidebar] { sidebar.toggle(); });
  }
  const bool plain_above =
      !above_style.background.has_value() &&
      above_style.background_visibility != composition::Visibility::hidden;
  composition::Element bars =
      floats_above
          ? components::VStack(
                components::VStackOptions{.spacing = theme::space::none},
                std::move(top).id(header_key))
                .frame({.max_width = composition::infinity})
          : components::VStack(
                components::VStackOptions{.spacing = theme::space::none},
                std::move(top).id(header_key), rule())
                .frame({.max_width = composition::infinity});
  if (plain_above) {
    if (!floats_above) {
      bars = std::move(bars).default_glass(theme::GlassSlot::toolbar,
                                           theme::Shape::rectangle());
    } else if (std::optional<composition::Element> backing =
                   edge_effect(context, true, above_style)) {
      bars = std::move(bars).background(std::move(*backing));
    }
  }
  bars = Layout::styled(std::move(bars), above_style);
  composition::Element page =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading},
          composition::PlaceholderContentView(content_).environment(
              SidebarToggle::key, std::optional<SidebarToggle>{sidebar}))
          .frame({.max_width = composition::infinity,
                  .min_height = 0.0F,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .id(content_key);
  std::optional<composition::Element> header_bar;
  if (above_style.hidden()) {
    if (available) {
      page = std::move(page).on_command(
          NavigationSplitView::toggle_button,
          std::string{NavigationSplitView::toggle_label},
          [sidebar] { sidebar.toggle(); });
    }
  } else if (floats_above) {
    page = std::move(page).overlay(
        composition::Alignment::top,
        std::move(bars).safe_area_bar(theme::Edge::top));
  } else {
    header_bar = std::move(bars);
  }
  std::optional<composition::Element> footer_bar;
  if (options_.shows_hints) {
    const bool plain_below =
        !below_style.background.has_value() &&
        below_style.background_visibility != composition::Visibility::hidden;
    composition::Element hints =
        floats_below
            ? components::VStack(
                  components::VStackOptions{.spacing = theme::space::none},
                  footer(used, true, metrics).id(footer_key))
                  .frame({.max_width = composition::infinity})
            : components::VStack(
                  components::VStackOptions{.spacing = theme::space::none},
                  rule(), footer(used, false, metrics).id(footer_key))
                  .frame({.max_width = composition::infinity});
    if (plain_below) {
      if (!floats_below) {
        hints = std::move(hints).default_glass(theme::GlassSlot::toolbar,
                                               theme::Shape::rectangle());
      } else if (std::optional<composition::Element> backing =
                     edge_effect(context, false, below_style)) {
        hints = std::move(hints).background(std::move(*backing));
      }
    }
    hints = Layout::styled(std::move(hints), below_style);
    if (floats_below) {
      page = std::move(page).overlay(
          composition::Alignment::bottom,
          std::move(hints).safe_area_bar(theme::Edge::bottom));
    } else {
      footer_bar = std::move(hints);
    }
  }
  composition::Element frame =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::none},
          std::move(header_bar),
          std::move(page)
              .frame({.max_width = composition::infinity,
                      .min_height = 0.0F,
                      .max_height = composition::infinity,
                      .alignment = composition::Alignment::leading})
              .flex_shrink(1.0F)
              .clip_shape(theme::Rectangle()),
          std::move(footer_bar))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity,
                  .alignment = composition::Alignment::leading});
  const std::optional<composition::PresentationBackground>& chosen =
      context.environment(
          composition::ContainerBackgroundPlacement::scaffold.key());
  if (chosen.has_value()) {
    frame = chosen->clad(std::move(frame), theme::Rectangle());
  } else if (!floats_above) {
    frame = std::move(frame).background(theme::ColourRole::background);
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(frame));
  return produced;
}

void Scaffold::Content::resolve(composition::Context& context,
                                const composition::Node& node) {
  Declared found{
      .title = node.preference(composition::NavigationDeclarations::title_key)
                   .value_or(std::nullopt),
      .toolbar =
          node.preference(composition::NavigationDeclarations::toolbar_key)
              .value_or(Items{}),
      .chrome = node.preference(composition::NavigationDeclarations::chrome_key)
                    .value_or(composition::NavigationChrome{})};
  const bool differs = found != context.retained(shown);
  context.retained(resolved) = std::move(found);
  if (differs) {
    const composition::StateHandle<std::uint64_t> bump =
        context.state(revision);
    bump.set(bump.get() + 1);
  }
}

auto Scaffold::Content::placement(composition::Context& context, bool top) const
    -> composition::BarPlacement {
  const std::optional<composition::BarPlacement> chosen =
      top ? options_.header : options_.footer;
  if (chosen.has_value()) {
    return *chosen;
  }
  const std::optional<composition::BarPlacement>& inherited =
      context.environment(top ? composition::BarPlacements::header
                              : composition::BarPlacements::footer);
  if (inherited.has_value()) {
    return *inherited;
  }
  const theme::Theme& current =
      context.environment(components::Environment::theme);
  return current.glass().toolbar.has_value()
             ? composition::BarPlacement::overlay
             : composition::BarPlacement::in_layout;
}

}
