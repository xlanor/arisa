#pragma once

#include <cstdint>

namespace arisa::composition {

enum class DropOperation : std::uint8_t {
  cancel,
  forbidden,
  copy,
  move,
  remove,
  alias,
};

class DropOperationSet {
 public:
  static const DropOperationSet cancel;
  static const DropOperationSet copy;
  static const DropOperationSet move;
  static const DropOperationSet forbidden;
  static const DropOperationSet remove;
  static const DropOperationSet alias;

  constexpr DropOperationSet() noexcept = default;

  static constexpr auto of(DropOperation operation) noexcept
      -> DropOperationSet {
    return DropOperationSet{
        static_cast<std::uint8_t>(1U << static_cast<std::uint8_t>(operation))};
  }

  constexpr auto contains(DropOperation operation) const noexcept -> bool {
    return contains(of(operation));
  }

  constexpr auto contains(DropOperationSet other) const noexcept -> bool {
    return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  friend constexpr auto operator|(DropOperationSet left,
                                  DropOperationSet right) noexcept
      -> DropOperationSet {
    return DropOperationSet{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(DropOperationSet, DropOperationSet)
      -> bool = default;

 private:
  constexpr explicit DropOperationSet(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr DropOperationSet DropOperationSet::cancel =
    DropOperationSet::of(DropOperation::cancel);
inline constexpr DropOperationSet DropOperationSet::copy =
    DropOperationSet::of(DropOperation::copy);
inline constexpr DropOperationSet DropOperationSet::move =
    DropOperationSet::of(DropOperation::move);
inline constexpr DropOperationSet DropOperationSet::forbidden =
    DropOperationSet::of(DropOperation::forbidden);
inline constexpr DropOperationSet DropOperationSet::remove =
    DropOperationSet::of(DropOperation::remove);
inline constexpr DropOperationSet DropOperationSet::alias =
    DropOperationSet::of(DropOperation::alias);

}
