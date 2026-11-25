#pragma once

#include <arisa/composition/element_key.hpp>

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace arisa::composition {

class ReorderableSingleCollectionIdentifier {
 public:
  friend constexpr auto operator==(ReorderableSingleCollectionIdentifier,
                                   ReorderableSingleCollectionIdentifier)
      -> bool = default;
};

template <class ItemID,
          class CollectionID = ReorderableSingleCollectionIdentifier>
class ReorderDifference {
 public:
  class Destination {
   public:
    class Position {
     public:
      static auto before(ItemID item) -> Position {
        return Position{std::optional<ItemID>{std::move(item)}};
      }

      static auto end() -> Position { return Position{std::nullopt}; }

      auto item() const noexcept -> const std::optional<ItemID>& {
        return item_;
      }

      auto is_end() const noexcept -> bool { return !item_.has_value(); }

      friend auto operator==(const Position&, const Position&)
          -> bool = default;

     private:
      explicit Position(std::optional<ItemID> item) : item_(std::move(item)) {}

      std::optional<ItemID> item_;
    };

    Destination(Position position, CollectionID collection_id)
        : position_(std::move(position)),
          collection_(std::move(collection_id)) {}

    explicit Destination(Position position)
      requires std::same_as<CollectionID, ReorderableSingleCollectionIdentifier>
        : position_(std::move(position)) {}

    auto position() const noexcept -> const Position& { return position_; }

    auto collection_id() const noexcept -> const CollectionID& {
      return collection_;
    }

    friend auto operator==(const Destination&, const Destination&)
        -> bool = default;

   private:
    Position position_;
    CollectionID collection_{};
  };

  ReorderDifference(std::vector<ItemID> sources, Destination destination)
      : sources_(std::move(sources)), destination_(std::move(destination)) {}

  auto sources() const noexcept -> const std::vector<ItemID>& {
    return sources_;
  }

  auto destination() const noexcept -> const Destination& {
    return destination_;
  }

  template <class Item>
  void apply(std::vector<Item>& items, auto&& id_of) const {
    std::vector<Item> moved;
    std::vector<Item> kept;
    for (Item& item : items) {
      if (std::ranges::contains(sources_, id_of(item))) {
        moved.push_back(std::move(item));
      } else {
        kept.push_back(std::move(item));
      }
    }
    auto at = kept.end();
    if (const std::optional<ItemID>& before = destination_.position().item()) {
      at = std::ranges::find_if(
          kept, [&](const Item& item) { return id_of(item) == *before; });
    }
    kept.insert(at, std::make_move_iterator(moved.begin()),
                std::make_move_iterator(moved.end()));
    items = std::move(kept);
  }

  friend auto operator==(const ReorderDifference&, const ReorderDifference&)
      -> bool = default;

 private:
  std::vector<ItemID> sources_;
  Destination destination_;
};

}
