#pragma once

#include <stage_control.hpp>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>

#include <chrono>

#include <functional>
#include <string_view>

namespace arisa::examples {

class GpuStageScreen final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> playing{"stage-playing", true};
  static constexpr composition::StateKey<double> speed{
      "stage-speed", StageControl::default_speed};
  static constexpr composition::StateKey<StageControl::Mode> mode{
      "stage-mode", StageControl::Mode::plasma};
  static constexpr composition::StateKey<provider::ColourMatrix> matrix{
      "stage-matrix", StageControl::encoded_matrix};
  static constexpr composition::StateKey<provider::ColourRange> range{
      "stage-range", StageControl::encoded_range};
  static constexpr float sidebar_width = 300.0F;
  static constexpr float sidebar_radius = 26.0F;
  static constexpr std::string_view play_button = "stage.play";
  static constexpr std::string_view speed_slider = "stage.speed";
  static constexpr std::string_view mode_picker = "stage.mode";
  static constexpr std::string_view page_button = "stage.page";
  static constexpr std::string_view format_button = "stage.format";
  static constexpr std::string_view matrix_picker = "stage.matrix";
  static constexpr std::string_view range_picker = "stage.range";
  static constexpr std::string_view full_screen_button = "stage.full-screen";
  static constexpr std::string_view under_button = "stage.under";
  static constexpr std::string_view decoder_button = "stage.decoder";
  static constexpr std::string_view decoder_status_text =
      "stage.decoder-status";
  static constexpr std::string_view surface_button = "stage.surface";
  static constexpr std::string_view surface_drive_button =
      "stage.surface-drive";
  static constexpr std::string_view surface_next_button = "stage.surface-next";
  static constexpr std::string_view surface_scroll = "stage.surface-scroll";
  static constexpr float surface_width = 320.0F;
  static constexpr float surface_height = 240.0F;
  static constexpr float surface_page_height = 900.0F;
  static constexpr composition::StateKey<bool> full_screen_toast{
      "stage-full-screen-toast", false};
  static constexpr std::chrono::seconds toast_duration{2};
  static constexpr float video_width = 480.0F;
  static constexpr float video_height = 270.0F;
  static constexpr float card_radius = 22.0F;

  explicit GpuStageScreen(StageControl& control) noexcept;

  auto operator==(const GpuStageScreen& other) const -> bool {
    return &control_.get() == &other.control_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  class VideoCard;
  class Backdrop;
  class DecoderLine;
  class SurfacePage;

  static auto modes() -> composition::Element;

  static auto matrices() -> composition::Element;

  static auto ranges() -> composition::Element;

  static auto decoding(composition::Context& context, StageControl& control)
      -> composition::Element;

  static auto hud() -> composition::Element;

  static auto surface_controls(composition::Context& context,
                               StageControl& control) -> composition::Element;

  std::reference_wrapper<StageControl> control_;
};

class GpuStageScreen::VideoCard final : public composition::Component {
 public:
  explicit VideoCard(const StageControl& control) noexcept
      : control_(control) {}

  auto operator==(const VideoCard& other) const -> bool {
    return &control_.get() == &other.control_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::reference_wrapper<const StageControl> control_;
};

class GpuStageScreen::Backdrop final : public composition::Component {
 public:
  explicit Backdrop(const StageControl& control) noexcept : control_(control) {}

  auto operator==(const Backdrop& other) const -> bool {
    return &control_.get() == &other.control_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::reference_wrapper<const StageControl> control_;
};

class GpuStageScreen::SurfacePage final : public composition::Component {
 public:
  explicit SurfacePage(const StageControl& control) noexcept
      : control_(control) {}

  auto operator==(const SurfacePage& other) const -> bool {
    return &control_.get() == &other.control_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(composition::Element content, std::string_view label,
                   bool glass) -> composition::Element;

  std::reference_wrapper<const StageControl> control_;
};

class GpuStageScreen::DecoderLine final : public composition::Component {
 public:
  explicit DecoderLine(const StageControl& control) noexcept
      : control_(control) {}

  auto operator==(const DecoderLine& other) const -> bool {
    return &control_.get() == &other.control_.get();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::reference_wrapper<const StageControl> control_;
};

}
