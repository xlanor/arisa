#pragma once

#include <arisa/structure/table_row_content.hpp>

#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class TupleTableRowContent : public TableRowContent<Value> {
 public:
  using Node = typename TableRowContent<Value>::Node;

  template <class... Rows>
    requires(TableRowsOf<Rows, Value> && ...)
  explicit TupleTableRowContent(Rows&&... rows)
      : TableRowContent<Value>(
            TableRowContent<Value>::joined(std::forward<Rows>(rows)...)) {}

  auto value() const noexcept -> const std::vector<Node>& {
    return this->table_row_body();
  }
};

}
