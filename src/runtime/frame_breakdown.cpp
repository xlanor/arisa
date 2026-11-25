#include <arisa/runtime/frame_breakdown.hpp>

#include <format>

namespace arisa::runtime {

auto FrameBreakdown::milliseconds(std::chrono::nanoseconds value) noexcept
    -> double {
  return std::chrono::duration<double, std::milli>(value).count();
}

auto FrameBreakdown::describe(const FrameTimings& frame,
                              std::optional<std::uint64_t> allocations)
    -> std::string {
  return std::format(
      "wall {:.3f} ms = evaluate {:.3f} + layout {:.3f} + input {:.3f} + "
      "walk {:.3f} + upload {:.3f} + emit {:.3f} + present {:.3f} (cpu {:.3f} "
      "blocked {:.3f}) + actions {:.3f} + unaccounted {:.3f} | {} record "
      "{:.3f} wait {:.3f} | nodes +{} evaluations {} layout "
      "computations {} shaping runs {} text layouts {} glyphs {} atlas {} B "
      "image reads {} decodes {} textures +{} gpu blocks +{} allocations {}",
      milliseconds(frame.wall), milliseconds(frame.reconcile),
      milliseconds(frame.layout), milliseconds(frame.input),
      milliseconds(frame.walk), milliseconds(frame.upload),
      milliseconds(frame.emit), milliseconds(frame.present),
      milliseconds(frame.present_cpu()), milliseconds(frame.blocked()),
      milliseconds(frame.actions), milliseconds(frame.unaccounted()),
      frame.threaded ? "render thread" : "inline", milliseconds(frame.record),
      milliseconds(frame.wait), frame.nodes_created, frame.evaluations,
      frame.layout_computations, frame.shaping_runs, frame.text_layouts,
      frame.rasterised, frame.atlas_bytes, frame.image_reads,
      frame.image_decodes, frame.textures_created, frame.memory_blocks_created,
      allocations.has_value() ? std::format("{}", *allocations)
                              : std::string{"-"});
}

}
