#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

struct AccessibilityChartDescriptor {
  struct Axis {
    std::string title;
    std::optional<double> lower{};
    std::optional<double> upper{};

    friend auto operator==(const Axis&, const Axis&) -> bool = default;
  };

  struct Series {
    std::string name;
    std::vector<double> values;
    bool continuous{false};

    friend auto operator==(const Series&, const Series&) -> bool = default;
  };

  std::string title;
  std::string summary;
  Axis x_axis;
  Axis y_axis;
  std::vector<Series> series;

  auto description() const -> std::string;

  friend auto operator==(const AccessibilityChartDescriptor&,
                         const AccessibilityChartDescriptor&) -> bool = default;
};

template <class T>
concept AccessibilityChartDescriptorRepresentable = requires(const T& value) {
  {
    value.make_chart_descriptor()
  } -> std::convertible_to<AccessibilityChartDescriptor>;
};

}
