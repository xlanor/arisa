#include <arisa/composition/accessibility_chart_descriptor.hpp>

#include <format>

namespace arisa::composition {

auto AccessibilityChartDescriptor::description() const -> std::string {
  std::string text = title;
  if (!summary.empty()) {
    text += text.empty() ? summary : std::format(", {}", summary);
  }
  if (!x_axis.title.empty() || !y_axis.title.empty()) {
    text += std::format(". {} by {}", y_axis.title, x_axis.title);
  }
  if (!series.empty()) {
    text += std::format(". {} series", series.size());
  }
  return text;
}

}
