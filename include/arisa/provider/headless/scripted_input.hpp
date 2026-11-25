#pragma once

#include <arisa/provider/input/provider.hpp>

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

namespace arisa::provider::headless {

class ScriptedInput final : public InputProvider {
 public:
  auto sample() -> InputFrame override;

  auto tuning() const noexcept -> InputTuning override;

  void set_tuning(InputTuning tuning) noexcept override;

  auto pointer_host() const noexcept -> PointerHost override;

  void present_pointer(PointerAppearance appearance) override;

  void push(InputFrame frame);

  void set_pointer_host(PointerHost host) noexcept;

  auto presented_pointer() const -> std::optional<PointerAppearance>;

  auto pointer_presentations() const -> std::size_t;

  auto pending() const -> std::size_t;

 private:
  mutable std::mutex mutex_;
  InputTuning tuning_;
  std::deque<InputFrame> frames_;
  PointerHost host_{PointerHost::system};
  std::optional<PointerAppearance> presented_;
  std::size_t presentations_{0};
};

}
