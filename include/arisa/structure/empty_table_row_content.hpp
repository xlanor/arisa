#pragma once

#include <arisa/structure/table_row_content.hpp>

namespace arisa::structure {

template <class Value>
class EmptyTableRowContent : public TableRowContent<Value> {
 public:
  EmptyTableRowContent() = default;
};

}
