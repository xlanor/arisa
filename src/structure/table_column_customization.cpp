#include <arisa/structure/table_column_customization.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <ranges>
#include <utility>

namespace arisa::structure {

auto TableColumnCustomization::visibility(std::string_view id) const
    -> composition::Visibility {
  const auto found = visibility_.find(id);
  return found == visibility_.end() ? composition::Visibility::automatic
                                    : found->second;
}

void TableColumnCustomization::set_visibility(
    std::string_view id, composition::Visibility visibility) {
  if (visibility == composition::Visibility::automatic) {
    const auto found = visibility_.find(id);
    if (found != visibility_.end()) {
      visibility_.erase(found);
    }
    return;
  }
  visibility_.insert_or_assign(std::string{id}, visibility);
}

auto TableColumnCustomization::width(std::string_view id) const
    -> std::optional<float> {
  const auto found = widths_.find(id);
  if (found == widths_.end()) {
    return std::nullopt;
  }
  return found->second;
}

void TableColumnCustomization::set_width(std::string_view id,
                                         std::optional<float> width) {
  if (!width.has_value()) {
    const auto found = widths_.find(id);
    if (found != widths_.end()) {
      widths_.erase(found);
    }
    return;
  }
  widths_.insert_or_assign(std::string{id}, *width);
}

void TableColumnCustomization::reorder(std::vector<std::string> order) {
  order_ = std::move(order);
}

auto TableColumnCustomization::arranged(
    std::span<const std::string> declared) const -> std::vector<std::size_t> {
  std::vector<std::size_t> placed;
  placed.reserve(declared.size());
  for (const std::string& id : order_) {
    const auto found = std::ranges::find(declared, id);
    if (found != declared.end()) {
      const auto index = static_cast<std::size_t>(found - declared.begin());
      if (!std::ranges::contains(placed, index)) {
        placed.push_back(index);
      }
    }
  }
  for (std::size_t index = 0; index < declared.size(); ++index) {
    if (!std::ranges::contains(placed, index)) {
      placed.push_back(index);
    }
  }
  return placed;
}

auto TableColumnCustomization::encoded() const -> std::string {
  std::string text;
  const auto line = [&text](char mark, std::string_view id) {
    if (id.find('\n') != std::string_view::npos) {
      return;
    }
    text.push_back(mark);
    text.append(id);
    text.push_back('\n');
  };
  for (const std::string& id : order_) {
    line(order_mark, id);
  }
  for (const auto& [id, shown] : visibility_) {
    line(shown == composition::Visibility::hidden ? hidden_mark : visible_mark,
         id);
  }
  for (const auto& [id, points] : widths_) {
    if (id.find(width_separator) == std::string::npos) {
      line(width_mark, std::format("{}{}{}", id, width_separator, points));
    }
  }
  return text;
}

auto TableColumnCustomization::decoded(std::string_view text)
    -> std::optional<TableColumnCustomization> {
  TableColumnCustomization made;
  for (const auto part : text | std::views::split('\n')) {
    const std::string_view line{part.begin(), part.end()};
    if (line.empty()) {
      continue;
    }
    const std::string id{line.substr(1)};
    switch (line.front()) {
      case order_mark:
        made.order_.push_back(id);
        break;
      case hidden_mark:
        made.set_visibility(id, composition::Visibility::hidden);
        break;
      case visible_mark:
        made.set_visibility(id, composition::Visibility::visible);
        break;
      case width_mark: {
        const std::size_t split = id.find(width_separator);
        if (split == std::string::npos) {
          return std::nullopt;
        }
        float points = 0.0F;
        const std::string_view number = std::string_view{id}.substr(split + 1);
        const auto [end, failure] = std::from_chars(
            number.data(), number.data() + number.size(), points);
        if (failure != std::errc{} || end != number.data() + number.size()) {
          return std::nullopt;
        }
        made.widths_.insert_or_assign(id.substr(0, split), points);
        break;
      }
      default:
        return std::nullopt;
    }
  }
  return made;
}

}
