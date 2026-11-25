#include <arisa/components/document.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace arisa::components {

auto DocumentContentTypes::matching(
    const std::vector<composition::UTType>& types,
    const composition::FileURL& file) -> composition::UTType {
  const std::string_view name = file.last_path_component();
  const std::size_t dot = name.rfind('.');
  if (dot != std::string_view::npos) {
    std::string extension{name.substr(dot + 1)};
    std::ranges::transform(extension, extension.begin(), [](char character) {
      return static_cast<char>(
          std::tolower(static_cast<unsigned char>(character)));
    });
    const auto found = std::ranges::find_if(
        types, [&extension](const composition::UTType& type) {
          return type.preferred_filename_extension() == extension;
        });
    if (found != types.end()) {
      return *found;
    }
  }
  return types.empty() ? composition::UTType::plain_text() : types.front();
}

}
