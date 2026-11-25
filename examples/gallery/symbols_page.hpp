#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace arisa::examples {

class SymbolsPage final : public composition::Component {
 public:
  class Opener;

  enum class Tint : std::uint8_t { red, green, blue };

  static constexpr std::string_view close_button = "symbols-close";
  static constexpr std::string_view split_menu = "symbols-split";
  static constexpr std::string_view select_all = "symbols-select-all";
  static constexpr std::string_view balance_slider = "symbols-balance";
  static constexpr std::string_view share_link = "symbols-share";
  static constexpr std::string_view panel = "symbols-panel";
  static constexpr std::string_view press_button = "symbols-press";
  static constexpr std::string_view signal_slider = "symbols-signal";
  static constexpr std::string_view pulse_toggle = "symbols-pulse";
  static constexpr std::string_view primary_label = "symbols-primary";

  static constexpr composition::StateKey<bool> left_on{"symbols-left", true};
  static constexpr composition::StateKey<bool> right_on{"symbols-right", false};
  static constexpr composition::StateKey<Tint> front{"symbols-front",
                                                     Tint::red};
  static constexpr composition::StateKey<Tint> back{"symbols-back",
                                                    Tint::green};
  static constexpr composition::StateKey<double> balance{"symbols-balance",
                                                         0.5};
  static constexpr composition::StateKey<double> signal{"symbols-signal", 0.6};
  static constexpr composition::StateKey<int> amount{"symbols-amount", 3};
  static constexpr composition::StateKey<int> primaries{"symbols-primaries", 0};
  static constexpr composition::StateKey<int> presses{"symbols-presses", 0};
  static constexpr composition::StateKey<bool> pulsing{"symbols-pulsing",
                                                       false};
  static constexpr composition::StateKey<std::string> renamed{"symbols-renamed",
                                                              ""};

  explicit SymbolsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const SymbolsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto framed_pixels() -> std::shared_ptr<const provider::Image>;

  static auto checker_pixels() -> std::shared_ptr<const provider::Image>;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto buttons(composition::StateHandle<int> pressed)
      -> composition::Element;

  static auto menus(composition::StateHandle<int> chosen)
      -> composition::Element;

  static auto mixed(composition::StateHandle<bool> left,
                    composition::StateHandle<bool> right,
                    composition::StateHandle<Tint> first,
                    composition::StateHandle<Tint> second)
      -> composition::Element;

  static auto sliders(composition::StateHandle<double> level,
                      composition::StateHandle<int> count)
      -> composition::Element;

  static auto sharing(composition::StateHandle<std::string> name)
      -> composition::Element;

  static auto symbols(composition::StateHandle<double> strength,
                      composition::StateHandle<bool> animating)
      -> composition::Element;

  static auto images() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class SymbolsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"symbols-shown",
                                                         false};
  static constexpr std::string_view button = "symbols-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
