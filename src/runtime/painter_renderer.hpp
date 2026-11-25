#pragma once

#include <arisa/layout/layout_context.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/gpu/frame_packet.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/runtime/application_error.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/text/font_collection.hpp>
#include <arisa/text/font_role.hpp>
#include <arisa/text/typesetter.hpp>

#include <arisa/theme/colour.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>

#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

class PainterRenderer {
 public:
  static constexpr char32_t controller_probe = U'\uE0E0';
  static constexpr double zoom_duration = 0.35;
  static constexpr std::chrono::nanoseconds display_period{16'666'667};

  [[nodiscard]] static auto create(provider::ProviderFactory& providers,
                                   const ApplicationOptions& options)
      -> std::expected<PainterRenderer, ApplicationError>;

  [[nodiscard]] static auto embedded(provider::ProviderFactory& providers,
                                     const ApplicationOptions& options,
                                     provider::FrameSource frames,
                                     provider::SurfaceSize extent)
      -> std::expected<PainterRenderer, ApplicationError>;

  static constexpr std::size_t geometry_rounds = 2;

  auto frames() const noexcept -> const std::optional<provider::FrameSource>& {
    return frames_;
  }

  auto take_frames() noexcept -> std::optional<provider::FrameSource> {
    return std::exchange(frames_, std::nullopt);
  }

  void set_extent(provider::SurfaceSize extent) noexcept { extent_ = extent; }

  struct Preparation {
    bool changed{false};
    bool resized{false};
    bool laid_out{false};
    std::chrono::nanoseconds layout{0};
    std::uint64_t computations{0};
  };

  [[nodiscard]] auto prepare(Root& root)
      -> std::expected<Preparation, ApplicationError>;

  struct Painted {
    FrameTimings timings;
    std::optional<provider::FramePacket> packet;
  };

  struct Request {
    bool exclusive{false};
    bool capture{false};
    std::optional<std::chrono::nanoseconds> predicted_present{};
    provider::FrameSchedule::Demand demand{};
    bool driven{false};
  };

  [[nodiscard]] auto render(
      Root& root, rendering::Painter::Highlights highlights,
      std::optional<std::reference_wrapper<rendering::ReplayScene>> replay,
      Request request) -> std::expected<Painted, ApplicationError>;

  void measure(FrameTimings& timings);

  void repaint_whole() noexcept { painter_.repaint_whole(); }

  auto scale() const noexcept -> provider::SurfaceScale {
    return viewport_.scale;
  }

  void set_ui_scale(float scale, std::chrono::nanoseconds now,
                    bool reduce_motion);

  auto ui_scale() const noexcept -> float { return ui_scale_; }

  auto display_mode() const noexcept -> provider::DisplayMode;

  auto zooming(std::chrono::nanoseconds now) const noexcept -> bool;

  auto controller_glyphs() const noexcept -> bool;

  auto typesetter() noexcept
      -> std::optional<std::reference_wrapper<text::Typesetter>>;

  auto shaped() const noexcept -> std::size_t;

  struct Counters {
    std::uint64_t shaping_runs{0};
    std::uint64_t text_layouts{0};
    std::uint64_t rasterised{0};
    std::uint64_t atlas_bytes{0};
    std::uint64_t textures_created{0};
    std::uint64_t memory_blocks_created{0};
  };

  auto counters() const noexcept -> Counters;

  auto painter() const noexcept -> const rendering::Painter& {
    return painter_;
  }

 private:
  PainterRenderer(provider::ProviderFactory& providers,
                  std::unique_ptr<text::FontCollection> fonts,
                  std::optional<text::Typesetter> typesetter,
                  layout::LayoutContext layout,
                  rendering::Painter&& painter) noexcept;

  static auto assemble(provider::ProviderFactory& providers,
                       const ApplicationOptions& options,
                       layout::LayoutContext layout,
                       rendering::Painter&& painter)
      -> std::expected<PainterRenderer, ApplicationError>;

  std::reference_wrapper<provider::SurfaceProvider> surface_;
  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::reference_wrapper<provider::ClockProvider> clock_;
  std::unique_ptr<text::FontCollection> fonts_;
  std::optional<text::Typesetter> typesetter_;
  layout::LayoutContext layout_;
  rendering::Painter painter_;
  auto zoom(std::chrono::nanoseconds now) const noexcept -> float;

  std::optional<provider::FrameSource> frames_;
  std::optional<provider::SurfaceSize> extent_;
  layout::Viewport viewport_{};
  std::optional<layout::Viewport> shown_;
  float ui_scale_{1.0F};
  bool scaled_{false};
  std::optional<theme::Colour> launch_;
  std::optional<motion::AnimationTrack> zoom_;
  std::uint64_t missed_{0};
  std::uint64_t encode_missed_{0};
};

}
