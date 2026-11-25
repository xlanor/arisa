#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/text_input/provider.hpp>
#include <arisa/runtime/text_input.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::controls {

class TextFieldLink : public composition::Element {
 public:
  class Content;

  using OnSubmit = std::function<void(std::string)>;

  TextFieldLink(std::string title, std::optional<std::string> prompt,
                OnSubmit on_submit);

  TextFieldLink(std::optional<std::string> prompt, composition::Element label,
                OnSubmit on_submit);
};

class TextFieldLink::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<int> requested{
      "text-field-link-requested", 0};

  Content(std::string title, std::optional<std::string> prompt, Source label,
          OnSubmit on_submit) noexcept
      : title_(std::move(title)),
        prompt_(std::move(prompt)),
        label_(std::move(label)),
        on_submit_(std::move(on_submit)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  static auto ask(int request, runtime::TextInput input,
                  provider::TextInputRequest details, OnSubmit on_submit)
      -> async::Task<void>;

  std::string title_;
  std::optional<std::string> prompt_;
  Source label_;
  OnSubmit on_submit_;
};

}
