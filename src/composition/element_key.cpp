#include <arisa/composition/element_key.hpp>

#include <format>
#include <functional>

namespace arisa::composition {

auto ElementKey::description() const -> std::string {
  return std::visit([](const auto& value) { return std::format("{}", value); },
                    value_);
}

auto ElementKey::hash() const noexcept -> std::size_t {
  return std::hash<std::variant<std::int64_t, std::string>>{}(value_);
}

}
