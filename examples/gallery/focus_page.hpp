#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <string_view>

namespace arisa::examples {

class FocusPage final : public composition::Component {
 public:
  static constexpr composition::FocusState<int> cell{"focus-cell"};
  static constexpr composition::StateKey<int> generation{"focus-generation", 0};
  static constexpr composition::StateKey<bool> dismissed{"focus-dismissed",
                                                         false};
  static constexpr composition::StateKey<std::string_view> pressed{
      "focus-pressed", "nothing"};
  static constexpr int cell_count = 6;
  static constexpr int default_cell = 4;
  static constexpr int row_count = 4;
  static constexpr std::string_view jump_button = "focus-jump";
  static constexpr std::string_view reload_button = "focus-reload";
  static constexpr std::string_view dismiss_button = "focus-dismiss";

  auto operator==(const FocusPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  using Pressed = composition::StateHandle<std::string_view>;

  static auto section(std::string_view title,
                      std::array<std::string_view, 3> labels, Pressed last)
      -> composition::Element;

  static auto cells(composition::FocusBinding<int> focus)
      -> composition::Element;

  static auto reloading(composition::StateHandle<int> loads,
                        composition::StateHandle<bool> gone, Pressed last)
      -> composition::Element;

  static auto heading(std::string_view title) -> composition::Element;
};

}
