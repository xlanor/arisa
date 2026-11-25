#pragma once

#include <arisa/components/command_mapping.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/composition/badge_prominence.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/sidebar_row_size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::structure {

class ListStyle;
class RowSelection;

class ListCell : public composition::Element {
 public:
  class Content;

  struct Placement {
    bool is_first{true};
    bool is_last{true};
    std::size_t index{0};
    std::size_t count{1};
    components::ForEach::Edits edits;
    composition::ListRow::Separator above{};
    std::size_t position{0};
    float indent{0.0F};
  };

  struct Traits {
    bool button{false};
    bool discloses{false};
    bool activatable{false};
    std::shared_ptr<const composition::TagValue> tag;
    std::shared_ptr<const composition::ListRow> row;
    controls::ContextMenu::Builder menu{};
    std::optional<controls::ContextMenu::Builder> menu_preview{};

    auto attributes() const noexcept -> const composition::ListRow& {
      return row != nullptr ? *row : none;
    }

    friend auto operator==(const Traits& left, const Traits& right) -> bool {
      return left.button == right.button && left.discloses == right.discloses &&
             left.activatable == right.activatable &&
             composition::TagKey{left.tag} == composition::TagKey{right.tag} &&
             !left.menu && !right.menu &&
             left.attributes() == right.attributes();
    }

   private:
    static const composition::ListRow none;
  };

  struct Origin {
    std::shared_ptr<const components::ForEach::Source> each;
    std::size_t index{0};
  };

  static constexpr provider::Button move_button =
      components::CommandMapping::move;
  static constexpr float focus_inset = 7.0F;
  static constexpr provider::Button delete_button =
      components::CommandMapping::remove;
  static constexpr float badge_gap = 8.0F;
  static constexpr float badge_extent = 17.0F;
  static constexpr float badge_padding = 5.0F;
  static constexpr float small_sidebar_row = 24.0F;
  static constexpr float large_sidebar_row = 40.0F;
  static constexpr composition::StateKey<bool> lifted{"list-cell-lifted",
                                                      false};
  static constexpr composition::StateKey<float> dragged{"list-cell-dragged",
                                                        0.0F};
  static constexpr composition::StateKey<float> extent{"list-cell-extent",
                                                       44.0F};
  static constexpr composition::StateKey<bool> menu_shown{"list-cell-menu",
                                                          false};

  ListCell(composition::Element content, Placement placement, Traits traits);

  ListCell(composition::Element content, Placement placement, Traits traits,
           Origin origin);

  static auto default_transition() noexcept -> motion::Transition;

  static auto targets(const std::optional<RowSelection>& selection,
                      const std::shared_ptr<const composition::TagValue>& tag)
      -> controls::ContextMenu::ForSelection::Tags;

  static auto minimum_height(const ListStyle& list,
                             composition::SidebarRowSize size,
                             std::optional<float> declared) noexcept
      -> std::optional<float>;

  static auto traits_of(const composition::Element& element) -> Traits;

  static auto targeted(composition::Element content, Traits traits,
                       const Placement& placement) -> composition::Element;

  static auto permitted(const components::ForEach::Edits& edits,
                        const Traits& traits) -> components::ForEach::Edits;

  static auto shows_separator(const Traits& traits,
                              const Placement& placement) noexcept -> bool;

 private:
  static auto fills(const Traits& traits, const Placement& placement) noexcept
      -> bool;
};

class ListCell::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source content, Placement placement, Traits traits,
          std::optional<Origin> origin) noexcept
      : content_(std::move(content)),
        placement_(std::move(placement)),
        traits_(traits),
        origin_(std::move(origin)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto adopt(composition::ElementContent& incoming) -> bool override;

  auto ignores_change(composition::Identity key,
                      const composition::EnvironmentOverride::Value& before,
                      const composition::EnvironmentOverride::Value& after)
      const -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto row(bool editing, bool choosing, bool chosen, bool contained,
           composition::BadgeProminence prominence,
           composition::StateHandle<bool> up,
           composition::StateHandle<float> drag,
           composition::StateHandle<float> height) const
      -> composition::Element;

  auto badge(composition::BadgeProminence prominence) const
      -> std::optional<composition::Element>;

  auto separator_tint() const -> std::optional<theme::ColourValue>;

  auto swiping(composition::Element body) const -> composition::Element;

  auto choosing(composition::Element body, const RowSelection& selection,
                bool editing) const -> composition::Element;

  auto handle(composition::StateHandle<bool> up,
              composition::StateHandle<float> drag,
              composition::StateHandle<float> height) const
      -> composition::Element;

  auto commanded(composition::Element body,
                 composition::StateHandle<bool> up) const
      -> composition::Element;

  void shift(std::size_t target) const;

  void remove() const;

  auto placed_like(const Placement& other) const noexcept -> bool;

  auto repeats(const Content& other) const -> bool;

  Source content_;
  Placement placement_;
  Traits traits_;
  std::optional<Origin> origin_;
};

}
