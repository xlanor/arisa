#include <arisa/structure/table_style.hpp>

namespace arisa::structure {

const composition::EnvironmentKey<TableStyle> TableStyle::key{
    "table_style", TableStyle::automatic()};

TableStyle::TableStyle(AutomaticTableStyle) noexcept
    : TableStyle(Kind::inset, true) {}

TableStyle::TableStyle(InsetTableStyle style) noexcept
    : TableStyle(Kind::inset, style.alternates_row_backgrounds()) {}

TableStyle::TableStyle(BorderedTableStyle style) noexcept
    : TableStyle(Kind::bordered, style.alternates_row_backgrounds()) {}

auto TableStyle::automatic() noexcept -> TableStyle {
  return TableStyle{Kind::inset, true};
}

auto TableStyle::inset() noexcept -> TableStyle {
  return TableStyle{Kind::inset, true};
}

auto TableStyle::bordered() noexcept -> TableStyle {
  return TableStyle{Kind::bordered, true};
}

auto TableStyle::bordered(bool alternates_row_backgrounds) noexcept
    -> TableStyle {
  return TableStyle{Kind::bordered, alternates_row_backgrounds};
}

}
