#pragma once

#include <arisa/components/edit_mode.hpp>
#include <arisa/components/primitive_button_style.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class GroupingsPage final : public composition::Component {
 public:
  class Opener;
  class Shown;

  enum class Demo : std::uint8_t {
    dates,
    colour,
    gauges,
    sizes,
    group_box,
    editing,
    menus,
    wheel,
  };

  enum class Flavour : std::uint8_t {
    vanilla,
    chocolate,
    strawberry,
    mint,
    caramel,
  };

  class HoldStyle {
   public:
    auto make_body(
        components::PrimitiveButtonStyle::Configuration configuration) const
        -> composition::Element;

    friend auto operator==(const HoldStyle&, const HoldStyle&)
        -> bool = default;
  };

  static constexpr std::string_view close_button = "groupings-close";
  static constexpr std::string_view date_picker = "groupings-date";
  static constexpr std::string_view graphical_picker = "groupings-graphical";
  static constexpr std::string_view multi_picker = "groupings-multi";
  static constexpr std::string_view colour_picker = "groupings-colour";
  static constexpr std::string_view tinted_preview = "groupings-tinted";
  static constexpr std::string_view sizes_grid = "groupings-sizes";
  static constexpr std::string_view columns_form = "groupings-columns";
  static constexpr std::string_view edit_list = "groupings-edit-list";
  static constexpr std::string_view repeat_button = "groupings-repeat";
  static constexpr std::string_view hold_button = "groupings-hold";
  static constexpr std::string_view wheel_picker = "groupings-wheel";

  static constexpr composition::StateKey<std::optional<Demo>> demo{
      "groupings-demo", std::nullopt};
  static constexpr composition::StateKey<std::chrono::sys_seconds> when{
      "groupings-when",
      std::chrono::sys_seconds{std::chrono::seconds{1'763'287'200}}};
  static const composition::StateKey<std::set<std::chrono::year_month_day>>
      days;
  static constexpr composition::StateKey<theme::Colour> colour{
      "groupings-colour", theme::Colour::hex(0x34C759)};
  static constexpr composition::StateKey<double> level{"groupings-level", 0.6};
  static constexpr composition::StateKey<bool> wifi{"groupings-wifi", true};
  static constexpr composition::StateKey<int> count{"groupings-count", 3};
  static constexpr composition::StateKey<Flavour> flavour{"groupings-flavour",
                                                          Flavour::chocolate};
  static constexpr composition::StateKey<components::EditMode> mode{
      "groupings-mode", components::EditMode::inactive};
  static const composition::StateKey<std::vector<std::string>> notes;
  static const composition::StateKey<std::string> name;
  static constexpr composition::StateKey<int> repeats{"groupings-repeats", 0};
  static constexpr composition::StateKey<int> holds{"groupings-holds", 0};
  static constexpr composition::StateKey<std::string_view> last_action{
      "groupings-chosen", "nothing yet"};

  explicit GroupingsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const GroupingsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto title_of(Demo chosen) -> std::string_view;

  static auto tile_key(Demo chosen) -> std::string;

 private:
  static auto tiles(composition::StateHandle<std::optional<Demo>> chosen)
      -> composition::Element;

  static auto tile(Demo each, std::string_view symbol,
                   composition::StateHandle<std::optional<Demo>> chosen)
      -> composition::Element;

  static auto flavours() -> composition::Element;

  static auto page(std::vector<composition::Element> content)
      -> composition::Element;

  static auto dates(composition::Context& context) -> composition::Element;

  static auto colours(composition::Context& context) -> composition::Element;

  static auto gauges(composition::Context& context) -> composition::Element;

  static auto sizes(composition::Context& context) -> composition::Element;

  static auto group_box(composition::Context& context) -> composition::Element;

  static auto editing(composition::Context& context) -> composition::Element;

  static auto menus(composition::Context& context) -> composition::Element;

  static auto wheel(composition::Context& context) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class GroupingsPage::Shown final : public composition::Component {
 public:
  explicit Shown(Demo chosen) noexcept : chosen_(chosen) {}

  auto operator==(const Shown&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Demo chosen_;
};

class GroupingsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"groupings-shown",
                                                         false};
  static constexpr std::string_view button = "groupings-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
