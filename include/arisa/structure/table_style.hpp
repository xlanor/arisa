#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <cstdint>
#include <utility>

namespace arisa::structure {

class AutomaticTableStyle;
class BorderedTableStyle;
class InsetTableStyle;

class TableStyleConfiguration {
 public:
  friend auto operator==(const TableStyleConfiguration&,
                         const TableStyleConfiguration&) -> bool = default;
};

class TableStyle {
 public:
  using Configuration = TableStyleConfiguration;

  enum class Kind : std::uint8_t {
    inset,
    bordered,
  };

  static const composition::EnvironmentKey<TableStyle> key;

  TableStyle(AutomaticTableStyle style) noexcept;

  TableStyle(InsetTableStyle style) noexcept;

  TableStyle(BorderedTableStyle style) noexcept;

  static auto automatic() noexcept -> TableStyle;

  static auto inset() noexcept -> TableStyle;

  static auto bordered() noexcept -> TableStyle;

  static auto bordered(bool alternates_row_backgrounds) noexcept -> TableStyle;

  auto kind() const noexcept -> Kind { return kind_; }

  auto alternates_row_backgrounds() const noexcept -> bool {
    return alternates_;
  }

  friend auto operator==(const TableStyle&, const TableStyle&)
      -> bool = default;

 private:
  TableStyle(Kind kind, bool alternates) noexcept
      : kind_(kind), alternates_(alternates) {}

  Kind kind_;
  bool alternates_;
};

class AutomaticTableStyle {
 public:
  friend auto operator==(const AutomaticTableStyle&, const AutomaticTableStyle&)
      -> bool = default;
};

class InsetTableStyle {
 public:
  InsetTableStyle() noexcept = default;

  explicit InsetTableStyle(bool alternates_row_backgrounds) noexcept
      : alternates_(alternates_row_backgrounds) {}

  auto alternates_row_backgrounds() const noexcept -> bool {
    return alternates_;
  }

  friend auto operator==(const InsetTableStyle&, const InsetTableStyle&)
      -> bool = default;

 private:
  bool alternates_{true};
};

class BorderedTableStyle {
 public:
  BorderedTableStyle() noexcept = default;

  explicit BorderedTableStyle(bool alternates_row_backgrounds) noexcept
      : alternates_(alternates_row_backgrounds) {}

  auto alternates_row_backgrounds() const noexcept -> bool {
    return alternates_;
  }

  friend auto operator==(const BorderedTableStyle&, const BorderedTableStyle&)
      -> bool = default;

 private:
  bool alternates_{true};
};

}

namespace arisa::composition {

template <class Style>
auto Element::table_style(Style style) && -> Element {
  return std::move(*this).environment(structure::TableStyle::key,
                                      structure::TableStyle{std::move(style)});
}

}
