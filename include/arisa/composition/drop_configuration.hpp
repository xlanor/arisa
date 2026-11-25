#pragma once

#include <arisa/composition/drop_operation.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/reorder_difference.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::composition {

class DropConfiguration {
 public:
  explicit DropConfiguration(DropOperation operation) noexcept
      : operation_(operation) {}

  template <class Target>
    requires requires(const Target& target) {
      target.position().item();
      target.collection_id();
    }
  DropConfiguration(DropOperation operation, const Target& destination)
      : operation_(operation) {
    using CollectionID =
        std::remove_cvref_t<decltype(destination.collection_id())>;
    const auto& item = destination.position().item();
    destination_ =
        Destination{.before = item.has_value()
                                  ? std::optional<ElementKey>{ElementKey{*item}}
                                  : std::nullopt,
                    .collection = std::nullopt};
    if constexpr (!std::same_as<CollectionID,
                                ReorderableSingleCollectionIdentifier>) {
      destination_->collection = ElementKey{destination.collection_id()};
    }
  }

  struct Destination {
    std::optional<ElementKey> before;
    std::optional<ElementKey> collection;

    friend auto operator==(const Destination&, const Destination&)
        -> bool = default;
  };

  auto operation() const noexcept -> DropOperation { return operation_; }

  auto accepted_item_count() const noexcept -> std::optional<std::size_t> {
    return accepted_;
  }

  void set_accepted_item_count(std::optional<std::size_t> count) noexcept {
    accepted_ = count;
  }

  auto destination() const noexcept -> const std::optional<Destination>& {
    return destination_;
  }

  friend auto operator==(const DropConfiguration&, const DropConfiguration&)
      -> bool = default;

 private:
  DropOperation operation_;
  std::optional<std::size_t> accepted_;
  std::optional<Destination> destination_;
};

}
