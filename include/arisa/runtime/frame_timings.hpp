#pragma once

#include <arisa/motion/unhinted_animation.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::runtime {

enum class FrameKind : std::uint8_t {
  idle,
  animation,
  update,
};

struct FrameTimings {
  FrameKind kind{FrameKind::update};
  std::chrono::nanoseconds reconcile{0};
  std::chrono::nanoseconds layout{0};
  std::chrono::nanoseconds input{0};
  std::chrono::nanoseconds paint{0};
  std::chrono::nanoseconds present{0};
  std::chrono::nanoseconds walk{0};
  std::chrono::nanoseconds upload{0};
  std::chrono::nanoseconds emit{0};
  std::chrono::nanoseconds stall{0};
  std::chrono::nanoseconds record{0};
  std::chrono::nanoseconds wait{0};
  bool threaded{false};
  std::chrono::nanoseconds actions{0};
  std::chrono::nanoseconds wall{0};
  bool laid_out{false};
  std::uint64_t layout_computations{0};
  std::uint64_t focus_collections{0};
  std::uint64_t focus_refreshes{0};
  std::uint64_t focus_uncovered{0};
  bool presented{false};
  bool animating{false};
  std::uint32_t quads{0};
  std::uint32_t commands{0};
  std::optional<double> frame_rate{};
  std::optional<motion::UnhintedAnimation> unhinted{};
  bool paced{false};
  bool exclusive{false};
  std::uint32_t culled{0};
  std::uint32_t visited{0};
  std::uint32_t recorded{0};
  std::uint32_t spliced{0};
  std::uint32_t occluded{0};
  std::uint32_t shadow_hits{0};
  std::uint32_t shadow_misses{0};
  std::uint32_t tessellations{0};
  std::uint32_t mesh_builds{0};
  std::uint32_t pictures{0};
  std::uint64_t picture_bytes{0};
  std::optional<std::chrono::nanoseconds> due{};
  std::uint64_t evaluations{0};
  std::size_t shaped{0};
  std::uint32_t backdrops{0};
  std::uint32_t blurs{0};
  std::uint32_t blur_hits{0};
  std::uint32_t shared_backdrops{0};
  std::uint32_t effect_passes{0};
  std::uint32_t layer_renders{0};
  std::uint32_t layer_hits{0};
  std::uint32_t targets_created{0};
  std::uint32_t targets_reused{0};
  std::uint32_t targets_released{0};
  std::uint32_t targets_evicted{0};
  std::uint32_t targets_refused{0};
  std::uint32_t uploaded_vertices{0};
  std::uint64_t uploaded_bytes{0};
  std::uint64_t damaged_pixels{0};
  std::optional<std::chrono::nanoseconds> effects_gpu{};
  std::optional<std::chrono::nanoseconds> interface_gpu{};
  std::optional<std::chrono::nanoseconds> hooks_gpu{};
  bool gpu_measured{false};
  std::uint64_t nodes_created{0};
  std::uint64_t shaping_runs{0};
  std::uint64_t text_layouts{0};
  std::uint64_t rasterised{0};
  std::uint64_t atlas_bytes{0};
  std::uint64_t image_reads{0};
  std::uint64_t image_decodes{0};
  std::uint64_t textures_created{0};
  std::uint64_t memory_blocks_created{0};
  std::uint64_t debug_evaluations{0};
  bool debug{false};
  std::uint64_t missed_vsyncs{0};
  std::uint64_t encode_misses{0};
  std::uint64_t encode_missed_texture{0};
  bool encode_missed_target{false};
  std::uint32_t swap_interval{1};
  std::chrono::nanoseconds sampled{0};
  std::optional<std::chrono::nanoseconds> displayed{};
  bool leased{false};
  bool taken_over{false};

  constexpr auto cpu() const noexcept -> std::chrono::nanoseconds {
    return reconcile + layout + input + paint;
  }

  constexpr auto unaccounted() const noexcept -> std::chrono::nanoseconds {
    return wall - reconcile - layout - input - walk - upload - emit - present -
           actions;
  }

  constexpr auto blocked() const noexcept -> std::chrono::nanoseconds {
    return threaded ? stall : stall + wait;
  }

  constexpr auto present_cpu() const noexcept -> std::chrono::nanoseconds {
    return std::max(present - blocked(), std::chrono::nanoseconds{0});
  }

  friend auto operator==(const FrameTimings&, const FrameTimings&)
      -> bool = default;
};

}
