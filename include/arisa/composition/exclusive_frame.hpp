#pragma once

#include <arisa/composition/overlay_layer.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <span>
#include <utility>
#include <vector>

namespace arisa::composition {

class Tree;

class ExclusiveFrame {
 public:
  struct Options {
    std::vector<OverlayLayer> keep;
    bool lease{false};

    friend auto operator==(const Options&, const Options&) -> bool = default;
  };

  auto active() const noexcept -> bool { return release_.valid(); }

  auto keep() const noexcept -> std::span<const OverlayLayer> {
    return options_.keep;
  }

  void release() noexcept { release_.reset(); }

 private:
  friend class Tree;

  ExclusiveFrame(Options options, support::ScopedCallback release) noexcept
      : options_(std::move(options)), release_(std::move(release)) {}

  Options options_;
  support::ScopedCallback release_;
};

}
