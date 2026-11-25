#include <stage_control.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <utility>

namespace arisa::examples {

void StageControl::bind(const provider::FrameHook& hook) noexcept {
  hook_ = std::cref(hook);
  drive();
}

void StageControl::bind_surfaces(
    const provider::GpuSurface& texture,
    const provider::GpuSurface& underlay) noexcept {
  texture_surface_ = std::cref(texture);
  underlay_surface_ = std::cref(underlay);
  drive();
}

void StageControl::set_surface_drive(provider::FrameDrive drive_wanted) {
  surface_drive_.store(drive_wanted, std::memory_order_relaxed);
  shown_surface_drive_.set(drive_wanted);
  drive();
  request_surfaces();
}

void StageControl::request_surfaces() const {
  for (const auto& surface : {texture_surface_, underlay_surface_}) {
    if (surface.has_value()) {
      surface->get().request_frame();
    }
  }
}

auto StageControl::advance(const provider::SurfaceFrame& frame) -> Uniforms {
  provider::FrameInfo info = frame.frame;
  info.extent = provider::SurfaceSize{
      .width = provider::SurfaceWidth{static_cast<std::uint32_t>(
          std::max(frame.viewport.width, 1.0F))},
      .height = provider::SurfaceHeight{
          static_cast<std::uint32_t>(std::max(frame.viewport.height, 1.0F))}};
  return advance(info);
}

void StageControl::play(bool playing) {
  playing_.store(playing, std::memory_order_relaxed);
  drive();
  refresh();
}

void StageControl::set_speed(double speed) {
  speed_.store(std::clamp(speed, 0.0, fastest), std::memory_order_relaxed);
  refresh();
}

void StageControl::set_mode(Mode mode) {
  mode_.store(mode, std::memory_order_relaxed);
  refresh();
}

void StageControl::set_page(Page page) {
  page_.store(page, std::memory_order_relaxed);
  shown_page_.set(page);
  drive();
  refresh();
}

void StageControl::set_video_format(VideoFormat format) {
  video_format_.store(format, std::memory_order_relaxed);
  shown_format_.set(format);
  changed_.store(true, std::memory_order_release);
  refresh();
}

void StageControl::set_full_screen(bool full_screen) {
  shown_full_screen_.set(full_screen);
  refresh();
}

void StageControl::set_under(bool under) {
  shown_under_.set(under);
  refresh();
}

void StageControl::offer_decoder() noexcept {
  decoder_offered_.store(true, std::memory_order_relaxed);
}

auto StageControl::decoder_offered() const noexcept -> bool {
  return decoder_offered_.load(std::memory_order_relaxed);
}

void StageControl::report(DecoderStatus status) {
  decoder_status_.set(std::move(status));
}

void StageControl::set_matrix(provider::ColourMatrix matrix) {
  matrix_.store(matrix, std::memory_order_relaxed);
  changed_.store(true, std::memory_order_release);
  refresh();
}

void StageControl::set_range(provider::ColourRange range) {
  range_.store(range, std::memory_order_relaxed);
  changed_.store(true, std::memory_order_release);
  refresh();
}

auto StageControl::video_format() const noexcept -> VideoFormat {
  return video_format_.load(std::memory_order_relaxed);
}

auto StageControl::declared(std::uint32_t width,
                            std::uint32_t height) const noexcept
    -> provider::ImportDescription {
  return provider::ImportDescription{
      .width = width,
      .height = height,
      .format = provider::TextureFormat::nv12,
      .filter = provider::SamplerFilter::linear,
      .premultiplied = false,
      .colour_matrix = matrix_.load(std::memory_order_relaxed),
      .range = range_.load(std::memory_order_relaxed)};
}

auto StageControl::take_change() noexcept -> bool {
  return changed_.exchange(false, std::memory_order_acq_rel) &&
         page() == Page::video;
}

auto StageControl::Uniforms::encoding(Plane target,
                                      std::uint32_t width) const noexcept
    -> Uniforms {
  Uniforms encoded = *this;
  encoded.plane = static_cast<float>(std::to_underlying(target));
  encoded.matrix = static_cast<float>(std::to_underlying(encoded_matrix));
  encoded.range = static_cast<float>(std::to_underlying(encoded_range));
  encoded.luma_width = static_cast<float>(width);
  return encoded;
}

auto StageControl::playing() const noexcept -> bool {
  return playing_.load(std::memory_order_relaxed);
}

auto StageControl::page() const noexcept -> Page {
  return page_.load(std::memory_order_relaxed);
}

void StageControl::show(provider::TextureId texture) { frame_.set(texture); }

auto StageControl::producing() const noexcept -> bool {
  return playing() && page() == Page::video;
}

auto StageControl::advance(const provider::FrameInfo& info) -> Uniforms {
  const std::scoped_lock lock{phase_mutex_};
  const double speed = speed_.load(std::memory_order_relaxed);
  if (last_.has_value() && playing()) {
    const std::chrono::nanoseconds step =
        std::clamp(info.predicted_present - *last_, std::chrono::nanoseconds{0},
                   std::chrono::nanoseconds{longest_step});
    phase_ += std::chrono::duration<double>(step).count() * speed;
  }
  last_ = info.predicted_present;
  const float height =
      std::max(static_cast<float>(info.extent.height.value()), 1.0F);
  return Uniforms{
      .time = static_cast<float>(phase_),
      .mode = static_cast<float>(
          std::to_underlying(mode_.load(std::memory_order_relaxed))),
      .aspect = static_cast<float>(info.extent.width.value()) / height,
      .speed = static_cast<float>(speed)};
}

void StageControl::refresh() const {
  if (hook_.has_value()) {
    hook_->get().request_frame();
  }
  request_surfaces();
}

void StageControl::drive() const {
  if (hook_.has_value()) {
    hook_->get().set_drive(playing() && page() == Page::stage
                               ? provider::FrameDrive::continuous
                               : provider::FrameDrive::on_demand);
  }
  const provider::FrameDrive surfaces =
      playing() && page() == Page::surface &&
              surface_drive_.load(std::memory_order_relaxed) ==
                  provider::FrameDrive::continuous
          ? provider::FrameDrive::continuous
          : provider::FrameDrive::on_demand;
  for (const auto& surface : {texture_surface_, underlay_surface_}) {
    if (surface.has_value()) {
      surface->get().set_drive(surfaces);
    }
  }
}

}
