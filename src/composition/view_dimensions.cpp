#include <arisa/composition/view_dimensions.hpp>

#include <algorithm>

namespace arisa::composition {

auto ViewDimensions::operator[](const AlignmentKey& key) const -> float {
  return explicit_value(key).value_or(default_value(key));
}

auto ViewDimensions::explicit_value(const AlignmentKey& key) const
    -> std::optional<float> {
  const auto guide = std::ranges::find(guides_, key, &AlignmentGuide::key);
  if (guide != guides_.end()) {
    return guide->value(ViewDimensions{width_, height_, baselines_});
  }
  const auto carried = std::ranges::find(propagated_, key, &Propagated::key);
  if (carried != propagated_.end()) {
    return carried->value;
  }
  return std::nullopt;
}

auto ViewDimensions::default_value(const AlignmentKey& key) const -> float {
  if (const auto horizontal = key.horizontal()) {
    switch (*horizontal) {
      case HorizontalAlignment::leading:
        return 0.0F;
      case HorizontalAlignment::center:
        return width_ * 0.5F;
      case HorizontalAlignment::trailing:
        return width_;
    }
    return width_ * 0.5F;
  }
  if (const auto vertical = key.vertical()) {
    switch (*vertical) {
      case VerticalAlignment::top:
        return 0.0F;
      case VerticalAlignment::center:
        return height_ * 0.5F;
      case VerticalAlignment::bottom:
        return height_;
      case VerticalAlignment::first_text_baseline:
        return baselines_.first.value_or(height_);
      case VerticalAlignment::last_text_baseline:
        return baselines_.last.value_or(height_);
    }
    return height_ * 0.5F;
  }
  return key.custom()->get().default_value(
      ViewDimensions{width_, height_, baselines_});
}

}
