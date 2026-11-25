#include <arisa/provider/diagnostics/crash_reporter.hpp>
#include <arisa/provider/horizon/console.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>
#include <frame_report.hpp>
#include <gpu_stage_screen.hpp>
#include <nxlink_log.hpp>
#include <stage_control.hpp>
#include <switch_console.hpp>
#include <ui_thread.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#endif

#if defined(__SWITCH2__)
#include <h264_annex_b.hpp>
#include <ounce_frame_decoder.hpp>
#endif

#if !defined(ARISA_HORIZON_DEKO3D)
#include <vulkan_decoded_video.hpp>
#include <vulkan_nv12_video.hpp>
#include <vulkan_plasma.hpp>
#include <vulkan_video.hpp>
#else
#include <deko3d_nv12_video.hpp>
#include <deko3d_plasma.hpp>
#include <deko3d_video.hpp>
#if defined(__SWITCH2__)
#include <deko3d_decoded_video.hpp>
#endif
#endif

#include <switch.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::nx {

class SwitchGpuStage {
 public:
#if !defined(ARISA_HORIZON_DEKO3D)
  using Plasma = examples::VulkanPlasma;
  using Video = examples::VulkanVideo;
  using Nv12Video = examples::VulkanNv12Video;
  using DecodedVideo = examples::VulkanDecodedVideo;
#else
  using Plasma = examples::Deko3dPlasma;
  using Video = examples::Deko3dVideo;
  using Nv12Video = examples::Deko3dNv12Video;
#if defined(__SWITCH2__)
  using DecodedVideo = examples::Deko3dDecodedVideo;
#endif
#endif

#if defined(__SWITCH2__)
  static constexpr std::string_view sample = "romfs:/arisa/video/stage.h264";
  static constexpr std::uint32_t sample_rate = 30;
  static constexpr std::uint32_t decoder_images = 6;
  static constexpr std::uint64_t report_every = 15;
  static constexpr DecodedVideo::Format sample_format{
      .width = 1280,
      .height = 720,
      .frame_interval = std::chrono::nanoseconds{1'000'000'000 / sample_rate},
      .colour_matrix = provider::ColourMatrix::bt709,
      .range = provider::ColourRange::limited};
#endif

  static auto run() -> int {
    const NxlinkLog log{"gpu_stage"};
    const auto crashes = provider::CrashReporter::install(
        {.directory = "sdmc:/switch/arisa/crashes", .app = "gpu_stage"});
    if (!crashes.has_value()) {
      log.emit("crash reporter unavailable");
    }
    log.emit("startup gpu_stage");
    std::string failure = render(log);
    if (failure.empty()) {
      log.emit("exit gpu_stage: ok");
      return 0;
    }
    log.emit("exit gpu_stage: " + failure);
    SwitchConsole::show(failure);
    return 1;
  }

 private:
#if defined(__SWITCH2__)
  static auto decoded_video(provider::horizon::Console::Gpu& gpu,
                            const NxlinkLog& log)
      -> std::optional<DecodedVideo> {
    auto stream = examples::H264AnnexB::load(std::string{sample});
    if (!stream.has_value()) {
      log.emit("decoder unavailable: " + stream.error());
      return std::nullopt;
    }
    const std::size_t units = stream->access_units();
    auto decoder = examples::OunceFrameDecoder::create(
        *stream, {.width = sample_format.width,
                  .height = sample_format.height,
                  .frame_rate = sample_rate,
                  .image_count = decoder_images});
    if (!decoder.has_value()) {
      log.emit("decoder unavailable: " + decoder.error());
      return std::nullopt;
    }
    auto video = DecodedVideo::create(gpu, std::move(*decoder),
                                      std::move(*stream), sample_format);
    if (!video.has_value()) {
      log.emit("decoder unavailable: " + video.error());
      return std::nullopt;
    }
    log.emit(std::format("decoder ready: {} access units, route {}", units,
                         video->route_name()));
    return std::move(*video);
  }

  static auto status(const DecodedVideo& video, std::string failure)
      -> examples::StageControl::DecoderStatus {
    const DecodedVideo::Statistics& statistics = video.statistics();
    if (failure.empty() && !video.fallback().empty()) {
      failure = "Zero-copy unavailable: " + std::string{video.fallback()};
    }
    const auto shown =
        static_cast<std::int64_t>(statistics.shown == 0 ? 1 : statistics.shown);
    return examples::StageControl::DecoderStatus{
        .route = video.route_name(),
        .decoded = statistics.decoded,
        .shown = statistics.shown,
        .dropped = statistics.dropped,
        .copy = std::chrono::duration_cast<std::chrono::microseconds>(
            statistics.copy_time / shown),
        .failure = std::move(failure)};
  }

  static void play(examples::StageControl& control, DecodedVideo& video,
                   std::chrono::nanoseconds now, const NxlinkLog& log) {
    auto advanced = video.advance(now);
    if (!advanced.has_value()) {
      log.emit("decoder failed: " + advanced.error());
      control.report(status(video, "Decoder failed: " + advanced.error()));
      control.set_video_format(examples::StageControl::VideoFormat::rgba);
      return;
    }
    if (!advanced->has_value()) {
      return;
    }
    control.show(**advanced);
    if (video.statistics().shown % report_every == 1) {
      control.report(status(video, {}));
    }
  }
#endif

  static auto render(const NxlinkLog& log) -> std::string {
    auto factory = provider::horizon::HorizonProviderFactory::create();
    if (!factory.has_value()) {
      return std::string{"HorizonProviderFactory failed: "} +
             factory.error().explanation();
    }
    const auto gpu = factory->stock_gpu<provider::horizon::Console::Gpu>();
    if (!gpu.has_value()) {
      return std::string{"the gpu stage needs the stock console gpu provider"};
    }
    examples::StageControl control;
    auto plasma = Plasma::create(gpu->get(), control);
    if (!plasma.has_value()) {
      return plasma.error();
    }
    control.bind(plasma->hook());
    control.bind_surfaces(plasma->texture_surface(),
                          plasma->underlay_surface());
    auto video = Video::create(gpu->get(), *plasma);
    if (!video.has_value()) {
      return video.error();
    }
    auto nv12 = Nv12Video::create(gpu->get(), *plasma, control);
    if (!nv12.has_value()) {
      return nv12.error();
    }
#if defined(__SWITCH2__)
    std::optional<DecodedVideo> decoded = decoded_video(gpu->get(), log);
    if (decoded.has_value()) {
      control.offer_decoder();
    }
#endif
    auto application = runtime::Application::create(
        *factory, examples::GpuStageScreen{control});
    if (!application.has_value()) {
      return std::string{"the application could not start: "} +
             application.error().explanation();
    }
#ifdef ARISA_INSPECTOR
    inspector::Inspector inspector = inspector::Inspector::attach(*application);
#endif
    FrameReport report{log, "gpu_stage"};
    PadState pad{};
    padInitializeDefault(&pad);
    if (auto ran = application->run_until([&](runtime::Root&) {
          const runtime::FrameTimings& timings = application->last_frame();
          report.record(
              FrameReport::Sample{.cpu = timings.cpu(),
                                  .layout = timings.layout,
                                  .input = timings.input,
                                  .present = timings.present,
                                  .laid_out = timings.laid_out,
                                  .presented = timings.presented,
                                  .missed_vsyncs = gpu->get().missed_vsyncs(),
                                  .paced = timings.paced,
                                  .animating = timings.animating,
                                  .culled = timings.culled,
                                  .occluded = timings.occluded,
                                  .frame_rate = timings.frame_rate,
                                  .due = timings.due,
                                  .now = factory->clock().now().since_origin(),
                                  .effects_gpu = timings.effects_gpu,
                                  .interface_gpu = timings.interface_gpu,
                                  .hooks_gpu = timings.hooks_gpu,
                                  .backdrops = timings.backdrops,
                                  .blurs = timings.blurs,
                                  .blur_hits = timings.blur_hits,
                                  .effect_passes = timings.effect_passes,
                                  .layer_renders = timings.layer_renders,
                                  .unhinted = timings.unhinted,
                                  .frame = timings,
                                  .allocations = std::nullopt,
                                  .focus = std::nullopt});
          if (control.take_change() || control.producing()) {
            const auto now = factory->clock().now().since_origin();
            switch (control.video_format()) {
              case examples::StageControl::VideoFormat::nv12:
                static_cast<void>(nv12->produce(control, now));
                break;
              case examples::StageControl::VideoFormat::decoder:
#if defined(__SWITCH2__)
                if (decoded.has_value()) {
                  play(control, *decoded, now, log);
                  break;
                }
#endif
                static_cast<void>(video->produce(control, now));
                break;
              case examples::StageControl::VideoFormat::rgba:
                static_cast<void>(video->produce(control, now));
                break;
            }
          }
          padUpdate(&pad);
          return !appletMainLoop() ||
                 (padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0;
        });
        !ran.has_value()) {
      return std::string{"frame failed: "} + ran.error().explanation();
    }
    return {};
  }
};

}

auto main() -> int {
  return arisa::nx::UiThread::run(
      [] { return arisa::nx::SwitchGpuStage::run(); });
}
