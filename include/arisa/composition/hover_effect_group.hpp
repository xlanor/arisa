#pragma once

#include <arisa/composition/namespace.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace arisa::composition {

class HoverEffectGroup {
 public:
  class Behavior {
   public:
    enum class Kind : std::uint8_t {
      activates_group,
      follows_group,
      ignores_group,
      preserves_group,
    };

    static const Behavior activates_group;
    static const Behavior follows_group;
    static const Behavior ignores_group;
    static const Behavior preserves_group;

    constexpr auto kind() const noexcept -> Kind { return kind_; }

    constexpr auto activates() const noexcept -> bool {
      return kind_ == Kind::activates_group || kind_ == Kind::preserves_group;
    }

    constexpr auto follows() const noexcept -> bool {
      return kind_ != Kind::ignores_group;
    }

    friend constexpr auto operator==(Behavior, Behavior) -> bool = default;

   private:
    constexpr explicit Behavior(Kind kind) noexcept : kind_(kind) {}

    Kind kind_;
  };

  explicit HoverEffectGroup(Namespace::ID space,
                            Behavior behavior = Behavior::activates_group)
      : space_(space), behavior_(behavior) {}

  HoverEffectGroup(std::optional<std::string> id, Namespace::ID space,
                   Behavior behavior = Behavior::activates_group)
      : id_(std::move(id)), space_(space), behavior_(behavior) {}

  static auto system_overlays() -> HoverEffectGroup;

  auto behavior(Behavior behavior) const -> HoverEffectGroup {
    HoverEffectGroup changed = *this;
    changed.behavior_ = behavior;
    return changed;
  }

  auto behavior() const noexcept -> Behavior { return behavior_; }

  auto id() const noexcept -> const std::optional<std::string>& { return id_; }

  auto space() const noexcept -> Namespace::ID { return space_; }

  auto same_group(const HoverEffectGroup& other) const -> bool {
    return space_ == other.space_ && id_ == other.id_;
  }

  friend auto operator==(const HoverEffectGroup&, const HoverEffectGroup&)
      -> bool = default;

 private:
  std::optional<std::string> id_;
  Namespace::ID space_;
  Behavior behavior_;
};

inline constexpr HoverEffectGroup::Behavior
    HoverEffectGroup::Behavior::activates_group{Kind::activates_group};
inline constexpr HoverEffectGroup::Behavior
    HoverEffectGroup::Behavior::follows_group{Kind::follows_group};
inline constexpr HoverEffectGroup::Behavior
    HoverEffectGroup::Behavior::ignores_group{Kind::ignores_group};
inline constexpr HoverEffectGroup::Behavior
    HoverEffectGroup::Behavior::preserves_group{Kind::preserves_group};

}
