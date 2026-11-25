#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace arisa::structure {

class TableColumnAlignment {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    leading,
    center,
    trailing,
    numeric,
  };

  static const TableColumnAlignment automatic;
  static const TableColumnAlignment leading;
  static const TableColumnAlignment center;
  static const TableColumnAlignment trailing;
  static const TableColumnAlignment numeric;

  static auto numeric_in(std::string numbering_system) -> TableColumnAlignment {
    return TableColumnAlignment{Kind::numeric, std::move(numbering_system)};
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto numbering_system() const noexcept -> const std::string& {
    return numbering_system_;
  }

  auto resolved() const noexcept -> Kind {
    switch (kind_) {
      case Kind::automatic:
        return Kind::leading;
      case Kind::numeric:
        return Kind::trailing;
      case Kind::leading:
      case Kind::center:
      case Kind::trailing:
        break;
    }
    return kind_;
  }

  friend auto operator==(const TableColumnAlignment&,
                         const TableColumnAlignment&) -> bool = default;

 private:
  explicit TableColumnAlignment(Kind kind, std::string numbering_system = {})
      : kind_(kind), numbering_system_(std::move(numbering_system)) {}

  Kind kind_;
  std::string numbering_system_;
};

inline const TableColumnAlignment TableColumnAlignment::automatic{
    TableColumnAlignment::Kind::automatic};
inline const TableColumnAlignment TableColumnAlignment::leading{
    TableColumnAlignment::Kind::leading};
inline const TableColumnAlignment TableColumnAlignment::center{
    TableColumnAlignment::Kind::center};
inline const TableColumnAlignment TableColumnAlignment::trailing{
    TableColumnAlignment::Kind::trailing};
inline const TableColumnAlignment TableColumnAlignment::numeric{
    TableColumnAlignment::Kind::numeric};

}
