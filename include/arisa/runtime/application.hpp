#pragma once

#include <arisa/appearance/appearance_model.hpp>
#include <arisa/appearance/theme_watcher.hpp>
#include <arisa/async/executors.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/provider/browser/url.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application_error.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/button_capture.hpp>
#include <arisa/runtime/captured_frame.hpp>
#include <arisa/runtime/embedded_root.hpp>
#include <arisa/runtime/frame_observer.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/introspection.hpp>
#include <arisa/runtime/passed_input.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/runtime/screen_reader.hpp>
#include <arisa/runtime/text_input.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::interaction {

class InputRouter;

}

namespace arisa::runtime {

class Application {
 public:
  using StopCondition = std::move_only_function<bool(Root&)>;

  static constexpr std::uint32_t background_workers = 2;
  static constexpr std::chrono::milliseconds idle_poll{16};

  [[nodiscard]] static auto create(
      provider::ProviderFactory& providers, composition::Element content,
      std::optional<async::Executors> executors = std::nullopt)
      -> std::expected<Application, ApplicationError>;

  [[nodiscard]] static auto create(provider::ProviderFactory& providers,
                                   composition::Element content,
                                   ApplicationOptions options)
      -> std::expected<Application, ApplicationError>;

  Application(Application&& other) noexcept;
  auto operator=(Application&& other) noexcept -> Application&;
  Application(const Application&) = delete;
  auto operator=(const Application&) -> Application& = delete;
  ~Application();

  [[nodiscard]] auto frame() -> std::expected<void, ApplicationError>;

  [[nodiscard]] auto run() -> std::expected<void, ApplicationError>;

  [[nodiscard]] auto run_until(StopCondition stop)
      -> std::expected<void, ApplicationError>;

  auto root() noexcept -> Root&;

  auto exclusive_frame(composition::ExclusiveFrame::Options options)
      -> composition::ExclusiveFrame;

  [[nodiscard]] auto embed(Root& overlay, provider::FrameSource frames,
                           provider::SurfaceSize extent)
      -> std::expected<EmbeddedRoot, ApplicationError>;

  auto block_input() -> InputBlock;

  auto pass_input(provider::Buttons buttons, PassedInput passed = {})
      -> InputPassthrough;

  auto capture_buttons() -> ButtonCapture;

  auto executors() const noexcept -> const async::Executors&;

  auto text_input() const noexcept -> TextInput;

  auto exit_requested() const noexcept -> bool;

  auto last_frame() const noexcept -> const FrameTimings&;

  auto waiting() const noexcept -> bool;

  auto interaction() const noexcept -> const interaction::InputRouter&;

  auto focused() const noexcept -> std::optional<composition::NodeId>;

  auto appearance() const noexcept
      -> const std::optional<appearance::AppearanceModel>&;

  auto theme_watcher() const noexcept
      -> std::optional<std::reference_wrapper<const appearance::ThemeWatcher>>;

  void wait();

  auto next_build() const noexcept -> std::optional<std::chrono::nanoseconds>;

  void request_capture() noexcept;

  auto take_capture() -> std::optional<CapturedFrame>;

  void step_animations();

  void drain();

  auto render_thread() const noexcept -> bool;

  auto replayed_frames() const noexcept -> std::uint64_t;

  auto animation_wakes() const noexcept -> std::uint64_t;

  auto introspection() const noexcept -> const Introspection&;

  auto screen_reader() const noexcept -> const ScreenReader&;

  void open_url(provider::Url url) const;

  auto observe(FrameObserver& observer) -> support::ScopedCallback;

 private:
  class Implementation;

  explicit Application(std::unique_ptr<Implementation> implementation) noexcept;

  std::unique_ptr<Implementation> implementation_;
};

}
