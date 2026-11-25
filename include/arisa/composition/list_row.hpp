#pragma once

#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/edge_lengths.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/list_item_tint.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace arisa::composition {

struct ListRow {
  using Actions = std::function<Element()>;

  struct Separator {
    Visibility visibility{Visibility::automatic};
    std::optional<theme::ColourValue> tint;

    friend auto operator==(const Separator&, const Separator&)
        -> bool = default;
  };

  struct SwipeActions {
    Actions content;
    bool allows_full_swipe{true};
  };

  std::optional<theme::Insets> insets;
  EdgeLengths edge_insets;
  Separator top;
  Separator bottom;
  std::shared_ptr<PlaceholderContentView::Source> background;
  std::optional<ListItemTint> item_tint;
  std::optional<std::string> badge;
  std::optional<BadgeProminence> badge_prominence;
  bool selection_disabled{false};
  bool move_disabled{false};
  bool delete_disabled{false};
  SwipeActions leading;
  SwipeActions trailing;
  std::optional<motion::HoverEffect> hover;
  bool hover_disabled{false};
  std::optional<float> section_spacing;
  std::optional<std::string> index_label;
  Actions section_actions;

  friend auto operator==(const ListRow& left, const ListRow& right) -> bool {
    return !left.leading.content && !left.trailing.content &&
           !right.leading.content && !right.trailing.content &&
           !left.section_actions && !right.section_actions &&
           left.insets == right.insets &&
           left.edge_insets == right.edge_insets && left.top == right.top &&
           left.bottom == right.bottom && left.background == right.background &&
           left.item_tint == right.item_tint && left.badge == right.badge &&
           left.badge_prominence == right.badge_prominence &&
           left.selection_disabled == right.selection_disabled &&
           left.move_disabled == right.move_disabled &&
           left.delete_disabled == right.delete_disabled &&
           left.hover == right.hover &&
           left.hover_disabled == right.hover_disabled &&
           left.section_spacing == right.section_spacing &&
           left.index_label == right.index_label;
  }
};

}
