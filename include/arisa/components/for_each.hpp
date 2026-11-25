#pragma once

#include <arisa/components/edit_actions.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/index_set.hpp>
#include <arisa/components/section_collection.hpp>
#include <arisa/components/subview.hpp>
#include <arisa/components/subviews_collection.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::components {

class ForEach : public composition::Element {
 public:
  class Content;
  class Source;
  class Reorderable;

  using MoveAction = std::function<void(const IndexSet&, std::size_t)>;
  using DeleteAction = std::function<void(const IndexSet&)>;

  struct Edits {
    MoveAction move;
    DeleteAction remove;
  };

  template <std::ranges::input_range Items, class KeyOf, class Build>
    requires std::convertible_to<
                 std::invoke_result_t<const KeyOf&,
                                      const std::ranges::range_value_t<Items>&>,
                 composition::ElementKey> &&
             std::convertible_to<
                 std::invoke_result_t<const Build&,
                                      const std::ranges::range_value_t<Items>&>,
                 composition::Element>
  ForEach(Items&& items, KeyOf key_of, Build build);

  template <class T, class KeyOf, class Build>
    requires std::copy_constructible<T> &&
             std::convertible_to<std::invoke_result_t<const KeyOf&, const T&>,
                                 composition::ElementKey> &&
             (std::invocable<const Build&, composition::Binding<T>> ||
              std::invocable<const Build&, const T&>)
  ForEach(composition::Binding<std::vector<T>> items, EditActions actions,
          KeyOf key_of, Build build);

  template <class Build>
    requires std::convertible_to<std::invoke_result_t<Build&, Subview>,
                                 composition::Element>
  ForEach(SubviewsCollection subviews, Build build)
      : Element(std::make_unique<Group::Content>(),
                built(std::move(subviews), build)) {}

  template <class Build>
    requires std::convertible_to<std::invoke_result_t<Build&, Subview>,
                                 composition::Element>
  ForEach(SubviewsCollectionSlice subviews, Build build)
      : Element(std::make_unique<Group::Content>(), built(subviews, build)) {}

  template <class Build>
    requires std::convertible_to<
        std::invoke_result_t<Build&, SectionConfiguration>,
        composition::Element>
  ForEach(SectionCollection sections, Build build)
      : Element(std::make_unique<Group::Content>(),
                built(std::move(sections), build)) {}

  auto on_move(MoveAction action) && -> ForEach;

  auto on_delete(DeleteAction action) && -> ForEach;

  auto reorderable() && -> ForEach;

  auto reorderable(composition::ElementKey collection_id) && -> ForEach;

  static auto with_move(composition::Element element, MoveAction action)
      -> composition::Element;

  static auto with_delete(composition::Element element, DeleteAction action)
      -> composition::Element;

  static auto edits_of(const composition::Element& element) -> Edits;

  template <class T>
  auto data() const -> std::optional<std::span<const T>>;

  template <class T>
  auto content() const
      -> std::optional<std::function<composition::Element(const T&)>>;

 private:
  auto source() const noexcept -> const Source*;

  static auto describe(std::shared_ptr<const Source> source)
      -> std::unique_ptr<composition::ElementContent>;

  template <class Build>
  static auto built(SubviewsCollection subviews, Build& build)
      -> std::vector<composition::Element> {
    return built(subviews.slice(0), build);
  }

  template <class Build>
  static auto built(SubviewsCollectionSlice subviews, Build& build)
      -> std::vector<composition::Element> {
    std::vector<composition::Element> children;
    children.reserve(subviews.size());
    for (Subview& subview : subviews) {
      composition::ElementKey key = subview.id();
      children.push_back(
          composition::Element{std::invoke(build, std::move(subview))}.id(
              std::move(key)));
    }
    return children;
  }

  template <class Build>
  static auto built(SectionCollection sections, Build& build)
      -> std::vector<composition::Element> {
    std::vector<composition::Element> children;
    children.reserve(sections.size());
    for (SectionConfiguration& section : sections) {
      composition::ElementKey key = section.id().key();
      children.push_back(
          composition::Element{std::invoke(build, std::move(section))}.id(
              std::move(key)));
    }
    return children;
  }

  template <class T, class KeyOf, class Build>
  static auto adapt(const composition::Binding<std::vector<T>>& items,
                    const KeyOf& key_of, Build build) {
    if constexpr (std::invocable<const Build&, composition::Binding<T>>) {
      return [items, key_of, build = std::move(build)](const T& value) {
        return composition::Element{
            std::invoke(build, element_binding(items, key_of, value))};
      };
    } else {
      return [build = std::move(build)](const T& value) {
        return composition::Element{std::invoke(build, value)};
      };
    }
  }

  template <class T, class KeyOf>
  static auto element_binding(const composition::Binding<std::vector<T>>& items,
                              const KeyOf& key_of, const T& value)
      -> composition::Binding<T> {
    const composition::ElementKey wanted{std::invoke(key_of, value)};
    return composition::Binding<T>{
        [items, key_of, wanted, value] {
          for (const T& each : items.get()) {
            if (composition::ElementKey{std::invoke(key_of, each)} == wanted) {
              return each;
            }
          }
          return value;
        },
        [items, key_of, wanted](T changed) {
          items.update([&](std::vector<T>& all) {
            for (T& each : all) {
              if (composition::ElementKey{std::invoke(key_of, each)} ==
                  wanted) {
                each = std::move(changed);
                return;
              }
            }
          });
        }};
  }
};

class ForEach::Source {
 public:
  template <class Item, class KeyOf, class Build>
  class Typed;

  Source() = default;
  Source(const Source&) = delete;
  auto operator=(const Source&) -> Source& = delete;
  Source(Source&&) = delete;
  auto operator=(Source&&) -> Source& = delete;
  virtual ~Source() = default;

  virtual auto count() const noexcept -> std::size_t = 0;

  virtual auto key(std::size_t index) const -> composition::ElementKey = 0;

  virtual auto build(std::size_t index) const -> composition::Element = 0;

  virtual auto same_item(std::size_t index, const Source& other,
                         std::size_t other_index) const -> bool {
    return &other == this && other_index == index;
  }

  auto element(std::size_t index) const -> composition::Element {
    return build(index).id(key(index));
  }

  virtual auto tag() const noexcept -> const void* { return nullptr; }

  virtual auto item_type() const noexcept -> composition::ElementType {
    return composition::ElementType::of<Source>();
  }

  virtual auto items() const noexcept -> const void* { return nullptr; }

  virtual auto builder() const -> std::shared_ptr<const void> {
    return nullptr;
  }
};

template <class Item, class KeyOf, class Build>
class ForEach::Source::Typed final : public ForEach::Source {
 public:
  Typed(std::vector<Item> items, KeyOf key_of, Build build)
      : items_(std::move(items)),
        key_of_(std::move(key_of)),
        build_(std::move(build)) {}

  auto count() const noexcept -> std::size_t override { return items_.size(); }

  auto key(std::size_t index) const -> composition::ElementKey override {
    return composition::ElementKey{std::invoke(key_of_, items_[index])};
  }

  auto build(std::size_t index) const -> composition::Element override {
    return composition::Element{std::invoke(build_, items_[index])};
  }

  auto same_item(std::size_t index, const Source& other,
                 std::size_t other_index) const -> bool override {
    if (&other == this) {
      return other_index == index;
    }
    if constexpr (std::equality_comparable<Item> &&
                  (std::is_empty_v<Build> || std::equality_comparable<Build>)) {
      if (other.tag() != tag() || index >= items_.size()) {
        return false;
      }
      const auto& typed = static_cast<const Typed&>(other);
      if (other_index >= typed.items_.size() ||
          !(typed.items_[other_index] == items_[index])) {
        return false;
      }
      if constexpr (std::is_empty_v<Build>) {
        return true;
      } else {
        return typed.build_ == build_;
      }
    } else {
      return false;
    }
  }

  auto tag() const noexcept -> const void* override { return &marker; }

  auto item_type() const noexcept -> composition::ElementType override {
    return composition::ElementType::of<Item>();
  }

  auto items() const noexcept -> const void* override { return &items_; }

  auto builder() const -> std::shared_ptr<const void> override {
    if constexpr (std::copy_constructible<Build>) {
      return std::make_shared<
          const std::function<composition::Element(const Item&)>>(
          [build = build_](const Item& item) {
            return composition::Element{std::invoke(build, item)};
          });
    } else {
      return nullptr;
    }
  }

 private:
  static constexpr char marker{};

  std::vector<Item> items_;
  KeyOf key_of_;
  Build build_;
};

class ForEach::Reorderable final : public ForEach::Source {
 public:
  Reorderable(std::shared_ptr<const Source> inner,
              std::optional<composition::ElementKey> collection) noexcept
      : inner_(std::move(inner)), collection_(std::move(collection)) {}

  auto count() const noexcept -> std::size_t override {
    return inner_->count();
  }

  auto key(std::size_t index) const -> composition::ElementKey override {
    return inner_->key(index);
  }

  auto build(std::size_t index) const -> composition::Element override;

  auto same_item(std::size_t index, const Source& other,
                 std::size_t other_index) const -> bool override;

  auto tag() const noexcept -> const void* override { return &marker; }

  auto item_type() const noexcept -> composition::ElementType override {
    return inner_->item_type();
  }

  auto items() const noexcept -> const void* override {
    return inner_->items();
  }

  auto builder() const -> std::shared_ptr<const void> override {
    return inner_->builder();
  }

 private:
  static constexpr char marker{};

  std::shared_ptr<const Source> inner_;
  std::optional<composition::ElementKey> collection_;
};

class ForEach::Content final : public composition::ElementContent {
 public:
  explicit Content(std::shared_ptr<const Source> source) noexcept
      : source_(std::move(source)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override;

  auto passes_through() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto source() const noexcept -> const std::shared_ptr<const Source>& {
    return source_;
  }

  auto edits() const noexcept -> const Edits& { return edits_; }

  void reorderable(std::optional<composition::ElementKey> collection);

  auto edits() noexcept -> Edits& { return edits_; }

 private:
  std::shared_ptr<const Source> source_;
  Edits edits_;
};

template <std::ranges::input_range Items, class KeyOf, class Build>
  requires std::convertible_to<
               std::invoke_result_t<const KeyOf&,
                                    const std::ranges::range_value_t<Items>&>,
               composition::ElementKey> &&
           std::convertible_to<
               std::invoke_result_t<const Build&,
                                    const std::ranges::range_value_t<Items>&>,
               composition::Element>
ForEach::ForEach(Items&& items, KeyOf key_of, Build build)
    : Element(describe(std::make_shared<const Source::Typed<
                           std::ranges::range_value_t<Items>, KeyOf, Build>>(
          std::forward<Items>(items) |
              std::ranges::to<std::vector<std::ranges::range_value_t<Items>>>(),
          std::move(key_of), std::move(build)))) {}

template <class T, class KeyOf, class Build>
  requires std::copy_constructible<T> &&
           std::convertible_to<std::invoke_result_t<const KeyOf&, const T&>,
                               composition::ElementKey> &&
           (std::invocable<const Build&, composition::Binding<T>> ||
            std::invocable<const Build&, const T&>)
ForEach::ForEach(composition::Binding<std::vector<T>> items,
                 EditActions actions, KeyOf key_of, Build build)
    : ForEach(items.get(), key_of, adapt(items, key_of, std::move(build))) {
  Edits& edits = edit<Content>().edits();
  if (actions.contains(EditActions::move)) {
    edits.move = [items](const IndexSet& from, std::size_t to) {
      items.update(
          [&from, to](std::vector<T>& all) { from.move_from(all, to); });
    };
  }
  if (actions.contains(EditActions::remove)) {
    edits.remove = [items](const IndexSet& offsets) {
      items.update(
          [&offsets](std::vector<T>& all) { offsets.remove_from(all); });
    };
  }
}

template <class T>
auto ForEach::data() const -> std::optional<std::span<const T>> {
  const Source* held = source();
  if (held == nullptr ||
      held->item_type() != composition::ElementType::of<T>()) {
    return std::nullopt;
  }
  return std::span<const T>{*static_cast<const std::vector<T>*>(held->items())};
}

template <class T>
auto ForEach::content() const
    -> std::optional<std::function<composition::Element(const T&)>> {
  const Source* held = source();
  if (held == nullptr ||
      held->item_type() != composition::ElementType::of<T>()) {
    return std::nullopt;
  }
  const std::shared_ptr<const void> build = held->builder();
  if (build == nullptr) {
    return std::nullopt;
  }
  return *static_cast<const std::function<composition::Element(const T&)>*>(
      build.get());
}

}
