#pragma once

#include <arisa/structure/table_column_content.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class RowValue>
class TupleTableColumnContent : public TableColumnContent<RowValue> {
 public:
  using Spec = typename TableColumnContent<RowValue>::Spec;

  template <class... Columns>
    requires(std::derived_from<std::remove_cvref_t<Columns>,
                               TableColumnContent<RowValue>> &&
             ...)
  explicit TupleTableColumnContent(Columns&&... columns)
      : TableColumnContent<RowValue>(
            joined(std::forward<Columns>(columns)...)) {}

  auto value() const noexcept -> const std::vector<Spec>& {
    return this->specs();
  }

 private:
  template <class... Columns>
  static auto joined(Columns&&... columns) -> std::vector<Spec> {
    std::vector<Spec> made;
    (TableColumnContent<RowValue>::append(
         made,
         static_cast<TableColumnContent<RowValue>&&>(
             std::remove_cvref_t<Columns>{std::forward<Columns>(columns)})),
     ...);
    return made;
  }
};

}
