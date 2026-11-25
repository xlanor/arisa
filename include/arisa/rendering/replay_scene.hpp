#pragma once

#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/vertex.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace arisa::rendering {

struct ReplayBinding {
  std::uint32_t parent{0};
  std::array<float, 4> linear{1.0F, 0.0F, 0.0F, 1.0F};
  float scale{1.0F};
  std::optional<motion::AnimationTrack> offset{};
  motion::AnimatableVector painted_offset{};
  std::optional<motion::AnimationTrack> opacity{};
  double painted_opacity{1.0};
  std::optional<motion::AnimationTrack> trim_start{};
  std::optional<motion::AnimationTrack> trim_end{};
  double painted_trim_start{0.0};
  double painted_trim_end{1.0};
  float trim_length{0.0F};
  bool trims{false};
};

struct ReplayScene {
  bool replayable{false};
  std::vector<provider::DrawCommand> commands;
  std::vector<std::uint32_t> owners;
  std::vector<provider::Index> indices;
  std::vector<ReplayBinding> bindings;
  std::uint32_t vertex_count{0};
  std::uint64_t generation{0};
  provider::Colour clear{};
  std::chrono::nanoseconds painted{0};
  std::optional<double> frame_rate{};
  bool driven{false};
};

}
