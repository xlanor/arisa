#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_drop_previews_formation.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class InteractionPage final : public composition::Component {
 public:
  class Opener;

  struct Tile {
    int id{0};
    std::string name;

    auto encoded() const -> std::string;

    static auto decoded(std::string_view text) -> std::optional<Tile>;

    static auto content_type() -> std::string;

    friend auto operator==(const Tile&, const Tile&) -> bool = default;
  };

  static constexpr int tile_count = 4;
  static constexpr float area_width = 160.0F;
  static constexpr float area_height = 72.0F;

  static constexpr std::string_view close_button = "interaction-close";
  static constexpr std::string_view touch_area = "interaction-touch";
  static constexpr std::string_view pointer_area = "interaction-pointer";
  static constexpr std::string_view controller_area = "interaction-controller";
  static constexpr std::string_view mapped_area = "interaction-mapped";
  static constexpr std::string_view tray = "interaction-tray";
  static constexpr std::string_view spring_button = "interaction-spring";
  static constexpr std::string_view repeat_button = "interaction-repeat";
  static constexpr std::string_view tile_prefix = "interaction-tile-";
  static constexpr std::string_view fruit_prefix = "interaction-fruit-";

  static constexpr composition::StateKey<int> touches{"interaction-touches", 0};
  static constexpr composition::StateKey<int> clicks{"interaction-clicks", 0};
  static constexpr composition::StateKey<int> presses{"interaction-presses", 0};
  static constexpr composition::StateKey<int> moved{"interaction-moved", 0};
  static constexpr composition::StateKey<int> repeats{"interaction-repeats", 0};
  static constexpr composition::StateKey<bool> sprung{"interaction-sprung",
                                                      false};
  static constexpr composition::StateKey<bool> option{"interaction-option",
                                                      false};
  static constexpr composition::StateKey<composition::DragDropPreviewsFormation>
      formation{"interaction-formation",
                composition::DragDropPreviewsFormation::pile};
  static const composition::StateKey<std::vector<std::string>> dropped;
  static const composition::StateKey<std::vector<int>> selected;
  static const composition::StateKey<std::vector<int>> left;
  static const composition::StateKey<std::vector<int>> right;
  static constexpr composition::Namespace tiles{"interaction-tiles"};

  explicit InteractionPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const InteractionPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto tile_key(int id) -> std::string;

  static auto fruit_key(std::string_view name) -> std::string;

  static auto tile_name(int id) -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto area(std::string_view key, std::string_view label)
      -> composition::Element;

  static auto kinds(composition::StateHandle<int> touched,
                    composition::StateHandle<int> clicked,
                    composition::StateHandle<int> pressed)
      -> composition::Element;

  static auto mapped(composition::StateHandle<int> travelled)
      -> composition::Element;

  static auto keys(composition::StateHandle<bool> held) -> composition::Element;

  static auto fruits() -> composition::Element;

  static auto carrying(
      composition::Namespace::ID space,
      composition::StateHandle<std::vector<int>> chosen,
      composition::StateHandle<std::vector<std::string>> received,
      composition::StateHandle<composition::DragDropPreviewsFormation> shape,
      composition::StateHandle<bool> opened) -> composition::Element;

  static auto reordering(composition::StateHandle<std::vector<int>> first,
                         composition::StateHandle<std::vector<int>> second)
      -> composition::Element;

  static auto repeating(composition::StateHandle<int> counted)
      -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class InteractionPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"interaction-shown",
                                                         false};
  static constexpr std::string_view button = "interaction-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
