#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/string_catalog.hpp>

#include <arisa/async/task.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/image.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class Button : public composition::Element {
 public:
  using Action = std::move_only_function<void()>;

  static constexpr theme::TextRole label_role = theme::TextRole::body;
  static constexpr theme::Insets label_insets =
      theme::Insets::symmetric(theme::metric::button_horizontal_padding,
                               theme::metric::button_vertical_padding);

  struct Options {
    Action action;
    std::optional<ButtonRole> role;
  };

  class Content;

  Button(std::string label, Action action);

  Button(std::string label, std::optional<ButtonRole> role, Action action);

  Button(Action action, composition::Element label);

  Button(std::optional<ButtonRole> role, Action action,
         composition::Element label);

  Button(std::optional<ButtonRole> role, Action action);

  Button(std::string title, Image::SystemName system_image, Action action);

  Button(std::string title, Image::SystemName system_image,
         std::optional<ButtonRole> role, Action action);

  Button(std::string title, Image::Resource image, Action action);

  Button(std::string title, Image::Resource image,
         std::optional<ButtonRole> role, Action action);

  static auto system_label(std::optional<ButtonRole> role)
      -> composition::Element;

  static auto content(composition::Element& element)
      -> std::optional<std::reference_wrapper<Content>>;
};

class Button::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> held{"button-repeat-held",
                                                    false};
  static constexpr composition::StateKey<bool> repeated{"button-repeat-fired",
                                                        false};
  static constexpr std::chrono::milliseconds repeat_delay{400};
  static constexpr std::chrono::milliseconds repeat_interval{120};

  Content(std::string label, std::optional<ButtonRole> role, Action action,
          Source source) noexcept
      : label_(std::move(label)),
        role_(role),
        action_(std::move(action)),
        source_(std::move(source)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto adopt(composition::ElementContent& incoming) -> bool override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto automatic() const noexcept -> bool { return automatic_; }

  auto activate() -> bool override;

  auto activatable() const noexcept -> bool override;

  auto label() const noexcept -> std::string_view { return label_; }

  auto localized_label(const text::StringCatalog& catalog,
                       const text::Locale& locale) const -> std::string_view;

  auto accessibility() const -> composition::AccessibilityDefaults override;

  auto role() const noexcept -> std::optional<ButtonRole> { return role_; }

  void then(Action after);

  static auto repeat(bool down, composition::StateHandle<bool> fired,
                     std::function<void()> fire) -> async::Task<void>;

  static auto repeating(composition::Context& context,
                        composition::Element body,
                        const std::function<void()>& fire)
      -> composition::Element;

  auto discloses() const noexcept -> bool { return discloses_; }

  void set_discloses(bool discloses) noexcept { discloses_ = discloses; }

  auto shared_action() -> std::shared_ptr<Action>;

 private:
  auto relay() const -> std::shared_ptr<Action>;

  struct Localized {
    std::string locale;
    text::StringCatalog catalog;
    std::string text;
  };

  std::string label_;
  mutable std::optional<Localized> localized_;
  std::optional<ButtonRole> role_;
  mutable Action action_;
  mutable std::shared_ptr<Action> relay_;
  Source source_;
  mutable bool automatic_{true};
  mutable bool takes_over_{false};
  bool discloses_{false};
};

}
