#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::inspector {

enum class InspectorPanel : std::uint8_t {
  tree,
  pick,
  focus,
  timeline,
  cost,
  gpu,
  tokens,
};

class InspectorPanels {
 public:
  static constexpr std::array all{
      InspectorPanel::tree,     InspectorPanel::pick, InspectorPanel::focus,
      InspectorPanel::timeline, InspectorPanel::cost, InspectorPanel::gpu,
      InspectorPanel::tokens};

  static auto name(InspectorPanel panel) noexcept -> std::string_view;

  static auto parse(std::string_view text) noexcept
      -> std::optional<InspectorPanel>;
};

}
