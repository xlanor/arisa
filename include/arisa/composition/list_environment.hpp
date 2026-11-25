#pragma once

#include <arisa/composition/alternating_row_background_behavior.hpp>
#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/edge_lengths.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/prominence.hpp>
#include <arisa/composition/sidebar_row_size.hpp>
#include <arisa/composition/swipe_host.hpp>
#include <arisa/composition/visibility.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace arisa::composition {

class ListEnvironment {
 public:
  using SectionActions = std::shared_ptr<const std::function<Element()>>;

  static const EnvironmentKey<Prominence> header_prominence;
  static const EnvironmentKey<std::optional<float>> row_spacing;
  static const EnvironmentKey<std::optional<float>> section_spacing;
  static const EnvironmentKey<std::optional<float>> card_corner_radius;
  static const EnvironmentKey<std::optional<float>> default_min_row_height;
  static const EnvironmentKey<std::optional<float>> default_min_header_height;
  static const EnvironmentKey<bool> alternating_row_backgrounds;
  static const EnvironmentKey<AlternatingRowBackgroundBehavior>
      alternating_row_background_behavior;
  static const EnvironmentKey<Visibility> section_index_visibility;
  static const EnvironmentKey<BadgeProminence> badge_prominence;
  static const EnvironmentKey<EdgeLengths> section_margins;
  static const EnvironmentKey<ListRow::Separator> section_separator_top;
  static const EnvironmentKey<ListRow::Separator> section_separator_bottom;
  static const EnvironmentKey<SectionActions> section_actions;
  static const EnvironmentKey<SidebarRowSize> sidebar_row_size;
  static const EnvironmentKey<bool> swipe_actions_container;
  static const EnvironmentKey<const SwipeHost*> swipe_host;
  static const EnvironmentKey<Visibility> table_column_headers;
};

}
