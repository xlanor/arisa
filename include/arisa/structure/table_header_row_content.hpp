#pragma once

#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/structure/table_row_content.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class TableHeaderRowContent : public TableRowContent<Value> {
 public:
  using Node = typename TableRowContent<Value>::Node;

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  explicit TableHeaderRowContent(std::string title, Rows&&... rows)
      : TableHeaderRowContent(
            titled(std::move(title)), std::nullopt,
            TableRowContent<Value>::joined(std::forward<Rows>(rows)...)) {}

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  TableHeaderRowContent(std::string title,
                        composition::Binding<bool> is_expanded, Rows&&... rows)
      : TableHeaderRowContent(
            titled(std::move(title)), std::move(is_expanded),
            TableRowContent<Value>::joined(std::forward<Rows>(rows)...)) {}

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  TableHeaderRowContent(composition::Element header, Rows&&... rows)
      : TableHeaderRowContent(
            std::move(header), std::nullopt,
            TableRowContent<Value>::joined(std::forward<Rows>(rows)...)) {}

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  TableHeaderRowContent(composition::Element header,
                        composition::Binding<bool> is_expanded, Rows&&... rows)
      : TableHeaderRowContent(
            std::move(header), std::move(is_expanded),
            TableRowContent<Value>::joined(std::forward<Rows>(rows)...)) {}

 private:
  TableHeaderRowContent(composition::Element header,
                        std::optional<composition::Binding<bool>> is_expanded,
                        std::vector<Node> rows)
      : TableRowContent<Value>(headed(std::move(header), std::move(is_expanded),
                                      std::move(rows))) {}

  static auto titled(std::string title) -> composition::Element {
    return components::Text(text::LocalizedStringKey{std::move(title)});
  }

  static auto headed(composition::Element header,
                     std::optional<composition::Binding<bool>> is_expanded,
                     std::vector<Node> rows) -> std::vector<Node> {
    std::vector<Node> made;
    made.push_back(Node{
        .kind = TableRowKind::header,
        .value = std::nullopt,
        .header = std::make_shared<composition::PlaceholderContentView::Source>(
            std::move(header)),
        .is_expanded = std::move(is_expanded),
        .children = std::move(rows),
        .insert = nullptr,
        .insert_index = 0});
    return made;
  }
};

}
