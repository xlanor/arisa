#include <arisa/components/control_metrics.hpp>

namespace arisa::components {

auto ControlMetrics::of(ControlSize size) noexcept -> ControlMetrics {
  switch (size) {
    case ControlSize::mini:
      return ControlMetrics{.button_horizontal_padding = 8.0F,
                            .button_vertical_padding = 1.0F,
                            .font_scale = 9.0F / 13.0F,
                            .capsule = false,
                            .switch_width = 36.0F,
                            .switch_height = 16.0F,
                            .knob_width = 21.0F,
                            .knob_height = 14.0F,
                            .knob_inset = 1.0F,
                            .checkbox_extent = 12.0F,
                            .slider_height = 12.0F,
                            .slider_knob_width = 16.0F,
                            .slider_knob_height = 12.0F,
                            .slider_track = 4.0F,
                            .stepper_height = 16.0F,
                            .segmented_height = 16.0F,
                            .spinner_extent = 10.0F,
                            .progress_thickness = 6.0F,
                            .text_field_height = 19.0F};
    case ControlSize::small:
      return ControlMetrics{.button_horizontal_padding = 10.0F,
                            .button_vertical_padding = 3.0F,
                            .font_scale = 11.0F / 13.0F,
                            .capsule = false,
                            .switch_width = 44.0F,
                            .switch_height = 20.0F,
                            .knob_width = 26.0F,
                            .knob_height = 16.0F,
                            .knob_inset = 2.0F,
                            .checkbox_extent = 14.0F,
                            .slider_height = 14.0F,
                            .slider_knob_width = 18.0F,
                            .slider_knob_height = 14.0F,
                            .slider_track = 4.0F,
                            .stepper_height = 20.0F,
                            .segmented_height = 20.0F,
                            .spinner_extent = 16.0F,
                            .progress_thickness = 6.0F,
                            .text_field_height = 21.0F};
    case ControlSize::regular:
      return ControlMetrics{};
    case ControlSize::large:
      return ControlMetrics{.button_horizontal_padding = 14.0F,
                            .button_vertical_padding = 6.0F,
                            .font_scale = 1.0F,
                            .capsule = true,
                            .switch_width = 64.0F,
                            .switch_height = 28.0F,
                            .knob_width = 38.0F,
                            .knob_height = 24.0F,
                            .knob_inset = 2.0F,
                            .checkbox_extent = 18.0F,
                            .slider_height = 20.0F,
                            .slider_knob_width = 24.0F,
                            .slider_knob_height = 20.0F,
                            .slider_track = 6.0F,
                            .stepper_height = 28.0F,
                            .segmented_height = 28.0F,
                            .spinner_extent = 32.0F,
                            .progress_thickness = 8.0F,
                            .text_field_height = 24.0F};
    case ControlSize::extra_large:
      return ControlMetrics{.button_horizontal_padding = 18.0F,
                            .button_vertical_padding = 10.0F,
                            .font_scale = 1.0F,
                            .capsule = true,
                            .switch_width = 80.0F,
                            .switch_height = 36.0F,
                            .knob_width = 47.0F,
                            .knob_height = 30.0F,
                            .knob_inset = 3.0F,
                            .checkbox_extent = 18.0F,
                            .slider_height = 20.0F,
                            .slider_knob_width = 24.0F,
                            .slider_knob_height = 20.0F,
                            .slider_track = 6.0F,
                            .stepper_height = 36.0F,
                            .segmented_height = 36.0F,
                            .spinner_extent = 32.0F,
                            .progress_thickness = 8.0F,
                            .text_field_height = 24.0F};
  }
  return ControlMetrics{};
}

}
