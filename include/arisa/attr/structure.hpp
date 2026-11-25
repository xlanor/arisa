#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/prominence.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/vertical_edges.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/structure/cell_style.hpp>
#include <arisa/structure/control_group_style.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/group_box_style.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tab_view_style.hpp>

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::attr {

class ListStyleAttribute {
 public:
  explicit ListStyleAttribute(structure::ListStyle style) noexcept
      : style_(style) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).list_style(style_);
  }

 private:
  structure::ListStyle style_;
};

class CellStyleAttribute {
 public:
  explicit CellStyleAttribute(structure::CellStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).cell_style(std::move(style_));
  }

 private:
  structure::CellStyle style_;
};

class TabViewStyleAttribute {
 public:
  explicit TabViewStyleAttribute(structure::TabViewStyle style) noexcept
      : style_(style) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).tab_view_style(style_);
  }

 private:
  structure::TabViewStyle style_;
};

class FormStyleAttribute {
 public:
  explicit FormStyleAttribute(structure::FormStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).form_style(std::move(style_));
  }

 private:
  structure::FormStyle style_;
};

class GroupBoxStyleAttribute {
 public:
  explicit GroupBoxStyleAttribute(structure::GroupBoxStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).group_box_style(std::move(style_));
  }

 private:
  structure::GroupBoxStyle style_;
};

class ControlGroupStyleAttribute {
 public:
  explicit ControlGroupStyleAttribute(structure::ControlGroupStyle style)
      : style_(std::move(style)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).control_group_style(std::move(style_));
  }

 private:
  structure::ControlGroupStyle style_;
};

class Toolbar {
 public:
  template <class... Items>
    requires(
        (std::same_as<std::remove_cvref_t<Items>, composition::ToolbarItem> ||
         std::same_as<std::remove_cvref_t<Items>,
                      composition::ToolbarItemGroup> ||
         std::same_as<std::remove_cvref_t<Items>,
                      composition::ToolbarSpacer>) &&
        ...)
  explicit Toolbar(Items&&... items) {
    (add(std::forward<Items>(items)), ...);
  }

  auto apply(composition::Element element) && -> composition::Element {
    for (composition::ToolbarItem& item : items_) {
      element = std::move(element).toolbar(std::move(item));
    }
    return element;
  }

 private:
  void add(composition::ToolbarItem item) { items_.push_back(std::move(item)); }

  void add(composition::ToolbarItemGroup group) {
    for (composition::ToolbarItem& item : std::move(group).items()) {
      items_.push_back(std::move(item));
    }
  }

  void add(const composition::ToolbarSpacer& spacer) {
    items_.push_back(spacer.item());
  }

  std::vector<composition::ToolbarItem> items_;
};

class OnMove {
 public:
  explicit OnMove(components::ForEach::MoveAction perform)
      : action_(std::move(perform)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return components::ForEach::with_move(std::move(element),
                                          std::move(action_));
  }

 private:
  components::ForEach::MoveAction action_;
};

class OnDelete {
 public:
  explicit OnDelete(components::ForEach::DeleteAction perform)
      : action_(std::move(perform)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return components::ForEach::with_delete(std::move(element),
                                            std::move(action_));
  }

 private:
  components::ForEach::DeleteAction action_;
};

class SectionHeader {
 public:
  explicit SectionHeader(composition::Element content)
      : content_(std::move(content)) {}

  void configure(structure::Section::Labels& labels) && {
    labels.header = std::move(content_);
  }

 private:
  composition::Element content_;
};

class SectionFooter {
 public:
  explicit SectionFooter(composition::Element content)
      : content_(std::move(content)) {}

  void configure(structure::Section::Labels& labels) && {
    labels.footer = std::move(content_);
  }

 private:
  composition::Element content_;
};

class Badge {
 public:
  explicit Badge(int count)
      : label_(count == 0 ? std::nullopt
                          : std::optional<std::string>{std::to_string(count)}) {
  }

  explicit Badge(std::optional<std::string> label) : label_(std::move(label)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).badge(std::move(label_));
  }

 private:
  std::optional<std::string> label_;
};

class ListRowInsets {
 public:
  explicit ListRowInsets(std::optional<theme::Insets> insets) noexcept
      : insets_(insets) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).list_row_insets(insets_);
  }

 private:
  std::optional<theme::Insets> insets_;
};

class ListRowSeparator {
 public:
  explicit ListRowSeparator(composition::Visibility visibility,
                            composition::VerticalEdges edges =
                                composition::VerticalEdges::all) noexcept
      : visibility_(visibility), edges_(edges) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).list_row_separator(visibility_, edges_);
  }

 private:
  composition::Visibility visibility_;
  composition::VerticalEdges edges_;
};

class RowFlag {
 public:
  enum class Kind : std::uint8_t {
    selection,
    move,
    remove,
  };

  constexpr RowFlag(Kind kind, bool off) noexcept
      : kind_(kind), disabled_(off) {}

  auto apply(composition::Element element) && -> composition::Element {
    switch (kind_) {
      case Kind::selection:
        return std::move(element).selection_disabled(disabled_);
      case Kind::move:
        return std::move(element).move_disabled(disabled_);
      case Kind::remove:
        return std::move(element).delete_disabled(disabled_);
    }
    return element;
  }

 private:
  Kind kind_;
  bool disabled_;
};

class HeaderProminence {
 public:
  constexpr explicit HeaderProminence(
      composition::Prominence prominence) noexcept
      : prominence_(prominence) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).header_prominence(prominence_);
  }

 private:
  composition::Prominence prominence_;
};

class SectionIndexLabel {
 public:
  explicit SectionIndexLabel(std::string label) : label_(std::move(label)) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).section_index_label(std::move(label_));
  }

 private:
  std::string label_;
};

class SectionIndexVisibility {
 public:
  constexpr explicit SectionIndexVisibility(
      composition::Visibility visibility) noexcept
      : visibility_(visibility) {}

  auto apply(composition::Element element) && -> composition::Element {
    return std::move(element).list_section_index_visibility(visibility_);
  }

 private:
  composition::Visibility visibility_;
};

using header = SectionHeader;
using footer = SectionFooter;
using list_style = ListStyleAttribute;
using cell_style = CellStyleAttribute;
using tab_view_style = TabViewStyleAttribute;
using form_style = FormStyleAttribute;
using group_box_style = GroupBoxStyleAttribute;
using control_group_style = ControlGroupStyleAttribute;
using toolbar = Toolbar;
using on_move = OnMove;
using on_delete = OnDelete;
using badge = Badge;
using list_row_insets = ListRowInsets;
using list_row_separator = ListRowSeparator;
using header_prominence = HeaderProminence;
using section_index_label = SectionIndexLabel;
using list_section_index_visibility = SectionIndexVisibility;
inline constexpr RowFlag selection_disabled{RowFlag::Kind::selection, true};
inline constexpr RowFlag move_disabled{RowFlag::Kind::move, true};
inline constexpr RowFlag delete_disabled{RowFlag::Kind::remove, true};

}
