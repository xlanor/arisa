#pragma once

#include <arisa/composition/tree.hpp>
#include <arisa/layout/layout_error.hpp>
#include <arisa/layout/viewport.hpp>
#include <arisa/text/typesetter.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>

namespace arisa::bridge::yoga {
class YogaLayout;
}

namespace arisa::layout {

class LayoutContext {
 public:
  using TypesetterRef = std::optional<std::reference_wrapper<text::Typesetter>>;

  [[nodiscard]] static auto create()
      -> std::expected<LayoutContext, LayoutError>;

  LayoutContext(LayoutContext&& other) noexcept;
  auto operator=(LayoutContext&& other) noexcept -> LayoutContext&;
  LayoutContext(const LayoutContext&) = delete;
  auto operator=(const LayoutContext&) -> LayoutContext& = delete;
  ~LayoutContext();

  [[nodiscard]] auto update(composition::Tree& tree, Viewport viewport,
                            TypesetterRef typesetter)
      -> std::expected<bool, LayoutError>;

  void invalidate() noexcept { stale_ = true; }

  auto passes() const noexcept -> std::uint64_t;

  auto measurements() const noexcept -> std::uint64_t;

  auto visits() const noexcept -> std::uint64_t;

  auto placements() const noexcept -> std::uint64_t;

  auto computations() const noexcept -> std::uint64_t;

 private:
  explicit LayoutContext(
      std::unique_ptr<bridge::yoga::YogaLayout> engine) noexcept;

  std::unique_ptr<bridge::yoga::YogaLayout> engine_;
  std::optional<std::uint64_t> revision_;
  std::optional<Viewport> viewport_;
  const text::Typesetter* typesetter_{nullptr};
  bool stale_{true};
};

}
