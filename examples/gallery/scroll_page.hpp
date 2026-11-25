#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <string_view>

namespace arisa::examples {

class ScrollPage final : public composition::Component {
 public:
  static constexpr int row_count = 1000;
  static constexpr int jump_row = 500;
  static constexpr int card_count = 12;
  static constexpr std::string_view jump_button = "scroll-jump";
  static constexpr std::string_view list_view = "scroll-list";
  static constexpr std::string_view carousel_view = "scroll-carousel";
  static constexpr std::string_view drag_card = "scroll-card";
  static constexpr std::string_view hold_tile = "scroll-hold";
  static constexpr std::string_view tap_tile = "scroll-taps";
  static constexpr composition::StateKey<composition::Point> dragged{
      "scroll-dragged", composition::Point{}};
  static constexpr composition::StateKey<bool> holding{"scroll-holding", false};
  static constexpr composition::StateKey<int> holds{"scroll-holds", 0};
  static constexpr composition::StateKey<int> taps{"scroll-taps", 0};
  static constexpr composition::StateKey<bool> searching{"scroll-search",
                                                         false};
  static constexpr composition::StateKey<bool> views{"scroll-views", false};
  static constexpr std::string_view search_button = "scroll-search-button";
  static constexpr std::string_view views_button = "scroll-views-button";

  auto operator==(const ScrollPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto list() -> composition::Element;

  static auto row(int index) -> composition::Element;

  static auto carousel() -> composition::Element;

  static auto card(composition::StateHandle<composition::Point> position)
      -> composition::Element;

  static auto hold(composition::StateHandle<bool> pressed,
                   composition::StateHandle<int> count) -> composition::Element;

  static auto tapping(composition::StateHandle<int> count)
      -> composition::Element;

  static auto heading(std::string_view title) -> composition::Element;

  static auto more(composition::StateHandle<bool> search,
                   composition::StateHandle<bool> modifiers)
      -> composition::Element;
};

}
