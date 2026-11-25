#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_library.hpp>
#include <arisa/async/executor.hpp>
#include <arisa/async/frame_scheduler.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace arisa::appearance {

class ThemeWatcher {
 public:
  using Post = std::function<void(std::move_only_function<void()>)>;

  static constexpr std::chrono::milliseconds default_interval{250};

  ThemeWatcher(AppearanceModel model, async::FrameScheduler& scheduler,
               async::Executor& background, Post post,
               std::chrono::milliseconds interval = default_interval);

  auto interval() const noexcept -> std::chrono::milliseconds;

  auto polls() const noexcept -> std::uint64_t;

  auto reloads() const noexcept -> std::uint64_t;

  auto failures() const noexcept -> std::uint64_t;

  auto watching() const -> const std::vector<ThemeLibrary::Location>&;

 private:
  class State;

  std::shared_ptr<State> state_;
};

}
