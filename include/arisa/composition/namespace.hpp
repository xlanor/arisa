#pragma once

#include <arisa/composition/identity.hpp>
#include <arisa/composition/node_id.hpp>

#include <compare>
#include <string_view>

namespace arisa::composition {

class Namespace {
 public:
  class ID {
   public:
    constexpr ID(Identity space, NodeId owner) noexcept
        : space_(space), owner_(owner) {}

    friend constexpr auto operator==(const ID&, const ID&) -> bool = default;

    friend constexpr auto operator<=>(const ID& left, const ID& right) noexcept
        -> std::strong_ordering {
      if (const std::strong_ordering order = left.space_ <=> right.space_;
          order != std::strong_ordering::equal) {
        return order;
      }
      return left.owner_ <=> right.owner_;
    }

   private:
    Identity space_;
    NodeId owner_;
  };

  constexpr explicit Namespace(std::string_view name) noexcept : name_(name) {}

  Namespace(const Namespace&) = delete;
  auto operator=(const Namespace&) -> Namespace& = delete;
  Namespace(Namespace&&) = delete;
  auto operator=(Namespace&&) -> Namespace& = delete;
  ~Namespace() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  auto identity() const noexcept -> Identity { return Identity{this}; }

  auto id(NodeId owner = NodeId{0}) const noexcept -> ID {
    return ID{identity(), owner};
  }

 private:
  std::string_view name_;
};

}
