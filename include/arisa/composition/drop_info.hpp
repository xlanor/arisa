#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/ut_type.hpp>

#include <algorithm>
#include <span>
#include <utility>
#include <vector>

namespace arisa::composition {

class DropInfo {
 public:
  DropInfo(Point location, std::span<const TransferItem> items) noexcept
      : location_(location), items_(items) {}

  auto location() const noexcept -> Point { return location_; }

  auto has_items_conforming_to(std::span<const UTType> types) const -> bool {
    return std::ranges::any_of(items_, [types](const TransferItem& item) {
      return std::ranges::contains(types, item.type());
    });
  }

  auto item_providers(std::span<const UTType> types) const
      -> std::vector<TransferItem> {
    std::vector<TransferItem> found;
    for (const TransferItem& item : items_) {
      if (std::ranges::contains(types, item.type())) {
        found.push_back(item);
      }
    }
    return found;
  }

 private:
  Point location_;
  std::span<const TransferItem> items_;
};

}
