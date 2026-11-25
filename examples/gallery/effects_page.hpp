#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/hover_effect.hpp>

#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class EffectsPage final : public composition::Component {
 public:
  static constexpr composition::StateKey<std::optional<int>> selected{
      "selected", std::nullopt};
  static constexpr composition::StateKey<int> jumps{"jumps", 0};
  static constexpr composition::StateKey<int> tally{"tally", 128};
  static constexpr composition::Namespace hero{"hero"};
  static constexpr int card_count = 3;
  static constexpr std::string_view first_card = "effects-card-0";
  static constexpr std::string_view jump_button = "effects-jump";
  static constexpr std::string_view increment_button = "effects-increment";
  static constexpr std::string_view detail_card = "effects-detail";
  static constexpr std::array<std::string_view, 3> choreography{
      first_card, jump_button, increment_button};

  auto operator==(const EffectsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static constexpr float stage_width = 680.0F;
  static constexpr float column_width = 220.0F;

  struct Hop {
    double scale{1.0};
    double lift{0.0};
    motion::Angle spin{};
  };

  static auto stage(composition::StateHandle<std::optional<int>> chosen,
                    composition::Namespace::ID space) -> composition::Element;

  static auto card(int index,
                   composition::StateHandle<std::optional<int>> chosen,
                   composition::Namespace::ID space) -> composition::Element;

  static auto detail(int index,
                     composition::StateHandle<std::optional<int>> chosen,
                     composition::Namespace::ID space) -> composition::Element;

  static auto jumper(composition::StateHandle<int> hops)
      -> composition::Element;

  static auto counter(composition::StateHandle<int> count)
      -> composition::Element;

  static auto focus_row(composition::StateHandle<int> count)
      -> composition::Element;

  static auto tile(std::string_view label, motion::HoverEffect effect,
                   composition::StateHandle<int> count) -> composition::Element;

  static auto title(int index) -> std::string;
};

}
