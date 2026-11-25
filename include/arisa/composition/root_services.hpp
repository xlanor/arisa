#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/executors.hpp>
#include <arisa/async/frame_scheduler.hpp>
#include <arisa/provider/diagnostics/provider.hpp>

#include <functional>
#include <optional>

namespace arisa::composition {

struct RootServices {
  std::reference_wrapper<async::Executor> root;
  std::reference_wrapper<async::FrameScheduler> frames;
  async::Executors executors;
  std::optional<std::reference_wrapper<provider::DiagnosticsProvider>>
      diagnostics{};
};

}
