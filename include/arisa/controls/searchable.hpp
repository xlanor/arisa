#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/scroll_geometry_observer.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/search_field_placement.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class Searchable : public composition::Element {
 public:
  static constexpr float toolbar_width = 280.0F;

  class Content;
  class Completer;

  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  struct Tokens {
    std::function<std::size_t()> count;
    std::function<composition::Element(std::size_t)> label;
    std::function<void(std::size_t)> remove;
    std::function<void()> clear;
    std::function<bool(const composition::TagValue&)> add;
    std::function<std::size_t()> suggested_count;
    std::function<composition::Element(std::size_t)> suggested_label;
    std::function<void(std::size_t)> choose;
  };

  static constexpr std::string_view cancel_label = "Cancel";
  static constexpr std::string_view scope_title = "Scope";

  Searchable(composition::Element content,
             composition::Binding<std::string> text,
             composition::SearchFieldPlacement placement,
             std::optional<std::string> prompt,
             std::optional<Tokens> tokens = std::nullopt);

  auto presented(composition::Binding<bool> is_presented) && -> Searchable;

  class Tracked;
  class Drawer;

  static constexpr float drawer_reveal = 36.0F;

  template <class Token, class Label>
  static auto editable(composition::Binding<std::vector<Token>> chosen,
                       Label label) -> Tokens {
    auto shared = std::make_shared<const Label>(std::move(label));
    return Tokens{
        .count = [chosen] { return chosen.get().size(); },
        .label =
            [chosen, shared](std::size_t index) {
              const composition::Binding<Token> each{
                  [chosen, index] { return chosen.get().at(index); },
                  [chosen, index](Token value) {
                    chosen.update([index, &value](std::vector<Token>& all) {
                      if (index < all.size()) {
                        all[index] = std::move(value);
                      }
                    });
                  }};
              return composition::Element{std::invoke(*shared, each)};
            },
        .remove =
            [chosen](std::size_t index) {
              chosen.update([index](std::vector<Token>& all) {
                if (index < all.size()) {
                  all.erase(all.begin() + static_cast<std::ptrdiff_t>(index));
                }
              });
            },
        .clear = [chosen] { chosen.set(std::vector<Token>{}); },
        .add =
            [chosen](const composition::TagValue& tag) {
              const Token* token = tag.get<Token>();
              if (token == nullptr) {
                return false;
              }
              chosen.update(
                  [token](std::vector<Token>& all) { all.push_back(*token); });
              return true;
            },
        .suggested_count = {},
        .suggested_label = {},
        .choose = {}};
  }

  template <class Token, class Label>
    requires std::equality_comparable<Token> && std::copy_constructible<Token>
  static auto tokens(
      composition::Binding<std::vector<Token>> chosen,
      std::optional<composition::Binding<std::vector<Token>>> suggested,
      Label label) -> Tokens {
    auto shared = std::make_shared<const Label>(std::move(label));
    Tokens made{
        .count = [chosen] { return chosen.get().size(); },
        .label =
            [chosen, shared](std::size_t index) {
              const std::vector<Token> all = chosen.get();
              return composition::Element{std::invoke(*shared, all.at(index))};
            },
        .remove =
            [chosen](std::size_t index) {
              chosen.update([index](std::vector<Token>& all) {
                if (index < all.size()) {
                  all.erase(all.begin() + static_cast<std::ptrdiff_t>(index));
                }
              });
            },
        .clear = [chosen] { chosen.set(std::vector<Token>{}); },
        .add =
            [chosen](const composition::TagValue& tag) {
              const Token* token = tag.get<Token>();
              if (token == nullptr) {
                return false;
              }
              chosen.update([token](std::vector<Token>& all) {
                if (std::ranges::find(all, *token) == all.end()) {
                  all.push_back(*token);
                }
              });
              return true;
            },
        .suggested_count = {},
        .suggested_label = {},
        .choose = {}};
    if (suggested.has_value()) {
      const auto offered = [chosen, suggested = *suggested] {
        std::vector<Token> left;
        const std::vector<Token> taken = chosen.get();
        for (const Token& token : suggested.get()) {
          if (std::ranges::find(taken, token) == taken.end()) {
            left.push_back(token);
          }
        }
        return left;
      };
      made.suggested_count = [offered] { return offered().size(); };
      made.suggested_label = [offered, shared](std::size_t index) {
        const std::vector<Token> left = offered();
        return composition::Element{std::invoke(*shared, left.at(index))};
      };
      made.choose = [offered, chosen](std::size_t index) {
        const std::vector<Token> left = offered();
        if (index < left.size()) {
          chosen.update([&left, index](std::vector<Token>& all) {
            all.push_back(left[index]);
          });
        }
      };
    }
    return made;
  }

  template <class Scope, class Scopes>
  static auto scopes(composition::Binding<Scope> scope, Scopes scopes,
                     composition::SearchScopeActivation activation)
      -> composition::SearchEnvironment::Scopes {
    return composition::SearchEnvironment::Scopes{
        .picker =
            [scope, held = std::make_shared<const Scopes>(std::move(scopes))] {
              return composition::Element{
                  Picker(std::string{scope_title}, scope, [held] {
                    return composition::Element{std::invoke(*held)};
                  }).picker_style(PickerStyle::segmented())};
            },
        .activation = activation};
  }

  template <class Completion>
  static auto completion(composition::Element label, Completion value)
      -> composition::Element {
    if constexpr (std::convertible_to<Completion, std::string>) {
      return complete_text(std::move(label), std::string{std::move(value)});
    } else {
      return complete_token(std::move(label),
                            composition::TagValue::of(std::move(value)));
    }
  }

 private:
  explicit Searchable(std::unique_ptr<Completer> completer);

  static auto complete_text(composition::Element label, std::string text)
      -> composition::Element;

  static auto complete_token(composition::Element label,
                             std::shared_ptr<const composition::TagValue> token)
      -> composition::Element;
};

class Searchable::Tracked : public composition::Element {
 public:
  Tracked(composition::Element content,
          std::shared_ptr<composition::ScrollGeometryObserver> observer);
};

class Searchable::Drawer final : public composition::ScrollGeometryObserver {
 public:
  explicit Drawer(composition::StateHandle<bool> hidden) noexcept
      : hidden_(std::move(hidden)) {}

  auto differs(const composition::ScrollGeometry& before,
               const composition::ScrollGeometry& after) const -> bool override;

  void notify(const composition::ScrollGeometry& before,
              const composition::ScrollGeometry& after) override;

 private:
  static auto concealed(const composition::ScrollGeometry& geometry) noexcept
      -> bool;

  composition::StateHandle<bool> hidden_;
};

class Searchable::Completer final : public composition::ElementContent {
 public:
  Completer(Source label, std::optional<std::string> text,
            std::shared_ptr<const composition::TagValue> token) noexcept
      : label_(std::move(label)),
        text_(std::move(text)),
        token_(std::move(token)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  std::optional<std::string> text_;
  std::shared_ptr<const composition::TagValue> token_;
};

class Searchable::Content final : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<bool> searching{"searchable-searching",
                                                         false};
  static constexpr composition::StateKey<bool> drawer_hidden{
      "searchable-drawer-hidden", false};

  Content(Source content, composition::Binding<std::string> text,
          composition::SearchFieldPlacement placement,
          std::optional<std::string> prompt, std::optional<Tokens> tokens,
          bool hosts_sidebar) noexcept
      : content_(std::move(content)),
        text_(std::move(text)),
        placement_(placement),
        prompt_(std::move(prompt)),
        tokens_(std::move(tokens)),
        hosts_sidebar_(hosts_sidebar) {}

  void present_with(composition::Binding<bool> is_presented) {
    presented_ = std::move(is_presented);
  }

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto placement() const noexcept -> composition::SearchFieldPlacement {
    return placement_;
  }

 private:
  static auto scoped(composition::SearchScopeActivation activation, bool active,
                     bool typed) noexcept -> bool;

  auto suggestions(composition::Context& context, bool active,
                   composition::SearchSuggestionsPlacement mode) const
      -> std::optional<composition::Element>;

  auto offered() const -> std::optional<composition::Element>;

  Source content_;
  composition::Binding<std::string> text_;
  composition::SearchFieldPlacement placement_;
  std::optional<std::string> prompt_;
  std::optional<Tokens> tokens_;
  bool hosts_sidebar_;
  std::optional<composition::Binding<bool>> presented_;
};

}

namespace arisa::composition {

template <class Text>
auto Element::searchable(Text text, SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  return controls::Searchable(std::move(*this),
                              Binding<std::string>{std::move(text)}, placement,
                              std::move(prompt));
}

template <class Text>
auto Element::searchable(Text text, Binding<bool> is_presented,
                         SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  return controls::Searchable(std::move(*this),
                              Binding<std::string>{std::move(text)}, placement,
                              std::move(prompt))
      .presented(std::move(is_presented));
}

template <class Text, class Tokens, class Token>
  requires BindingSource<Tokens> &&
           requires { typename BoundValue<Tokens>::value_type; }
auto Element::searchable(Text text, Tokens tokens, Binding<bool> is_presented,
                         Token token, SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  using Value = typename BoundValue<Tokens>::value_type;
  return controls::Searchable(
             std::move(*this), Binding<std::string>{std::move(text)}, placement,
             std::move(prompt),
             controls::Searchable::tokens<Value>(
                 Binding<std::vector<Value>>{std::move(tokens)}, std::nullopt,
                 std::move(token)))
      .presented(std::move(is_presented));
}

template <class Text, class Tokens, class Suggested, class Token>
  requires BindingSource<Tokens> && BindingSource<Suggested> &&
           requires { typename BoundValue<Tokens>::value_type; } &&
           requires { typename BoundValue<Suggested>::value_type; }
auto Element::searchable(Text text, Tokens tokens, Suggested suggested_tokens,
                         Binding<bool> is_presented, Token token,
                         SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  using Value = typename BoundValue<Tokens>::value_type;
  return controls::Searchable(
             std::move(*this), Binding<std::string>{std::move(text)}, placement,
             std::move(prompt),
             controls::Searchable::tokens<Value>(
                 Binding<std::vector<Value>>{std::move(tokens)},
                 Binding<std::vector<Value>>{std::move(suggested_tokens)},
                 std::move(token)))
      .presented(std::move(is_presented));
}

template <class Text, class T, class Token>
auto Element::searchable(Text text, EditableTokens<T> editable_tokens,
                         Token token, SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  return controls::Searchable(std::move(*this),
                              Binding<std::string>{std::move(text)}, placement,
                              std::move(prompt),
                              controls::Searchable::editable<T>(
                                  editable_tokens.binding(), std::move(token)));
}

template <class Text, class T, class Token>
auto Element::searchable(Text text, EditableTokens<T> editable_tokens,
                         Binding<bool> is_presented, Token token,
                         SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  return controls::Searchable(std::move(*this),
                              Binding<std::string>{std::move(text)}, placement,
                              std::move(prompt),
                              controls::Searchable::editable<T>(
                                  editable_tokens.binding(), std::move(token)))
      .presented(std::move(is_presented));
}

template <class Text, class Tokens, class Token>
  requires BindingSource<Tokens> &&
           requires { typename BoundValue<Tokens>::value_type; }
auto Element::searchable(Text text, Tokens tokens, Token token,
                         SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  using Value = typename BoundValue<Tokens>::value_type;
  return controls::Searchable(
      std::move(*this), Binding<std::string>{std::move(text)}, placement,
      std::move(prompt),
      controls::Searchable::tokens<Value>(
          Binding<std::vector<Value>>{std::move(tokens)}, std::nullopt,
          std::move(token)));
}

template <class Text, class Tokens, class Suggested, class Token>
  requires BindingSource<Tokens> && BindingSource<Suggested> &&
           requires { typename BoundValue<Tokens>::value_type; } &&
           requires { typename BoundValue<Suggested>::value_type; }
auto Element::searchable(Text text, Tokens tokens, Suggested suggested_tokens,
                         Token token, SearchFieldPlacement placement,
                         std::optional<std::string> prompt) && -> Element {
  using Value = typename BoundValue<Tokens>::value_type;
  return controls::Searchable(
      std::move(*this), Binding<std::string>{std::move(text)}, placement,
      std::move(prompt),
      controls::Searchable::tokens<Value>(
          Binding<std::vector<Value>>{std::move(tokens)},
          Binding<std::vector<Value>>{std::move(suggested_tokens)},
          std::move(token)));
}

template <class Scope, class Scopes>
  requires BindingSource<Scope>
auto Element::search_scopes(Scope scope, Scopes scopes,
                            SearchScopeActivation activation) && -> Element {
  using Value = BoundValue<Scope>;
  return std::move(*this).environment(
      SearchEnvironment::scopes,
      std::make_shared<const SearchEnvironment::Scopes>(
          controls::Searchable::scopes(Binding<Value>{std::move(scope)},
                                       std::move(scopes), activation)));
}

template <class Completion>
auto Element::search_completion(Completion completion) && -> Element {
  return controls::Searchable::completion(std::move(*this),
                                          std::move(completion));
}

}
