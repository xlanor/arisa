#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/decoration.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/state_key.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::composition {

template <class T>
class PreferenceReader : public Element {
 public:
  using Build = std::function<Element(const T&)>;

  class Content;

  PreferenceReader(const PreferenceKey<T>& key, Alignment alignment,
                   bool overlay, Element content, Build build);

  static auto demanded(const PreferenceKey<T>& key, Element content)
      -> Element {
    if (key.demand() == nullptr) {
      return content;
    }
    return std::move(content).environment(*key.demand(), true);
  }
};

template <class T>
class PreferenceReader<T>::Content final : public ElementContent {
 public:
  inline static const StateKey<std::optional<T>> resolved{"preference-reader",
                                                          std::nullopt};

  Content(const PreferenceKey<T>& key, Alignment alignment, bool overlay,
          std::shared_ptr<PlaceholderContentView::Source> source,
          std::shared_ptr<const Build> build) noexcept
      : key_(key),
        alignment_(alignment),
        overlay_(overlay),
        source_(std::move(source)),
        build_(std::move(build)) {}

  auto type() const noexcept -> ElementType override {
    return ElementType::of<Content>();
  }

  auto composes() const noexcept -> bool override { return true; }

  auto body(Context& context) const -> std::vector<Element> override {
    const StateHandle<std::optional<T>> held = context.state(resolved);
    const std::optional<T>& found = held.get();
    const T& value = found.has_value() ? *found : key_.get().default_value();
    Element base{PlaceholderContentView(source_)};
    Element extra = (*build_)(value);
    std::vector<Element> produced;
    produced.push_back(
        overlay_ ? std::move(base).overlay(alignment_, std::move(extra))
                 : std::move(base).background(alignment_, std::move(extra)));
    return produced;
  }

  auto reads_preferences() const noexcept -> bool override { return true; }

  void hold(Element content) {
    *source_ = PlaceholderContentView::Source{std::move(content)};
  }

  void resolve(Context& context, const Node& node) override {
    if (node.child_count() == 0) {
      return;
    }
    const auto top = node.children();
    const Node& decoration = *top.begin();
    const auto placed = decoration.content_as<Decoration>();
    if (!placed.has_value() ||
        decoration.child_count() <= placed->get().base()) {
      return;
    }
    const auto inside = decoration.children();
    const Node& base = *std::ranges::next(
        inside.begin(), static_cast<std::ptrdiff_t>(placed->get().base()));
    context.state(resolved).set(
        base.preference(key_.get()).value_or(key_.get().default_value()));
  }

 private:
  std::reference_wrapper<const PreferenceKey<T>> key_;
  Alignment alignment_;
  bool overlay_;
  std::shared_ptr<PlaceholderContentView::Source> source_;
  std::shared_ptr<const Build> build_;
};

template <class T>
PreferenceReader<T>::PreferenceReader(const PreferenceKey<T>& key,
                                      Alignment alignment, bool overlay,
                                      Element content, Build build)
    : Element(std::make_unique<Content>(
          key, alignment, overlay,
          std::make_shared<PlaceholderContentView::Source>(),
          std::make_shared<const Build>(std::move(build)))) {
  inherit(*this, content);
  edit<Content>().hold(std::move(content));
}

template <class T, class Build>
  requires std::equality_comparable<T>
auto Element::overlay_preference_value(const PreferenceKey<T>& key,
                                       Build build) && -> Element {
  return std::move(*this).overlay_preference_value(key, Alignment::center,
                                                   std::move(build));
}

template <class T, class Build>
  requires std::equality_comparable<T>
auto Element::overlay_preference_value(const PreferenceKey<T>& key,
                                       Alignment alignment,
                                       Build build) && -> Element {
  return PreferenceReader<T>{
      key, alignment, true,
      PreferenceReader<T>::demanded(key, std::move(*this)),
      typename PreferenceReader<T>::Build{
          [build = std::move(build)](const T& value) -> Element {
            return Element{std::invoke(build, value)};
          }}};
}

template <class T, class Build>
  requires std::equality_comparable<T>
auto Element::background_preference_value(const PreferenceKey<T>& key,
                                          Build build) && -> Element {
  return std::move(*this).background_preference_value(key, Alignment::center,
                                                      std::move(build));
}

template <class T, class Build>
  requires std::equality_comparable<T>
auto Element::background_preference_value(const PreferenceKey<T>& key,
                                          Alignment alignment,
                                          Build build) && -> Element {
  return PreferenceReader<T>{
      key, alignment, false,
      PreferenceReader<T>::demanded(key, std::move(*this)),
      typename PreferenceReader<T>::Build{
          [build = std::move(build)](const T& value) -> Element {
            return Element{std::invoke(build, value)};
          }}};
}

}
