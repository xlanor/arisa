#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/text_input_environment.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class TextInputSuggestions : public composition::Element {
 public:
  class Content;

  static constexpr std::string_view list_key = "text-input-suggestions";

  TextInputSuggestions(composition::Element field,
                       composition::TextInputEnvironment::Builder build);

  static auto applied(composition::Element field,
                      composition::TextInputEnvironment::Builder build)
      -> composition::Element;
};

class TextInputSuggestions::Content final : public composition::ElementContent {
 public:
  Content(composition::Element field,
          composition::TextInputEnvironment::Builder build);

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::shared_ptr<composition::PlaceholderContentView::Source> field_;
  composition::TextInputEnvironment::Builder build_;
};

class TextInputCompletion : public composition::Element {
 public:
  class Content;

  static constexpr std::string_view completion_key = "arisa.completion.";

  TextInputCompletion(composition::Element row, std::string completion);
};

class TextInputCompletion::Content final : public composition::ElementContent {
 public:
  Content(composition::Element row, std::string completion);

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::shared_ptr<composition::PlaceholderContentView::Source> row_;
  std::string completion_;
};

}

namespace arisa::composition {

template <class Content>
auto Element::text_input_suggestions(Content content) && -> Element {
  return controls::TextInputSuggestions::applied(
      std::move(*this),
      [content = std::move(content)] { return Element{std::invoke(content)}; });
}

template <class Data, class Build>
auto Element::text_input_suggestions(Data data, Build build) && -> Element {
  return controls::TextInputSuggestions::applied(
      std::move(*this), [data = std::move(data), build = std::move(build)] {
        std::vector<Element> rows;
        for (const auto& item : data) {
          rows.push_back(Element{std::invoke(build, item)});
        }
        return Element{components::VStack(
            components::VStackOptions{
                .alignment = components::HorizontalAlignment::leading},
            std::move(rows))};
      });
}

template <class Data, class Id, class Build>
auto Element::text_input_suggestions(Data data, Id id,
                                     Build build) && -> Element {
  return controls::TextInputSuggestions::applied(
      std::move(*this),
      [data = std::move(data), id = std::move(id), build = std::move(build)] {
        return Element{components::VStack(
            components::VStackOptions{
                .alignment = components::HorizontalAlignment::leading},
            components::ForEach(
                data,
                [id](const auto& item) {
                  return ElementKey{std::invoke(id, item)};
                },
                build))};
      });
}

template <class Completion>
auto Element::text_input_completion(Completion completion) && -> Element {
  return controls::TextInputCompletion(std::move(*this),
                                       std::string{std::move(completion)});
}

}
