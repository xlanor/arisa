#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/components/proposed_view_size.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/structure/key_path_comparator.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/row_selection.hpp>
#include <arisa/structure/sort_order.hpp>
#include <arisa/structure/table_column_alignment.hpp>
#include <arisa/structure/table_column_content.hpp>
#include <arisa/structure/table_column_customization.hpp>
#include <arisa/structure/table_column_customization_behavior.hpp>
#include <arisa/structure/table_column_width.hpp>
#include <arisa/structure/table_for_each_content.hpp>
#include <arisa/structure/table_outline_group_content.hpp>
#include <arisa/structure/table_row_content.hpp>
#include <arisa/structure/table_row_kind.hpp>
#include <arisa/structure/table_style.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class KeyOf, class Value>
using TableIdOf =
    std::remove_cvref_t<std::invoke_result_t<const KeyOf&, const Value&>>;

template <class ChildrenOf, class Value>
concept TableChildrenOf =
    std::invocable<const ChildrenOf&, const Value&> &&
    (std::same_as<std::remove_cvref_t<
                      std::invoke_result_t<const ChildrenOf&, const Value&>>,
                  std::vector<Value>> ||
     std::same_as<std::remove_cvref_t<
                      std::invoke_result_t<const ChildrenOf&, const Value&>>,
                  std::optional<std::vector<Value>>>);

template <class Arg, class Value, class ID>
concept TableBindingOf =
    composition::BindingSource<Arg> &&
    (std::same_as<composition::BoundValue<Arg>, std::optional<ID>> ||
     std::same_as<composition::BoundValue<Arg>, std::set<ID>> ||
     std::same_as<composition::BoundValue<Arg>,
                  std::vector<KeyPathComparator<Value>>> ||
     std::same_as<composition::BoundValue<Arg>, TableColumnCustomization>);

template <class Arg, class Value, class ID>
concept TableArgument =
    std::derived_from<std::remove_cvref_t<Arg>, TableColumnContent<Value>> ||
    TableRowsOf<Arg, Value> || TableBindingOf<Arg, Value, ID>;

class Table : public composition::Element {
 public:
  class Content;
  class Line;
  class Lines;
  class ColumnLayout;

  struct Column {
    std::string title;
    std::shared_ptr<composition::PlaceholderContentView::Source> label;
    std::function<composition::Element(std::size_t)> cell;
    std::string id;
    TableColumnWidth width;
    TableColumnAlignment alignment{TableColumnAlignment::automatic};
    composition::Visibility default_visibility{
        composition::Visibility::automatic};
    TableColumnCustomizationBehavior disabled;
    std::function<void()> sort;
    std::function<std::optional<SortOrder>()> sorted;
  };

  struct Drop {
    std::vector<composition::UTType> types;
    std::function<void(std::vector<composition::TransferItem>)> perform;
  };

  struct Node {
    TableRowKind kind{TableRowKind::row};
    std::optional<std::size_t> value;
    composition::ElementKey key{0};
    std::shared_ptr<const composition::TagValue> tag;
    std::shared_ptr<composition::PlaceholderContentView::Source> header;
    std::optional<composition::Binding<bool>> is_expanded;
    std::vector<Node> children;
    std::shared_ptr<const Drop> drop;
  };

  struct Model {
    std::vector<Column> columns;
    std::vector<Node> roots;
    std::optional<List::Selection> selection;
    std::optional<composition::Binding<TableColumnCustomization>> customization;
  };

  static constexpr float row_height = 24.0F;
  static constexpr float header_height = 28.0F;
  static constexpr float header_gap = 5.0F;
  static constexpr float inset = 10.0F;
  static constexpr float cell_padding = 17.0F;
  static constexpr float cell_leading = 6.0F;
  static constexpr float header_leading = 8.5F;
  static constexpr float row_radius = 5.0F;
  static constexpr float outline_indent = 16.0F;
  static constexpr float chevron_extent = 10.0F;
  static constexpr float divider_height = 14.0F;
  static constexpr float resize_handle = 12.0F;
  static constexpr float resize_step = 20.0F;
  static constexpr float minimum_column = 16.0F;
  static constexpr float border_width = 1.0F;
  static constexpr float lifted_scale = 1.04F;
  static constexpr std::string_view wider_label = "Wider";
  static constexpr std::string_view narrower_label = "Narrower";

  template <std::ranges::input_range Items, class KeyOf, class... Args>
    requires std::invocable<const KeyOf&,
                            const std::ranges::range_value_t<Items>&> &&
             (TableArgument<
                  Args, std::ranges::range_value_t<Items>,
                  TableIdOf<KeyOf, std::ranges::range_value_t<Items>>> &&
              ...)
  Table(Items&& data, KeyOf key_of, Args&&... args)
      : Table(describe<std::ranges::range_value_t<Items>>(
            std::move(key_of),
            parts<std::ranges::range_value_t<Items>,
                  TableIdOf<KeyOf, std::ranges::range_value_t<Items>>>(
                TableForEachContent<std::ranges::range_value_t<Items>>{
                    std::forward<Items>(data)},
                std::forward<Args>(args)...))) {}

  template <std::ranges::input_range Items, class KeyOf, class ChildrenOf,
            class... Args>
    requires std::invocable<const KeyOf&,
                            const std::ranges::range_value_t<Items>&> &&
             TableChildrenOf<ChildrenOf, std::ranges::range_value_t<Items>> &&
             (TableArgument<
                  Args, std::ranges::range_value_t<Items>,
                  TableIdOf<KeyOf, std::ranges::range_value_t<Items>>> &&
              ...)
  Table(Items&& data, KeyOf key_of, ChildrenOf children_of, Args&&... args)
      : Table(describe<std::ranges::range_value_t<Items>>(
            std::move(key_of),
            parts<std::ranges::range_value_t<Items>,
                  TableIdOf<KeyOf, std::ranges::range_value_t<Items>>>(
                TableOutlineGroupContent<std::ranges::range_value_t<Items>>{
                    std::forward<Items>(data), std::move(children_of)},
                std::forward<Args>(args)...))) {}

  template <class Value, class KeyOf, class... Args>
    requires std::invocable<const KeyOf&, const Value&> &&
             (TableArgument<Args, Value, TableIdOf<KeyOf, Value>> && ...)
  Table(std::type_identity<Value>, KeyOf key_of, Args&&... args)
      : Table(describe<Value>(std::move(key_of),
                              parts<Value, TableIdOf<KeyOf, Value>>(
                                  std::forward<Args>(args)...))) {}

  explicit Table(Model model);

  static auto content(const composition::Element& element)
      -> std::optional<std::reference_wrapper<const Content>>;

 private:
  template <class Value, class ID>
  struct Parts {
    std::vector<typename TableColumnContent<Value>::Spec> columns;
    std::vector<typename TableRowContent<Value>::Node> rows;
    std::optional<List::Selection> selection;
    std::optional<composition::Binding<std::vector<KeyPathComparator<Value>>>>
        sort_order;
    std::optional<composition::Binding<TableColumnCustomization>> customization;
  };

  template <class Value, class ID, class... Args>
  static auto parts(Args&&... args) -> Parts<Value, ID> {
    Parts<Value, ID> found;
    (add<Value, ID>(found, std::forward<Args>(args)), ...);
    return found;
  }

  template <class Value, class ID, class Arg>
  static void add(Parts<Value, ID>& found, Arg&& arg) {
    using Plain = std::remove_cvref_t<Arg>;
    if constexpr (std::derived_from<Plain, TableColumnContent<Value>>) {
      for (auto& spec : Plain{std::forward<Arg>(arg)}.take_specs()) {
        found.columns.push_back(std::move(spec));
      }
    } else if constexpr (std::derived_from<Plain, TableRowContent<Value>>) {
      for (auto& node : Plain{std::forward<Arg>(arg)}.take_nodes()) {
        found.rows.push_back(std::move(node));
      }
    } else {
      using Bound = composition::BoundValue<Plain>;
      composition::Binding<Bound> bound{std::forward<Arg>(arg)};
      if constexpr (std::same_as<Bound, std::optional<ID>>) {
        found.selection = single(std::move(bound));
      } else if constexpr (std::same_as<Bound, std::set<ID>>) {
        found.selection = multiple(std::move(bound));
      } else if constexpr (std::same_as<
                               Bound, std::vector<KeyPathComparator<Value>>>) {
        found.sort_order = std::move(bound);
      } else {
        found.customization = std::move(bound);
      }
    }
  }

  template <class ID>
  static auto single(composition::Binding<std::optional<ID>> selection)
      -> List::Selection {
    return List::Selection{
        .current =
            [selection] {
              const std::optional<ID> value = selection.get();
              return value.has_value() ? composition::TagKey::of(*value)
                                       : composition::TagKey{};
            },
        .select =
            [selection](const composition::TagValue& tag) {
              if (const ID* value = tag.get<ID>()) {
                selection.set(*value);
              }
            }};
  }

  template <class ID>
  static auto multiple(composition::Binding<std::set<ID>> selection)
      -> List::Selection {
    return List::Selection{
        .current = [] { return composition::TagKey{}; },
        .select =
            [selection](const composition::TagValue& tag) {
              if (const ID* value = tag.get<ID>()) {
                selection.set(std::set<ID>{*value});
              }
            },
        .members =
            [selection] {
              std::vector<composition::TagKey> keys;
              for (const ID& value : selection.get()) {
                keys.push_back(composition::TagKey::of(value));
              }
              return keys;
            },
        .toggle =
            [selection](const composition::TagValue& tag) {
              if (const ID* value = tag.get<ID>()) {
                selection.update([value](std::set<ID>& chosen) {
                  if (chosen.erase(*value) == 0) {
                    chosen.insert(*value);
                  }
                });
              }
            }};
  }

  template <class Value, class KeyOf, class ID>
  static auto describe(KeyOf key_of, Parts<Value, ID> found) -> Model {
    auto values = std::make_shared<std::vector<Value>>();
    std::size_t headers = 0;
    Model model;
    model.roots =
        convert<Value>(key_of, std::move(found.rows), *values, headers);
    model.selection = std::move(found.selection);
    model.customization = std::move(found.customization);
    std::shared_ptr<const std::vector<Value>> shared = std::move(values);
    for (std::size_t index = 0; index < found.columns.size(); ++index) {
      auto& spec = found.columns[index];
      Column column;
      column.title = spec.title;
      column.label = spec.label;
      column.cell = [shared, content = spec.content](std::size_t at) {
        return content((*shared)[at]);
      };
      column.id = spec.customization.empty() ? std::to_string(index)
                                             : spec.customization;
      column.width = spec.width;
      column.alignment = spec.alignment;
      column.default_visibility = spec.default_visibility;
      column.disabled = spec.disabled;
      if (spec.comparator.has_value() && found.sort_order.has_value()) {
        const KeyPathComparator<Value> comparator = *spec.comparator;
        const composition::Binding<std::vector<KeyPathComparator<Value>>>
            order = *found.sort_order;
        column.sort = [comparator, order] {
          order.update(
              [&comparator](std::vector<KeyPathComparator<Value>>& held) {
                if (!held.empty() && held.front().same_key(comparator)) {
                  held.front() = held.front().reversed();
                  return;
                }
                std::erase_if(held, [&comparator](const auto& existing) {
                  return existing.same_key(comparator);
                });
                held.insert(held.begin(), comparator);
              });
        };
        column.sorted = [comparator, order]() -> std::optional<SortOrder> {
          const std::vector<KeyPathComparator<Value>> held = order.get();
          if (held.empty() || !held.front().same_key(comparator)) {
            return std::nullopt;
          }
          return held.front().order();
        };
      }
      model.columns.push_back(std::move(column));
    }
    return model;
  }

  template <class Value, class KeyOf>
  static auto convert(const KeyOf& key_of,
                      std::vector<typename TableRowContent<Value>::Node> nodes,
                      std::vector<Value>& values, std::size_t& headers)
      -> std::vector<Node> {
    std::vector<Node> made;
    made.reserve(nodes.size());
    for (auto& node : nodes) {
      Node erased;
      erased.kind = node.kind;
      erased.header = std::move(node.header);
      erased.is_expanded = std::move(node.is_expanded);
      if (node.value.has_value()) {
        const auto id = std::invoke(key_of, *node.value);
        erased.key = composition::ElementKey::identifying(id);
        erased.tag = composition::TagValue::of(id);
        erased.value = values.size();
        values.push_back(std::move(*node.value));
      } else {
        erased.key = composition::ElementKey{
            std::string{"arisa.table.header."} + std::to_string(headers++)};
      }
      if (node.insert != nullptr) {
        const auto insert = node.insert;
        const std::size_t at = node.insert_index;
        erased.drop = std::make_shared<const Drop>(
            Drop{.types = insert->types,
                 .perform = [insert,
                             at](std::vector<composition::TransferItem> items) {
                   insert->action(at, std::move(items));
                 }});
      }
      erased.children =
          convert<Value>(key_of, std::move(node.children), values, headers);
      made.push_back(std::move(erased));
    }
    return made;
  }
};

class Table::ColumnLayout {
 public:
  struct Frame {
    float x{0.0F};
    float width{0.0F};
  };

  ColumnLayout(std::vector<TableColumnWidth> widths,
               std::vector<TableColumnAlignment::Kind> alignments,
               float table_inset, float leading, bool header) noexcept
      : widths_(std::move(widths)),
        alignments_(std::move(alignments)),
        inset_(table_inset),
        leading_(leading),
        header_(header) {}

  auto frames(float width) const -> std::vector<Frame>;

  auto size_that_fits(components::ProposedViewSize proposal,
                      const components::LayoutSubviews& subviews) const
      -> composition::Size;

  void place_subviews(composition::Rect bounds,
                      components::ProposedViewSize proposal,
                      const components::LayoutSubviews& subviews) const;

  friend auto operator==(const ColumnLayout&, const ColumnLayout&)
      -> bool = default;

 private:
  auto content_width(const Frame& frame) const noexcept -> float;

  std::vector<TableColumnWidth> widths_;
  std::vector<TableColumnAlignment::Kind> alignments_;
  float inset_;
  float leading_;
  bool header_;
};

class Table::Lines final : public components::ForEach::Source,
                           public std::enable_shared_from_this<Lines> {
 public:
  struct Entry {
    const Node* node{nullptr};
    std::size_t depth{0};
    std::size_t position{0};
    std::string path;
  };

  struct Look {
    TableStyle style{TableStyle::automatic()};
    bool alternates{true};
    bool compact{false};
  };

  Lines(std::shared_ptr<const Model> model, std::vector<Entry> entries,
        std::vector<std::size_t> columns, ColumnLayout layout, Look look,
        std::optional<RowSelection> selection,
        composition::StateHandle<std::size_t> cursor,
        composition::StateHandle<std::vector<std::string>> expanded,
        std::shared_ptr<const controls::ContextMenu::ForSelection> menu,
        composition::StateHandle<std::optional<std::string>> menu_row)
      : model_(std::move(model)),
        entries_(std::move(entries)),
        columns_(std::move(columns)),
        layout_(std::move(layout)),
        look_(look),
        selection_(std::move(selection)),
        cursor_(std::move(cursor)),
        expanded_(std::move(expanded)),
        menu_(std::move(menu)),
        menu_row_(std::move(menu_row)) {}

  auto count() const noexcept -> std::size_t override {
    return entries_.size();
  }

  auto key(std::size_t index) const -> composition::ElementKey override;

  auto build(std::size_t index) const -> composition::Element override;

  auto entry(std::size_t index) const -> const Entry& {
    return entries_[index];
  }

  auto model() const noexcept -> const Model& { return *model_; }

  auto columns() const noexcept -> const std::vector<std::size_t>& {
    return columns_;
  }

  auto layout() const noexcept -> const ColumnLayout& { return layout_; }

  auto look() const noexcept -> const Look& { return look_; }

  auto selection() const noexcept -> const std::optional<RowSelection>& {
    return selection_;
  }

  auto cursor() const noexcept -> const composition::StateHandle<std::size_t>& {
    return cursor_;
  }

  auto menu() const noexcept
      -> const std::shared_ptr<const controls::ContextMenu::ForSelection>& {
    return menu_;
  }

  auto expanded(const Entry& entry) const -> bool;

  void toggle(const Entry& entry) const;

 private:
  auto targets(const Entry& entry) const
      -> controls::ContextMenu::ForSelection::Tags;

  auto decorated(composition::Element line, const Entry& entry) const
      -> composition::Element;

  std::shared_ptr<const Model> model_;
  std::vector<Entry> entries_;
  std::vector<std::size_t> columns_;
  ColumnLayout layout_;
  Look look_;
  std::optional<RowSelection> selection_;
  composition::StateHandle<std::size_t> cursor_;
  composition::StateHandle<std::vector<std::string>> expanded_;
  std::shared_ptr<const controls::ContextMenu::ForSelection> menu_;
  composition::StateHandle<std::optional<std::string>> menu_row_;
};

class Table::Line : public composition::Element {
 public:
  class Content;

  Line(std::shared_ptr<const Lines> lines, std::size_t index);
};

class Table::Line::Content final : public composition::ElementContent {
 public:
  Content(std::shared_ptr<const Lines> lines, std::size_t index) noexcept
      : lines_(std::move(lines)), index_(index) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto activatable() const noexcept -> bool override;

  auto activate(composition::Context& context, const composition::Node& node,
                std::optional<composition::Point> at) -> bool override;

  auto steer(composition::Context& context, const composition::Node& node,
             composition::MoveCommandDirection direction) -> bool override;

  void enter(composition::Context& context, const composition::Node& node,
             composition::MoveCommandDirection direction) override;

 private:
  auto cells(bool focused, bool chosen) const
      -> std::vector<composition::Element>;

  auto backdrop(bool chosen) const -> std::optional<composition::Element>;

  std::shared_ptr<const Lines> lines_;
  std::size_t index_;
};

class Table::Content final : public composition::ElementContent {
 public:
  static const composition::StateKey<TableColumnCustomization> arrangement;
  static const composition::StateKey<std::vector<std::string>> expanded_rows;
  static constexpr composition::StateKey<std::size_t> column_cursor{
      "table-column-cursor", 0};
  static const composition::StateKey<std::optional<std::string>> lifted;
  static const composition::StateKey<std::optional<std::string>> menu_row;
  static constexpr composition::StateKey<float> measured{"table-width", 0.0F};
  static constexpr composition::StateKey<bool> menu_open{"table-column-menu",
                                                         false};
  static const composition::FocusState<composition::TagKey> row_focus;

  explicit Content(std::shared_ptr<const Model> model) noexcept
      : model_(std::move(model)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto model() const noexcept -> const Model& { return *model_; }

  static auto visible_columns(const Model& model,
                              const TableColumnCustomization& customization,
                              bool compact) -> std::vector<std::size_t>;

  static auto flatten(const Model& model,
                      const std::vector<std::string>& expanded)
      -> std::vector<Lines::Entry>;

 private:
  using Customization = composition::Binding<TableColumnCustomization>;

  static void walk(const std::vector<Node>& nodes, std::size_t depth,
                   const std::string& prefix,
                   const std::vector<std::string>& expanded,
                   std::vector<Lines::Entry>& out);

  static auto open(const Node& node, const std::string& path,
                   const std::vector<std::string>& expanded) -> bool;

  auto header(const std::vector<std::size_t>& columns,
              const ColumnLayout& layout, const TableStyle& style,
              Customization customization,
              composition::StateHandle<std::optional<std::string>> moving,
              composition::StateHandle<float> width) const
      -> composition::Element;

  auto header_cell(std::size_t column, std::size_t position,
                   const std::vector<std::size_t>& columns,
                   const ColumnLayout& layout, Customization customization,
                   composition::StateHandle<std::optional<std::string>> moving,
                   composition::StateHandle<float> width) const
      -> composition::Element;

  auto column_menu(std::size_t column, const ColumnLayout& layout,
                   const std::vector<std::size_t>& columns,
                   Customization customization,
                   composition::StateHandle<float> width) const
      -> composition::Element;

  static void shift(const Model& model, Customization customization,
                    const std::vector<std::size_t>& columns,
                    std::size_t position, long steps);

  static void resize(const Model& model, Customization customization,
                     std::size_t column, float points);

  static auto layout_for(const Model& model,
                         const TableColumnCustomization& customization,
                         const std::vector<std::size_t>& columns,
                         const TableStyle& style, bool header) -> ColumnLayout;

  std::shared_ptr<const Model> model_;
};

}
