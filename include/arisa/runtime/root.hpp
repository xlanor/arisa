#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/executors.hpp>
#include <arisa/async/frame_scheduler.hpp>
#include <arisa/async/frame_timeline.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/button_capture.hpp>
#include <arisa/runtime/dispatcher.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/overlays.hpp>
#include <arisa/runtime/passed_input.hpp>
#include <arisa/runtime/renderer.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::runtime {

class InputGate;

class Root {
 public:
  static constexpr std::size_t geometry_loop_frames = 8;

  Root(composition::Element content, provider::ProviderFactory& providers,
       std::optional<async::Executors> executors = std::nullopt);

  Root(const Root&) = delete;
  auto operator=(const Root&) -> Root& = delete;
  Root(Root&&) = delete;
  auto operator=(Root&&) -> Root& = delete;
  ~Root();

  auto dispatcher() const -> Dispatcher;

  auto executor() noexcept -> async::Executor& { return dispatcher_; }

  auto scheduler() noexcept -> async::FrameScheduler& { return timeline_; }

  auto executors() const noexcept -> const async::Executors& {
    return executors_;
  }

  void frame();

  void frame(Renderer& renderer);

  void settle();

  void settle_geometry();

  void wait(std::chrono::nanoseconds limit);

  auto tree() noexcept -> composition::Tree& { return tree_; }

  auto tree() const noexcept -> const composition::Tree& { return tree_; }

  auto activate(const composition::ElementKey& key) -> bool;

  auto activate(composition::NodeId node) -> bool;

  auto reset_boundary(const composition::ElementKey& key) -> bool;

  auto exclusive_frame(composition::ExclusiveFrame::Options options)
      -> composition::ExclusiveFrame;

  auto block_input() -> InputBlock;

  auto pass_input(provider::Buttons buttons, PassedInput passed = {})
      -> InputPassthrough;

  auto capture_buttons() -> ButtonCapture;

  auto input_blocked() const -> bool;

  auto overlays() noexcept -> Overlays& { return overlays_; }

  auto input_gate() noexcept -> InputGate& { return *gate_; }

  auto owner() const noexcept -> provider::ThreadId { return owner_; }

  auto on_owner_thread() const noexcept -> bool;

  auto frames() const noexcept -> std::uint64_t { return frames_; }

 private:
  void report_duplicate_keys();

  void report_geometry_loops();

  void report_failures();

  std::reference_wrapper<provider::DiagnosticsProvider> diagnostics_;
  std::reference_wrapper<provider::ThreadingProvider> threading_;
  provider::ThreadId owner_;
  std::shared_ptr<DispatchQueue> queue_;
  Dispatcher dispatcher_;
  async::FrameTimeline timeline_;
  async::Executors executors_;
  std::unique_ptr<InputGate> gate_;
  composition::Tree tree_;
  Overlays overlays_;
  std::uint64_t frames_{0};
};

}
