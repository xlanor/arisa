#pragma once

#include <arisa/composition/drop_operation.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/reorder_difference.hpp>
#include <arisa/composition/size.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::composition {

class DropSession {
 public:
  class ID {
   public:
    constexpr explicit ID(std::uint64_t value) noexcept : value_(value) {}

    constexpr auto value() const noexcept -> std::uint64_t { return value_; }

    friend constexpr auto operator==(ID, ID) -> bool = default;

   private:
    std::uint64_t value_;
  };

  class LocalSession {
   public:
    explicit LocalSession(std::vector<ElementKey> items) noexcept
        : items_(std::move(items)) {}

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

    auto item_keys() const noexcept -> const std::vector<ElementKey>& {
      return items_;
    }

   private:
    std::vector<ElementKey> items_;
  };

  class Phase {
   public:
    enum class Kind : std::uint8_t {
      entering,
      active,
      exiting,
      ended,
      data_transfer_completed,
    };

    static constexpr auto entering() noexcept -> Phase {
      return Phase{Kind::entering, std::nullopt};
    }

    static constexpr auto active() noexcept -> Phase {
      return Phase{Kind::active, std::nullopt};
    }

    static constexpr auto exiting() noexcept -> Phase {
      return Phase{Kind::exiting, std::nullopt};
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

  struct ReorderTarget {
    std::optional<ElementKey> before;
    std::optional<ElementKey> collection;

    friend auto operator==(const ReorderTarget&, const ReorderTarget&)
        -> bool = default;
  };

  struct Snapshot {
    std::uint64_t id{0};
    Phase phase{Phase::entering()};
    std::optional<LocalSession> local;
    std::size_t items_count{0};
    DropOperationSet suggested{};
    Size size{};
    Point location{};
    std::optional<ReorderTarget> reorder;
  };

  explicit DropSession(Snapshot snapshot)
      : id_(snapshot.id),
        phase_(snapshot.phase),
        local_(std::move(snapshot.local)),
        items_count_(snapshot.items_count),
        suggested_(snapshot.suggested),
        size_(snapshot.size),
        location_(snapshot.location),
        reorder_(std::move(snapshot.reorder)) {}

  auto id() const noexcept -> ID { return id_; }

  auto phase() const noexcept -> const Phase& { return phase_; }

  auto local_session() const noexcept -> const std::optional<LocalSession>& {
    return local_;
  }

  auto items_count() const noexcept -> std::size_t { return items_count_; }

  auto suggested_operations() const noexcept -> DropOperationSet {
    return suggested_;
  }

  auto size() const noexcept -> Size { return size_; }

  auto location() const noexcept -> Point { return location_; }

  auto reorder_target() const noexcept -> const std::optional<ReorderTarget>& {
    return reorder_;
  }

  template <class ItemID,
            class CollectionID = ReorderableSingleCollectionIdentifier>
  auto reorder_destination() const -> std::optional<
      typename ReorderDifference<ItemID, CollectionID>::Destination> {
    using Destination =
        typename ReorderDifference<ItemID, CollectionID>::Destination;
    using Placement = typename Destination::Position;
    if (!reorder_.has_value()) {
      return std::nullopt;
    }
    std::optional<Placement> position;
    if (reorder_->before.has_value()) {
      std::optional<ItemID> item = reorder_->before->as<ItemID>();
      if (!item.has_value()) {
        return std::nullopt;
      }
      position = Placement::before(std::move(*item));
    } else {
      position = Placement::end();
    }
    if constexpr (std::same_as<CollectionID,
                               ReorderableSingleCollectionIdentifier>) {
      return Destination{std::move(*position)};
    } else {
      if (!reorder_->collection.has_value()) {
        return std::nullopt;
      }
      std::optional<CollectionID> collection =
          reorder_->collection->as<CollectionID>();
      if (!collection.has_value()) {
        return std::nullopt;
      }
      return Destination{std::move(*position), std::move(*collection)};
    }
  }

 private:
  ID id_;
  Phase phase_;
  std::optional<LocalSession> local_;
  std::size_t items_count_;
  DropOperationSet suggested_;
  Size size_;
  Point location_;
  std::optional<ReorderTarget> reorder_;
};

}
