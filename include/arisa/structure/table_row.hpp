#pragma once

#include <arisa/structure/table_row_content.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class TableRow : public TableRowContent<Value> {
 public:
  using Node = typename TableRowContent<Value>::Node;

  explicit TableRow(Value value)
      : TableRowContent<Value>(row(std::move(value))) {}

 private:
  static auto row(Value value) -> std::vector<Node> {
    std::vector<Node> made;
    made.push_back(Node{.kind = TableRowKind::row,
                        .value = std::move(value),
                        .header = nullptr,
                        .is_expanded = std::nullopt,
                        .children = {},
                        .insert = nullptr,
                        .insert_index = 0});
    return made;
  }
};

}
