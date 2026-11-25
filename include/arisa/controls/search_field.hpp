#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/submission.hpp>
#include <arisa/controls/text_editing_session.hpp>
#include <arisa/provider/text_input/provider.hpp>
#include <arisa/runtime/text_input.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::controls {

class SearchField : public composition::Element {
 public:
  class Content;

  struct Chips {
    std::function<std::size_t()> count;
    std::function<composition::Element(std::size_t)> label;
    std::function<void(std::size_t)> remove;
  };

  struct Options {
    std::string prompt;
    std::function<void()> began;
    std::function<void()> cleared;
    std::optional<Chips> chips;
  };

  static constexpr std::string_view default_prompt = "Search";
  static constexpr std::string_view clear_label = "Clear";
  static constexpr std::string_view symbol = "search";
  static constexpr std::string_view clear_symbol = "cancel";

  SearchField(composition::Binding<std::string> text, Options options);
};

class SearchField::Content final : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<int> requests{"search-field-requests",
                                                       0};

  Content(composition::Binding<std::string> text, Options options) noexcept
      : text_(std::move(text)),
        options_(std::move(options)),
        session_(std::make_shared<TextEditingSession>()) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto activate() -> bool override;

  auto activatable() const noexcept -> bool override { return true; }

  auto press(composition::Context& context, const composition::Node& node,
             const composition::KeyPress& press) -> bool override;

  auto steer(composition::Context& context, const composition::Node& node,
             composition::MoveCommandDirection direction) -> bool override;

  auto session() const noexcept -> const std::shared_ptr<TextEditingSession>& {
    return session_;
  }

 private:
  static auto edit(int request, runtime::TextInput input,
                   provider::TextInputRequest details,
                   composition::Binding<std::string> text,
                   composition::Submission submit) -> async::Task<void>;

  auto chips() const -> std::vector<composition::Element>;

  auto presented(composition::Context& context, const std::string& value) const
      -> composition::Element;

  composition::Binding<std::string> text_;
  Options options_;
  std::shared_ptr<TextEditingSession> session_;
  mutable std::optional<composition::StateHandle<int>> request_;
};

}
