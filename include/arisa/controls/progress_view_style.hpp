#pragma once

#include <arisa/components/control_size.hpp>

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/metrics.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class DefaultProgressViewStyle;
class LinearProgressViewStyle;
class CircularProgressViewStyle;
class SegmentedProgressViewStyle;

class ProgressViewStyle {
 public:
  struct Configuration {
    std::optional<double> fraction_completed;
    std::optional<composition::Element> label;
    std::optional<composition::Element> current_value_label{};
    components::ControlSize control_size{components::ControlSize::regular};
  };

  class Indeterminate;

  static const composition::EnvironmentKey<ProgressViewStyle> key;

  ProgressViewStyle(DefaultProgressViewStyle style) noexcept;

  ProgressViewStyle(LinearProgressViewStyle style) noexcept;

  ProgressViewStyle(CircularProgressViewStyle style) noexcept;

  ProgressViewStyle(SegmentedProgressViewStyle style) noexcept;

  static constexpr float spinner_extent =
      theme::metrics::progress.spinner_extent;
  static constexpr std::size_t spokes = 12;
  static constexpr float bar_thickness = theme::metrics::progress.bar_thickness;
  static constexpr float sweep_share = 0.3F;
  static constexpr std::chrono::milliseconds spin_period{1000};
  static constexpr std::chrono::milliseconds sweep_period{1400};
  static constexpr double indeterminate_frame_rate = 30.0;
  static constexpr float segment_width = 26.0F;
  static constexpr float current_segment_width = 34.0F;
  static constexpr float segment_thickness = 6.0F;
  static constexpr float segment_spacing = 9.0F;
  static constexpr float segment_glow_radius = 7.0F;
  static constexpr float segment_glow_opacity = 0.75F;
  static constexpr float pending_segment_opacity = 0.28F;
  static constexpr double segment_tolerance = 1e-9;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, ProgressViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           DefaultProgressViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           LinearProgressViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           CircularProgressViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           SegmentedProgressViewStyle> &&
             components::StyleOf<Style, Configuration>)
  ProgressViewStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> ProgressViewStyle;

  static auto linear() noexcept -> ProgressViewStyle;

  static auto circular() noexcept -> ProgressViewStyle;

  static auto segmented(std::size_t segments) noexcept -> ProgressViewStyle;

  static auto current_segment(std::size_t segments,
                              std::optional<double> fraction) noexcept
      -> std::optional<std::size_t>;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const ProgressViewStyle& left,
                         const ProgressViewStyle& right) -> bool;

  static auto spinner(double phase, std::optional<double> fraction)
      -> composition::Element;

  static auto bar(double start, double share, float thickness = bar_thickness)
      -> composition::Element;

  static auto segments(std::size_t count, std::optional<double> fraction)
      -> composition::Element;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    linear,
    circular,
    segmented,
    custom,
  };

  explicit ProgressViewStyle(Kind kind) noexcept : kind_(kind) {}

  static auto lined(Configuration configuration) -> composition::Element;

  static auto circled(Configuration configuration) -> composition::Element;

  static auto segmented(Configuration configuration, std::size_t count)
      -> composition::Element;

  static auto labelled(composition::Element indicator,
                       std::optional<composition::Element> label)
      -> composition::Element;

  static auto sized(composition::Element indicator,
                    components::ControlSize size) -> composition::Element;

  Kind kind_;
  std::size_t segments_{0};
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class ProgressViewStyle::Indeterminate : public composition::Element {
 public:
  class Content;

  enum class Motion : std::uint8_t {
    spin,
    sweep,
  };

  explicit Indeterminate(Motion motion);

  static auto cycle(std::chrono::milliseconds period) noexcept
      -> motion::Animation;
};

class ProgressViewStyle::Indeterminate::Content final
    : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<bool> running{"progress-running",
                                                       false};
  static constexpr composition::StateKey<float> width{"progress-width", 0.0F};

  explicit Content(Motion motion) noexcept : motion_(motion) {}

  auto motion() const noexcept -> Motion { return motion_; }

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto spin(composition::StateHandle<bool> spinning) const
      -> composition::Element;

  auto sweep(composition::StateHandle<bool> sweeping,
             composition::StateHandle<float> measured) const
      -> composition::Element;

  Motion motion_;
};

class DefaultProgressViewStyle {
 public:
  auto make_body(ProgressViewStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const DefaultProgressViewStyle&,
                         const DefaultProgressViewStyle&) -> bool = default;
};

class LinearProgressViewStyle {
 public:
  auto make_body(ProgressViewStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const LinearProgressViewStyle&,
                         const LinearProgressViewStyle&) -> bool = default;
};

class CircularProgressViewStyle {
 public:
  auto make_body(ProgressViewStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const CircularProgressViewStyle&,
                         const CircularProgressViewStyle&) -> bool = default;
};

class SegmentedProgressViewStyle {
 public:
  explicit SegmentedProgressViewStyle(std::size_t segments) noexcept
      : segments_(segments) {}

  auto segments() const noexcept -> std::size_t { return segments_; }

  auto make_body(ProgressViewStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const SegmentedProgressViewStyle&,
                         const SegmentedProgressViewStyle&) -> bool = default;

 private:
  std::size_t segments_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::progress_view_style(Style style) && -> Element {
  return std::move(*this).environment(
      controls::ProgressViewStyle::key,
      controls::ProgressViewStyle{std::move(style)});
}

}
