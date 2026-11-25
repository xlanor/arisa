#include <ui_thread.hpp>

#include <arisa/provider/standard/std_threading.hpp>

#include <optional>
#include <utility>

namespace arisa::nx {

auto UiThread::run(std::function<int()> body) -> int {
  int result = 1;
  provider::standard::StdThreadingProvider threads;
  std::optional<provider::Thread> thread;
  if (auto started =
          threads.start(provider::ThreadSpec{.name = "arisa.app",
                                             .role = provider::ThreadRole::ui},
                        [&body, &result](std::stop_token) { result = body(); });
      started.has_value()) {
    thread.emplace(std::move(*started));
  } else {
    return body();
  }
  thread.reset();
  return result;
}

}
