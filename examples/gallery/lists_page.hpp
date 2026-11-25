#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class ListsPage final : public composition::Component {
 public:
  enum class Detail : std::uint8_t {
    about,
    storage,
  };

  enum class Accent : std::uint8_t {
    blue,
    green,
    orange,
  };

  enum class Quality : std::uint8_t {
    low,
    balanced,
    high,
  };

  class Settings;

  static constexpr composition::StateKey<bool> wifi{"lists-wifi", true};
  static constexpr composition::StateKey<bool> bluetooth{"lists-bluetooth",
                                                         false};
  static constexpr composition::StateKey<bool> editing{"lists-editing", false};
  static constexpr composition::StateKey<double> volume{"lists-volume", 0.6};
  static constexpr composition::StateKey<Accent> accent{"lists-accent",
                                                        Accent::blue};
  static constexpr composition::StateKey<Quality> quality{"lists-quality",
                                                          Quality::balanced};
  static constexpr composition::StateKey<int> added{"lists-added", 0};
  static constexpr composition::StateKey<bool> row_actions{"lists-row-actions",
                                                           false};
  static const composition::StateKey<std::string> name;
  static const composition::StateKey<std::vector<std::string>> fruits;
  static const composition::StateKey<std::vector<Detail>> path;
  static constexpr std::string_view add_button = "lists-add";
  static constexpr std::string_view remove_button = "lists-remove";
  static constexpr std::string_view shuffle_button = "lists-shuffle";
  static constexpr std::string_view row_actions_button = "lists-row-actions";
  static constexpr std::string_view settings_form = "lists-settings";
  static constexpr std::string_view fruit_list = "lists-fruits";

  auto operator==(const ListsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto initial_fruits() -> std::vector<std::string>;

  static auto detail(Detail shown) -> composition::Element;

 private:
  static auto fruit_rows(composition::Context& context) -> composition::Element;

  static auto quality_name(Quality value) -> std::string_view;

  static auto colour_of(Accent value) noexcept -> theme::Colour;
};

class ListsPage::Settings final : public composition::Component {
 public:
  explicit Settings(composition::StateHandle<Accent> tint) noexcept
      : tint_(std::move(tint)), shown_(tint_.get()) {}

  auto operator==(const Settings& other) const -> bool {
    return tint_.identity() == other.tint_.identity() && shown_ == other.shown_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  composition::StateHandle<Accent> tint_;
  Accent shown_;
};

}
