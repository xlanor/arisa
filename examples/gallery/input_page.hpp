#pragma once

#include <gallery_tile.hpp>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/gesture_state.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class InputPage final : public composition::Component {
 public:
  class Opener;
  class Shown;

  enum class Demo : std::uint8_t {
    pinch,
    location,
    sequence,
    clipboard,
    home,
    focused,
    reset,
    keyboard,
  };

  static constexpr std::size_t grid_cells = 9;
  static constexpr std::array<std::string_view, 3> palette{"Red", "Green",
                                                           "Blue"};
  static constexpr std::array<std::string_view, 4> dock{"Mail", "Music",
                                                        "Photos", "Games"};
  static constexpr std::array<std::string_view, 4> shelf{"Alpha", "Beta",
                                                         "Gamma", "Delta"};
  static constexpr int page_count = 5;

  static constexpr std::string_view close_button = "input-close";
  static constexpr std::string_view pinch_target = "input-pinch-target";
  static constexpr std::string_view location_target = "input-location-target";
  static constexpr std::string_view sequence_target = "input-sequence-target";
  static constexpr std::string_view pasted_list = "input-pasted";
  static constexpr std::string_view favourite_button = "input-favourite";
  static constexpr std::string_view reload_button = "input-reload";
  static constexpr std::string_view keyboard_area = "input-keys";
  static constexpr std::string_view next_button = "input-next";

  static constexpr composition::StateKey<std::optional<Demo>> demo{
      "input-demo", std::nullopt};
  static constexpr composition::StateKey<double> scale{"input-scale", 1.0};
  static constexpr composition::StateKey<double> angle{"input-angle", 0.0};
  static constexpr composition::GestureState<double> magnifying{
      "input-magnifying", 1.0};
  static constexpr composition::GestureState<double> turning{"input-turning",
                                                             0.0};
  static constexpr composition::StateKey<std::optional<composition::Point>>
      tapped{"input-tapped", std::nullopt};
  static constexpr composition::StateKey<composition::Point> moved{
      "input-moved", composition::Point{}};
  static constexpr composition::GestureState<composition::Size> pulling{
      "input-pulling", composition::Size{}};
  static constexpr composition::StateKey<bool> armed{"input-armed", false};
  static constexpr composition::StateKey<std::vector<std::string>> pasted{
      "input-pasted", {}};
  static const composition::StateKey<
      std::array<std::optional<std::string>, grid_cells>>
      cells;
  static constexpr composition::StateKey<int> targeted{"input-targeted", -1};
  static constexpr composition::StateKey<int> placed{"input-placed", 0};
  static constexpr composition::StateKey<std::vector<std::string>> favourites{
      "input-favourites", {}};
  static constexpr composition::StateKey<int> generation{"input-generation", 0};
  static constexpr composition::StateKey<std::string> pressed_key{
      "input-pressed-key", std::string{}};
  static constexpr composition::StateKey<int> page{"input-page", 1};
  static constexpr composition::StateKey<int> shortcuts{"input-shortcuts", 0};
  static constexpr composition::Namespace list_space{"input-list"};
  static const composition::FocusedValueKey<std::string> picked;

  explicit InputPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const InputPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto title_of(Demo chosen) -> std::string_view;

  static auto tile_key(std::string_view prefix, std::size_t index)
      -> std::string;

 private:
  static auto tiles(composition::StateHandle<std::optional<Demo>> chosen)
      -> composition::Element;

  static auto tile(Demo each, std::string_view symbol,
                   composition::StateHandle<std::optional<Demo>> chosen)
      -> composition::Element;

  static auto card(std::string_view name) -> composition::Element;

  static auto pinch(composition::Context& context) -> composition::Element;

  static auto location(composition::Context& context) -> composition::Element;

  static auto sequence(composition::Context& context) -> composition::Element;

  static auto clipboard(composition::Context& context) -> composition::Element;

  static auto home(composition::Context& context) -> composition::Element;

  static auto focused(composition::Context& context) -> composition::Element;

  static auto reset(composition::Context& context) -> composition::Element;

  static auto keyboard(composition::Context& context) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class InputPage::Shown final : public composition::Component {
 public:
  explicit Shown(Demo chosen) noexcept : chosen_(chosen) {}

  auto operator==(const Shown&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Demo chosen_;
};

class InputPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"input-shown", false};
  static constexpr std::string_view button = "input-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
