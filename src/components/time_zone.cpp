#include <arisa/components/time_zone.hpp>

#include <cstdlib>
#include <format>

namespace arisa::components {

auto TimeZone::identifier() const -> std::string {
  if (!name_.empty()) {
    return name_;
  }
  const auto total = offset_.count();
  if (total == 0) {
    return "GMT";
  }
  const auto magnitude = std::llabs(total);
  return std::format("GMT{}{:02}:{:02}", total < 0 ? '-' : '+',
                     magnitude / 3600, (magnitude % 3600) / 60);
}

}
