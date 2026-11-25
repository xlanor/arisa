#pragma once

#include <arisa/composition/rect.hpp>
#include <arisa/support/strong_value.hpp>
#include <arisa/theme/space.hpp>

#include <cstdint>

namespace arisa::composition {

class ReservedRegion {
 public:
  struct IdTag {};

  using ID = support::StrongValue<std::uint64_t, IdTag>;

  enum class Kind : std::uint8_t {
    occlusion,
    division,
  };

  class QueryOptions {
   public:
    static const QueryOptions include_inactive;

    constexpr QueryOptions() noexcept = default;

    constexpr auto contains(QueryOptions other) const noexcept -> bool {
      return other.bits_ != 0U && (bits_ & other.bits_) == other.bits_;
    }

    friend constexpr auto operator|(QueryOptions left,
                                    QueryOptions right) noexcept
        -> QueryOptions {
      return QueryOptions{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
    }

    friend constexpr auto operator==(QueryOptions, QueryOptions)
        -> bool = default;

   private:
    constexpr explicit QueryOptions(std::uint8_t bits) noexcept : bits_(bits) {}

    std::uint8_t bits_{0};
  };

  constexpr ReservedRegion(ID id, Kind kind, Rect frame,
                           theme::Edges margins = {},
                           bool is_active = true) noexcept
      : id_(id),
        kind_(kind),
        frame_(frame),
        margins_(margins),
        active_(is_active) {}

  constexpr auto id() const noexcept -> ID { return id_; }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto frame() const noexcept -> Rect { return frame_; }

  constexpr auto margins() const noexcept -> theme::Edges { return margins_; }

  constexpr auto is_active() const noexcept -> bool { return active_; }

  constexpr auto moved_to(Rect frame, theme::Edges margins) const noexcept
      -> ReservedRegion {
    return ReservedRegion{id_, kind_, frame, margins, active_};
  }

  friend constexpr auto operator==(const ReservedRegion&, const ReservedRegion&)
      -> bool = default;

 private:
  ID id_;
  Kind kind_;
  Rect frame_;
  theme::Edges margins_;
  bool active_;
};

inline constexpr ReservedRegion::QueryOptions
    ReservedRegion::QueryOptions::include_inactive{1U};

}
