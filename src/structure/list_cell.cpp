#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/background_environment.hpp>
#include <arisa/composition/content_shape_kinds.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/tint.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/help.hpp>
#include <arisa/structure/cell_style.hpp>
#include <arisa/structure/list_cell.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/row_selection.hpp>
#include <arisa/structure/swipe_row.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <type_traits>
#include <utility>

namespace arisa::structure {

const composition::ListRow ListCell::Traits::none{};

ListCell::ListCell(composition::Element content, Placement placement,
                   Traits traits)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(content)),
          std::move(placement), traits, std::nullopt)) {}

ListCell::ListCell(composition::Element content, Placement placement,
                   Traits traits, Origin origin)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(content)),
          std::move(placement), traits, std::move(origin))) {}

auto ListCell::fills(const Traits& traits, const Placement& placement) noexcept
    -> bool {
  return traits.activatable && !placement.edits.move && !placement.edits.remove;
}

auto ListCell::permitted(const components::ForEach::Edits& edits,
                         const Traits& traits) -> components::ForEach::Edits {
  const composition::ListRow& row = traits.attributes();
  return components::ForEach::Edits{
      .move =
          row.move_disabled ? components::ForEach::MoveAction{} : edits.move,
      .remove = row.delete_disabled ? components::ForEach::DeleteAction{}
                                    : edits.remove};
}

auto ListCell::shows_separator(const Traits& traits,
                               const Placement& placement) noexcept -> bool {
  return traits.attributes().top.visibility !=
             composition::Visibility::hidden &&
         placement.above.visibility != composition::Visibility::hidden;
}

auto ListCell::targeted(composition::Element content, Traits traits,
                        const Placement& placement) -> composition::Element {
  if (!fills(traits, placement)) {
    return content;
  }
  if (presentation::Help::label(content).has_value()) {
    return presentation::Help::relabel(
        std::move(content), [&traits, &placement](composition::Element label) {
          return targeted(std::move(label), traits, placement);
        });
  }
  if (controls::ContextMenu::content(content).has_value()) {
    return controls::ContextMenu::relabel(
        std::move(content), [](composition::Element label) {
          return std::move(label)
              .padding(CellStyle::insets())
              .frame({.max_width = composition::infinity,
                      .min_height = CellStyle::minimum_height});
        });
  }
  return std::move(content)
      .padding(CellStyle::insets())
      .frame({.max_width = composition::infinity,
              .min_height = CellStyle::minimum_height});
}

auto ListCell::Content::placed_like(const Placement& other) const noexcept
    -> bool {
  return placement_.is_first == other.is_first &&
         placement_.is_last == other.is_last &&
         placement_.index == other.index && placement_.count == other.count &&
         static_cast<bool>(placement_.edits.move) ==
             static_cast<bool>(other.edits.move) &&
         static_cast<bool>(placement_.edits.remove) ==
             static_cast<bool>(other.edits.remove) &&
         placement_.above == other.above &&
         placement_.position == other.position &&
         placement_.indent == other.indent;
}

auto ListCell::Content::same_as(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& next = static_cast<const Content&>(other);
  return traits_ == next.traits_ && placed_like(next.placement_);
}

auto ListCell::Content::repeats(const Content& other) const -> bool {
  return origin_.has_value() && other.origin_.has_value() &&
         origin_->each != nullptr && other.origin_->each != nullptr &&
         origin_->each->same_item(origin_->index, *other.origin_->each,
                                  other.origin_->index);
}

auto ListCell::Content::adopt(composition::ElementContent& incoming) -> bool {
  auto& newer = static_cast<Content&>(incoming);
  placement_.edits = std::move(newer.placement_.edits);
  traits_ = std::move(newer.traits_);
  const bool unchanged = repeats(newer);
  origin_ = std::move(newer.origin_);
  if (unchanged) {
    return false;
  }
  content_->forward(*newer.content_);
  return true;
}

auto ListCell::Content::ignores_change(
    composition::Identity key,
    const composition::EnvironmentOverride::Value& before,
    const composition::EnvironmentOverride::Value& after) const -> bool {
  if (key != RowSelection::key.identity()) {
    return false;
  }
  using Selected =
      composition::EnvironmentOverride::Typed<std::optional<RowSelection>>;
  const std::optional<RowSelection>& was =
      static_cast<const Selected&>(before).get();
  const std::optional<RowSelection>& now =
      static_cast<const Selected&>(after).get();
  return was.has_value() && now.has_value() && was->agrees(*now, traits_.tag);
}

auto ListCell::minimum_height(const ListStyle& list,
                              composition::SidebarRowSize size,
                              std::optional<float> declared) noexcept
    -> std::optional<float> {
  if (declared.has_value() || list.kind() != ListStyle::Kind::sidebar) {
    return declared;
  }
  switch (size) {
    case composition::SidebarRowSize::small:
      return small_sidebar_row;
    case composition::SidebarRowSize::large:
      return large_sidebar_row;
    case composition::SidebarRowSize::medium:
      break;
  }
  return declared;
}

auto ListCell::targets(const std::optional<RowSelection>& selection,
                       const std::shared_ptr<const composition::TagValue>& tag)
    -> controls::ContextMenu::ForSelection::Tags {
  controls::ContextMenu::ForSelection::Tags tags;
  if (selection.has_value() && selection->multiple() &&
      selection->selects(tag)) {
    for (const composition::TagKey& key : selection->chosen()) {
      tags.push_back(key.tag());
    }
    return tags;
  }
  tags.push_back(tag);
  return tags;
}

auto ListCell::default_transition() noexcept -> motion::Transition {
  return motion::Transition::opacity().combined(
      motion::Transition::scale(0.92F, motion::UnitPoint::top));
}

auto ListCell::traits_of(const composition::Element& element) -> Traits {
  Traits traits{.button = false,
                .discloses = false,
                .activatable = element.content().activatable(),
                .tag = element.tag(),
                .row = element.list_row()};
  if (const auto menu = controls::ContextMenu::content(element)) {
    traits.activatable = menu->get().wrapped().activatable;
    traits.button = menu->get().wrapped().button;
    traits.discloses = menu->get().wrapped().discloses;
    traits.menu = menu->get().items();
    traits.menu_preview = menu->get().preview();
    return traits;
  }
  if (const auto label = presentation::Help::label(element)) {
    Traits wrapped = traits_of(label->get());
    traits.activatable = wrapped.activatable;
    traits.button = wrapped.button;
    traits.discloses = wrapped.discloses;
    traits.menu = std::move(wrapped.menu);
    traits.menu_preview = std::move(wrapped.menu_preview);
    return traits;
  }
  if (element.type() ==
      composition::ElementType::of<components::Button::Content>()) {
    traits.button = true;
    traits.discloses =
        static_cast<const components::Button::Content&>(element.content())
            .discloses();
  }
  if (const auto declared = element.declared_environment(
          composition::NavigationDeclarations::link_indicator_visibility);
      declared.has_value() &&
      declared->get() == composition::Visibility::hidden) {
    traits.discloses = false;
  }
  return traits;
}

auto ListCell::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

void ListCell::Content::shift(std::size_t target) const {
  const std::size_t from = placement_.index;
  if (!placement_.edits.move || target == from || target >= placement_.count) {
    return;
  }
  const std::size_t destination = target > from ? target + 1 : target;
  motion::with_animation(
      motion::Animation::snappy(0.3), [this, from, destination] {
        placement_.edits.move(components::IndexSet{from}, destination);
      });
}

void ListCell::Content::remove() const {
  if (!placement_.edits.remove) {
    return;
  }
  const std::size_t index = placement_.index;
  motion::with_animation(motion::Animation::smooth(0.3), [this, index] {
    placement_.edits.remove(components::IndexSet{index});
  });
}

auto ListCell::Content::handle(composition::StateHandle<bool> up,
                               composition::StateHandle<float> drag,
                               composition::StateHandle<float> height) const
    -> composition::Element {
  const std::size_t index = placement_.index;
  const std::size_t count = placement_.count;
  return components::Image::system_name("drag_indicator")
      .font(theme::Font::title3)
      .foreground_style(theme::ColourRole::text_secondary)
      .padding(theme::space::xs)
      .content_shape(theme::Shape::rectangle())
      .high_priority_gesture(
          composition::DragGesture(0.0F, composition::CoordinateSpace::global)
              .on_changed(
                  [up, drag](const composition::DragGesture::Value& value) {
                    up.set(true);
                    drag.set(value.translation.height);
                  })
              .on_ended([this, up, drag, height, index,
                         count](const composition::DragGesture::Value& value) {
                const float row = std::max(height.get(), 1.0F);
                const long steps = std::lround(value.translation.height / row);
                const long wanted =
                    std::clamp(static_cast<long>(index) + steps, 0L,
                               static_cast<long>(count) - 1L);
                const auto target = static_cast<std::size_t>(wanted);
                motion::with_animation(motion::Animation::snappy(0.3), [&] {
                  const components::ForEach::MoveAction& move =
                      placement_.edits.move;
                  if (move && target != index) {
                    move(components::IndexSet{index},
                         target > index ? target + 1 : target);
                  }
                  up.set(false);
                  drag.set(0.0F);
                });
              }))
      .id("arisa.list.handle");
}

auto ListCell::Content::badge(composition::BadgeProminence prominence) const
    -> std::optional<composition::Element> {
  const std::optional<std::string>& label = traits_.attributes().badge;
  if (!label.has_value()) {
    return std::nullopt;
  }
  switch (prominence) {
    case composition::BadgeProminence::increased:
      return components::Text(*label)
          .font(theme::Font::subheadline)
          .foreground_style(theme::ColourRole::on_accent)
          .line_limit(1)
          .padding(theme::Insets::symmetric(theme::Space{badge_padding},
                                            theme::space::none))
          .frame({.min_width = badge_extent, .min_height = badge_extent})
          .background(theme::ColourRole::red, theme::Shape::capsule())
          .id("arisa.list.badge");
    case composition::BadgeProminence::decreased:
      return components::Text(*label)
          .foreground_style(theme::ColourRole::tertiary_label)
          .line_limit(1)
          .id("arisa.list.badge");
    case composition::BadgeProminence::standard:
      break;
  }
  return components::Text(*label)
      .foreground_style(theme::ColourRole::text_secondary)
      .line_limit(1)
      .id("arisa.list.badge");
}

auto ListCell::Content::separator_tint() const
    -> std::optional<theme::ColourValue> {
  if (traits_.attributes().top.tint.has_value()) {
    return traits_.attributes().top.tint;
  }
  return placement_.above.tint;
}

auto ListCell::Content::row(bool editing, bool choosing, bool chosen,
                            bool contained,
                            composition::BadgeProminence prominence,
                            composition::StateHandle<bool> up,
                            composition::StateHandle<float> drag,
                            composition::StateHandle<float> height) const
    -> composition::Element {
  std::vector<composition::Element> items;
  if (choosing) {
    items.push_back(
        components::Image::system_name(chosen ? "check_circle"
                                              : "radio_button_unchecked")
            .symbol_variant(chosen ? composition::SymbolVariants::fill
                                   : composition::SymbolVariants{})
            .font(theme::Font::title3)
            .foreground_style(chosen ? theme::ColourRole::accent
                                     : theme::ColourRole::tertiary_label)
            .padding(theme::space::xxs)
            .id("arisa.list.select"));
  } else if (editing && placement_.edits.remove) {
    items.push_back(
        components::Button([this] { remove(); },
                           components::Image::system_name("remove_circle")
                               .font(theme::Font::title3)
                               .foreground_style(theme::ColourRole::destructive)
                               .padding(theme::space::xxs))
            .button_style(components::ButtonStyle::plain())
            .id("arisa.list.delete"));
  }
  composition::Element content =
      composition::Element{composition::PlaceholderContentView(content_)}.id(
          "arisa.list.content");
  if (choosing) {
    content = std::move(content).disabled().environment(
        composition::Enablement::is_enabled, true);
  }
  if (traits_.menu) {
    content =
        std::move(content).environment(controls::ContextMenu::hosted, true);
  }
  if (contained) {
    content = std::move(content).environment(
        composition::ListEnvironment::swipe_actions_container, false);
  }
  items.push_back(std::move(content));
  std::optional<composition::Element> shown = badge(prominence);
  if (shown.has_value() || placement_.edits.move) {
    items.push_back(components::Spacer().id("arisa.list.spacer"));
  }
  if (shown.has_value()) {
    items.push_back(std::move(*shown));
  }
  if (placement_.edits.move) {
    items.push_back(handle(up, drag, height));
  }
  composition::Element line = components::HStack(
      components::HStackOptions{
          .alignment = components::VerticalAlignment::center,
          .spacing = theme::space::sm},
      std::move(items));
  if (placement_.indent > 0.0F) {
    line = std::move(line).padding(theme::Edge::leading,
                                   theme::Space{placement_.indent});
  }
  return line;
}

auto ListCell::Content::choosing(composition::Element body,
                                 const RowSelection& selection,
                                 bool editing) const -> composition::Element {
  if (!editing) {
    return body;
  }
  const auto flip = [selection, tag = traits_.tag] { selection.toggle(tag); };
  return std::move(body)
      .on_command(components::CommandMapping::select,
                  std::string{components::CommandMapping::select_label}, flip)
      .on_tap_gesture(flip);
}

auto ListCell::Content::commanded(composition::Element body,
                                  composition::StateHandle<bool> up) const
    -> composition::Element {
  const components::ForEach::Edits& edits = placement_.edits;
  if (edits.remove) {
    body = std::move(body).on_command(
        delete_button, std::string{components::CommandMapping::remove_label},
        [this] { remove(); });
  }
  if (!edits.move) {
    return body;
  }
  if (!up.get()) {
    return std::move(body).on_command(
        move_button, std::string{components::CommandMapping::move_label},
        [up] { up.set(true); });
  }
  const auto drop = [up] {
    motion::with_animation(motion::Animation::snappy(0.25),
                           [&up] { up.set(false); });
  };
  const std::string label{components::CommandMapping::drop_label};
  return std::move(body)
      .on_command(move_button, label, drop)
      .on_command(components::CommandMapping::select, label, drop)
      .on_exit_command(label, drop)
      .on_move_command([this](composition::MoveCommandDirection direction) {
        const std::size_t index = placement_.index;
        if (direction == composition::MoveCommandDirection::up && index > 0) {
          shift(index - 1);
        } else if (direction == composition::MoveCommandDirection::down) {
          shift(index + 1);
        }
      });
}

auto ListCell::Content::swiping(composition::Element body) const
    -> composition::Element {
  const composition::ListRow& attributes = traits_.attributes();
  if (!attributes.leading.content && !attributes.trailing.content) {
    return body;
  }
  return SwipeRow(std::move(body), attributes.leading, attributes.trailing)
      .id("arisa.list.swipe");
}

auto ListCell::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const ListStyle& list = context.environment(ListStyle::key);
  const CellStyle& style = context.environment(CellStyle::key);
  const bool editing =
      context.environment(components::Environment::edit_mode) !=
      components::EditMode::inactive;
  const auto up = context.state(lifted);
  const auto drag = context.state(dragged);
  const auto height = context.state(extent);
  const bool raised = up.get();
  const std::optional<RowSelection>& selecting =
      context.environment(RowSelection::key);
  const composition::ListRow& attributes = traits_.attributes();
  const bool selectable = selecting.has_value() && traits_.tag != nullptr &&
                          !attributes.selection_disabled;
  const bool multiple = selectable && selecting->multiple();
  const bool picking = multiple && editing;
  const bool chosen = selecting.has_value() && selecting->selects(traits_.tag);
  const bool isolated =
      context.environment(composition::ListEnvironment::row_spacing)
          .value_or(0.0F) > 0.0F;
  const bool alternate =
      context
          .environment(
              composition::ListEnvironment::alternating_row_background_behavior)
          .alternates(
              context.environment(
                  composition::ListEnvironment::alternating_row_backgrounds) ||
              list.alternates_row_backgrounds()) &&
      placement_.position % 2 == 1;
  std::optional<composition::Element> background;
  if (attributes.background != nullptr) {
    background = composition::Element{
        composition::PlaceholderContentView(attributes.background)};
  } else if (alternate) {
    background =
        components::ZStack()
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity})
            .background(
                theme::ColourRole::alternating_content_background_alternate);
  }
  const composition::EdgeLengths& margins =
      context.environment(composition::ListEnvironment::section_margins);
  const composition::ListRow::Separator& section_top =
      context.environment(composition::ListEnvironment::section_separator_top);
  const composition::ListRow::Separator& section_bottom = context.environment(
      composition::ListEnvironment::section_separator_bottom);
  const bool opens_section = placement_.is_first && placement_.index == 0;
  const bool closes_section =
      placement_.is_last && placement_.index + 1 == placement_.count;
  std::optional<theme::Insets> insets = attributes.insets;
  if (!attributes.edge_insets.empty()) {
    insets = attributes.edge_insets.over(
        attributes.insets.value_or(CellStyle::insets(list)));
  }
  composition::Element body = style.make_body(CellStyle::Configuration{
      .content = row(editing, picking, chosen,
                     context.environment(
                         composition::ListEnvironment::swipe_actions_container),
                     attributes.badge_prominence.value_or(context.environment(
                         composition::ListEnvironment::badge_prominence)),
                     up, drag, height),
      .list_style = list,
      .is_first = isolated || placement_.is_first,
      .is_last = isolated || placement_.is_last,
      .is_focused = context.is_focused(),
      .is_selected = chosen && (!multiple || editing),
      .is_lifted = raised,
      .shows_disclosure =
          traits_.discloses && !picking &&
          context.environment(
              composition::NavigationDeclarations::link_indicator_visibility) !=
              composition::Visibility::hidden,
      .fills = fills(traits_, placement_) && !picking,
      .insets = insets,
      .shows_separator =
          !isolated && shows_separator(traits_, placement_) &&
          !(opens_section &&
            section_top.visibility == composition::Visibility::hidden),
      .separator_tint = separator_tint(),
      .background = std::move(background),
      .minimum_height = minimum_height(
          list,
          context.environment(composition::ListEnvironment::sidebar_row_size),
          context.environment(
              composition::ListEnvironment::default_min_row_height)),
      .corner_radius = context.environment(
          composition::ListEnvironment::card_corner_radius)});
  if (chosen && (!multiple || editing)) {
    body = std::move(body).environment(
        composition::BackgroundEnvironment::prominence,
        composition::BackgroundProminence::increased);
  }
  if (list.kind() == ListStyle::Kind::sidebar) {
    switch (
        context.environment(composition::ListEnvironment::sidebar_row_size)) {
      case composition::SidebarRowSize::small:
        body = std::move(body).font(theme::Font::subheadline);
        break;
      case composition::SidebarRowSize::large:
        body = std::move(body).font(theme::Font::title3);
        break;
      case composition::SidebarRowSize::medium:
        break;
    }
  }
  if (attributes.item_tint.has_value()) {
    const composition::ListItemTint tint = *attributes.item_tint;
    switch (tint.kind()) {
      case composition::ListItemTint::Kind::fixed:
        body = std::move(body).tint(tint.colour());
        break;
      case composition::ListItemTint::Kind::preferred:
        if (!context.environment(composition::Tint::key).has_value()) {
          body = std::move(body).tint(tint.colour());
        }
        break;
      case composition::ListItemTint::Kind::monochrome: {
        const theme::Theme& current =
            context.environment(components::Environment::theme);
        body = std::move(body).tint(
            current.colour(theme::ColourRole::gray, current.scheme()));
        break;
      }
    }
  }
  if (!traits_.activatable || picking) {
    body = std::move(body)
               .focus_fallback()
               .focus_ring_inset(focus_inset)
               .content_shape(composition::ContentShapeKinds::focus_effect,
                              theme::Shape::rounded(theme::radius::card, true));
    if (selectable && !multiple) {
      body = selecting->row(std::move(body), traits_.tag);
    }
  }
  if (picking) {
    body = choosing(std::move(body), *selecting, editing);
  }
  if (traits_.menu && !picking) {
    body = controls::ContextMenu::attach(std::move(body),
                                         context.state(menu_shown),
                                         traits_.menu, traits_.menu_preview);
  } else if (const std::shared_ptr<const controls::ContextMenu::ForSelection>&
                 for_selection =
                     context.environment(controls::ContextMenu::selection);
             for_selection != nullptr && traits_.tag != nullptr && !picking) {
    controls::ContextMenu::ForSelection::Tags tags =
        targets(selecting, traits_.tag);
    if (for_selection->has_primary() && !editing) {
      const auto act = [for_selection, tags] { for_selection->perform(tags); };
      body = std::move(body)
                 .on_command(
                     components::CommandMapping::select,
                     std::string{components::CommandMapping::select_label}, act)
                 .on_tap_gesture(act);
    }
    body = controls::ContextMenu::attach(
        std::move(body), context.state(menu_shown),
        for_selection->items(std::move(tags)), std::nullopt);
  }
  body = swiping(std::move(body));
  if (opens_section &&
      section_top.visibility == composition::Visibility::visible) {
    body =
        std::move(body).overlay(composition::Alignment::top,
                                CellStyle::section_separator(section_top.tint));
  }
  if (closes_section &&
      section_bottom.visibility == composition::Visibility::visible) {
    body = std::move(body).overlay(
        composition::Alignment::bottom,
        CellStyle::section_separator(section_bottom.tint));
  }
  if (!margins.leading.has_value() && !margins.trailing.has_value()) {
    body = components::VStack(std::move(body))
               .padding(theme::Edge::horizontal, list.margin())
               .frame({.max_width = composition::infinity});
  } else {
    body = components::VStack(std::move(body))
               .padding(theme::Insets{
                   .top = theme::space::none,
                   .leading = margins.leading.has_value()
                                  ? theme::Space{*margins.leading}
                                  : list.margin(),
                   .bottom = theme::space::none,
                   .trailing = margins.trailing.has_value()
                                   ? theme::Space{*margins.trailing}
                                   : list.margin()})
               .frame({.max_width = composition::infinity});
  }
  if (attributes.hover.has_value()) {
    body = std::move(body).hover_effect(*attributes.hover);
  }
  if (attributes.hover_disabled) {
    body = std::move(body).hover_effect_disabled();
  }
  if (raised && drag.get() != 0.0F) {
    body = std::move(body).offset(0.0F, drag.get());
  }
  const components::ForEach::Edits& edits = placement_.edits;
  if (edits.move) {
    body = std::move(body).on_geometry_change(
        std::type_identity<float>{},
        [](const composition::GeometryProxy& proxy) {
          return proxy.size().height;
        },
        [height](float value) { height.set(value); });
  }
  std::vector<composition::Element> produced;
  produced.push_back(commanded(std::move(body), up));
  return produced;
}

}
