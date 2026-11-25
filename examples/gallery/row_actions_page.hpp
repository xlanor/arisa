#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class RowActionsPage final : public composition::Component {
 public:
  enum class Panel : std::uint8_t {
    saves,
    settings,
    files,
    titles,
    tiles,
  };

  struct Save {
    int id{0};
    std::string title;
    int slots{0};
    bool pinned{false};

    friend auto operator==(const Save&, const Save&) -> bool = default;
  };

  struct Entry {
    std::string name;
    std::optional<std::vector<Entry>> children;

    friend auto operator==(const Entry&, const Entry&) -> bool = default;
  };

  static constexpr int save_count = 1000;
  static constexpr int title_count = 2000;
  static constexpr int tile_count = 48;
  static constexpr std::string_view close_button = "row-actions-close";
  static constexpr std::string_view edit_button = "row-actions-edit";
  static constexpr std::string_view delete_button = "row-actions-delete";
  static constexpr std::string_view saves_list = "row-actions-saves";
  static constexpr std::string_view titles_list = "row-actions-titles";
  static constexpr std::string_view tiles_grid = "row-actions-tiles";
  static constexpr composition::StateKey<Panel> panel_key{"row-actions-panel",
                                                          Panel::saves};
  static constexpr composition::StateKey<bool> editing_key{
      "row-actions-editing", false};
  static const composition::StateKey<std::vector<Save>> saves_key;
  static const composition::StateKey<std::set<int>> chosen_key;
  static const composition::StateKey<std::vector<int>> tiles_key;

  explicit RowActionsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const RowActionsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto initial_saves() -> std::vector<Save>;

  static auto initial_tiles() -> std::vector<int>;

  static auto files() -> std::vector<Entry>;

  static auto title_name(int index) -> std::string;

 private:
  static auto saves(composition::Context& context) -> composition::Element;

  static auto settings() -> composition::Element;

  static auto file_tree() -> composition::Element;

  static auto titles() -> composition::Element;

  static auto tiles(composition::Context& context) -> composition::Element;

  static auto save_row(const Save& save,
                       composition::StateHandle<std::vector<Save>> all)
      -> composition::Element;

  composition::StateHandle<bool> shown_;
};

}
