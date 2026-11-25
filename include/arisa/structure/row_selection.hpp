#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/tag_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/sidebar_selection.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::structure {

class RowSelection {
 public:
  static const composition::EnvironmentKey<std::optional<RowSelection>> key;

  RowSelection(List::Selection selection,
               composition::FocusBinding<composition::TagKey> focus,
               std::optional<SidebarSelection> sidebar)
      : selected_(selection.current()),
        chosen_(selection.members ? selection.members()
                                  : std::vector<composition::TagKey>{}),
        selection_(std::move(selection)),
        focus_(std::move(focus)),
        sidebar_(std::move(sidebar)) {}

  auto selects(const std::shared_ptr<const composition::TagValue>& tag) const
      -> bool;

  auto chosen() const noexcept -> const std::vector<composition::TagKey>& {
    return chosen_;
  }

  auto multiple() const noexcept -> bool {
    return static_cast<bool>(selection_.toggle);
  }

  void toggle(const std::shared_ptr<const composition::TagValue>& tag) const;

  auto agrees(const RowSelection& other,
              const std::shared_ptr<const composition::TagValue>& tag) const
      -> bool {
    return selects(tag) == other.selects(tag) &&
           focus_.identity() == other.focus_.identity() &&
           sidebar_ == other.sidebar_;
  }

  auto row(composition::Element body,
           std::shared_ptr<const composition::TagValue> tag) const
      -> composition::Element;

  friend auto operator==(const RowSelection& left, const RowSelection& right)
      -> bool {
    return left.selected_ == right.selected_ && left.chosen_ == right.chosen_ &&
           left.focus_.identity() == right.focus_.identity() &&
           left.sidebar_ == right.sidebar_;
  }

 private:
  void choose(const std::shared_ptr<const composition::TagValue>& tag,
              bool now) const;

  composition::TagKey selected_;
  std::vector<composition::TagKey> chosen_;
  List::Selection selection_;
  composition::FocusBinding<composition::TagKey> focus_;
  std::optional<SidebarSelection> sidebar_;
};

}
