#include <arisa/composition/focus_follow.hpp>
#include <arisa/composition/focus_request.hpp>
#include <arisa/structure/row_selection.hpp>

#include <algorithm>

namespace arisa::structure {

const composition::EnvironmentKey<std::optional<RowSelection>>
    RowSelection::key{"row_selection", std::nullopt};

auto RowSelection::selects(
    const std::shared_ptr<const composition::TagValue>& tag) const -> bool {
  if (tag == nullptr) {
    return false;
  }
  const composition::TagKey wanted{tag};
  if (multiple()) {
    return std::ranges::contains(chosen_, wanted);
  }
  return wanted == selected_;
}

void RowSelection::toggle(
    const std::shared_ptr<const composition::TagValue>& tag) const {
  if (tag != nullptr && selection_.toggle) {
    selection_.toggle(*tag);
  }
}

void RowSelection::choose(
    const std::shared_ptr<const composition::TagValue>& tag, bool now) const {
  if (selection_.select) {
    selection_.select(*tag);
  }
  if (now && sidebar_.has_value()) {
    sidebar_->present(composition::TagKey{tag});
  }
}

auto RowSelection::row(composition::Element body,
                       std::shared_ptr<const composition::TagValue> tag) const
    -> composition::Element {
  return std::move(body)
      .focused(std::make_unique<composition::FocusFollow>(
          composition::FocusRequest::of(focus_, composition::TagKey{tag}),
          [chooser = *this, tag] { chooser.choose(tag, false); }))
      .on_tap_gesture([chooser = *this, tag] { chooser.choose(tag, true); });
}

}
