#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/label_column.hpp>
#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/pinned_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <format>
#include <utility>

namespace arisa::structure {

Section::Section(Labels labels, std::vector<composition::Element> rows)
    : Section(std::move(labels), std::nullopt, std::move(rows), true) {}

Section::Section(Labels labels, composition::Binding<bool> is_expanded,
                 std::vector<composition::Element> rows)
    : Section(std::move(labels),
              std::optional<composition::Binding<bool>>{std::move(is_expanded)},
              std::move(rows), true) {}

Section::Section(Labels labels,
                 std::optional<composition::Binding<bool>> is_expanded,
                 std::vector<composition::Element> rows, bool)
    : Element([&labels, &rows, &is_expanded] {
        const auto wrap = [](std::optional<composition::Element>& element)
            -> std::shared_ptr<composition::PlaceholderContentView::Source> {
          if (!element.has_value()) {
            return nullptr;
          }
          return std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(*element));
        };
        std::vector<Row> collected;
        collected.reserve(rows.size());
        for (composition::Element& row : rows) {
          if (row.type() !=
              composition::ElementType::of<components::Group::Content>()) {
            collected.push_back(Row::one(std::move(row)));
            continue;
          }
          for (Row& each : Row::of(std::move(row))) {
            collected.push_back(std::move(each));
          }
        }
        return std::make_unique<Content>(
            wrap(labels.header), wrap(labels.footer), std::move(collected),
            std::move(is_expanded));
      }()) {}

auto Section::collapsible(bool is_collapsible) && -> Section {
  edit<Content>().set_collapsible(is_collapsible);
  return std::move(*this);
}

const composition::StateKey<std::vector<std::string>>
    Section::Content::disclosed{"section-disclosed", {}};

auto Section::titled(std::string header) -> Labels {
  return Labels{
      .header = components::Text(text::LocalizedStringKey{std::move(header)}),
      .footer = std::nullopt};
}

auto Section::content(const composition::Element& element)
    -> std::optional<std::reference_wrapper<const Content>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return std::cref(static_cast<const Content&>(element.content()));
}

auto Section::Row::of(composition::Element element) -> std::vector<Row> {
  std::vector<Row> rows;
  if (element.type() ==
      composition::ElementType::of<components::Group::Content>()) {
    for (composition::Element& member :
         components::Group::members(std::move(element))) {
      for (Row& each : of(std::move(member))) {
        rows.push_back(std::move(each));
      }
    }
    return rows;
  }
  rows.push_back(one(std::move(element)));
  return rows;
}

auto Section::Row::one(composition::Element element) -> Row {
  if (element.type() ==
      composition::ElementType::of<components::ForEach::Content>()) {
    const auto& each =
        static_cast<const components::ForEach::Content&>(element.content());
    return Row{each.source(), each.edits(),       nullptr,
               std::nullopt,  ListCell::Traits{}, nullptr};
  }
  std::shared_ptr<const Outline> outline;
  if (element.type() ==
      composition::ElementType::of<DisclosureGroup::Content>()) {
    const auto& group =
        static_cast<const DisclosureGroup::Content&>(element.content());
    outline = std::make_shared<const Outline>(
        Outline{.label = group.label(),
                .is_expanded = group.is_expanded(),
                .content = group.builder()});
  }
  const ListCell::Traits traits = ListCell::traits_of(element);
  std::optional<composition::ElementKey> key = element.key();
  return Row{nullptr,
             components::ForEach::Edits{},
             std::make_shared<composition::PlaceholderContentView::Source>(
                 ListCell::targeted(std::move(element), traits,
                                    ListCell::Placement{})),
             std::move(key),
             traits,
             std::move(outline)};
}

auto Section::Row::placed(composition::Element element,
                          composition::ElementKey slot, std::size_t depth)
    -> Row {
  Row row = one(std::move(element));
  row.outline_ = nullptr;
  row.slot_ = std::move(slot);
  row.depth_ = depth;
  return row;
}

auto Section::Row::matches(const Row& other) const -> bool {
  return each_ == nullptr && other.each_ == nullptr && single_ != nullptr &&
         other.single_ != nullptr && key_ == other.key_ &&
         traits_ == other.traits_ &&
         (outline_ == nullptr) == (other.outline_ == nullptr);
}

void Section::Row::forward(const Row& newer) const {
  single_->forward(*newer.single_);
}

auto Section::Row::count() const noexcept -> std::size_t {
  if (each_ != nullptr) {
    return each_->count();
  }
  return 1;
}

auto Section::Row::key(std::size_t index) const
    -> std::optional<composition::ElementKey> {
  if (each_ != nullptr) {
    return each_->key(index);
  }
  return key_;
}

auto Section::Row::build(std::size_t index) const -> composition::Element {
  if (each_ != nullptr) {
    return each_->build(index);
  }
  return composition::PlaceholderContentView(single_);
}

auto Section::Cells::count() const noexcept -> std::size_t {
  return row_.count();
}

auto Section::Cells::key(std::size_t index) const -> composition::ElementKey {
  return row_.key(index).value_or(
      composition::ElementKey{static_cast<std::int64_t>(index)});
}

void Section::Cells::remember(std::size_t index,
                              composition::ListRow::Separator separator) const {
  if (bottoms_.size() < row_.count()) {
    bottoms_.resize(row_.count());
  }
  if (index < bottoms_.size()) {
    bottoms_[index] = std::move(separator);
  }
}

auto Section::Cells::bottom(std::size_t index) const
    -> composition::ListRow::Separator {
  if (index < bottoms_.size() && bottoms_[index].has_value()) {
    return *bottoms_[index];
  }
  const composition::Element built = row_.build(index);
  const std::shared_ptr<const composition::ListRow> row = built.list_row();
  composition::ListRow::Separator found =
      row != nullptr ? row->bottom : composition::ListRow::Separator{};
  remember(index, found);
  return found;
}

auto Section::Cells::build(std::size_t index) const -> composition::Element {
  composition::Element content = row_.build(index);
  const ListCell::Traits traits = ListCell::traits_of(content);
  remember(index, traits.attributes().bottom);
  const motion::Transition transition =
      content.transition().value_or(ListCell::default_transition());
  const std::size_t total = row_.count();
  ListCell::Placement placement{
      .is_first = opens_ && index == 0,
      .is_last = closes_ && index + 1 == total,
      .index = index,
      .count = total,
      .edits = ListCell::permitted(row_.edits(), traits),
      .above = index == 0 ? above_ : bottom(index - 1),
      .position = position_ + index};
  composition::Element targeted =
      ListCell::targeted(std::move(content), traits, placement);
  return composition::Element{
      ListCell(std::move(targeted), std::move(placement), traits,
               ListCell::Origin{.each = row_.each(), .index = index})}
      .transition(transition);
}

Section::Caption::Caption(
    std::shared_ptr<composition::PlaceholderContentView::Source> source,
    bool header)
    : Caption(std::move(source), header, Disclosure{}) {}

Section::Caption::Caption(
    std::shared_ptr<composition::PlaceholderContentView::Source> source,
    bool header, Disclosure disclosure)
    : Element(std::make_unique<Content>(std::move(source), header,
                                        std::move(disclosure))) {}

auto Section::Caption::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Section::Caption::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const ListStyle& list = context.environment(ListStyle::key);
  const bool grouped = list.carded();
  const bool increased =
      header_ &&
      context.environment(composition::ListEnvironment::header_prominence) ==
          composition::Prominence::increased;
  float inset = theme::metrics::list.sidebar_selection_inset;
  switch (list.kind()) {
    case ListStyle::Kind::inset_grouped:
      inset = theme::metrics::list.grouped_margin +
              theme::metrics::list.grouped_row_inset;
      break;
    case ListStyle::Kind::grouped:
      inset = theme::metrics::list.grouped_row_inset;
      break;
    case ListStyle::Kind::plain:
    case ListStyle::Kind::bordered:
      inset = theme::metrics::list.plain_row_inset;
      break;
    case ListStyle::Kind::inset:
      inset = theme::metrics::list.grouped_margin +
              theme::metrics::list.plain_row_inset;
      break;
    case ListStyle::Kind::sidebar:
      break;
  }
  const composition::EdgeLengths& margins =
      context.environment(composition::ListEnvironment::section_margins);
  const float leading_inset =
      margins.leading.has_value()
          ? *margins.leading + (inset - list.margin_points())
          : inset;
  const float trailing_inset =
      margins.trailing.has_value()
          ? *margins.trailing + (inset - list.margin_points())
          : inset;
  const bool emphasised = (grouped && header_) || increased;
  theme::Font font =
      emphasised ? theme::Font::headline : theme::Font::subheadline;
  if (increased) {
    font = theme::Font::title3;
  }
  std::vector<composition::Element> line;
  line.push_back(composition::PlaceholderContentView(source_));
  line.push_back(components::Spacer());
  if (header_) {
    if (const composition::ListEnvironment::SectionActions& actions =
            context.environment(composition::ListEnvironment::section_actions);
        actions != nullptr && *actions) {
      line.push_back((*actions)()
                         .button_style(components::ButtonStyle::borderless())
                         .id("arisa.list.section-actions"));
    }
  }
  const bool toggles =
      header_ && disclosure_.collapsible && disclosure_.binding.has_value();
  if (toggles) {
    line.push_back(
        components::Glyph::chevron_right(theme::ColourRole::accent,
                                         chevron_extent)
            .rotation_effect(motion::Angle::degrees(
                disclosure_.expanded ? expanded_degrees : 0.0))
            .animation(motion::Animation::snappy(0.3), disclosure_.expanded)
            .id("arisa.list.section-chevron"));
  }
  const std::optional<float> minimum = context.environment(
      composition::ListEnvironment::default_min_header_height);
  composition::Element text =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::sm},
          std::move(line))
          .foreground_style(emphasised ? theme::ColourRole::text_primary
                                       : theme::ColourRole::text_secondary)
          .font(font)
          .padding(theme::Insets{
              .top = header_
                         ? theme::space::sm
                         : (grouped ? theme::Space{theme::metric::
                                                       list_grouped_footer_gap}
                                    : theme::space::xs),
              .leading = theme::Space{leading_inset},
              .bottom =
                  header_
                      ? (grouped ? theme::Space{theme::metric::
                                                    list_grouped_header_gap}
                                 : theme::space::xs)
                      : theme::space::none,
              .trailing = theme::Space{trailing_inset}})
          .frame(
              {.max_width = composition::infinity,
               .min_height =
                   header_ && minimum.has_value()
                       ? std::optional<composition::Length>{composition::Length{
                             *minimum}}
                       : std::nullopt});
  if (toggles) {
    text = toggler(*disclosure_.binding, std::move(text))
               .id("arisa.list.section-toggle");
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(text));
  return produced;
}

auto Section::toggler(composition::Binding<bool> open,
                      composition::Element label) -> composition::Element {
  return components::Button(
             [open] {
               motion::with_animation(motion::Animation::snappy(0.3),
                                      [&open] { open.set(!open.get()); });
             },
             std::move(label))
      .button_style(components::ButtonStyle::plain())
      .frame({.max_width = composition::infinity});
}

auto Section::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Section::Content::same_as(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& next = static_cast<const Content&>(other);
  const auto open = [](const Content& section) {
    return !section.expansion_.has_value() || section.expansion_->get();
  };
  const auto outlines = [](const Content& section) {
    return std::ranges::any_of(section.rows_, [](const Row& row) {
      return row.outline() != nullptr && row.outline()->is_expanded.has_value();
    });
  };
  return (header_ == nullptr) == (next.header_ == nullptr) &&
         (footer_ == nullptr) == (next.footer_ == nullptr) &&
         expansion_.has_value() == next.expansion_.has_value() &&
         shown_open_ == open(next) && !outlines(next) &&
         collapsible_ == next.collapsible_ &&
         std::ranges::equal(rows_, next.rows_, &Row::matches);
}

auto Section::Content::adopt(composition::ElementContent& incoming) -> bool {
  auto& newer = static_cast<Content&>(incoming);
  forward(header_, newer.header_);
  forward(footer_, newer.footer_);
  for (std::size_t index = 0; index < rows_.size(); ++index) {
    rows_[index].forward(newer.rows_[index]);
  }
  expansion_ = std::move(newer.expansion_);
  return true;
}

void Section::Content::forward(const Source& held, const Source& newer) {
  if (held != nullptr && newer != nullptr) {
    held->forward(*newer);
  }
}

auto Section::Content::section_parts() const
    -> std::optional<composition::SectionParts> {
  composition::SectionParts parts;
  if (header_ != nullptr) {
    parts.header = composition::PlaceholderContentView(header_);
  }
  if (footer_ != nullptr) {
    parts.footer = composition::PlaceholderContentView(footer_);
  }
  for (const Row& row : rows_) {
    for (std::size_t index = 0; index < row.count(); ++index) {
      composition::Element built = row.build(index);
      if (const std::optional<composition::ElementKey> key = row.key(index)) {
        built = std::move(built).id(*key);
      }
      parts.content.push_back(std::move(built));
    }
  }
  return parts;
}

auto Section::Content::disclosure(composition::Context& context) const
    -> Disclosure {
  const bool active = collapsible_.value_or(expansion_.has_value());
  if (!active) {
    return Disclosure{};
  }
  const composition::Binding<bool> open =
      expansion_.has_value()
          ? *expansion_
          : composition::Binding<bool>{context.state(expanded)};
  return Disclosure{
      .collapsible = true, .expanded = open.get(), .binding = open};
}

auto Section::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (context.environment(components::LabelColumn::key).has_value()) {
    produced.push_back(columned(*this));
    return produced;
  }
  const Disclosure shown = disclosure(context);
  shown_open_ = shown.expanded;
  const bool outlines = std::ranges::any_of(
      rows_, [](const Row& row) { return row.outline() != nullptr; });
  const std::vector<Row> flattened =
      outlines ? outlined(rows_, context.state(disclosed)) : std::vector<Row>{};
  composition::Element rendered = render(
      *this, outlines ? flattened : rows_,
      context.environment(composition::ListEnvironment::row_spacing)
          .value_or(0.0F),
      context.environment(ListStyle::key).kind() == ListStyle::Kind::plain,
      shown);
  const composition::EdgeLengths& margins =
      context.environment(composition::ListEnvironment::section_margins);
  if (margins.top.has_value()) {
    rendered = std::move(rendered).padding(theme::Edge::top,
                                           theme::Space{*margins.top});
  }
  if (margins.bottom.has_value()) {
    rendered = std::move(rendered).padding(theme::Edge::bottom,
                                           theme::Space{*margins.bottom});
  }
  produced.push_back(std::move(rendered));
  return produced;
}

auto Section::outlined(const std::vector<Row>& rows, Disclosed disclosed)
    -> std::vector<Row> {
  std::vector<Row> out;
  out.reserve(rows.size());
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Row& row = rows[index];
    if (row.outline() == nullptr) {
      out.push_back(row);
      continue;
    }
    disclose(out, row, 0, std::format("{}", index), disclosed);
  }
  return out;
}

void Section::disclose(std::vector<Row>& out, const Row& row, std::size_t depth,
                       const std::string& path, const Disclosed& disclosed) {
  const Row::Outline& outline = *row.outline();
  const composition::Binding<bool> open =
      outline.is_expanded.has_value()
          ? *outline.is_expanded
          : composition::Binding<bool>{
                [disclosed, path] {
                  return std::ranges::contains(disclosed.get(), path);
                },
                [disclosed, path](bool value) {
                  disclosed.update(
                      [&path, value](std::vector<std::string>& paths) {
                        std::erase(paths, path);
                        if (value) {
                          paths.push_back(path);
                        }
                      });
                }};
  const bool shown = open.get();
  composition::Element label =
      components::Button(
          [open] {
            motion::with_animation(motion::Animation::snappy(0.3),
                                   [&open] { open.set(!open.get()); });
          },
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::center,
                  .spacing = theme::space::sm},
              composition::PlaceholderContentView(outline.label),
              components::Spacer(),
              components::Glyph::chevron_right(theme::ColourRole::accent,
                                               chevron_extent)
                  .rotation_effect(
                      motion::Angle::degrees(shown ? expanded_degrees : 0.0))
                  .animation(motion::Animation::snappy(0.3), shown)
                  .id("arisa.disclosure.chevron"))
              .frame({.max_width = composition::infinity}))
          .id("arisa.disclosure.label");
  out.push_back(Row::placed(
      std::move(label),
      composition::ElementKey{std::format("arisa.list.outline.{}", path)},
      depth));
  if (!shown || !outline.content) {
    return;
  }
  std::size_t child = 0;
  for (composition::Element& member :
       components::Group::members(outline.content())) {
    flatten(out, std::move(member), depth + 1,
            std::format("{}.{}", path, child++), disclosed);
  }
}

void Section::flatten(std::vector<Row>& out, composition::Element element,
                      std::size_t depth, const std::string& path,
                      const Disclosed& disclosed) {
  if (element.type() ==
      composition::ElementType::of<components::Group::Content>()) {
    std::size_t child = 0;
    for (composition::Element& member :
         components::Group::members(std::move(element))) {
      flatten(out, std::move(member), depth,
              std::format("{}.{}", path, child++), disclosed);
    }
    return;
  }
  if (element.type() ==
      composition::ElementType::of<components::ForEach::Content>()) {
    const std::shared_ptr<const components::ForEach::Source> source =
        static_cast<const components::ForEach::Content&>(element.content())
            .source();
    for (std::size_t index = 0; index < source->count(); ++index) {
      flatten(out, source->element(index), depth,
              std::format("{}.{}", path, source->key(index).description()),
              disclosed);
    }
    return;
  }
  if (element.type() ==
      composition::ElementType::of<DisclosureGroup::Content>()) {
    disclose(out, Row::one(std::move(element)), depth, path, disclosed);
    return;
  }
  out.push_back(Row::placed(
      std::move(element),
      composition::ElementKey{std::format("arisa.list.outline.{}", path)},
      depth));
}

auto Section::columned(const Content& section) -> composition::Element {
  std::vector<composition::Element> column;
  if (section.header() != nullptr) {
    column.push_back(composition::Element{
        composition::PlaceholderContentView(section.header())}
                         .font(theme::Font::headline)
                         .id("arisa.list.header"));
  }
  const std::vector<Row>& rows = section.rows();
  for (std::size_t index = 0; index < rows.size(); ++index) {
    const Row& row = rows[index];
    for (std::size_t each = 0; each < row.count(); ++each) {
      composition::Element built = row.build(each);
      const std::optional<composition::ElementKey> key = row.key(each);
      column.push_back(
          key.has_value()
              ? std::move(built).id(*key)
              : std::move(built).id(composition::ElementKey{
                    std::format("arisa.list.row.{}.{}", index, each)}));
    }
  }
  if (section.footer() != nullptr) {
    column.push_back(composition::Element{
        composition::PlaceholderContentView(section.footer())}
                         .font(theme::Font::footnote)
                         .foreground_style(theme::ColourRole::text_secondary)
                         .id("arisa.list.footer"));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::Space{FormStyle::columns_row_spacing}},
             std::move(column))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .focus_section();
}

auto Section::render(const Content& section) -> composition::Element {
  return render(section, 0.0F);
}

auto Section::render(const Content& section, float spacing)
    -> composition::Element {
  return render(section, spacing, false);
}

auto Section::render(const Content& section, float spacing, bool pinned)
    -> composition::Element {
  return render(section, section.rows(), spacing, pinned, Disclosure{});
}

auto Section::render(const Content& section, const std::vector<Row>& rows,
                     float spacing, bool pinned, const Disclosure& disclosure)
    -> composition::Element {
  const auto caption = [pinned](composition::Element label,
                                bool header) -> composition::Element {
    if (!pinned) {
      return label;
    }
    return components::PinnedView(
        std::move(label).background(theme::ColourRole::system_background,
                                    theme::Shape::rectangle()),
        composition::Axis::vertical, !header);
  };
  std::vector<composition::Element> column;
  if (section.header() != nullptr) {
    column.push_back(caption(
        Caption(section.header(), true, disclosure).id("arisa.list.header"),
        true));
  }
  composition::ListRow::Separator above{};
  std::size_t position = 0;
  const std::size_t shown = disclosure.expanded ? rows.size() : 0;
  for (std::size_t index = 0; index < shown; ++index) {
    const Row& row = rows[index];
    const bool opens = index == 0;
    const bool closes = index + 1 == shown;
    if (row.repeated()) {
      auto cells =
          std::make_shared<const Cells>(row, opens, closes, above, position);
      position += row.count();
      above = row.count() > 0 && !closes ? cells->bottom(row.count() - 1)
                                         : composition::ListRow::Separator{};
      components::LazyStack::Sources sources;
      sources.push_back(std::move(cells));
      column.push_back(components::LazyVStack(
                           components::LazyVStackOptions{.spacing = spacing},
                           std::move(sources))
                           .frame({.max_width = composition::infinity})
                           .id(static_cast<std::int64_t>(index)));
      continue;
    }
    composition::Element cell =
        ListCell(row.build(0),
                 ListCell::Placement{.is_first = opens,
                                     .is_last = closes,
                                     .index = 0,
                                     .count = 1,
                                     .edits = {},
                                     .above = above,
                                     .position = position,
                                     .indent = static_cast<float>(row.depth()) *
                                               outline_indent},
                 row.traits());
    ++position;
    above = row.traits().attributes().bottom;
    column.push_back(std::move(cell).id(row.slot().value_or(
        composition::ElementKey{std::format("arisa.list.row.{}", index)})));
  }
  if (section.footer() != nullptr && disclosure.expanded) {
    column.push_back(caption(
        Caption(section.footer(), false).id("arisa.list.footer"), false));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = spacing},
             std::move(column))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .focus_section();
}

}
