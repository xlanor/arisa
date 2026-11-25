#include <arisa/runtime/introspection.hpp>

#include <toml.hpp>

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

namespace arisa::runtime {

auto Introspection::serialized_data() const -> std::vector<std::byte> {
  const auto rect = [](const composition::Rect& value) {
    return toml::table{{"x", static_cast<double>(value.x)},
                       {"y", static_cast<double>(value.y)},
                       {"width", static_cast<double>(value.width)},
                       {"height", static_cast<double>(value.height)}};
  };
  toml::array listed;
  for (const NodeRecord& record : nodes()) {
    toml::table entry{{"id", static_cast<std::int64_t>(record.id.value())},
                      {"depth", static_cast<std::int64_t>(record.depth)},
                      {"name", record.name},
                      {"key", record.key},
                      {"frame", rect(record.frame)},
                      {"bounds", rect(record.bounds)},
                      {"children", static_cast<std::int64_t>(record.children)},
                      {"focusable", record.flags.focusable}};
    if (record.parent.has_value()) {
      entry.insert("parent", static_cast<std::int64_t>(record.parent->value()));
    }
    listed.push_back(std::move(entry));
  }
  const toml::table document{{"nodes", std::move(listed)}};
  std::ostringstream stream;
  stream << toml::json_formatter{document};
  const std::string text = std::move(stream).str();
  std::vector<std::byte> bytes(text.size());
  std::ranges::transform(text, bytes.begin(), [](char character) {
    return static_cast<std::byte>(character);
  });
  return bytes;
}

}
