#pragma once

#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <utility>

namespace arisa::runtime {

class InputGate;

class ButtonCapture {
 public:
  class Held {
   public:
    void store(const std::array<provider::PadState, provider::max_pads>& pads);

    auto read() const -> std::array<provider::PadState, provider::max_pads>;

   private:
    mutable std::mutex mutex_;
    std::array<provider::PadState, provider::max_pads> pads_{};
  };

  auto active() const noexcept -> bool { return release_.valid(); }

  auto held() const -> provider::Buttons;

  auto held(std::size_t pad) const -> provider::Buttons;

  auto connected(std::size_t pad) const -> bool;

  auto pad(std::size_t pad) const -> provider::PadState;

  void release() noexcept;

 private:
  friend class InputGate;

  ButtonCapture(std::shared_ptr<Held> held,
                support::ScopedCallback release) noexcept
      : held_(std::move(held)), release_(std::move(release)) {}

  std::shared_ptr<Held> held_;
  support::ScopedCallback release_;
};

}
