#pragma once

#include <arisa/composition/element_key.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::composition {

class HoverEffectPhaseOverride {
 public:
  enum class Kind : std::uint8_t {
    active,
    inactive,
    toggled,
  };

  static constexpr std::chrono::milliseconds temporary_duration{1000};

  static auto active() -> HoverEffectPhaseOverride {
    return HoverEffectPhaseOverride{Kind::active, std::nullopt, false};
  }

  static auto inactive() -> HoverEffectPhaseOverride {
    return HoverEffectPhaseOverride{Kind::inactive, std::nullopt, false};
  }

  template <Identifiable T>
  static auto toggled(const T& trigger) -> HoverEffectPhaseOverride {
    return HoverEffectPhaseOverride{Kind::toggled,
                                    ElementKey::identifying(trigger), false};
  }

  template <Identifiable T>
  static auto active_temporarily(const T& trigger) -> HoverEffectPhaseOverride {
    return HoverEffectPhaseOverride{Kind::active,
                                    ElementKey::identifying(trigger), true};
  }

  template <Identifiable T>
  static auto inactive_temporarily(const T& trigger)
      -> HoverEffectPhaseOverride {
    return HoverEffectPhaseOverride{Kind::inactive,
                                    ElementKey::identifying(trigger), true};
  }

  template <Identifiable T>
  static auto toggled_temporarily(const T& trigger)
      -> HoverEffectPhaseOverride {
    return HoverEffectPhaseOverride{Kind::toggled,
                                    ElementKey::identifying(trigger), true};
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto trigger() const noexcept -> const std::optional<ElementKey>& {
    return trigger_;
  }

  auto temporary() const noexcept -> bool { return temporary_; }

  auto applied(bool natural) const noexcept -> bool {
    switch (kind_) {
      case Kind::active:
        return true;
      case Kind::inactive:
        return false;
      case Kind::toggled:
        return !natural;
    }
    return natural;
  }

  friend auto operator==(const HoverEffectPhaseOverride&,
                         const HoverEffectPhaseOverride&) -> bool = default;

 private:
  HoverEffectPhaseOverride(Kind kind, std::optional<ElementKey> trigger,
                           bool temporary) noexcept
      : kind_(kind), trigger_(std::move(trigger)), temporary_(temporary) {}

  Kind kind_;
  std::optional<ElementKey> trigger_;
  bool temporary_;
};

}
