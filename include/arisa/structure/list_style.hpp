#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/theme.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::structure {

class BorderedListStyle;
class DefaultListStyle;
class GroupedListStyle;
class InsetGroupedListStyle;
class InsetListStyle;
class PlainListStyle;
class SidebarListStyle;

class ListStyle {
 public:
  enum class Kind : std::uint8_t {
    inset_grouped,
    plain,
    sidebar,
    grouped,
    inset,
    bordered,
  };

  static const composition::EnvironmentKey<ListStyle> key;

  ListStyle(DefaultListStyle style) noexcept;

  ListStyle(GroupedListStyle style) noexcept;

  ListStyle(InsetGroupedListStyle style) noexcept;

  ListStyle(InsetListStyle style) noexcept;

  ListStyle(PlainListStyle style) noexcept;

  ListStyle(SidebarListStyle style) noexcept;

  ListStyle(BorderedListStyle style) noexcept;

  static auto automatic() noexcept -> ListStyle;

  static auto grouped() noexcept -> ListStyle;

  static auto inset_grouped() noexcept -> ListStyle;

  static auto inset() noexcept -> ListStyle;

  static auto plain() noexcept -> ListStyle;

  static auto sidebar() noexcept -> ListStyle;

  static auto bordered() noexcept -> ListStyle;

  static auto bordered(bool alternates_row_backgrounds) noexcept -> ListStyle;

  auto kind() const noexcept -> Kind { return kind_; }

  auto alternates_row_backgrounds() const noexcept -> bool {
    return alternates_;
  }

  auto margin() const noexcept -> theme::Space;

  auto margin_points() const noexcept -> float;

  auto section_spacing() const noexcept -> theme::Space;

  auto carded() const noexcept -> bool {
    return kind_ == Kind::inset_grouped || kind_ == Kind::grouped;
  }

  auto row_inset() const noexcept -> float;

  auto content_background(composition::Visibility visibility,
                          const theme::Theme& theme) const noexcept
      -> std::optional<theme::ColourRole>;

  friend auto operator==(const ListStyle&, const ListStyle&) -> bool = default;

 private:
  explicit ListStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  bool alternates_{false};
};

class BorderedListStyle {
 public:
  BorderedListStyle() noexcept = default;

  explicit BorderedListStyle(bool alternates_row_backgrounds) noexcept
      : alternates_(alternates_row_backgrounds) {}

  auto alternates_row_backgrounds() const noexcept -> bool {
    return alternates_;
  }

  friend auto operator==(const BorderedListStyle&, const BorderedListStyle&)
      -> bool = default;

 private:
  bool alternates_{false};
};

class DefaultListStyle {
 public:
  friend auto operator==(const DefaultListStyle&, const DefaultListStyle&)
      -> bool = default;
};

class GroupedListStyle {
 public:
  friend auto operator==(const GroupedListStyle&, const GroupedListStyle&)
      -> bool = default;
};

class InsetListStyle {
 public:
  friend auto operator==(const InsetListStyle&, const InsetListStyle&)
      -> bool = default;
};

class InsetGroupedListStyle {
 public:
  friend auto operator==(const InsetGroupedListStyle&,
                         const InsetGroupedListStyle&) -> bool = default;
};

class PlainListStyle {
 public:
  friend auto operator==(const PlainListStyle&, const PlainListStyle&)
      -> bool = default;
};

class SidebarListStyle {
 public:
  friend auto operator==(const SidebarListStyle&, const SidebarListStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::list_style(Style style) && -> Element {
  return std::move(*this).environment(structure::ListStyle::key,
                                      structure::ListStyle{std::move(style)});
}

}
