#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/structure/outline_group.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

class List : public composition::Element {
 public:
  class Content;

  struct Marker {
    std::optional<float> spacing;
    std::optional<std::string> index_label;
  };

  struct Selection {
    std::function<composition::TagKey()> current;
    std::function<void(const composition::TagValue&)> select;
    std::function<std::vector<composition::TagKey>()> members{};
    std::function<void(const composition::TagValue&)> toggle{};
  };

  static const composition::FocusState<composition::TagKey> row_focus;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit List(Children&&... children)
      : List(std::optional<ListStyle>{},
             collect(std::forward<Children>(children)...)) {}

  template <composition::BindingSource Source, class... Children>
    requires std::equality_comparable<composition::BoundValue<Source>> &&
             std::copy_constructible<composition::BoundValue<Source>> &&
             (composition::ViewContent<Children> && ...)
  explicit List(Source selection, Children&&... children)
      : List(bind(composition::Binding<composition::BoundValue<Source>>{
                 std::move(selection)}),
             collect(std::forward<Children>(children)...)) {}

  template <std::ranges::input_range Items, class KeyOf, class ChildrenOf,
            class Build>
    requires std::invocable<const KeyOf&,
                            const std::ranges::range_value_t<Items>&> &&
             std::invocable<const ChildrenOf&,
                            const std::ranges::range_value_t<Items>&> &&
             std::invocable<const Build&,
                            const std::ranges::range_value_t<Items>&>
  List(Items&& roots, KeyOf key_of, ChildrenOf children_of, Build build)
      : List(collect(composition::Element{
            OutlineGroup(std::forward<Items>(roots), std::move(key_of),
                         std::move(children_of), std::move(build))})) {}

  explicit List(std::vector<composition::Element> children);

  List(Selection selection, std::vector<composition::Element> children);

 protected:
  List(std::optional<ListStyle> forced,
       std::vector<composition::Element> children);

 private:
  template <class T>
  static auto bind(composition::Binding<T> selection) -> Selection {
    return Selection{
        .current =
            [selection] { return composition::TagKey::of(selection.get()); },
        .select =
            [selection](const composition::TagValue& tag) {
              if (const T* value = tag.get<T>()) {
                selection.set(*value);
              }
            }};
  }

  template <class T>
  static auto bind(composition::Binding<std::optional<T>> selection)
      -> Selection {
    return Selection{.current =
                         [selection] {
                           const std::optional<T> value = selection.get();
                           return value.has_value()
                                      ? composition::TagKey::of(*value)
                                      : composition::TagKey{};
                         },
                     .select =
                         [selection](const composition::TagValue& tag) {
                           if (const T* value = tag.get<T>()) {
                             selection.set(*value);
                           }
                         }};
  }

  template <class T>
  static auto bind(composition::Binding<std::set<T>> selection) -> Selection {
    return Selection{.current = [] { return composition::TagKey{}; },
                     .select =
                         [selection](const composition::TagValue& tag) {
                           if (const T* value = tag.get<T>()) {
                             selection.set(std::set<T>{*value});
                           }
                         },
                     .members =
                         [selection] {
                           std::vector<composition::TagKey> keys;
                           for (const T& value : selection.get()) {
                             keys.push_back(composition::TagKey::of(value));
                           }
                           return keys;
                         },
                     .toggle =
                         [selection](const composition::TagValue& tag) {
                           if (const T* value = tag.get<T>()) {
                             selection.update([value](std::set<T>& chosen) {
                               if (chosen.erase(*value) == 0) {
                                 chosen.insert(*value);
                               }
                             });
                           }
                         }};
  }

  static auto sections(std::vector<composition::Element> children)
      -> std::vector<
          std::shared_ptr<composition::PlaceholderContentView::Source>>;

  static auto markers(const std::vector<composition::Element>& children)
      -> std::vector<Marker>;

  static auto describe(std::optional<ListStyle> forced,
                       std::optional<Selection> selection,
                       std::vector<composition::Element> children)
      -> std::unique_ptr<Content>;
};

class List::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(std::optional<ListStyle> forced, std::optional<Selection> selection,
          std::vector<Source> sections, std::vector<Marker> markers) noexcept
      : forced_(forced),
        selection_(std::move(selection)),
        sections_(std::move(sections)),
        markers_(std::move(markers)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto section_count() const noexcept -> std::size_t {
    return sections_.size();
  }

  void set_form() noexcept { form_ = true; }

  static constexpr composition::StateKey<float> label_width{"form-label-width",
                                                            0.0F};
  static constexpr float border_width = 1.0F;

 private:
  auto columns(composition::Context& context) const -> composition::Element;

  auto selecting(composition::Context& context,
                 composition::Element scroller) const -> composition::Element;

  auto indexed(composition::Context& context) const -> bool;

  std::optional<ListStyle> forced_;
  std::optional<Selection> selection_;
  std::vector<Source> sections_;
  std::vector<Marker> markers_;
  bool form_{false};
};

}
