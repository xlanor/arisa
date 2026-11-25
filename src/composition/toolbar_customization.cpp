#include <arisa/composition/toolbar_customization.hpp>

#include <ranges>

namespace arisa::composition {

auto ToolbarCustomization::visibility(std::string_view id) const -> Visibility {
  const auto found = visibility_.find(id);
  return found == visibility_.end() ? Visibility::automatic : found->second;
}

void ToolbarCustomization::set_visibility(std::string_view id,
                                          Visibility visibility) {
  if (visibility == Visibility::automatic) {
    if (const auto found = visibility_.find(id); found != visibility_.end()) {
      visibility_.erase(found);
    }
    return;
  }
  visibility_.insert_or_assign(std::string{id}, visibility);
}

void ToolbarCustomization::reset() { visibility_.clear(); }

auto ToolbarCustomization::encoded() const -> std::string {
  std::string text;
  for (const auto& [id, visibility] : visibility_) {
    if (id.find(separator) != std::string::npos) {
      continue;
    }
    text.push_back(visibility == Visibility::hidden ? hidden_mark
                                                    : visible_mark);
    text.append(id);
    text.push_back(separator);
  }
  return text;
}

auto ToolbarCustomization::decoded(std::string_view text)
    -> std::optional<ToolbarCustomization> {
  ToolbarCustomization made;
  for (const auto part : text | std::views::split(separator)) {
    const std::string_view line{part.begin(), part.end()};
    if (line.empty()) {
      continue;
    }
    switch (line.front()) {
      case hidden_mark:
        made.set_visibility(line.substr(1), Visibility::hidden);
        break;
      case visible_mark:
        made.set_visibility(line.substr(1), Visibility::visible);
        break;
      default:
        return std::nullopt;
    }
  }
  return made;
}

}
