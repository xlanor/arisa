#include <arisa/structure/tab_view_customization.hpp>

#include <algorithm>
#include <ranges>
#include <utility>

namespace arisa::structure {

auto TabViewCustomization::sidebar_visibility(std::string_view id) const
    -> composition::Visibility {
  const auto found = visibility_.find(id);
  return found == visibility_.end() ? composition::Visibility::automatic
                                    : found->second;
}

void TabViewCustomization::set_sidebar_visibility(
    std::string_view id, composition::Visibility visibility) {
  if (visibility == composition::Visibility::automatic) {
    if (const auto found = visibility_.find(id); found != visibility_.end()) {
      visibility_.erase(found);
    }
    return;
  }
  visibility_.insert_or_assign(std::string{id}, visibility);
}

auto TabViewCustomization::tab(std::string_view id) const -> TabCustomization {
  return TabCustomization{sidebar_visibility(id), tab_bar_visibility(id)};
}

auto TabViewCustomization::tab_bar_visibility(std::string_view id) const
    -> composition::Visibility {
  const auto found = bar_.find(id);
  return found == bar_.end() ? composition::Visibility::automatic
                             : found->second;
}

void TabViewCustomization::set_tab_bar_visibility(
    std::string_view id, composition::Visibility visibility) {
  if (visibility == composition::Visibility::automatic) {
    if (const auto found = bar_.find(id); found != bar_.end()) {
      bar_.erase(found);
    }
    return;
  }
  bar_.insert_or_assign(std::string{id}, visibility);
}

void TabViewCustomization::reset_visibility(std::span<const std::string> ids) {
  for (const std::string& id : ids) {
    set_sidebar_visibility(id, composition::Visibility::automatic);
    set_tab_bar_visibility(id, composition::Visibility::automatic);
  }
}

void TabViewCustomization::reset_tab_order() { order_.clear(); }

auto TabViewCustomization::section(std::string_view id) const
    -> SectionCustomization {
  const auto found = sections_.find(id);
  return found == sections_.end() ? SectionCustomization{}
                                  : SectionCustomization{found->second};
}

void TabViewCustomization::set_section(std::string_view id,
                                       SectionCustomization section) {
  reorder_section(id, section.tab_order().value_or(std::vector<std::string>{}));
}

auto TabViewCustomization::section_order(std::string_view id) const
    -> std::optional<std::vector<std::string>> {
  return section(id).tab_order();
}

void TabViewCustomization::reorder_section(std::string_view id,
                                           std::vector<std::string> order) {
  if (order.empty()) {
    reset_section_order(id);
    return;
  }
  sections_.insert_or_assign(std::string{id}, std::move(order));
}

void TabViewCustomization::reset_section_order() { sections_.clear(); }

void TabViewCustomization::reset_section_order(std::string_view id) {
  const auto found = sections_.find(id);
  if (found != sections_.end()) {
    sections_.erase(found);
  }
}

void TabViewCustomization::reorder(std::vector<std::string> order) {
  order_ = std::move(order);
}

auto TabViewCustomization::arranged(std::span<const std::string> declared) const
    -> std::vector<std::size_t> {
  std::vector<std::size_t> shown = ordered(declared);
  std::erase_if(shown, [this, declared](std::size_t placed) {
    return !declared[placed].empty() && sidebar_visibility(declared[placed]) ==
                                            composition::Visibility::hidden;
  });
  return shown;
}

auto TabViewCustomization::ordered(std::span<const std::string> declared) const
    -> std::vector<std::size_t> {
  std::vector<std::size_t> movable;
  for (const std::string& id : order_) {
    const auto found = std::ranges::find(declared, id);
    if (!id.empty() && found != declared.end()) {
      movable.push_back(static_cast<std::size_t>(found - declared.begin()));
    }
  }
  for (std::size_t index = 0; index < declared.size(); ++index) {
    if (!declared[index].empty() && !std::ranges::contains(movable, index)) {
      movable.push_back(index);
    }
  }
  std::vector<std::size_t> shown;
  shown.reserve(declared.size());
  auto next = movable.begin();
  for (std::size_t index = 0; index < declared.size(); ++index) {
    shown.push_back(declared[index].empty() ? index : *next++);
  }
  return shown;
}

auto TabViewCustomization::encoded() const -> std::string {
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
  for (const auto& [id, visibility] : visibility_) {
    line(visibility == composition::Visibility::hidden ? hidden_mark
                                                       : visible_mark,
         id);
  }
  for (const auto& [id, visibility] : bar_) {
    line(visibility == composition::Visibility::hidden ? bar_hidden_mark
                                                       : bar_visible_mark,
         id);
  }
  for (const auto& [id, order] : sections_) {
    if (id.find(section_separator) != std::string::npos) {
      continue;
    }
    for (const std::string& tab : order) {
      line(section_mark, id + section_separator + tab);
    }
  }
  return text;
}

auto TabViewCustomization::decoded(std::string_view text)
    -> std::optional<TabViewCustomization> {
  TabViewCustomization made;
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
        made.set_sidebar_visibility(id, composition::Visibility::hidden);
        break;
      case visible_mark:
        made.set_sidebar_visibility(id, composition::Visibility::visible);
        break;
      case bar_hidden_mark:
        made.set_tab_bar_visibility(id, composition::Visibility::hidden);
        break;
      case bar_visible_mark:
        made.set_tab_bar_visibility(id, composition::Visibility::visible);
        break;
      case section_mark: {
        const std::size_t split = id.find(section_separator);
        if (split == std::string::npos) {
          return std::nullopt;
        }
        made.sections_[id.substr(0, split)].push_back(id.substr(split + 1));
        break;
      }
      default:
        return std::nullopt;
    }
  }
  return made;
}

}
