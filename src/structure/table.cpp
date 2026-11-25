#include <arisa/components/any_layout.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/glyph.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/user_interface_size_class.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/table.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace arisa::structure {

Table::Table(Model model)
    : Element(std::make_unique<Content>(
          std::make_shared<const Model>(std::move(model)))) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto Table::content(const composition::Element& element)
    -> std::optional<std::reference_wrapper<const Content>> {
  if (element.type() != composition::ElementType::of<Content>()) {
    return std::nullopt;
  }
  return std::cref(static_cast<const Content&>(element.content()));
}

auto Table::ColumnLayout::frames(float width) const -> std::vector<Frame> {
  const std::size_t count = widths_.size();
  std::vector<Frame> found(count);
  if (count == 0) {
    return found;
  }
  const float available = std::max(width - (inset_ * 2.0F), 0.0F);
  std::vector<float> sizes(count, 0.0F);
  std::vector<bool> settled(count, false);
  float remaining = available;
  std::size_t open = 0;
  for (std::size_t index = 0; index < count; ++index) {
    if (widths_[index].fixed.has_value()) {
      sizes[index] = *widths_[index].fixed + cell_padding;
      settled[index] = true;
      remaining -= sizes[index];
    } else {
      ++open;
    }
  }
  while (open > 0) {
    const float share = std::max(remaining, 0.0F) / static_cast<float>(open);
    bool clamped = false;
    for (std::size_t index = 0; index < count; ++index) {
      if (settled[index]) {
        continue;
      }
      const float lower =
          widths_[index].min.value_or(minimum_column) + cell_padding;
      const float upper = widths_[index].max.has_value()
                              ? *widths_[index].max + cell_padding
                              : std::numeric_limits<float>::infinity();
      const float wanted = std::clamp(share, lower, std::max(lower, upper));
      if (wanted != share) {
        sizes[index] = wanted;
        settled[index] = true;
        remaining -= wanted;
        --open;
        clamped = true;
      }
    }
    if (!clamped) {
      for (std::size_t index = 0; index < count; ++index) {
        if (!settled[index]) {
          sizes[index] = share;
          settled[index] = true;
        }
      }
      open = 0;
    }
  }
  float x = inset_;
  for (std::size_t index = 0; index < count; ++index) {
    found[index] = Frame{.x = x, .width = sizes[index]};
    x += sizes[index];
  }
  return found;
}

auto Table::ColumnLayout::content_width(const Frame& frame) const noexcept
    -> float {
  return std::max(frame.width - cell_padding, 0.0F);
}

auto Table::ColumnLayout::size_that_fits(
    components::ProposedViewSize proposal,
    const components::LayoutSubviews& subviews) const -> composition::Size {
  float natural = inset_ * 2.0F;
  for (const TableColumnWidth& width : widths_) {
    natural += width.fixed.value_or(
                   width.ideal.value_or(width.min.value_or(minimum_column))) +
               cell_padding;
  }
  const float width =
      proposal.width.has_value() && std::isfinite(*proposal.width)
          ? *proposal.width
          : natural;
  const std::vector<Frame> placed = frames(width);
  float height = 0.0F;
  for (std::size_t index = 0; index < subviews.size() && index < placed.size();
       ++index) {
    const composition::Size size =
        subviews[index].size_that_fits(components::ProposedViewSize{
            content_width(placed[index]), std::nullopt});
    height = std::max(height, size.height);
  }
  return composition::Size{.width = width, .height = height};
}

void Table::ColumnLayout::place_subviews(
    composition::Rect bounds, components::ProposedViewSize,
    const components::LayoutSubviews& subviews) const {
  const std::vector<Frame> placed = frames(bounds.width);
  for (std::size_t index = 0; index < subviews.size(); ++index) {
    if (index >= placed.size()) {
      subviews[index].place(composition::Point{.x = bounds.x, .y = bounds.y},
                            motion::UnitPoint::top_leading,
                            components::ProposedViewSize::zero);
      continue;
    }
    const Frame& frame = placed[index];
    const float lead = header_ ? header_leading : cell_leading;
    const float room = content_width(frame);
    const components::ProposedViewSize offered{room, std::nullopt};
    const composition::Size size = subviews[index].size_that_fits(offered);
    const float used = std::min(size.width, room);
    const TableColumnAlignment::Kind alignment =
        index < alignments_.size() ? alignments_[index]
                                   : TableColumnAlignment::Kind::leading;
    float x = bounds.x + frame.x + lead;
    if (alignment == TableColumnAlignment::Kind::trailing) {
      x = bounds.x + frame.x + lead + (room - used);
    } else if (alignment == TableColumnAlignment::Kind::center) {
      x = bounds.x + frame.x + lead + ((room - used) * 0.5F);
    }
    const float y = bounds.y + ((bounds.height - size.height) * 0.5F);
    subviews[index].place(composition::Point{.x = x, .y = y},
                          motion::UnitPoint::top_leading, offered);
  }
}

auto Table::Lines::key(std::size_t index) const -> composition::ElementKey {
  return composition::ElementKey{entries_[index].path};
}

auto Table::Lines::build(std::size_t index) const -> composition::Element {
  return decorated(Line(shared_from_this(), index), entries_[index]);
}

auto Table::Lines::targets(const Entry& entry) const
    -> controls::ContextMenu::ForSelection::Tags {
  controls::ContextMenu::ForSelection::Tags tags;
  if (selection_.has_value() && selection_->multiple() &&
      selection_->selects(entry.node->tag)) {
    for (const composition::TagKey& key : selection_->chosen()) {
      tags.push_back(key.tag());
    }
    return tags;
  }
  if (selection_.has_value() && !selection_->multiple() &&
      selection_->selects(entry.node->tag)) {
    tags.push_back(entry.node->tag);
    return tags;
  }
  tags.push_back(entry.node->tag);
  return tags;
}

auto Table::Lines::decorated(composition::Element line,
                             const Entry& entry) const -> composition::Element {
  const Node& node = *entry.node;
  line = std::move(line).focusable().focus_ring_inset(
      look_.style.kind() == TableStyle::Kind::inset ? inset : 0.0F);
  if (node.kind != TableRowKind::row &&
      (!selection_.has_value() || node.tag == nullptr)) {
    line = std::move(line).on_tap_gesture(
        [self = shared_from_this(), at = &entry] { self->toggle(*at); });
  } else if (selection_.has_value() && node.tag != nullptr) {
    if (selection_->multiple()) {
      line = std::move(line).on_tap_gesture(
          [selection = *selection_, tag = node.tag] { selection.toggle(tag); });
    } else {
      line = selection_->row(std::move(line), node.tag);
    }
  }
  if (menu_ != nullptr && node.tag != nullptr) {
    controls::ContextMenu::ForSelection::Tags tags = targets(entry);
    if (menu_->has_primary()) {
      const auto act = [menu = menu_, tags] { menu->perform(tags); };
      line = std::move(line).on_command(
          components::CommandMapping::select,
          std::string{components::CommandMapping::select_label}, act);
    }
    const std::string path = entry.path;
    const composition::StateHandle<std::optional<std::string>> open = menu_row_;
    line = controls::ContextMenu::attach(
        std::move(line),
        composition::Binding<bool>{
            [open, path] { return open.get() == path; },
            [open, path](bool shown) {
              open.set(shown ? std::optional<std::string>{path} : std::nullopt);
            }},
        menu_->items(std::move(tags)), std::nullopt);
  }
  return line;
}

auto Table::Lines::expanded(const Entry& entry) const -> bool {
  if (entry.node->is_expanded.has_value()) {
    return entry.node->is_expanded->get();
  }
  if (entry.node->kind == TableRowKind::header) {
    return !std::ranges::contains(expanded_.get(), entry.path);
  }
  return std::ranges::contains(expanded_.get(), entry.path);
}

void Table::Lines::toggle(const Entry& entry) const {
  if (entry.node->is_expanded.has_value()) {
    const composition::Binding<bool> open = *entry.node->is_expanded;
    motion::with_animation(motion::Animation::snappy(0.3),
                           [&open] { open.set(!open.get()); });
    return;
  }
  const std::string path = entry.path;
  motion::with_animation(motion::Animation::snappy(0.3), [this, &path] {
    expanded_.update([&path](std::vector<std::string>& paths) {
      if (std::erase(paths, path) == 0) {
        paths.push_back(path);
      }
    });
  });
}

Table::Line::Line(std::shared_ptr<const Lines> lines, std::size_t index)
    : Element(std::make_unique<Content>(lines, index)) {
  const Node& node = *lines->entry(index).node;
  if (node.drop != nullptr) {
    declared().drop = std::make_shared<composition::DropTarget>(
        node.drop->types,
        [drop = node.drop](std::span<const composition::TransferItem> items,
                           composition::Point) {
          drop->perform(std::vector<composition::TransferItem>(items.begin(),
                                                               items.end()));
          return true;
        },
        composition::DropTarget::Targeted{});
  }
}

auto Table::Line::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Table::Line::Content::activatable() const noexcept -> bool {
  const Lines::Entry& entry = lines_->entry(index_);
  return entry.node->kind != TableRowKind::row ||
         (lines_->selection().has_value() && entry.node->tag != nullptr);
}

auto Table::Line::Content::activate(composition::Context&,
                                    const composition::Node&,
                                    std::optional<composition::Point>) -> bool {
  const Lines::Entry& entry = lines_->entry(index_);
  if (entry.node->kind == TableRowKind::header) {
    lines_->toggle(entry);
    return true;
  }
  const std::optional<RowSelection>& selection = lines_->selection();
  if (selection.has_value() && entry.node->tag != nullptr) {
    if (selection->multiple()) {
      selection->toggle(entry.node->tag);
    } else if (lines_->model().selection.has_value() &&
               lines_->model().selection->select) {
      lines_->model().selection->select(*entry.node->tag);
    }
    return true;
  }
  if (entry.node->kind == TableRowKind::disclosure) {
    lines_->toggle(entry);
    return true;
  }
  return false;
}

auto Table::Line::Content::steer(composition::Context&,
                                 const composition::Node&,
                                 composition::MoveCommandDirection direction)
    -> bool {
  const std::size_t count = lines_->columns().size();
  const composition::StateHandle<std::size_t>& cursor = lines_->cursor();
  const std::size_t at = std::min(cursor.get(), count == 0 ? 0 : count - 1);
  const Lines::Entry& entry = lines_->entry(index_);
  if (entry.node->kind == TableRowKind::disclosure && at == 0) {
    const bool open = lines_->expanded(entry);
    if ((direction == composition::MoveCommandDirection::right && !open) ||
        (direction == composition::MoveCommandDirection::left && open)) {
      lines_->toggle(entry);
      return true;
    }
  }
  if (direction == composition::MoveCommandDirection::right && at + 1 < count) {
    cursor.set(at + 1);
    return true;
  }
  if (direction == composition::MoveCommandDirection::left && at > 0) {
    cursor.set(at - 1);
    return true;
  }
  return false;
}

void Table::Line::Content::enter(composition::Context&,
                                 const composition::Node&,
                                 composition::MoveCommandDirection direction) {
  const std::size_t count = lines_->columns().size();
  if (count == 0) {
    return;
  }
  if (direction == composition::MoveCommandDirection::right) {
    lines_->cursor().set(0);
  } else if (direction == composition::MoveCommandDirection::left) {
    lines_->cursor().set(count - 1);
  }
}

auto Table::Line::Content::backdrop(bool chosen) const
    -> std::optional<composition::Element> {
  const Lines::Entry& entry = lines_->entry(index_);
  const Lines::Look& look = lines_->look();
  const bool rounded = look.style.kind() == TableStyle::Kind::inset;
  const theme::Shape shape = rounded ? theme::Shape::rounded(row_radius, true)
                                     : theme::Shape::rectangle();
  std::optional<theme::ColourValue> fill;
  if (chosen) {
    fill = theme::ColourValue{theme::ColourRole::accent};
  } else if (look.alternates && entry.position % 2 == 1) {
    fill = theme::ColourValue{
        theme::ColourRole::alternating_content_background_alternate};
  }
  if (!fill.has_value()) {
    return std::nullopt;
  }
  composition::Element made = components::ZStack()
                                  .frame({.max_width = composition::infinity,
                                          .max_height = composition::infinity})
                                  .background(*fill, shape)
                                  .allows_hit_testing(false);
  if (rounded) {
    made =
        std::move(made).padding(theme::Edge::horizontal, theme::Space{inset});
  }
  return made;
}

auto Table::Line::Content::cells(bool focused, bool chosen) const
    -> std::vector<composition::Element> {
  const Lines::Entry& entry = lines_->entry(index_);
  const Node& node = *entry.node;
  const Model& model = lines_->model();
  const std::vector<std::size_t>& columns = lines_->columns();
  std::vector<composition::Element> made;
  made.reserve(columns.size());
  const std::size_t cursor =
      columns.empty() ? 0
                      : std::min(lines_->cursor().get(), columns.size() - 1);
  for (std::size_t position = 0; position < columns.size(); ++position) {
    const Column& column = model.columns[columns[position]];
    composition::Element cell = components::Group();
    if (node.kind == TableRowKind::header) {
      if (position == 0 && node.header != nullptr) {
        cell = composition::Element{composition::PlaceholderContentView(
                                        node.header)}
                   .font(theme::Font::headline);
      }
    } else if (node.value.has_value() && column.cell) {
      cell = column.cell(*node.value);
    }
    if (position == 0 && (node.kind != TableRowKind::row || entry.depth > 0)) {
      std::vector<composition::Element> line;
      if (node.kind != TableRowKind::row) {
        const bool open = lines_->expanded(entry);
        line.push_back(
            components::Glyph::chevron_right(
                chosen ? theme::ColourValue{theme::ColourRole::on_accent}
                       : theme::ColourValue{theme::ColourRole::text_secondary},
                chevron_extent)
                .rotation_effect(motion::Angle::degrees(open ? 90.0 : 0.0))
                .animation(motion::Animation::snappy(0.3), open)
                .frame({.width = chevron_extent})
                .id("arisa.table.chevron"));
      } else {
        line.push_back(components::ZStack().frame({.width = chevron_extent}));
      }
      line.push_back(std::move(cell));
      cell = components::HStack(
                 components::HStackOptions{
                     .alignment = components::VerticalAlignment::center,
                     .spacing = theme::space::xs},
                 std::move(line))
                 .padding(theme::Edge::leading,
                          theme::Space{static_cast<float>(entry.depth) *
                                       outline_indent});
    }
    if (focused && position == cursor && columns.size() > 1) {
      cell =
          std::move(cell).background(theme::ColourRole::tertiary_system_fill,
                                     theme::Shape::rounded(row_radius, true));
    }
    made.push_back(std::move(cell).id(composition::ElementKey{
        std::format("arisa.table.cell.{}", column.id)}));
  }
  return made;
}

auto Table::Line::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const Lines::Entry& entry = lines_->entry(index_);
  const std::optional<RowSelection>& selection = lines_->selection();
  const bool chosen =
      selection.has_value() && selection->selects(entry.node->tag);
  const bool focused = context.is_focused();
  composition::Element row =
      components::AnyLayout(lines_->layout())(cells(focused, chosen))
          .frame(
              {.max_width = composition::infinity, .min_height = row_height});
  if (chosen) {
    row = std::move(row).foreground_style(theme::ColourRole::on_accent);
  }
  if (std::optional<composition::Element> fill = backdrop(chosen)) {
    row = std::move(row).background(composition::Alignment::center,
                                    std::move(*fill));
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(row));
  return produced;
}

const composition::StateKey<TableColumnCustomization>
    Table::Content::arrangement{"table-arrangement", {}};
const composition::StateKey<std::vector<std::string>>
    Table::Content::expanded_rows{"table-expanded", {}};
const composition::StateKey<std::optional<std::string>> Table::Content::lifted{
    "table-lifted", std::nullopt};
const composition::StateKey<std::optional<std::string>>
    Table::Content::menu_row{"table-menu-row", std::nullopt};
const composition::FocusState<composition::TagKey> Table::Content::row_focus{
    "table-row-focus"};

auto Table::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Table::Content::visible_columns(
    const Model& model, const TableColumnCustomization& customization,
    bool compact) -> std::vector<std::size_t> {
  std::vector<std::string> ids;
  ids.reserve(model.columns.size());
  for (const Column& column : model.columns) {
    ids.push_back(column.id);
  }
  std::vector<std::size_t> shown;
  for (const std::size_t index : customization.arranged(ids)) {
    const Column& column = model.columns[index];
    const composition::Visibility picked = customization.visibility(column.id);
    const bool hidden =
        picked == composition::Visibility::hidden ||
        (picked == composition::Visibility::automatic &&
         column.default_visibility == composition::Visibility::hidden);
    if (!hidden) {
      shown.push_back(index);
    }
    if (compact && !shown.empty()) {
      break;
    }
  }
  return shown;
}

auto Table::Content::open(const Node& node, const std::string& path,
                          const std::vector<std::string>& expanded) -> bool {
  if (node.is_expanded.has_value()) {
    return node.is_expanded->get();
  }
  if (node.kind == TableRowKind::header) {
    return !std::ranges::contains(expanded, path);
  }
  return std::ranges::contains(expanded, path);
}

void Table::Content::walk(const std::vector<Node>& nodes, std::size_t depth,
                          const std::string& prefix,
                          const std::vector<std::string>& expanded,
                          std::vector<Lines::Entry>& out) {
  for (const Node& node : nodes) {
    std::string path = prefix.empty() ? node.key.description()
                                      : prefix + "/" + node.key.description();
    const std::size_t position = out.size();
    out.push_back(Lines::Entry{
        .node = &node, .depth = depth, .position = position, .path = path});
    if (node.kind != TableRowKind::row && !node.children.empty() &&
        open(node, path, expanded)) {
      walk(node.children, node.kind == TableRowKind::header ? depth : depth + 1,
           path, expanded, out);
    }
  }
}

auto Table::Content::flatten(const Model& model,
                             const std::vector<std::string>& expanded)
    -> std::vector<Lines::Entry> {
  std::vector<Lines::Entry> out;
  walk(model.roots, 0, std::string{}, expanded, out);
  return out;
}

auto Table::Content::layout_for(const Model& model,
                                const TableColumnCustomization& customization,
                                const std::vector<std::size_t>& columns,
                                const TableStyle& style, bool header)
    -> ColumnLayout {
  std::vector<TableColumnWidth> widths;
  std::vector<TableColumnAlignment::Kind> alignments;
  widths.reserve(columns.size());
  alignments.reserve(columns.size());
  for (const std::size_t index : columns) {
    const Column& column = model.columns[index];
    TableColumnWidth width = column.width;
    if (const std::optional<float> custom = customization.width(column.id)) {
      width =
          TableColumnWidth{.fixed = *custom, .min = {}, .ideal = {}, .max = {}};
    }
    widths.push_back(width);
    alignments.push_back(column.alignment.resolved());
  }
  return ColumnLayout{std::move(widths), std::move(alignments),
                      style.kind() == TableStyle::Kind::inset ? inset : 0.0F,
                      header ? header_leading : cell_leading, header};
}

void Table::Content::shift(const Model& model, Customization customization,
                           const std::vector<std::size_t>& columns,
                           std::size_t position, long steps) {
  if (columns.empty() || position >= columns.size()) {
    return;
  }
  const long wanted = std::clamp(static_cast<long>(position) + steps, 0L,
                                 static_cast<long>(columns.size()) - 1L);
  const auto target = static_cast<std::size_t>(wanted);
  if (target == position) {
    return;
  }
  const Column& moving = model.columns[columns[position]];
  const Column& other = model.columns[columns[target]];
  if (moving.disabled.contains(TableColumnCustomizationBehavior::reorder) ||
      other.disabled.contains(TableColumnCustomizationBehavior::reorder)) {
    return;
  }
  std::vector<std::string> ids;
  ids.reserve(model.columns.size());
  for (const Column& column : model.columns) {
    ids.push_back(column.id);
  }
  motion::with_animation(motion::Animation::snappy(0.3), [&] {
    customization.update([&](TableColumnCustomization& held) {
      std::vector<std::string> order;
      for (const std::size_t index : held.arranged(ids)) {
        order.push_back(ids[index]);
      }
      const auto from = std::ranges::find(order, moving.id);
      const auto to = std::ranges::find(order, other.id);
      if (from != order.end() && to != order.end()) {
        std::iter_swap(from, to);
      }
      held.reorder(std::move(order));
    });
  });
}

void Table::Content::resize(const Model& model, Customization customization,
                            std::size_t column, float points) {
  const Column& resized = model.columns[column];
  if (resized.disabled.contains(TableColumnCustomizationBehavior::resize)) {
    return;
  }
  const float lower = resized.width.min.value_or(minimum_column);
  const float upper =
      resized.width.max.value_or(std::numeric_limits<float>::infinity());
  const float clamped = std::clamp(points, lower, std::max(lower, upper));
  customization.update([&resized, clamped](TableColumnCustomization& held) {
    held.set_width(resized.id, clamped);
  });
}

auto Table::Content::column_menu(std::size_t column, const ColumnLayout& layout,
                                 const std::vector<std::size_t>& columns,
                                 Customization customization,
                                 composition::StateHandle<float> width) const
    -> composition::Element {
  std::vector<composition::Element> items;
  const std::shared_ptr<const Model> model = model_;
  for (std::size_t index = 0; index < model->columns.size(); ++index) {
    const Column& each = model->columns[index];
    if (each.disabled.contains(TableColumnCustomizationBehavior::visibility)) {
      continue;
    }
    const bool shown = std::ranges::contains(columns, index);
    items.push_back(
        components::Button(
            [customization, id = each.id, shown, model] {
              motion::with_animation(motion::Animation::snappy(0.3), [&] {
                customization.update([&](TableColumnCustomization& held) {
                  held.set_visibility(id,
                                      shown ? composition::Visibility::hidden
                                            : composition::Visibility::visible);
                });
              });
            },
            components::Label(
                each.title,
                shown ? composition::Element{components::Image::system_name(
                            "check")}
                      : composition::Element{components::ZStack()}))
            .disabled(shown && columns.size() == 1)
            .id(composition::ElementKey{
                std::format("arisa.table.visibility.{}", each.id)}));
  }
  const Column& current = model->columns[column];
  if (!current.disabled.contains(TableColumnCustomizationBehavior::resize)) {
    const std::vector<ColumnLayout::Frame> frames = layout.frames(width.get());
    const auto position = static_cast<std::size_t>(
        std::ranges::find(columns, column) - columns.begin());
    const float now =
        position < frames.size()
            ? std::max(frames[position].width - cell_padding, minimum_column)
            : minimum_column;
    items.push_back(
        components::Button(std::string{wider_label}, [model, customization,
                                                      column, now] {
          resize(*model, customization, column, now + resize_step);
        }).id("arisa.table.wider"));
    items.push_back(
        components::Button(std::string{narrower_label}, [model, customization,
                                                         column, now] {
          resize(*model, customization, column, now - resize_step);
        }).id("arisa.table.narrower"));
  }
  return components::Group(std::move(items));
}

auto Table::Content::header_cell(
    std::size_t column, std::size_t position,
    const std::vector<std::size_t>& columns, const ColumnLayout& layout,
    Customization customization,
    composition::StateHandle<std::optional<std::string>> moving,
    composition::StateHandle<float> width) const -> composition::Element {
  const std::shared_ptr<const Model> model = model_;
  const Column& spec = model->columns[column];
  const std::optional<SortOrder> sorted =
      spec.sorted ? spec.sorted() : std::nullopt;
  composition::Element label =
      spec.label != nullptr
          ? composition::Element{composition::PlaceholderContentView(
                spec.label)}
          : composition::Element{
                components::Text(text::LocalizedStringKey{spec.title})};
  std::vector<composition::Element> line;
  line.push_back(std::move(label).line_limit(1).font_weight(
      sorted.has_value() ? theme::Font::Weight::bold
                         : theme::Font::Weight::regular));
  line.push_back(components::Spacer());
  if (sorted.has_value()) {
    composition::Element chevron =
        components::Glyph::chevron_down(theme::ColourRole::text_secondary,
                                        chevron_extent)
            .id("arisa.table.sort-indicator");
    if (*sorted == SortOrder::forward) {
      chevron =
          std::move(chevron).rotation_effect(motion::Angle::degrees(180.0));
    }
    line.push_back(std::move(chevron));
  }
  const bool lifting = moving.get() == spec.id;
  composition::Element face =
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::xs},
          std::move(line))
          .font(theme::Font::subheadline)
          .foreground_style(theme::ColourRole::text_secondary)
          .frame({.max_width = composition::infinity,
                  .min_height = header_height});
  const std::function<void()> sort = spec.sort;
  composition::Element cell =
      components::Button(
          [sort] {
            if (sort) {
              motion::with_animation(motion::Animation::snappy(0.3),
                                     [&sort] { sort(); });
            }
          },
          std::move(face))
          .button_style(components::ButtonStyle::plain());
  if (lifting) {
    cell = std::move(cell)
               .background(theme::ColourRole::tertiary_system_fill,
                           theme::Shape::rounded(row_radius, true))
               .scale_effect(lifted_scale);
  }
  const bool reorders =
      !spec.disabled.contains(TableColumnCustomizationBehavior::reorder);
  if (reorders) {
    const std::string label_text{components::CommandMapping::move_label};
    const std::string drop_text{components::CommandMapping::drop_label};
    if (!lifting) {
      cell = std::move(cell).on_command(
          components::CommandMapping::move, label_text,
          [moving, id = spec.id] { moving.set(id); });
    } else {
      const auto drop = [moving] { moving.set(std::nullopt); };
      cell =
          std::move(cell)
              .on_command(components::CommandMapping::move, drop_text, drop)
              .on_command(components::CommandMapping::select, drop_text, drop)
              .on_exit_command(drop_text, drop)
              .on_move_command(
                  [model, customization, columns,
                   position](composition::MoveCommandDirection direction) {
                    if (direction == composition::MoveCommandDirection::left) {
                      shift(*model, customization, columns, position, -1);
                    } else if (direction ==
                               composition::MoveCommandDirection::right) {
                      shift(*model, customization, columns, position, 1);
                    }
                  },
                  composition::Axis::horizontal);
    }
    const std::vector<ColumnLayout::Frame> frames = layout.frames(width.get());
    const float pitch = position < frames.size()
                            ? std::max(frames[position].width, 1.0F)
                            : 1.0F;
    cell = std::move(cell).simultaneous_gesture(
        composition::DragGesture(8.0F, composition::CoordinateSpace::global)
            .on_ended([model, customization, columns, position,
                       pitch](const composition::DragGesture::Value& value) {
              const long steps = std::lround(value.translation.width / pitch);
              if (steps != 0) {
                shift(*model, customization, columns, position, steps);
              }
            }));
  }
  cell = controls::ContextMenu::with_items(
      std::move(cell),
      [this, column, layout, columns, customization, width] {
        return column_menu(column, layout, columns, customization, width);
      },
      std::nullopt);
  if (position + 1 < columns.size()) {
    composition::Element divider =
        components::ZStack()
            .frame({.width = 1.0F, .height = divider_height})
            .background(theme::ColourRole::separator);
    if (!spec.disabled.contains(TableColumnCustomizationBehavior::resize)) {
      const std::vector<ColumnLayout::Frame> frames =
          layout.frames(width.get());
      const float start = position < frames.size()
                              ? frames[position].width - cell_padding
                              : minimum_column;
      divider =
          components::ZStack(
              components::ZStackOptions{.alignment =
                                            composition::Alignment::center},
              std::move(divider))
              .frame({.width = resize_handle, .height = header_height})
              .content_shape(theme::Shape::rectangle())
              .pointer_style(composition::PointerStyle::column_resize())
              .high_priority_gesture(
                  composition::DragGesture(0.0F,
                                           composition::CoordinateSpace::global)
                      .on_changed(
                          [model, customization, column, start](
                              const composition::DragGesture::Value& value) {
                            resize(*model, customization, column,
                                   start + value.translation.width);
                          }))
              .id("arisa.table.resize");
    }
    cell = std::move(cell).overlay(
        composition::Alignment::trailing,
        std::move(divider).offset(header_leading + 0.5F, 0.0F));
  }
  return std::move(cell).id(
      composition::ElementKey{std::format("arisa.table.header.{}", spec.id)});
}

auto Table::Content::header(
    const std::vector<std::size_t>& columns, const ColumnLayout& layout,
    const TableStyle& style, Customization customization,
    composition::StateHandle<std::optional<std::string>> moving,
    composition::StateHandle<float> width) const -> composition::Element {
  std::vector<composition::Element> cells;
  cells.reserve(columns.size());
  for (std::size_t position = 0; position < columns.size(); ++position) {
    cells.push_back(header_cell(columns[position], position, columns, layout,
                                customization, moving, width));
  }
  composition::Element row =
      components::AnyLayout(layout)(std::move(cells))
          .frame(
              {.max_width = composition::infinity, .min_height = header_height})
          .overlay(
              composition::Alignment::bottom,
              components::ZStack()
                  .frame({.height = 1.0F, .max_width = composition::infinity})
                  .background(theme::ColourRole::separator)
                  .allows_hit_testing(false))
          .focus_section()
          .id("arisa.table.headers");
  if (style.kind() == TableStyle::Kind::inset) {
    row = std::move(row).padding(theme::Edge::bottom, theme::Space{header_gap});
  }
  return row;
}

auto Table::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const Model& model = *model_;
  const TableStyle& style = context.environment(TableStyle::key);
  const bool compact =
      context.environment(components::Environment::horizontal_size_class) ==
      components::UserInterfaceSizeClass::compact;
  const composition::StateHandle<TableColumnCustomization> local =
      context.state(arrangement);
  const Customization customization = model.customization.has_value()
                                          ? *model.customization
                                          : Customization{local};
  const TableColumnCustomization current = customization.get();
  const std::vector<std::size_t> columns =
      visible_columns(model, current, compact);
  const composition::StateHandle<float> width = context.state(measured);
  const composition::StateHandle<std::optional<std::string>> moving =
      context.state(lifted);
  const composition::StateHandle<std::vector<std::string>> opened =
      context.state(expanded_rows);
  std::optional<RowSelection> selection;
  composition::Element anchor = components::Group();
  if (model.selection.has_value()) {
    selection =
        RowSelection{*model.selection, context.state(row_focus), std::nullopt};
  }
  const bool alternates =
      context
          .environment(
              composition::ListEnvironment::alternating_row_background_behavior)
          .alternates(
              style.alternates_row_backgrounds() ||
              context.environment(
                  composition::ListEnvironment::alternating_row_backgrounds));
  auto lines = std::make_shared<const Lines>(
      model_, flatten(model, opened.get()), columns,
      layout_for(model, current, columns, style, false),
      Lines::Look{.style = style, .alternates = alternates, .compact = compact},
      selection, context.state(column_cursor), opened,
      context.environment(controls::ContextMenu::selection),
      context.state(menu_row));
  components::LazyStack::Sources sources;
  sources.push_back(lines);
  composition::Element rows =
      components::ScrollView(
          components::LazyVStack(
              components::LazyVStackOptions{
                  .alignment = components::HorizontalAlignment::leading,
                  .spacing = theme::space::none,
                  .estimated_extent = row_height},
              std::move(sources))
              .frame({.max_width = composition::infinity}))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .id("arisa.table.rows");
  std::vector<composition::Element> column_stack;
  const bool headed =
      !compact &&
      context.environment(composition::ListEnvironment::table_column_headers) !=
          composition::Visibility::hidden;
  if (headed) {
    column_stack.push_back(
        header(columns, layout_for(model, current, columns, style, true), style,
               customization, moving, width));
  } else if (style.kind() == TableStyle::Kind::inset) {
    rows = std::move(rows).padding(theme::Edge::top, theme::Space{header_gap});
  }
  column_stack.push_back(std::move(rows));
  composition::Element table =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::none},
          std::move(column_stack))
          .frame({.max_width = composition::infinity,
                  .max_height = composition::infinity})
          .background(theme::ColourRole::system_background,
                      theme::Shape::rectangle())
          .on_geometry_change(
              std::type_identity<float>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.size().width;
              },
              [width](float value) {
                if (width.get() != value) {
                  width.set(value);
                }
              });
  if (style.kind() == TableStyle::Kind::bordered) {
    table = std::move(table).border(theme::ColourRole::separator, border_width);
  }
  if (selection.has_value() && !selection->multiple()) {
    table = std::move(table).default_focus(
        context.state(row_focus), model.selection->current(),
        composition::DefaultFocusEvaluationPriority::user_initiated);
  }
  static_cast<void>(anchor);
  std::vector<composition::Element> produced;
  produced.push_back(std::move(table));
  return produced;
}

}
