#pragma once

#include <arisa/components/control_size.hpp>

namespace arisa::components {

struct ControlMetrics {
  float button_horizontal_padding{12.0F};
  float button_vertical_padding{4.0F};
  float font_scale{1.0F};
  bool capsule{false};
  float switch_width{54.0F};
  float switch_height{24.0F};
  float knob_width{32.0F};
  float knob_height{20.0F};
  float knob_inset{2.0F};
  float checkbox_extent{16.0F};
  float slider_height{16.0F};
  float slider_knob_width{20.0F};
  float slider_knob_height{16.0F};
  float slider_track{6.0F};
  float stepper_height{24.0F};
  float segmented_height{24.0F};
  float spinner_extent{32.0F};
  float progress_thickness{8.0F};
  float text_field_height{24.0F};

  static auto of(ControlSize size) noexcept -> ControlMetrics;
};

}
