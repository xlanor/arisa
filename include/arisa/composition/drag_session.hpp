#pragma once

#include <arisa/composition/drop_operation.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/point.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::composition {

class DragSession {
 public:
  class ID {
   public:
    constexpr explicit ID(std::uint64_t value) noexcept : value_(value) {}

    constexpr auto value() const noexcept -> std::uint64_t { return value_; }

    friend constexpr auto operator==(ID, ID) -> bool = default;

   private:
    std::uint64_t value_;
  };

  class Phase {
   public:
    enum class Kind : std::uint8_t {
      initial,
      active,
      ending,
      ended,
      data_transfer_completed,
    };

    static constexpr auto initial() noexcept -> Phase {
      return Phase{Kind::initial, std::nullopt};
    }

    static constexpr auto active() noexcept -> Phase {
      return Phase{Kind::active, std::nullopt};
    }

    static constexpr auto ending(DropOperation operation) noexcept -> Phase {
      return Phase{Kind::ending, operation};
    }

    static constexpr auto ended(DropOperation operation) noexcept -> Phase {
      return Phase{Kind::ended, operation};
    }

    static constexpr auto data_transfer_completed() noexcept -> Phase {
      return Phase{Kind::data_transfer_completed, std::nullopt};
    }

    constexpr auto kind() const noexcept -> Kind { return kind_; }

    constexpr auto operation() const noexcept -> std::optional<DropOperation> {
      return operation_;
    }

    friend constexpr auto operator==(const Phase&, const Phase&)
        -> bool = default;

   private:
    constexpr Phase(Kind kind, std::optional<DropOperation> operation) noexcept
        : kind_(kind), operation_(operation) {}

    Kind kind_;
    std::optional<DropOperation> operation_;
  };

  DragSession(ID id, Phase phase, std::vector<ElementKey> items,
              std::size_t index, Point location)
      : id_(id),
        phase_(phase),
        items_(std::move(items)),
        index_(index),
        location_(location) {}

  auto id() const noexcept -> ID { return id_; }

  auto phase() const noexcept -> const Phase& { return phase_; }

  auto dragged_item_index() const noexcept -> std::size_t { return index_; }

  auto location() const noexcept -> Point { return location_; }

  void set_location(Point location) noexcept { location_ = location; }

  auto item_keys() const noexcept -> const std::vector<ElementKey>& {
    return items_;
  }

  template <class ItemID>
  auto dragged_item_ids() const -> std::vector<ItemID> {
    std::vector<ItemID> found;
    for (const ElementKey& key : items_) {
      if (std::optional<ItemID> value = key.as<ItemID>()) {
        found.push_back(std::move(*value));
      }
    }
    return found;
  }

 private:
  ID id_;
  Phase phase_;
  std::vector<ElementKey> items_;
  std::size_t index_;
  Point location_;
};

}
