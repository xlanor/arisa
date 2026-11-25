#pragma once

#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace arisa::runtime {

class EmbeddedRoot {
 public:
  EmbeddedRoot() = default;

  auto attached() const noexcept -> bool {
    return detach_.valid() && !live_.expired();
  }

  auto resize(provider::SurfaceSize extent) -> bool {
    return attached() && resize_ && resize_(extent);
  }

  void release() noexcept {
    live_.reset();
    resize_ = nullptr;
    detach_.reset();
  }

 private:
  friend class Application;

  using Resize = std::move_only_function<bool(provider::SurfaceSize)>;

  EmbeddedRoot(support::ScopedCallback detach, std::weak_ptr<const void> live,
               Resize resize) noexcept
      : detach_(std::move(detach)),
        live_(std::move(live)),
        resize_(std::move(resize)) {}

  support::ScopedCallback detach_;
  std::weak_ptr<const void> live_;
  Resize resize_;
};

}
