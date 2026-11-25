#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/alignment_key.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/container_relative_frame.hpp>
#include <arisa/composition/container_value_key.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/layout_value_key.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/view_dimensions.hpp>
#include <arisa/theme/space.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::attr {

class Padding {
 public:
  constexpr Padding() noexcept : length_(theme::space::lg) {}

  constexpr explicit Padding(theme::Space length) noexcept : length_(length) {}

  constexpr explicit Padding(theme::Insets insets) noexcept
      : length_(theme::space::none), insets_(insets) {}

  constexpr Padding(theme::Edge edges, theme::Space length) noexcept
      : edges_(edges), length_(length) {}

  auto apply(composition::Element element) const -> composition::Element {
    if (insets_.has_value()) {
      return std::move(element).padding(*insets_);
    }
    return std::move(element).padding(edges_, length_);
  }

 private:
  theme::Edge edges_{theme::Edge::all};
  theme::Space length_;
  std::optional<theme::Insets> insets_;
};

class Margin {
 public:
  constexpr explicit Margin(theme::Space length) noexcept
      : insets_(theme::Insets::all(length)) {}

  constexpr explicit Margin(theme::Insets insets) noexcept : insets_(insets) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).margin(insets_);
  }

 private:
  theme::Insets insets_;
};

class Frame {
 public:
  constexpr explicit Frame(composition::FrameSpec spec) noexcept
      : spec_(std::move(spec)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).frame(spec_);
  }

 private:
  composition::FrameSpec spec_;
};

class FlexGrow {
 public:
  constexpr explicit FlexGrow(float factor) noexcept : factor_(factor) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).flex_grow(factor_);
  }

 private:
  float factor_;
};

class FlexShrink {
 public:
  constexpr explicit FlexShrink(float factor) noexcept : factor_(factor) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).flex_shrink(factor_);
  }

 private:
  float factor_;
};

class AspectRatio {
 public:
  constexpr explicit AspectRatio(
      std::optional<float> ratio = std::nullopt,
      composition::ContentMode mode = composition::ContentMode::fit) noexcept
      : ratio_(ratio), mode_(mode) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).aspect_ratio(ratio_, mode_);
  }

 private:
  std::optional<float> ratio_;
  composition::ContentMode mode_;
};

inline constexpr AspectRatio scaled_to_fit{std::nullopt,
                                           composition::ContentMode::fit};
inline constexpr AspectRatio scaled_to_fill{std::nullopt,
                                            composition::ContentMode::fill};

class FixedSize {
 public:
  constexpr FixedSize() noexcept = default;

  constexpr FixedSize(bool horizontal, bool vertical) noexcept
      : horizontal_(horizontal), vertical_(vertical) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).fixed_size(horizontal_, vertical_);
  }

 private:
  bool horizontal_{true};
  bool vertical_{true};
};

class LayoutPriority {
 public:
  constexpr explicit LayoutPriority(double value) noexcept : value_(value) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).layout_priority(value_);
  }

 private:
  double value_;
};

class Position {
 public:
  constexpr Position(float x, float y) noexcept : x_(x), y_(y) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).position(x_, y_);
  }

 private:
  float x_;
  float y_;
};

class ZIndex {
 public:
  constexpr explicit ZIndex(double value) noexcept : value_(value) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).z_index(value_);
  }

 private:
  double value_;
};

class RelativeFrame {
 public:
  explicit RelativeFrame(
      composition::Axes axes,
      composition::Alignment alignment = composition::Alignment::center)
      : frame_(axes, alignment) {}

  RelativeFrame(
      composition::Axes axes, std::uint32_t count, std::uint32_t span,
      theme::Space gap,
      composition::Alignment alignment = composition::Alignment::center)
      : frame_(axes, count, span, gap, alignment) {}

  RelativeFrame(composition::Axes axes, composition::Alignment alignment,
                composition::ContainerRelativeFrame::Length length)
      : frame_(axes, alignment, std::move(length)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).container_relative_frame(frame_);
  }

 private:
  composition::ContainerRelativeFrame frame_;
};

class CoordinateSpaceName {
 public:
  explicit CoordinateSpaceName(composition::ElementKey name)
      : name_(std::move(name)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).coordinate_space(name_);
  }

 private:
  composition::ElementKey name_;
};

using padding = Padding;
using margin = Margin;
using frame = Frame;
using flex_grow = FlexGrow;
using flex_shrink = FlexShrink;
using aspect_ratio = AspectRatio;
using fixed_size = FixedSize;
class AlignmentGuide {
 public:
  using Compute = std::function<float(const composition::ViewDimensions&)>;

  AlignmentGuide(composition::AlignmentKey key, Compute compute)
      : key_(key), compute_(std::move(compute)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).alignment_guide(key_, compute_);
  }

 private:
  composition::AlignmentKey key_;
  Compute compute_;
};

template <class T>
class LayoutValue {
 public:
  LayoutValue(const composition::LayoutValueKey<T>& key, T value)
      : key_(key), value_(std::move(value)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).layout_value(key_.get(), value_);
  }

 private:
  std::reference_wrapper<const composition::LayoutValueKey<T>> key_;
  T value_;
};

template <class T>
class ContainerValue {
 public:
  ContainerValue(const composition::ContainerValueKey<T>& key, T value)
      : key_(key), value_(std::move(value)) {}

  auto apply(composition::Element element) const -> composition::Element {
    return std::move(element).container_value(key_.get(), value_);
  }

 private:
  std::reference_wrapper<const composition::ContainerValueKey<T>> key_;
  T value_;
};

using alignment_guide = AlignmentGuide;
template <class T>
using layout_value = LayoutValue<T>;
template <class T>
using container_value = ContainerValue<T>;
using layout_priority = LayoutPriority;
using position = Position;
using z_index = ZIndex;
using container_relative_frame = RelativeFrame;
using coordinate_space = CoordinateSpaceName;

}
