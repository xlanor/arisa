#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/toolbar_environment.hpp>
#include <arisa/presentation/menu_panel.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/presentation/toolbar_layout.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <format>
#include <numeric>
#include <type_traits>
#include <utility>

namespace arisa::presentation {

using composition::ToolbarItem;
using composition::ToolbarItemPlacement;

const composition::StateKey<std::vector<float>> ToolbarLayout::Fitted::widths{
    "toolbar-fitted-widths", {}};

ToolbarLayout::ToolbarLayout(Items items, composition::ToolbarRole role)
    : ToolbarLayout(std::move(items), role, Options{}) {}

ToolbarLayout::ToolbarLayout(Items items, composition::ToolbarRole role,
                             Options options)
    : items_(std::move(items)), role_(role), options_(std::move(options)) {
  relocate();
}

void ToolbarLayout::relocate() {
  std::optional<composition::ToolbarCustomization> chosen;
  if (options_.customization.has_value()) {
    chosen = options_.customization->get();
  }
  const bool moves_search = std::ranges::any_of(items_, [](const auto& item) {
    return item->kind() == ToolbarItem::Kind::default_item &&
           item->default_kind() == composition::ToolbarDefaultItemKind::search;
  });
  Items kept;
  kept.reserve(items_.size());
  for (const auto& item : items_) {
    if (item->is_hidden()) {
      continue;
    }
    if (item->kind() == ToolbarItem::Kind::default_item) {
      relocations_.emplace_back(item->default_kind(), item->placement());
      if (item->default_kind() == composition::ToolbarDefaultItemKind::search) {
        const auto found =
            std::ranges::find_if(items_, [](const auto& candidate) {
              return candidate->system() ==
                         composition::ToolbarDefaultItemKind::search &&
                     !candidate->is_hidden();
            });
        if (found != items_.end()) {
          ToolbarItem moved = **found;
          kept.push_back(std::make_shared<const ToolbarItem>(
              std::move(moved).placed(item->placement())));
        }
      }
      continue;
    }
    if (moves_search &&
        item->system() == composition::ToolbarDefaultItemKind::search) {
      continue;
    }
    if (item->customizable()) {
      const composition::Visibility picked =
          chosen.has_value() ? chosen->visibility(item->id())
                             : composition::Visibility::automatic;
      const bool visible =
          picked == composition::Visibility::visible ||
          (picked == composition::Visibility::automatic &&
           item->default_visibility() != composition::Visibility::hidden);
      if (!visible) {
        if (item->options().contains(
                composition::ToolbarCustomizationOptions::always_available)) {
          available_.push_back(item);
        }
        continue;
      }
    }
    kept.push_back(item);
  }
  items_ = std::move(kept);
}

auto ToolbarLayout::customization_id(const Items& items)
    -> std::shared_ptr<const std::string> {
  for (const auto& item : items) {
    if (item->toolbar() != nullptr) {
      return item->toolbar();
    }
  }
  return nullptr;
}

auto ToolbarLayout::storage_key(const std::string& id) -> std::string {
  return std::format("arisa.toolbar.{}", id);
}

auto ToolbarLayout::prefers_vertical(const Items& items) -> bool {
  return std::ranges::any_of(items, [](const auto& item) {
    return item->kind() == ToolbarItem::Kind::item && !item->is_hidden() &&
           item->axis() ==
               composition::ToolbarItemAxisBehavior::vertical_preferred;
  });
}

auto ToolbarLayout::relocated(composition::ToolbarDefaultItemKind kind) const
    -> std::optional<ToolbarItemPlacement> {
  for (const auto& [moved, placement] : relocations_) {
    if (moved == kind) {
      return placement;
    }
  }
  return std::nullopt;
}

auto ToolbarLayout::matches(const ToolbarItem& item, Side side) const noexcept
    -> bool {
  if (item.kind() != ToolbarItem::Kind::item &&
      item.kind() != ToolbarItem::Kind::spacer) {
    return false;
  }
  const ToolbarItemPlacement placement = item.placement();
  const bool upright =
      options_.vertical &&
      item.axis() == composition::ToolbarItemAxisBehavior::vertical_preferred &&
      item.kind() == ToolbarItem::Kind::item;
  switch (side) {
    case Side::vertical:
      return upright;
    case Side::leading:
      return !upright && item.leading();
    case Side::principal:
      return !upright && placement == ToolbarItemPlacement::principal;
    case Side::trailing:
      return !upright &&
             (item.trailing() ||
              (placement == ToolbarItemPlacement::secondary_action &&
               role_ == composition::ToolbarRole::editor));
    case Side::bottom:
      return !upright && placement == ToolbarItemPlacement::bottom_bar;
    case Side::status:
      return placement == ToolbarItemPlacement::status;
    case Side::title:
      return placement == ToolbarItemPlacement::title;
    case Side::subtitle:
      return placement == ToolbarItemPlacement::subtitle;
    case Side::large_title:
      return placement == ToolbarItemPlacement::large_title;
    case Side::large_subtitle:
      return placement == ToolbarItemPlacement::large_subtitle;
    case Side::keyboard:
      return placement == ToolbarItemPlacement::keyboard;
  }
  return false;
}

auto ToolbarLayout::shown(const ToolbarItem& item, Side side)
    -> composition::Element {
  composition::Element view =
      composition::PlaceholderContentView(item.content());
  if (item.placement() == ToolbarItemPlacement::confirmation_action) {
    view = std::move(view).button_style(
        components::ButtonStyle::bordered_prominent());
  }
  if (item.placement() == ToolbarItemPlacement::bottom_bar ||
      item.placement() == ToolbarItemPlacement::keyboard) {
    view = std::move(view).environment(
        composition::ControlEnvironment::menu_opens_upward, true);
  }
  if (const auto& source = item.transition_source(); source.has_value()) {
    view = std::move(view).matched_transition_source(source->id, source->space);
  }
  if (side == Side::bottom || side == Side::keyboard) {
    view = std::move(view).environment(
        composition::ToolbarEnvironment::vertical_edge,
        std::optional<composition::VerticalEdge>{
            composition::VerticalEdge::bottom});
  } else if (side != Side::vertical && side != Side::status) {
    view = std::move(view).environment(
        composition::ToolbarEnvironment::vertical_edge,
        std::optional<composition::VerticalEdge>{
            composition::VerticalEdge::top});
  }
  return view;
}

auto ToolbarLayout::has(Side side) const -> bool {
  return std::ranges::any_of(items_, [this, side](const auto& item) {
    return item->kind() == ToolbarItem::Kind::item && matches(*item, side);
  });
}

auto ToolbarLayout::segments(Side side) const -> std::vector<Segment> {
  std::vector<Segment> found;
  Segment current;
  const ToolbarItem::Group* group = nullptr;
  bool flexible = false;
  const auto close = [&] {
    if (!current.items.empty()) {
      found.push_back(std::move(current));
    }
    current = Segment{};
    group = nullptr;
  };
  const auto visit = [&](const ToolbarItem& item) {
    if (item.kind() == ToolbarItem::Kind::spacer) {
      close();
      flexible =
          flexible || item.sizing() == composition::SpacerSizing::flexible;
      return;
    }
    const bool alone =
        item.shared_background() == composition::Visibility::hidden ||
        item.margins_removed();
    const ToolbarItem::Group* joined = item.group().get();
    if (!current.items.empty() && (joined != group || alone)) {
      close();
    }
    if (current.items.empty()) {
      current.flexible_before = std::exchange(flexible, false);
    }
    group = joined;
    current.bare = current.bare ||
                   item.shared_background() == composition::Visibility::hidden;
    current.tight = current.tight || item.margins_removed();
    current.items.push_back(shown(item, side));
    if (alone) {
      close();
    }
  };
  for (const auto& item : items_) {
    if (matches(*item, side) && !(side == Side::trailing && item->pinned())) {
      visit(*item);
    }
  }
  if (side == Side::trailing) {
    for (const auto& item : items_) {
      if (matches(*item, side) && item->pinned()) {
        visit(*item);
      }
    }
  }
  close();
  return found;
}

auto ToolbarLayout::flattened(Side side, float gap) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> flat;
  bool first = true;
  for (Segment& segment : segments(side)) {
    if (segment.flexible_before) {
      flat.push_back(components::Spacer());
    } else if (!first) {
      flat.push_back(components::ZStack().frame({.width = gap}));
    }
    first = false;
    for (composition::Element& item : segment.items) {
      flat.push_back(std::move(item));
    }
  }
  return flat;
}

auto ToolbarLayout::overflows(const ToolbarItem& item) noexcept -> bool {
  return item.builder() != nullptr && !item.pinned();
}

auto ToolbarLayout::shown_in_bar(const ToolbarItem& item) const -> bool {
  return item.kind() == ToolbarItem::Kind::item &&
         item.placement() == ToolbarItemPlacement::secondary_action &&
         role_ != composition::ToolbarRole::editor;
}

auto ToolbarLayout::overflow() const -> std::optional<composition::Element> {
  Items hidden;
  for (const auto& item : items_) {
    if (shown_in_bar(*item) ||
        item->kind() == ToolbarItem::Kind::overflow_menu) {
      hidden.push_back(item);
    }
  }
  for (const auto& item : available_) {
    hidden.push_back(item);
  }
  Items customizable;
  if (options_.customization.has_value()) {
    for (const auto& item : items_) {
      if (item->customizable()) {
        customizable.push_back(item);
      }
    }
    for (const auto& item : available_) {
      customizable.push_back(item);
    }
  }
  if (hidden.empty() && customizable.empty()) {
    return std::nullopt;
  }
  return composition::Element{Overflow{std::move(hidden),
                                       options_.customization,
                                       std::move(customizable)}}
      .id(overflow_key);
}

auto ToolbarLayout::fitted(float gap) const
    -> std::optional<composition::Element> {
  std::vector<Fitted::Unit> units;
  bool flexible = false;
  const ToolbarItem::Group* group = nullptr;
  const auto visit = [&](const std::shared_ptr<const ToolbarItem>& item) {
    if (item->kind() == ToolbarItem::Kind::spacer) {
      flexible =
          flexible || item->sizing() == composition::SpacerSizing::flexible;
      group = nullptr;
      return;
    }
    const ToolbarItem::Group* joined = item->group().get();
    if (units.empty() || joined == nullptr || joined != group) {
      units.push_back(Fitted::Unit{
          .items = {},
          .priority = item->priority(),
          .pinned = false,
          .flexible_before = std::exchange(flexible, false),
          .label = joined != nullptr ? item->group()->label() : nullptr});
    }
    group = joined;
    Fitted::Unit& unit = units.back();
    unit.items.push_back(item);
    unit.priority = std::max(unit.priority, item->priority());
    unit.pinned = unit.pinned || !overflows(*item);
  };
  for (const auto& item : items_) {
    if (matches(*item, Side::trailing) && !item->pinned()) {
      visit(item);
    }
  }
  for (const auto& item : items_) {
    if (matches(*item, Side::trailing) && item->pinned()) {
      visit(item);
    }
  }
  Items extra;
  for (const auto& item : items_) {
    if (shown_in_bar(*item) ||
        item->kind() == ToolbarItem::Kind::overflow_menu) {
      extra.push_back(item);
    }
  }
  for (const auto& item : available_) {
    extra.push_back(item);
  }
  Items customizable;
  if (options_.customization.has_value()) {
    for (const auto& item : items_) {
      if (item->customizable()) {
        customizable.push_back(item);
      }
    }
    for (const auto& item : available_) {
      customizable.push_back(item);
    }
  }
  if (units.empty() && extra.empty() && customizable.empty()) {
    return std::nullopt;
  }
  return composition::Element{Fitted{std::move(units), std::move(extra),
                                     options_.customization,
                                     std::move(customizable), gap}}
      .id(fitted_key);
}

auto ToolbarLayout::title_menu() const -> Builder {
  for (const auto& item : items_) {
    if (item->kind() == ToolbarItem::Kind::title_menu) {
      return item->builder();
    }
  }
  return nullptr;
}

auto ToolbarLayout::title_font(composition::ToolbarTitleDisplayMode mode,
                               theme::Font inline_font) noexcept
    -> theme::Font {
  switch (mode) {
    case composition::ToolbarTitleDisplayMode::inline_large:
      return theme::Font::title2;
    case composition::ToolbarTitleDisplayMode::large:
      return theme::Font::large_title;
    case composition::ToolbarTitleDisplayMode::automatic:
    case composition::ToolbarTitleDisplayMode::inline_title:
      break;
  }
  return inline_font;
}

auto ToolbarLayout::titled(std::optional<std::string> title,
                           const std::optional<std::string>& subtitle,
                           theme::Font font, bool large) const
    -> std::optional<composition::Element> {
  const Side title_side = large ? Side::large_title : Side::title;
  const Side subtitle_side = large ? Side::large_subtitle : Side::subtitle;
  const bool custom_title = has(title_side);
  const bool custom_subtitle = has(subtitle_side);
  if (!custom_title && !custom_subtitle) {
    return heading(std::move(title), subtitle, font, title_menu());
  }
  std::vector<composition::Element> lines;
  if (custom_title) {
    lines.push_back(components::HStack(
        components::HStackOptions{
            .alignment = components::VerticalAlignment::center,
            .spacing = theme::space::sm},
        flattened(title_side, title_gap)));
  } else if (title.has_value()) {
    lines.push_back(
        components::Text(text::LocalizedStringKey{std::move(*title)})
            .font(font)
            .font_weight(theme::Font::Weight::bold)
            .line_limit(1));
  }
  if (custom_subtitle) {
    lines.push_back(components::HStack(
                        components::HStackOptions{
                            .alignment = components::VerticalAlignment::center,
                            .spacing = theme::space::sm},
                        flattened(subtitle_side, title_gap))
                        .font(theme::Font::subheadline)
                        .foreground_style(theme::ColourRole::text_secondary));
  } else if (subtitle.has_value()) {
    lines.push_back(components::Text(text::LocalizedStringKey{*subtitle})
                        .font(theme::Font::subheadline)
                        .foreground_style(theme::ColourRole::text_secondary)
                        .line_limit(1));
  }
  composition::Element stacked = components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::none},
      std::move(lines));
  if (Builder menu = title_menu(); menu != nullptr) {
    return composition::Element{TitleMenu{std::move(stacked), std::move(menu)}}
        .id(title_menu_key);
  }
  return stacked;
}

auto ToolbarLayout::heading(std::optional<std::string> title,
                            const std::optional<std::string>& subtitle,
                            theme::Font font, Builder menu)
    -> std::optional<composition::Element> {
  if (!title.has_value() && !subtitle.has_value()) {
    return std::nullopt;
  }
  std::vector<composition::Element> lines;
  if (title.has_value()) {
    lines.push_back(
        components::Text(text::LocalizedStringKey{std::move(*title)})
            .font(font)
            .font_weight(theme::Font::Weight::bold)
            .line_limit(1));
  }
  if (subtitle.has_value()) {
    lines.push_back(components::Text(text::LocalizedStringKey{*subtitle})
                        .font(theme::Font::subheadline)
                        .foreground_style(theme::ColourRole::text_secondary)
                        .line_limit(1));
  }
  composition::Element stacked = components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::none},
      std::move(lines));
  if (menu != nullptr) {
    return composition::Element{TitleMenu{std::move(stacked), std::move(menu)}}
        .id(title_menu_key);
  }
  return stacked;
}

auto ToolbarLayout::metrics(const theme::Theme& current) noexcept -> Metrics {
  return Metrics{
      .height = current.metric(theme::MetricToken::toolbar_group_height),
      .padding = current.metric(theme::MetricToken::toolbar_group_padding),
      .spacing = current.metric(theme::MetricToken::toolbar_group_spacing),
      .inset = current.metric(theme::MetricToken::toolbar_group_inset),
      .gap = current.metric(theme::MetricToken::toolbar_group_gap)};
}

auto ToolbarLayout::styled(composition::Element bar,
                           const composition::NavigationChrome::Bar& style)
    -> composition::Element {
  if (style.foreground.has_value()) {
    bar = std::move(bar).foreground_style(*style.foreground);
  }
  if (style.background.has_value() &&
      style.background_visibility != composition::Visibility::hidden) {
    bar = std::move(bar).background(*style.background);
  }
  if (style.color_scheme.has_value()) {
    bar = std::move(bar).environment(components::Environment::colour_scheme,
                                     style.color_scheme);
  }
  return bar;
}

void ToolbarLayout::capsules(std::vector<composition::Element>& items) {
  for (composition::Element& item : items) {
    item = std::move(item).content_shape(
        composition::ContentShapeKinds::focus_effect, theme::Shape::capsule());
  }
}

auto ToolbarLayout::group(std::vector<composition::Element> items,
                          const Metrics& metrics) -> composition::Element {
  return group(Segment{.items = std::move(items)}, metrics);
}

auto ToolbarLayout::group(Segment segment, const Metrics& metrics)
    -> composition::Element {
  capsules(segment.items);
  composition::Element row =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::Space{metrics.spacing}},
          std::move(segment.items))
          .padding(theme::Insets::symmetric(segment.tight
                                                ? theme::space::none
                                                : theme::Space{metrics.padding},
                                            theme::space::none))
          .frame({.min_height = metrics.height});
  if (segment.bare) {
    return row;
  }
  return std::move(row).default_glass(theme::GlassSlot::toolbar,
                                      theme::Shape::capsule());
}

auto ToolbarLayout::grouped(Side side, const Metrics& metrics) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> groups;
  for (Segment& segment : segments(side)) {
    const bool flexible = segment.flexible_before;
    if (flexible) {
      groups.push_back(components::Spacer());
    }
    groups.push_back(group(std::move(segment), metrics));
  }
  return groups;
}

auto ToolbarLayout::Overflow::operator==(const Overflow& other) const -> bool {
  return items_ == other.items_ && customizable_ == other.customizable_ &&
         label_ == other.label_ &&
         customization_.has_value() == other.customization_.has_value() &&
         (!customization_.has_value() ||
          customization_->identity() == other.customization_->identity());
}

auto ToolbarLayout::Overflow::body(composition::Context& context) const
    -> composition::Element {
  const auto open = context.state(shown);
  const auto editor = context.state(editing);
  composition::Element face =
      label_ != nullptr
          ? composition::Element{composition::PlaceholderContentView(label_)}
          : composition::Element{
                components::Image::system_name(std::string{overflow_symbol})
                    .font(theme::Font::title3)};
  composition::Element button =
      components::Button(
          [open, items = items_] {
            for (const auto& item : items) {
              if (item->builder() == nullptr && item->content() != nullptr) {
                item->content()->renew();
              }
            }
            open.set(true);
          },
          std::move(face))
          .popover(
              open, composition::PopoverAttachmentAnchor::rect(),
              theme::Edge::top,
              [items = items_, open, editor,
               customizes =
                   customization_.has_value() && !customizable_.empty()] {
                std::vector<composition::Element> rows;
                rows.reserve(items.size() + 1);
                for (const auto& item : items) {
                  if (item->builder() != nullptr) {
                    rows.push_back((*item->builder())());
                  } else {
                    rows.push_back(
                        composition::PlaceholderContentView(item->content()));
                  }
                }
                if (customizes) {
                  rows.push_back(
                      components::Button(std::string{customize_label}, [editor,
                                                                        open] {
                        open.set(false);
                        editor.set(true);
                      }).id(std::string{customize_key}));
                }
                return MenuPanel::items(components::Group(std::move(rows)),
                                        open);
              });
  if (!customization_.has_value() || customizable_.empty()) {
    return button;
  }
  return std::move(button).sheet(
      composition::Binding<bool>{editor},
      [customization = *customization_, items = customizable_, editor] {
        return composition::Element{Editor{customization, items, editor}};
      });
}

auto ToolbarLayout::Editor::operator==(const Editor& other) const -> bool {
  return items_ == other.items_ &&
         customization_.identity() == other.customization_.identity() &&
         editing_.identity() == other.editing_.identity();
}

auto ToolbarLayout::Editor::row_key(std::string_view id)
    -> composition::ElementKey {
  return composition::ElementKey{std::format("arisa.toolbar.editor.{}", id)};
}

auto ToolbarLayout::Editor::body(composition::Context&) const
    -> composition::Element {
  const composition::ToolbarCustomization current = customization_.get();
  std::vector<composition::Element> rows;
  rows.reserve(items_.size() + 1);
  for (const auto& item : items_) {
    const composition::Visibility picked = current.visibility(item->id());
    const bool visible =
        picked == composition::Visibility::visible ||
        (picked == composition::Visibility::automatic &&
         item->default_visibility() != composition::Visibility::hidden);
    composition::Element preview =
        item->builder() != nullptr
            ? (*item->builder())()
            : composition::Element{components::Text(item->id())};
    rows.push_back(
        components::Button(
            [binding = customization_, id = item->id(), visible] {
              binding.update(
                  [&id, visible](composition::ToolbarCustomization& held) {
                    held.set_visibility(
                        id, visible ? composition::Visibility::hidden
                                    : composition::Visibility::visible);
                  });
            },
            components::HStack(
                components::HStackOptions{
                    .alignment = components::VerticalAlignment::center,
                    .spacing = theme::space::md},
                components::Image::system_name(
                    visible ? "check_circle" : "radio_button_unchecked")
                    .foreground_style(
                        visible ? theme::ColourValue{theme::ColourRole::accent}
                                : theme::ColourValue{theme::ColourRole::
                                                         tertiary_label}),
                std::move(preview).allows_hit_testing(false).environment(
                    composition::ToolbarEnvironment::customizing, true),
                components::Spacer())
                .frame({.max_width = composition::infinity})
                .opacity(visible ? 1.0F : 0.6F))
            .button_style(components::ButtonStyle::plain())
            .frame({.max_width = composition::infinity})
            .id(row_key(item->id())));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::HStack(
                 components::HStackOptions{
                     .alignment = components::VerticalAlignment::center,
                     .spacing = theme::space::md},
                 components::Text(
                     text::LocalizedStringKey{std::string{customize_label}})
                     .font(theme::Font::title3)
                     .font_weight(theme::Font::Weight::bold),
                 components::Spacer(),
                 components::Button(
                     std::string{editor_reset_label},
                     [binding = customization_] {
                       binding.update(
                           [](composition::ToolbarCustomization& held) {
                             held.reset();
                           });
                     })
                     .id("arisa.toolbar.editor.reset"),
                 components::Button(
                     std::string{editor_done_label},
                     [editing = editing_] { editing.set(false); })
                     .button_style(
                         components::ButtonStyle::bordered_prominent())
                     .id("arisa.toolbar.editor.done")),
             components::ScrollView(
                 components::VStack(
                     components::VStackOptions{
                         .alignment = components::HorizontalAlignment::leading,
                         .spacing = theme::space::sm},
                     std::move(rows))
                     .frame({.max_width = composition::infinity}))
                 .frame({.max_width = composition::infinity,
                         .max_height = composition::infinity}))
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading})
      .id(editor_key);
}

auto ToolbarLayout::Fitted::operator==(const Fitted& other) const -> bool {
  return units_ == other.units_ && overflow_ == other.overflow_ &&
         customizable_ == other.customizable_ && gap_ == other.gap_ &&
         customization_.has_value() == other.customization_.has_value() &&
         (!customization_.has_value() ||
          customization_->identity() == other.customization_->identity());
}

auto ToolbarLayout::Fitted::kept(const std::vector<Unit>& units,
                                 const std::vector<float>& measured,
                                 float space, float more, float gap,
                                 bool overflowing) -> std::vector<bool> {
  std::vector<bool> keep(units.size(), true);
  if (space <= 0.0F || measured.size() != units.size()) {
    return keep;
  }
  const auto width = [&measured](std::size_t index) { return measured[index]; };
  float total = 0.0F;
  for (std::size_t index = 0; index < units.size(); ++index) {
    total += width(index) + (index == 0 ? 0.0F : gap);
  }
  if (overflowing) {
    total += more + gap;
  }
  if (total <= space) {
    return keep;
  }
  std::vector<std::size_t> order(units.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::ranges::stable_sort(
      order, [&units](std::size_t left, std::size_t right) {
        if (units[left].pinned != units[right].pinned) {
          return units[left].pinned;
        }
        if (units[left].priority != units[right].priority) {
          return units[left].priority > units[right].priority;
        }
        return left > right;
      });
  float budget = space - (more + gap);
  bool first = true;
  for (const std::size_t index : order) {
    const float need = width(index) + (first ? 0.0F : gap);
    if (units[index].pinned || need <= budget) {
      budget -= need;
      first = false;
      continue;
    }
    keep[index] = false;
  }
  return keep;
}

auto ToolbarLayout::Fitted::measured(
    composition::Element unit, std::size_t index,
    composition::StateHandle<std::vector<float>> sizes)
    -> composition::Element {
  return std::move(unit).on_geometry_change(
      std::type_identity<float>{},
      [](const composition::GeometryProxy& proxy) {
        return proxy.size().width;
      },
      [sizes, index](float width) {
        const std::vector<float>& known = sizes.get();
        if (index < known.size() && known[index] == width) {
          return;
        }
        sizes.update([index, width](std::vector<float>& kept) {
          if (kept.size() <= index) {
            kept.resize(index + 1, 0.0F);
          }
          kept[index] = width;
        });
      });
}

auto ToolbarLayout::Fitted::collapsed(const Unit& unit)
    -> composition::Element {
  return composition::Element{
      Overflow{unit.items, std::nullopt, {}, unit.label}};
}

auto ToolbarLayout::Fitted::body(composition::Context& context) const
    -> composition::Element {
  const auto sizes = context.state(widths);
  const auto space = context.state(room);
  const auto more = context.state(more_width);
  if (sizes.get().size() > units_.size()) {
    sizes.set(std::vector<float>(units_.size(), 0.0F));
  }
  std::vector<float> known = sizes.get();
  known.resize(units_.size(), 0.0F);
  const bool extra = !overflow_.empty() ||
                     (customization_.has_value() && !customizable_.empty());
  const std::vector<bool> keep =
      kept(units_, known, space.get(), more.get(), gap_, extra);
  Items spilled = overflow_;
  std::vector<composition::Element> row;
  row.reserve(units_.size() + 2);
  for (std::size_t index = 0; index < units_.size(); ++index) {
    const Unit& unit = units_[index];
    if (!keep[index] && unit.label == nullptr) {
      for (const auto& item : unit.items) {
        spilled.push_back(item);
      }
      continue;
    }
    if (unit.flexible_before) {
      row.push_back(components::Spacer());
    }
    if (!keep[index]) {
      row.push_back(collapsed(unit).id(composition::ElementKey{
          std::format("arisa.toolbar.collapsed.{}", index)}));
      continue;
    }
    std::vector<composition::Element> parts;
    parts.reserve(unit.items.size());
    for (const auto& item : unit.items) {
      parts.push_back(item->builder() != nullptr
                          ? (*item->builder())().environment(
                                composition::ToolbarEnvironment::vertical_edge,
                                std::optional<composition::VerticalEdge>{
                                    composition::VerticalEdge::top})
                          : shown(*item, Side::trailing));
    }
    row.push_back(
        measured(components::HStack(
                     components::HStackOptions{
                         .alignment = components::VerticalAlignment::center,
                         .spacing = theme::Space{gap_}},
                     std::move(parts)),
                 index, sizes)
            .id(composition::ElementKey{
                std::format("arisa.toolbar.unit.{}", index)}));
  }
  if (!spilled.empty() ||
      (customization_.has_value() && !customizable_.empty())) {
    row.push_back(composition::Element{
        Overflow{std::move(spilled), customization_, customizable_}}
                      .on_geometry_change(
                          std::type_identity<float>{},
                          [](const composition::GeometryProxy& proxy) {
                            return proxy.size().width;
                          },
                          [more](float width) {
                            if (more.get() != width) {
                              more.set(width);
                            }
                          })
                      .id(overflow_key));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::Space{gap_}},
             std::move(row))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::trailing})
      .on_geometry_change(
          std::type_identity<float>{},
          [](const composition::GeometryProxy& proxy) {
            return proxy.size().width;
          },
          [space](float width) {
            if (space.get() != width) {
              space.set(width);
            }
          });
}

ToolbarLayout::TitleMenu::TitleMenu(composition::Element title, Builder menu)
    : title_(std::make_shared<composition::PlaceholderContentView::Source>(
          std::move(title))),
      menu_(std::move(menu)) {}

auto ToolbarLayout::TitleMenu::body(composition::Context& context) const
    -> composition::Element {
  const auto open = context.state(shown);
  return components::Button(
             [open] { open.set(true); },
             components::HStack(
                 components::HStackOptions{
                     .alignment = components::VerticalAlignment::center,
                     .spacing = theme::space::xs},
                 composition::PlaceholderContentView(title_),
                 components::Glyph::chevron_down(
                     theme::ColourRole::text_secondary, 10.0F)))
      .popover(
          open, composition::PopoverAttachmentAnchor::rect(), theme::Edge::top,
          [menu = menu_, open] { return MenuPanel::items((*menu)(), open); });
}

}
