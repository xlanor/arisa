#pragma once

#include <arisa/components/image.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/controls/picker_style.hpp>

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::controls {

class Picker : public composition::Element {
 public:
  class Content;

  using Builder = std::function<composition::Element()>;

  struct Selection {
    std::function<bool(const composition::TagValue&)> is_selected;
    std::function<void(const composition::TagValue&)> select;
    std::function<bool(const composition::TagValue&)> is_mixed{};
  };

  template <composition::BindingSource Source>
    requires std::equality_comparable<composition::BoundValue<Source>> &&
             std::copy_constructible<composition::BoundValue<Source>>
  Picker(std::string title, Source source, Builder content)
      : Picker(std::move(title),
               composition::Binding<composition::BoundValue<Source>>{
                   std::move(source)},
               std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::string title, composition::Binding<T> selection, Builder content)
      : Picker(std::move(title), chosen(std::move(selection)),
               std::move(content)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(composition::Binding<T> selection, Builder content,
         composition::Element label)
      : Picker(std::string{}, chosen(std::move(selection)), std::move(content),
               std::optional<composition::Element>{std::move(label)}) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::string title, components::Image::SystemName system_image,
         composition::Binding<T> selection, Builder content)
      : Picker(std::move(selection), std::move(content),
               labelled(std::move(title), std::move(system_image))) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::string title, components::Image::Resource image,
         composition::Binding<T> selection, Builder content)
      : Picker(std::move(selection), std::move(content),
               labelled(std::move(title), std::move(image))) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::string title, std::vector<composition::Binding<T>> sources,
         Builder content)
      : Picker(std::move(title), gathered(std::move(sources)),
               std::move(content), std::nullopt) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::string title, components::Image::SystemName system_image,
         std::vector<composition::Binding<T>> sources, Builder content)
      : Picker(std::string{}, gathered(std::move(sources)), std::move(content),
               std::optional<composition::Element>{
                   labelled(std::move(title), std::move(system_image))}) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::string title, components::Image::Resource image,
         std::vector<composition::Binding<T>> sources, Builder content)
      : Picker(std::string{}, gathered(std::move(sources)), std::move(content),
               std::optional<composition::Element>{
                   labelled(std::move(title), std::move(image))}) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  Picker(std::vector<composition::Binding<T>> sources, Builder content,
         composition::Element label)
      : Picker(std::string{}, gathered(std::move(sources)), std::move(content),
               std::optional<composition::Element>{std::move(label)}) {}

  static auto options(const Builder& content, const Selection& selection)
      -> std::vector<PickerStyle::Option>;

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  static auto chosen(composition::Binding<T> selection) -> Selection {
    return Selection{.is_selected =
                         [selection](const composition::TagValue& tag) {
                           const T* value = tag.get<T>();
                           return value != nullptr && *value == selection.get();
                         },
                     .select =
                         [selection](const composition::TagValue& tag) {
                           if (const T* value = tag.get<T>()) {
                             selection.set(*value);
                           }
                         },
                     .is_mixed = {}};
  }

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  static auto gathered(std::vector<composition::Binding<T>> sources)
      -> Selection {
    auto shared = std::make_shared<const std::vector<composition::Binding<T>>>(
        std::move(sources));
    const auto matches = [shared](const composition::TagValue& tag) {
      const T* value = tag.get<T>();
      return value == nullptr
                 ? std::size_t{0}
                 : static_cast<std::size_t>(std::ranges::count_if(
                       *shared, [value](const composition::Binding<T>& source) {
                         return source.get() == *value;
                       }));
    };
    return Selection{
        .is_selected =
            [shared, matches](const composition::TagValue& tag) {
              return !shared->empty() && matches(tag) == shared->size();
            },
        .select =
            [shared](const composition::TagValue& tag) {
              if (const T* value = tag.get<T>()) {
                for (const composition::Binding<T>& source : *shared) {
                  if (!(source.get() == *value)) {
                    source.set(*value);
                  }
                }
              }
            },
        .is_mixed =
            [shared, matches](const composition::TagValue& tag) {
              const std::size_t found = matches(tag);
              return found > 0 && found < shared->size();
            }};
  }

  static auto labelled(std::string title,
                       components::Image::SystemName system_image)
      -> composition::Element;

  static auto labelled(std::string title, components::Image::Resource image)
      -> composition::Element;

 private:
  Picker(std::string title, Selection selection, Builder content);

  Picker(std::string title, Selection selection, Builder content,
         std::optional<composition::Element> label);
};

class Picker::Content final : public composition::ElementContent {
 public:
  static constexpr composition::StateKey<std::int64_t> changes{"picker-changes",
                                                               0};

  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(std::string title, Selection selection, Builder content,
          Source label = nullptr) noexcept
      : title_(std::move(title)),
        selection_(std::move(selection)),
        content_(std::move(content)),
        label_(std::move(label)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  static auto counted(const Selection& selection,
                      composition::StateHandle<std::int64_t> changed)
      -> Selection;

  auto title_label() const -> composition::Element;

  std::string title_;
  Selection selection_;
  Builder content_;
  Source label_;
};

}
