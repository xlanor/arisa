#pragma once

#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/structure/table_row.hpp>
#include <arisa/structure/table_row_content.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class TableForEachContent : public TableRowContent<Value> {
 public:
  using Node = typename TableRowContent<Value>::Node;
  using Insert = typename TableRowContent<Value>::Insert;

  template <std::ranges::input_range Data>
    requires std::convertible_to<std::ranges::range_value_t<Data>, Value>
  explicit TableForEachContent(Data&& data)
      : TableForEachContent(std::forward<Data>(data), [](const Value& value) {
          return TableRow<Value>{value};
        }) {}

  template <std::ranges::input_range Data, class Build>
    requires TableRowsOf<
        std::invoke_result_t<const Build&,
                             const std::ranges::range_value_t<Data>&>,
        Value>
  TableForEachContent(Data&& data, Build build)
      : TableRowContent<Value>(rows(std::forward<Data>(data), build)) {}

  auto data() const noexcept -> std::size_t {
    return this->table_row_body().size();
  }

  auto on_insert(
      std::vector<composition::UTType> types,
      std::function<void(std::size_t, std::vector<composition::TransferItem>)>
          action) && -> TableForEachContent {
    const auto insert = std::make_shared<const Insert>(
        Insert{.types = std::move(types), .action = std::move(action)});
    std::size_t index = 0;
    for (Node& node : this->edit_nodes()) {
      node.insert = insert;
      node.insert_index = index++;
    }
    return std::move(*this);
  }

 private:
  template <class Data, class Build>
  static auto rows(Data&& data, const Build& build) -> std::vector<Node> {
    std::vector<Node> made;
    for (const auto& item : data) {
      for (Node& node : std::invoke(build, item).take_nodes()) {
        made.push_back(std::move(node));
      }
    }
    return made;
  }
};

template <class Content>
concept DynamicTableRowContent = requires(const Content& content) {
  { content.data() } -> std::convertible_to<std::size_t>;
} && requires(Content content) {
  std::move(content).on_insert(
      std::vector<composition::UTType>{},
      std::function<void(std::size_t,
                         std::vector<composition::TransferItem>)>{});
};

}
