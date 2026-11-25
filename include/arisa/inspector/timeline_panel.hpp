#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/inspector/inspector_model.hpp>

#include <arisa/inspector/timeline_scale.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/theme/colour_value.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <utility>

namespace arisa::inspector {

class TimelinePanel final : public composition::Component {
 public:
  explicit TimelinePanel(std::shared_ptr<InspectorModel> model) noexcept
      : model_(std::move(model)) {}

  auto operator==(const TimelinePanel&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  struct Phase {
    std::string_view label;
    theme::ColourRole colour;
    std::chrono::nanoseconds (*measure)(const runtime::FrameTimings&);
  };

  static constexpr float cpu_height = 120.0F;
  static constexpr float gpu_height = 48.0F;
  static constexpr float chart_inset = 64.0F;
  static constexpr float marker_height = 3.0F;
  static constexpr theme::ColourRole cap_colour = theme::ColourRole::orange;
  static constexpr theme::ColourRole missed_colour = theme::ColourRole::red;

  static const std::array<Phase, 8> cpu_phases;
  static const std::array<Phase, 2> render_phases;
  static const std::array<Phase, 3> gpu_phases;

  template <std::size_t Count>
  auto chart(const std::array<Phase, Count>& phases, TimelineScale scale,
             bool marks) const -> composition::Element;

  static auto legend() -> composition::Element;

  static auto swatch(std::string_view label, theme::ColourRole colour)
      -> composition::Element;

  auto controls() const -> composition::Element;

  auto selection() const -> composition::Element;

  std::shared_ptr<InspectorModel> model_;
};

}
