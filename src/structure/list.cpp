#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glass_panel.hpp>
#include <arisa/components/label_column.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/scroll_environment.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/row_selection.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/section_index.hpp>
#include <arisa/structure/sidebar_selection.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::structure {

List::List(std::vector<composition::Element> children)
    : List(std::optional<ListStyle>{}, std::move(children)) {}

List::List(std::optional<ListStyle> forced,
           std::vector<composition::Element> children)
    : Element(describe(forced, std::nullopt, std::move(children))) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

List::List(Selection selection, std::vector<composition::Element> children)
    : Element(
          describe(std::nullopt, std::move(selection), std::move(children))) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto List::describe(std::optional<ListStyle> forced,
                    std::optional<Selection> selection,
                    std::vector<composition::Element> children)
    -> std::unique_ptr<Content> {
  std::vector<Marker> marked = markers(children);
  return std::make_unique<Content>(forced, std::move(selection),
                                   sections(std::move(children)),
                                   std::move(marked));
}

auto List::markers(const std::vector<composition::Element>& children)
    -> std::vector<Marker> {
  std::vector<Marker> found;
  bool loose = false;
  for (const composition::Element& child : children) {
    if (Section::content(child).has_value() ||
        child.content().contributes_section()) {
      if (loose) {
        found.push_back(Marker{});
        loose = false;
      }
      const std::shared_ptr<const composition::ListRow> row = child.list_row();
      found.push_back(row != nullptr ? Marker{.spacing = row->section_spacing,
                                              .index_label = row->index_label}
                                     : Marker{});
      continue;
    }
    loose = true;
  }
  if (loose) {
    found.push_back(Marker{});
  }
  return found;
}

const composition::FocusState<composition::TagKey> List::row_focus{
    "list-row-focus"};

auto List::sections(std::vector<composition::Element> children) -> std::vector<
    std::shared_ptr<composition::PlaceholderContentView::Source>> {
  std::vector<std::shared_ptr<composition::PlaceholderContentView::Source>>
      found;
  std::vector<composition::Element> loose;
  const auto flush = [&found, &loose] {
    if (loose.empty()) {
      return;
    }
    found.push_back(
        std::make_shared<composition::PlaceholderContentView::Source>(
            Section(Section::Labels{}, std::exchange(loose, {}))));
  };
  for (composition::Element& child : children) {
    if (Section::content(child).has_value() ||
        child.content().contributes_section()) {
      flush();
      found.push_back(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(child)));
      continue;
    }
    loose.push_back(std::move(child));
  }
  flush();
  return found;
}

auto List::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto List::Content::indexed(composition::Context& context) const -> bool {
  return context.environment(
             composition::ListEnvironment::section_index_visibility) ==
             composition::Visibility::visible &&
         std::ranges::any_of(markers_, [](const Marker& marker) {
           return marker.index_label.has_value();
         });
}

auto List::Content::columns(composition::Context& context) const
    -> composition::Element {
  const composition::StateHandle<float> width = context.state(label_width);
  std::vector<composition::Element> column;
  column.reserve(sections_.size());
  for (std::size_t at = 0; at < sections_.size(); ++at) {
    column.push_back(
        composition::Element{composition::PlaceholderContentView(sections_[at])}
            .id(static_cast<std::int64_t>(at)));
  }
  composition::Element scroller =
      components::ScrollView(
          components::VStack(
              components::VStackOptions{
                  .alignment = components::HorizontalAlignment::leading,
                  .spacing = theme::Space{FormStyle::columns_section_spacing}},
              std::move(column))
              .padding(theme::Space{FormStyle::columns_margin})
              .frame({.max_width = composition::infinity,
                      .alignment = composition::Alignment::leading}))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .environment(components::LabelColumn::key,
                       std::optional<components::LabelColumn>{
                           components::LabelColumn{width}});
  if (context.environment(composition::ScrollEnvironment::content_background) ==
      composition::Visibility::visible) {
    scroller = std::move(scroller).background(
        theme::ColourRole::system_background, theme::Shape::rectangle());
  }
  return scroller;
}

auto List::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const FormStyle& form =
      form_ ? context.environment(FormStyle::key) : FormStyle::automatic();
  if (form.is_columns()) {
    std::vector<composition::Element> produced;
    produced.push_back(selecting(context, columns(context)));
    return produced;
  }
  const ListStyle style = forced_.value_or(context.environment(ListStyle::key));
  const std::optional<float> spaced =
      context.environment(composition::ListEnvironment::section_spacing);
  const bool custom = spaced.has_value() ||
                      std::ranges::any_of(markers_, [](const Marker& marker) {
                        return marker.spacing.has_value();
                      });
  const bool index = indexed(context);
  std::vector<composition::Element> column;
  column.reserve(sections_.size());
  std::vector<SectionIndex::Entry> entries;
  for (std::size_t at = 0; at < sections_.size(); ++at) {
    composition::Element section =
        composition::PlaceholderContentView(sections_[at]);
    if (index) {
      section = std::move(section).id(SectionIndex::key_of(at));
      if (at < markers_.size() && markers_[at].index_label.has_value()) {
        entries.push_back(
            SectionIndex::Entry{.label = *markers_[at].index_label,
                                .key = SectionIndex::key_of(at)});
      }
    } else {
      section = std::move(section).id(static_cast<std::int64_t>(at));
    }
    if (custom && at > 0) {
      const std::optional<float> before =
          at - 1 < markers_.size() ? markers_[at - 1].spacing : std::nullopt;
      const theme::Space gap =
          before.has_value() ? theme::Space{*before}
                             : (spaced.has_value() ? theme::Space{*spaced}
                                                   : style.section_spacing());
      section = std::move(section).padding(theme::Edge::top, gap);
    }
    column.push_back(std::move(section));
  }
  composition::Element stacked =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = custom ? theme::space::none : style.section_spacing()},
          std::move(column))
          .padding(theme::Edge::vertical,
                   style.kind() == ListStyle::Kind::plain ||
                           style.kind() == ListStyle::Kind::bordered
                       ? theme::space::none
                       : theme::space::md)
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading});
  if (index) {
    stacked =
        std::move(stacked).padding(theme::Edge::trailing, SectionIndex::width);
  }
  composition::Element scroller =
      components::ScrollView(std::move(stacked))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .environment(ListStyle::key, style)
          .button_style(components::ButtonStyle::plain());
  if (form.is_cards() && style.carded()) {
    scroller = std::move(scroller).environment(
        composition::ListEnvironment::card_corner_radius,
        std::optional<float>{
            components::GlassPanel::points(FormStyle::card_radius)});
  }
  const bool bordered = style.kind() == ListStyle::Kind::bordered;
  if (const std::optional<theme::ColourRole> painted = style.content_background(
          context.environment(
              composition::ScrollEnvironment::content_background),
          context.environment(components::Environment::theme))) {
    scroller =
        std::move(scroller).background(*painted, theme::Shape::rectangle());
  }
  if (bordered) {
    scroller =
        std::move(scroller).border(theme::ColourRole::separator, border_width);
  }
  if (index) {
    scroller = std::move(scroller).overlay(
        composition::Alignment::trailing,
        SectionIndex(std::move(entries), context.scroll_view_proxy()));
  }
  if (form.is_custom()) {
    scroller = form.make_body(
        FormStyle::Configuration{.content = std::move(scroller)});
  }
  std::vector<composition::Element> produced;
  produced.push_back(selecting(context, std::move(scroller)));
  return produced;
}

auto List::Content::selecting(composition::Context& context,
                              composition::Element scroller) const
    -> composition::Element {
  if (!selection_.has_value()) {
    return scroller;
  }
  const std::optional<SidebarSelection>& sidebar =
      context.environment(SidebarSelection::key);
  const composition::FocusBinding<composition::TagKey> focus =
      sidebar.has_value() ? sidebar->focus() : context.state(row_focus);
  if (sidebar.has_value()) {
    sidebar->read(selection_->current);
  }
  const composition::TagKey current = selection_->current();
  composition::Element chosen = std::move(scroller).environment(
      RowSelection::key,
      std::optional<RowSelection>{RowSelection{*selection_, focus, sidebar}});
  if (selection_->toggle) {
    return chosen;
  }
  return std::move(chosen).default_focus(
      focus, current,
      composition::DefaultFocusEvaluationPriority::user_initiated);
}
}
