#pragma once

#include <arisa/composition/observable.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/gpu/texture.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class StageControl {
 public:
  enum class Mode : std::uint8_t {
    plasma,
    ripples,
    tunnel,
  };

  enum class Page : std::uint8_t {
    stage,
    video,
    surface,
  };

  enum class VideoFormat : std::uint8_t {
    rgba,
    nv12,
    decoder,
  };

  enum class Plane : std::uint8_t {
    rgb,
    luma,
    chroma,
  };

  struct DecoderStatus {
    std::string_view route;
    std::uint64_t decoded{0};
    std::uint64_t shown{0};
    std::uint64_t dropped{0};
    std::chrono::microseconds copy{0};
    std::string failure;

    friend auto operator==(const DecoderStatus&, const DecoderStatus&)
        -> bool = default;
  };

  struct Uniforms {
    float time{0.0F};
    float mode{0.0F};
    float aspect{1.0F};
    float speed{1.0F};
    float plane{0.0F};
    float matrix{0.0F};
    float range{0.0F};
    float luma_width{1.0F};

    auto encoding(Plane target, std::uint32_t width) const noexcept -> Uniforms;
  };

  static constexpr double default_speed = 1.0;
  static constexpr double fastest = 3.0;
  static constexpr std::chrono::milliseconds longest_step{100};
  static constexpr provider::ColourMatrix encoded_matrix =
      provider::ColourMatrix::bt709;
  static constexpr provider::ColourRange encoded_range =
      provider::ColourRange::limited;
  static constexpr provider::FrameHookOptions playing_options{
      .drive = provider::FrameDrive::continuous, .frame_rate = std::nullopt};

  void bind(const provider::FrameHook& hook) noexcept;

  void bind_surfaces(const provider::GpuSurface& texture,
                     const provider::GpuSurface& underlay) noexcept;

  auto texture_surface() const noexcept
      -> std::optional<std::reference_wrapper<const provider::GpuSurface>> {
    return texture_surface_;
  }

  auto underlay_surface() const noexcept
      -> std::optional<std::reference_wrapper<const provider::GpuSurface>> {
    return underlay_surface_;
  }

  void set_surface_drive(provider::FrameDrive drive);

  auto shown_surface_drive() const noexcept
      -> const composition::Observable<provider::FrameDrive>& {
    return shown_surface_drive_;
  }

  void request_surfaces() const;

  auto advance(const provider::SurfaceFrame& frame) -> Uniforms;

  void play(bool playing);

  void set_speed(double speed);

  void set_mode(Mode mode);

  void set_page(Page page);

  void set_video_format(VideoFormat format);

  void set_matrix(provider::ColourMatrix matrix);

  void set_range(provider::ColourRange range);

  void set_full_screen(bool full_screen);

  void set_under(bool under);

  void offer_decoder() noexcept;

  auto decoder_offered() const noexcept -> bool;

  void report(DecoderStatus status);

  auto decoder_status() const noexcept
      -> const composition::Observable<DecoderStatus>& {
    return decoder_status_;
  }

  auto shown_full_screen() const noexcept
      -> const composition::Observable<bool>& {
    return shown_full_screen_;
  }

  auto shown_under() const noexcept -> const composition::Observable<bool>& {
    return shown_under_;
  }

  auto video_format() const noexcept -> VideoFormat;

  auto shown_format() const noexcept
      -> const composition::Observable<VideoFormat>& {
    return shown_format_;
  }

  auto declared(std::uint32_t width, std::uint32_t height) const noexcept
      -> provider::ImportDescription;

  auto take_change() noexcept -> bool;

  auto playing() const noexcept -> bool;

  auto page() const noexcept -> Page;

  auto shown_page() const noexcept -> const composition::Observable<Page>& {
    return shown_page_;
  }

  auto frame() const noexcept
      -> const composition::Observable<provider::TextureId>& {
    return frame_;
  }

  void show(provider::TextureId texture);

  auto producing() const noexcept -> bool;

  auto advance(const provider::FrameInfo& info) -> Uniforms;

 private:
  void refresh() const;

  void drive() const;

  std::atomic<bool> playing_{true};
  std::atomic<double> speed_{default_speed};
  std::atomic<Mode> mode_{Mode::plasma};
  std::atomic<Page> page_{Page::stage};
  std::atomic<VideoFormat> video_format_{VideoFormat::rgba};
  std::atomic<provider::ColourMatrix> matrix_{encoded_matrix};
  std::atomic<provider::ColourRange> range_{encoded_range};
  std::atomic<bool> changed_{false};
  std::atomic<bool> decoder_offered_{false};
  composition::Observable<VideoFormat> shown_format_{VideoFormat::rgba};
  composition::Observable<Page> shown_page_{Page::stage};
  composition::Observable<bool> shown_full_screen_{false};
  composition::Observable<bool> shown_under_{false};
  composition::Observable<provider::TextureId> frame_{provider::TextureId{0}};
  composition::Observable<DecoderStatus> decoder_status_{};
  composition::Observable<provider::FrameDrive> shown_surface_drive_{
      provider::FrameDrive::continuous};
  std::atomic<provider::FrameDrive> surface_drive_{
      provider::FrameDrive::continuous};
  std::optional<std::reference_wrapper<const provider::FrameHook>> hook_;
  std::optional<std::reference_wrapper<const provider::GpuSurface>>
      texture_surface_;
  std::optional<std::reference_wrapper<const provider::GpuSurface>>
      underlay_surface_;
  std::mutex phase_mutex_;
  std::optional<std::chrono::nanoseconds> last_;
  double phase_{0.0};
};

}
