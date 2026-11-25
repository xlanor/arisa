#pragma once

#include <arisa/structure/table_row_content.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class TableOutlineGroupContent : public TableRowContent<Value> {
 public:
  using Node = typename TableRowContent<Value>::Node;

  template <std::ranges::input_range Data, class ChildrenOf>
    requires std::convertible_to<std::ranges::range_value_t<Data>, Value> &&
             std::invocable<const ChildrenOf&, const Value&>
  TableOutlineGroupContent(Data&& roots, ChildrenOf children_of)
      : TableRowContent<Value>(nodes(std::forward<Data>(roots), children_of)) {}

 private:
  static auto children(std::vector<Value> found) -> std::vector<Value> {
    return found;
  }

  static auto children(std::optional<std::vector<Value>> found)
      -> std::vector<Value> {
    return found.value_or(std::vector<Value>{});
  }

  template <class Data, class ChildrenOf>
  static auto nodes(Data&& items, const ChildrenOf& children_of)
      -> std::vector<Node> {
    std::vector<Node> made;
    for (const auto& item : items) {
      const Value& value = item;
      auto found = std::invoke(children_of, value);
      const bool branches = has(found);
      std::vector<Value> below = children(std::move(found));
      made.push_back(
          Node{.kind = branches ? TableRowKind::disclosure : TableRowKind::row,
               .value = value,
               .header = nullptr,
               .is_expanded = std::nullopt,
               .children = nodes(below, children_of),
               .insert = nullptr,
               .insert_index = 0});
    }
    return made;
  }

  static auto has(const std::vector<Value>& found) -> bool {
    return !found.empty();
  }

  static auto has(const std::optional<std::vector<Value>>& found) -> bool {
    return found.has_value();
  }
};

}
