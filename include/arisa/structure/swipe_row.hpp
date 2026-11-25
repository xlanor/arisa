#pragma once

#include <arisa/components/button_style.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/horizontal_edge.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/swipe_host.hpp>
#include <arisa/theme/colour.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::structure {

class SwipeRow : public composition::Element {
 public:
  class Content;
  class Host;

  enum class Reveal : std::uint8_t {
    closed,
    leading,
    trailing,
  };

  struct ActionStyle {
    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> composition::Element;

    friend auto operator==(const ActionStyle&, const ActionStyle&)
        -> bool = default;
  };

  static constexpr float action_width = 74.0F;
  static constexpr float full_swipe_fraction = 0.6F;
  static constexpr float open_fraction = 0.5F;

  SwipeRow(composition::Element body,
           composition::ListRow::SwipeActions leading,
           composition::ListRow::SwipeActions trailing);

  static auto host() noexcept -> const composition::SwipeHost*;

  static auto hosted(composition::Element row,
                     const composition::ListRow& attributes)
      -> composition::Element;
};

class SwipeRow::Host final : public composition::SwipeHost {
 public:
  static const Host shared;

  auto wrap(composition::Element row,
            const composition::ListRow& attributes) const
      -> composition::Element override;
};

class SwipeRow::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<Reveal> revealed{"swipe-revealed",
                                                          Reveal::closed};
  static constexpr composition::StateKey<float> dragged{"swipe-dragged", 0.0F};
  static constexpr composition::StateKey<float> width{"swipe-width", 0.0F};
  static constexpr composition::StateKey<std::size_t> chosen{"swipe-chosen", 0};

  Content(Source body, composition::ListRow::SwipeActions leading,
          composition::ListRow::SwipeActions trailing) noexcept
      : body_(std::move(body)),
        leading_(std::move(leading)),
        trailing_(std::move(trailing)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto actions(composition::HorizontalEdge edge) const
      -> std::vector<composition::Element>;

  auto edge_of(Reveal reveal) const noexcept
      -> const composition::ListRow::SwipeActions&;

  auto strip(Reveal reveal, composition::StateHandle<Reveal> open,
             std::optional<std::size_t> highlighted,
             theme::Colour untinted) const -> composition::Element;

  auto opening() const noexcept -> Reveal;

  static auto members_of(const composition::ListRow::SwipeActions& side)
      -> std::vector<composition::Element>;

  static void perform(std::vector<composition::Element>& members,
                      std::size_t index);

  static auto label_of(std::vector<composition::Element>& members,
                       std::size_t index) -> std::string;

  Source body_;
  composition::ListRow::SwipeActions leading_;
  composition::ListRow::SwipeActions trailing_;
};

}

namespace arisa::composition {

template <class Host>
auto Element::swipe_actions_container() && -> Element {
  return std::move(*this)
      .environment(ListEnvironment::swipe_actions_container, true)
      .environment(ListEnvironment::swipe_host,
                   static_cast<const Host*>(structure::SwipeRow::host()));
}

}
