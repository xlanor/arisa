#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/controls/text_editing_session.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::controls {

class FindNavigator : public composition::Element {
 public:
  class Content;

  using Range = composition::TextSelection::Range;

  static constexpr std::string_view find_label = "Find";
  static constexpr std::string_view replace_field_label = "Replace";
  static constexpr std::string_view previous_label = "Previous";
  static constexpr std::string_view next_label = "Next";
  static constexpr std::string_view done_label = "Done";
  static constexpr std::string_view replace_label = "Replace";
  static constexpr std::string_view replace_all_label = "All";

  FindNavigator(std::shared_ptr<TextEditingSession> session,
                composition::Binding<std::string> query,
                composition::Binding<bool> presented, bool replaces);

  static auto matches(std::string_view text, std::string_view query)
      -> std::vector<Range>;

  static auto status(std::size_t current, std::size_t count) -> std::string;
};

class FindNavigator::Content final : public composition::ElementContent {
 public:
  static const composition::StateKey<std::size_t> current;
  static const composition::StateKey<std::string> replacement;

  Content(std::shared_ptr<TextEditingSession> session,
          composition::Binding<std::string> query,
          composition::Binding<bool> presented, bool replaces)
      : session_(std::move(session)),
        query_(std::move(query)),
        presented_(std::move(presented)),
        replaces_(replaces) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::shared_ptr<TextEditingSession> session_;
  composition::Binding<std::string> query_;
  composition::Binding<bool> presented_;
  bool replaces_{false};
};

}
