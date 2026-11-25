#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/transfer_item.hpp>

#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::composition {

class Element;

class DragSource {
 public:
  explicit DragSource(TransferItem item) { items_.push_back(std::move(item)); }

  explicit DragSource(std::vector<TransferItem> items) noexcept
      : items_(std::move(items)) {}

  DragSource(std::vector<TransferItem> items, std::optional<ElementKey> id,
             std::optional<Namespace::ID> container) noexcept
      : items_(std::move(items)), id_(std::move(id)), container_(container) {}

  DragSource(const DragSource&) = delete;
  auto operator=(const DragSource&) -> DragSource& = delete;
  DragSource(DragSource&&) = delete;
  auto operator=(DragSource&&) -> DragSource& = delete;
  virtual ~DragSource() = default;

  auto item() const noexcept -> const TransferItem& { return items_.front(); }

  auto items() const noexcept -> std::span<const TransferItem> {
    return items_;
  }

  auto id() const noexcept -> const std::optional<ElementKey>& { return id_; }

  auto container() const noexcept -> const std::optional<Namespace::ID>& {
    return container_;
  }

  virtual auto preview() const -> std::optional<Element>;

 private:
  std::vector<TransferItem> items_;
  std::optional<ElementKey> id_;
  std::optional<Namespace::ID> container_;
};

}
