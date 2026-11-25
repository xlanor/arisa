#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::presentation {

class Inspector : public composition::Element {
 public:
  class Content;

  using Builder = std::function<composition::Element()>;

  static constexpr std::string_view close_label = "Close";
  static constexpr float divider_width = 1.0F;

  Inspector(composition::Element content,
            composition::Binding<bool> is_presented, Builder build);

  static constexpr composition::StateKey<std::optional<float>> dragged{
      "arisa.inspector.dragged", std::nullopt};
  static constexpr composition::StateKey<std::optional<float>> anchor{
      "arisa.inspector.anchor", std::nullopt};

  static auto column(composition::Element content,
                     composition::Binding<bool> is_presented)
      -> composition::Element;

  static auto column(composition::Element content,
                     composition::Binding<bool> is_presented,
                     composition::StateHandle<std::optional<float>> width,
                     composition::StateHandle<std::optional<float>> held)
      -> composition::Element;
};

class Inspector::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source presenter, composition::Binding<bool> is_presented,
          Builder build) noexcept
      : presenter_(std::move(presenter)),
        is_presented_(std::move(is_presented)),
        build_(std::move(build)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  void hold(Source presenter) noexcept { presenter_ = std::move(presenter); }

 private:
  Source presenter_;
  composition::Binding<bool> is_presented_;
  Builder build_;
};

}

namespace arisa::composition {

template <class Content>
auto Element::inspector(Binding<bool> is_presented,
                        Content content) && -> Element {
  return presentation::Inspector(
      std::move(*this), std::move(is_presented),
      [held = std::make_shared<Content>(std::move(content))] {
        return Element{std::invoke(*held)};
      });
}

}
