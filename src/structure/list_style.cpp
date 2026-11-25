#include <arisa/structure/list_style.hpp>
#include <arisa/theme/metrics.hpp>

namespace arisa::structure {

const composition::EnvironmentKey<ListStyle> ListStyle::key{
    "list_style", ListStyle::automatic()};

ListStyle::ListStyle(DefaultListStyle) noexcept : kind_(Kind::inset_grouped) {}

ListStyle::ListStyle(GroupedListStyle) noexcept : kind_(Kind::grouped) {}

ListStyle::ListStyle(InsetGroupedListStyle) noexcept
    : kind_(Kind::inset_grouped) {}

ListStyle::ListStyle(InsetListStyle) noexcept : kind_(Kind::inset) {}

ListStyle::ListStyle(PlainListStyle) noexcept : kind_(Kind::plain) {}

ListStyle::ListStyle(SidebarListStyle) noexcept : kind_(Kind::sidebar) {}

ListStyle::ListStyle(BorderedListStyle style) noexcept
    : kind_(Kind::bordered), alternates_(style.alternates_row_backgrounds()) {}

auto ListStyle::automatic() noexcept -> ListStyle {
  return ListStyle{Kind::inset_grouped};
}

auto ListStyle::grouped() noexcept -> ListStyle {
  return ListStyle{Kind::grouped};
}

auto ListStyle::inset() noexcept -> ListStyle { return ListStyle{Kind::inset}; }

auto ListStyle::inset_grouped() noexcept -> ListStyle {
  return ListStyle{Kind::inset_grouped};
}

auto ListStyle::plain() noexcept -> ListStyle { return ListStyle{Kind::plain}; }

auto ListStyle::sidebar() noexcept -> ListStyle {
  return ListStyle{Kind::sidebar};
}

auto ListStyle::bordered() noexcept -> ListStyle {
  return ListStyle{BorderedListStyle{}};
}

auto ListStyle::bordered(bool alternates_row_backgrounds) noexcept
    -> ListStyle {
  return ListStyle{BorderedListStyle{alternates_row_backgrounds}};
}

auto ListStyle::margin() const noexcept -> theme::Space {
  switch (kind_) {
    case Kind::inset_grouped:
    case Kind::inset:
      return theme::metric::list_grouped_margin;
    case Kind::plain:
    case Kind::grouped:
    case Kind::bordered:
      return theme::space::none;
    case Kind::sidebar:
      return theme::metric::sidebar_selection_inset;
  }
  return theme::space::none;
}

auto ListStyle::margin_points() const noexcept -> float {
  switch (kind_) {
    case Kind::inset_grouped:
    case Kind::inset:
      return theme::metrics::list.grouped_margin;
    case Kind::plain:
    case Kind::grouped:
    case Kind::bordered:
      return 0.0F;
    case Kind::sidebar:
      return theme::metrics::list.sidebar_selection_inset;
  }
  return 0.0F;
}

auto ListStyle::section_spacing() const noexcept -> theme::Space {
  switch (kind_) {
    case Kind::inset_grouped:
    case Kind::grouped:
      return theme::metric::list_grouped_section_gap;
    case Kind::plain:
    case Kind::bordered:
      return theme::space::none;
    case Kind::sidebar:
    case Kind::inset:
      return theme::space::md;
  }
  return theme::space::none;
}

auto ListStyle::row_inset() const noexcept -> float {
  switch (kind_) {
    case Kind::inset_grouped:
    case Kind::grouped:
      return theme::metrics::list.grouped_row_inset;
    case Kind::plain:
    case Kind::inset:
    case Kind::bordered:
      return theme::metrics::list.plain_row_inset;
    case Kind::sidebar:
      return theme::metrics::list.sidebar_selection_inset;
  }
  return theme::metrics::list.grouped_row_inset;
}

auto ListStyle::content_background(composition::Visibility visibility,
                                   const theme::Theme& theme) const noexcept
    -> std::optional<theme::ColourRole> {
  const theme::ColourRole role =
      carded() ? theme::ColourRole::system_grouped_background
               : theme::ColourRole::system_background;
  if (kind_ == Kind::bordered ||
      visibility == composition::Visibility::visible) {
    return role;
  }
  if (visibility == composition::Visibility::hidden || kind_ == Kind::sidebar) {
    return std::nullopt;
  }
  const theme::GlassDefaults& glass = theme.glass();
  if (glass.sidebar.has_value() || glass.toolbar.has_value() ||
      glass.sheet.has_value()) {
    return std::nullopt;
  }
  return role;
}

}
