#pragma once

#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::structure {

class TableColumnCustomizationBehavior {
 public:
  static const TableColumnCustomizationBehavior resize;
  static const TableColumnCustomizationBehavior reorder;
  static const TableColumnCustomizationBehavior visibility;
  static const TableColumnCustomizationBehavior all;

  constexpr TableColumnCustomizationBehavior() noexcept = default;

  constexpr auto contains(
      TableColumnCustomizationBehavior member) const noexcept -> bool {
    return member.bits_ != 0U && (bits_ & member.bits_) == member.bits_;
  }

  constexpr auto empty() const noexcept -> bool { return bits_ == 0U; }

  constexpr auto union_with(TableColumnCustomizationBehavior other)
      const noexcept -> TableColumnCustomizationBehavior {
    return TableColumnCustomizationBehavior{
        static_cast<std::uint8_t>(bits_ | other.bits_)};
  }

  constexpr auto intersection(TableColumnCustomizationBehavior other)
      const noexcept -> TableColumnCustomizationBehavior {
    return TableColumnCustomizationBehavior{
        static_cast<std::uint8_t>(bits_ & other.bits_)};
  }

  constexpr auto symmetric_difference(TableColumnCustomizationBehavior other)
      const noexcept -> TableColumnCustomizationBehavior {
    return TableColumnCustomizationBehavior{
        static_cast<std::uint8_t>(bits_ ^ other.bits_)};
  }

  constexpr auto insert(TableColumnCustomizationBehavior member) noexcept
      -> std::pair<bool, TableColumnCustomizationBehavior> {
    const bool inserted = !contains(member);
    bits_ = static_cast<std::uint8_t>(bits_ | member.bits_);
    return {inserted, member};
  }

  constexpr auto remove(TableColumnCustomizationBehavior member) noexcept
      -> std::optional<TableColumnCustomizationBehavior> {
    const TableColumnCustomizationBehavior present = intersection(member);
    bits_ = static_cast<std::uint8_t>(bits_ & ~member.bits_);
    if (present.empty()) {
      return std::nullopt;
    }
    return present;
  }

  constexpr auto update(TableColumnCustomizationBehavior member) noexcept
      -> std::optional<TableColumnCustomizationBehavior> {
    const bool present = contains(member);
    bits_ = static_cast<std::uint8_t>(bits_ | member.bits_);
    if (!present) {
      return std::nullopt;
    }
    return member;
  }

  constexpr void form_union(TableColumnCustomizationBehavior other) noexcept {
    bits_ = static_cast<std::uint8_t>(bits_ | other.bits_);
  }

  constexpr void form_intersection(
      TableColumnCustomizationBehavior other) noexcept {
    bits_ = static_cast<std::uint8_t>(bits_ & other.bits_);
  }

  constexpr void form_symmetric_difference(
      TableColumnCustomizationBehavior other) noexcept {
    bits_ = static_cast<std::uint8_t>(bits_ ^ other.bits_);
  }

  friend constexpr auto operator==(TableColumnCustomizationBehavior,
                                   TableColumnCustomizationBehavior)
      -> bool = default;

 private:
  static constexpr std::uint8_t resize_bit = 1U;
  static constexpr std::uint8_t reorder_bit = 2U;
  static constexpr std::uint8_t visibility_bit = 4U;

  constexpr explicit TableColumnCustomizationBehavior(
      std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr TableColumnCustomizationBehavior
    TableColumnCustomizationBehavior::resize{
        TableColumnCustomizationBehavior::resize_bit};
inline constexpr TableColumnCustomizationBehavior
    TableColumnCustomizationBehavior::reorder{
        TableColumnCustomizationBehavior::reorder_bit};
inline constexpr TableColumnCustomizationBehavior
    TableColumnCustomizationBehavior::visibility{
        TableColumnCustomizationBehavior::visibility_bit};
inline constexpr TableColumnCustomizationBehavior
    TableColumnCustomizationBehavior::all{static_cast<std::uint8_t>(
        TableColumnCustomizationBehavior::resize_bit |
        TableColumnCustomizationBehavior::reorder_bit |
        TableColumnCustomizationBehavior::visibility_bit)};

}
