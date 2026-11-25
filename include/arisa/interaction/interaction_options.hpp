#pragma once

#include <arisa/interaction/confirm_back_layout.hpp>
#include <arisa/interaction/spatial_search.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <string_view>

namespace arisa::interaction {

struct InteractionOptions {
  ConfirmBackLayout layout{ConfirmBackLayout::nintendo};
  provider::Button play_pause{provider::Button::x};
  provider::Button remove{provider::Button::minus};
  provider::Button move{provider::Button::x};
  provider::Button page_previous{provider::Button::zl};
  provider::Button page_next{provider::Button::zr};
  float alignment_bias{SpatialSearch::default_alignment_bias};
  std::string_view confirm_label{"OK"};
  float focus_scroll_margin{24.0F};
  bool right_stick_scrolls{true};
  bool hides_focus_on_touch{false};

  friend constexpr auto operator==(const InteractionOptions&,
                                   const InteractionOptions&) -> bool = default;
};

}
