#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class ContainerRelativeFrame {
 public:
  using Length = std::function<float(float length, Axis axis)>;

  ContainerRelativeFrame(Axes axes, Alignment alignment) noexcept
      : axes_(axes), alignment_(alignment) {}

  ContainerRelativeFrame(Axes axes, std::uint32_t count, std::uint32_t span,
                         theme::Space spacing, Alignment alignment) noexcept
      : axes_(axes),
        alignment_(alignment),
        count_(std::max(count, 1U)),
        span_(std::clamp(span, 1U, std::max(count, 1U))),
        spacing_(spacing) {}

  ContainerRelativeFrame(Axes axes, Alignment alignment, Length length)
      : axes_(axes),
        alignment_(alignment),
        length_(std::make_shared<const Length>(std::move(length))) {}

  auto axes() const noexcept -> Axes { return axes_; }

  auto alignment() const noexcept -> Alignment { return alignment_; }

  auto spacing() const noexcept -> const theme::Space& { return spacing_; }

  auto length(float container, float spacing, Axis axis) const -> float {
    if (length_ != nullptr && *length_) {
      return std::max((*length_)(container, axis), 0.0F);
    }
    const auto count = static_cast<float>(count_);
    const auto span = static_cast<float>(span_);
    const float column =
        (container - (spacing * (count - 1.0F))) / std::max(count, 1.0F);
    return std::max((column * span) + (spacing * (span - 1.0F)), 0.0F);
  }

  friend auto operator==(const ContainerRelativeFrame& left,
                         const ContainerRelativeFrame& right) -> bool {
    return left.axes_ == right.axes_ && left.alignment_ == right.alignment_ &&
           left.count_ == right.count_ && left.span_ == right.span_ &&
           left.spacing_ == right.spacing_ && left.length_ == right.length_;
  }

 private:
  Axes axes_;
  Alignment alignment_;
  std::uint32_t count_{1};
  std::uint32_t span_{1};
  theme::Space spacing_{0.0F};
  std::shared_ptr<const Length> length_;
};

}
