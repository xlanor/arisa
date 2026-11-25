#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/structure/table_row_content.hpp>

#include <optional>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class DisclosureTableRow : public TableRowContent<Value> {
 public:
  using Node = typename TableRowContent<Value>::Node;

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  explicit DisclosureTableRow(Value value, Rows&&... children)
      : TableRowContent<Value>(disclosed(
            std::move(value), std::nullopt,
            TableRowContent<Value>::joined(std::forward<Rows>(children)...))) {}

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  DisclosureTableRow(Value value, composition::Binding<bool> is_expanded,
                     Rows&&... children)
      : TableRowContent<Value>(disclosed(
            std::move(value), std::move(is_expanded),
            TableRowContent<Value>::joined(std::forward<Rows>(children)...))) {}

 private:
  static auto disclosed(Value value,
                        std::optional<composition::Binding<bool>> is_expanded,
                        std::vector<Node> children) -> std::vector<Node> {
    std::vector<Node> made;
    made.push_back(Node{.kind = TableRowKind::disclosure,
                        .value = std::move(value),
                        .header = nullptr,
                        .is_expanded = std::move(is_expanded),
                        .children = std::move(children),
                        .insert = nullptr,
                        .insert_index = 0});
    return made;
  }
};

}
