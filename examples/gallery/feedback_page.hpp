#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace arisa::examples {

class FeedbackPage final : public composition::Component {
 public:
  static constexpr std::size_t kinds = 17;
  static constexpr composition::StateKey<std::array<int, kinds>> plays{
      "feedback-plays", {}};
  static constexpr composition::StateKey<int> silent{"feedback-silent", 0};
  static constexpr std::string_view first_button = "feedback-0";
  static constexpr std::string_view silent_button = "feedback-silent";

  auto operator==(const FeedbackPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto key(std::size_t index) -> std::string;
};

}
