#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/structure/table_row_kind.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::structure {

template <class Value>
class TableRowContent {
 public:
  using TableRowValue = Value;

  struct Insert {
    std::vector<composition::UTType> types;
    std::function<void(std::size_t, std::vector<composition::TransferItem>)>
        action;
  };

  struct Node {
    TableRowKind kind{TableRowKind::row};
    std::optional<Value> value;
    std::shared_ptr<composition::PlaceholderContentView::Source> header;
    std::optional<composition::Binding<bool>> is_expanded;
    std::vector<Node> children;
    std::shared_ptr<const Insert> insert;
    std::size_t insert_index{0};
  };

  auto table_row_body() const noexcept -> const std::vector<Node>& {
    return nodes_;
  }

  auto take_nodes() && -> std::vector<Node> { return std::move(nodes_); }

 protected:
  TableRowContent() = default;

  explicit TableRowContent(std::vector<Node> nodes)
      : nodes_(std::move(nodes)) {}

  auto edit_nodes() noexcept -> std::vector<Node>& { return nodes_; }

  template <class... Rows>
  static auto joined(Rows&&... rows) -> std::vector<Node> {
    std::vector<Node> made;
    (append(made, std::remove_cvref_t<Rows>{std::forward<Rows>(rows)}), ...);
    return made;
  }

 private:
  static void append(std::vector<Node>& into, TableRowContent&& content) {
    for (Node& node : content.nodes_) {
      into.push_back(std::move(node));
    }
  }

  std::vector<Node> nodes_;
};

template <class Rows, class Value>
concept TableRowsOf =
    std::derived_from<std::remove_cvref_t<Rows>, TableRowContent<Value>>;

}
