#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/dismiss_search_action.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_request.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/search_scope_activation.hpp>
#include <arisa/composition/search_suggestions_placement.hpp>
#include <arisa/composition/search_toolbar_behavior.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/composition/visibility.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::composition {

class SearchEnvironment {
 public:
  using Builder = std::function<Element()>;

  struct Scopes {
    Builder picker;
    SearchScopeActivation activation{SearchScopeActivation::automatic};
  };

  class Completion {
   public:
    using Text = std::function<void(const std::string&)>;
    using Token = std::function<void(const TagValue&)>;

    Completion() = default;

    Completion(Identity source, Text text, Token token)
        : source_(source), text_(std::move(text)), token_(std::move(token)) {}

    void complete(const std::string& text) const {
      if (text_) {
        text_(text);
      }
    }

    void complete(const TagValue& token) const {
      if (token_) {
        token_(token);
      }
    }

    friend auto operator==(const Completion& left, const Completion& right)
        -> bool {
      return left.source_ == right.source_;
    }

   private:
    std::optional<Identity> source_;
    Text text_;
    Token token_;
  };

  class Activation {
   public:
    Activation() = default;

    Activation(Identity source, std::function<void()> dismissed)
        : source_(source), dismissed_(std::move(dismissed)) {}

    auto active() const noexcept -> bool { return source_.has_value(); }

    void dismissed() const {
      if (dismissed_) {
        dismissed_();
      }
    }

    friend auto operator==(const Activation& left, const Activation& right)
        -> bool {
      return left.source_ == right.source_;
    }

   private:
    std::optional<Identity> source_;
    std::function<void()> dismissed_;
  };

  struct SuggestionVisibility {
    std::optional<Visibility> menu;
    std::optional<Visibility> content;

    auto allows(SearchSuggestionsPlacement placement) const noexcept -> bool {
      const std::optional<Visibility>& chosen =
          placement == SearchSuggestionsPlacement::menu ? menu : content;
      return chosen.value_or(Visibility::automatic) != Visibility::hidden;
    }

    static auto merged(const SuggestionVisibility& inherited,
                       const SuggestionVisibility& declared)
        -> SuggestionVisibility {
      return SuggestionVisibility{
          .menu = declared.menu.has_value() ? declared.menu : inherited.menu,
          .content = declared.content.has_value() ? declared.content
                                                  : inherited.content};
    }

    friend auto operator==(const SuggestionVisibility&,
                           const SuggestionVisibility&) -> bool = default;
  };

  static const EnvironmentKey<bool> is_searching;
  static const EnvironmentKey<Activation> activation;
  static const EnvironmentKey<SuggestionVisibility> suggestion_visibility;
  static const EnvironmentKey<SearchSuggestionsPlacement> suggestions_placement;
  static const EnvironmentKey<SearchPresentationToolbarBehavior>
      presentation_toolbar_behavior;
  static const EnvironmentKey<SearchToolbarBehavior> toolbar_behavior;
  static const EnvironmentKey<std::shared_ptr<const Builder>> sidebar_field;
  static const EnvironmentKey<DismissSearchAction> dismiss_search;
  using FocusTarget = std::function<std::unique_ptr<FocusRequest>()>;

  static const EnvironmentKey<std::shared_ptr<const FocusBinding<bool>>>
      focused;
  static const EnvironmentKey<std::shared_ptr<const FocusTarget>> focus_target;
  static const EnvironmentKey<std::shared_ptr<const Builder>> suggestions;
  static const EnvironmentKey<std::shared_ptr<const Scopes>> scopes;
  static const EnvironmentKey<Completion> completion;
  static const EnvironmentKey<
      std::shared_ptr<const Binding<std::optional<TextSelection>>>>
      selection;
};

template <std::equality_comparable T>
  requires(!std::same_as<T, bool>)
auto Element::search_focused(FocusBinding<T> focused,
                             std::type_identity_t<T> equals) && -> Element {
  return std::move(*this).environment(
      SearchEnvironment::focus_target,
      std::make_shared<const SearchEnvironment::FocusTarget>(
          [focused = std::move(focused), equals = std::move(equals)] {
            return FocusRequest::of(focused, equals);
          }));
}

template <class Suggestions>
auto Element::search_suggestions(Suggestions suggestions) && -> Element {
  return std::move(*this).environment(
      SearchEnvironment::suggestions,
      std::make_shared<const SearchEnvironment::Builder>(
          [held = std::move(suggestions)] {
            return Element{std::invoke(held)};
          }));
}

}
