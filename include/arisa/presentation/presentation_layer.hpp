#pragma once

#include <arisa/composition/presentation_background.hpp>

#include <arisa/async/task.hpp>
#include <arisa/components/layout_subviews.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/dialog_severity.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_attachment.hpp>
#include <arisa/composition/identity.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/popover_attachment_anchor.hpp>
#include <arisa/composition/popover_placement.hpp>
#include <arisa/composition/presentation_options.hpp>
#include <arisa/composition/proposed_view_size.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/toast_options.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/support/full_order_walk.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/space.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::presentation {

class ModalPresentation;

class PresentationLayer {
 public:
  struct Presence {
    bool presented{false};
    std::function<void()> dismiss;
    composition::Identity source;
    std::optional<composition::ElementKey> identity{};
  };

  using Dismissed = std::move_only_function<void()>;

  static constexpr std::string_view close_label = "Close";
  static constexpr std::string_view cancel_label = "Cancel";
  static constexpr std::string_view ok_label = "OK";
  static constexpr float alert_width = theme::metrics::alert.width;
  static constexpr float dialog_width = theme::metrics::alert.width;
  static constexpr float sheet_width = 0.62F;
  static constexpr float sheet_height = 0.88F;
  static constexpr float toast_lift = 72.0F;
  static constexpr float form_width = 0.5F;
  static constexpr float form_height = 0.72F;
  static constexpr float dialog_icon_size = 64.0F;
  static constexpr float dialog_badge_size = 26.0F;
  static constexpr std::string_view critical_symbol = "warning";
  static constexpr std::string_view suppressed_symbol = "check_box";
  static constexpr std::string_view unsuppressed_symbol =
      "check_box_outline_blank";

  static auto message(std::string text) -> composition::Element;

  static auto handoff(composition::Element panel, composition::Gesture gesture,
                      composition::ScrollHandoff handed)
      -> composition::Element;

  static auto sheet(composition::Element presenter, Presence presence,
                    Dismissed on_dismiss,
                    std::optional<composition::Element> content)
      -> composition::Element;

  static auto full_screen_cover(composition::Element presenter,
                                Presence presence, Dismissed on_dismiss,
                                std::optional<composition::Element> content)
      -> composition::Element;

  static auto alert(composition::Element presenter, Presence presence,
                    std::string title,
                    std::optional<composition::Element> actions,
                    std::optional<composition::Element> message)
      -> composition::Element;

  static auto confirmation_dialog(composition::Element presenter,
                                  Presence presence, std::string title,
                                  composition::Visibility title_visibility,
                                  std::optional<composition::Element> actions,
                                  std::optional<composition::Element> message)
      -> composition::Element;

  static auto popover(composition::Element presenter, Presence presence,
                      composition::PopoverAttachmentAnchor attachment_anchor,
                      theme::Edge arrow_edge,
                      std::optional<composition::Element> content)
      -> composition::Element;

  static auto toast(composition::Element presenter, Presence presence,
                    std::chrono::nanoseconds duration,
                    composition::ToastOptions options,
                    std::optional<composition::Element> content)
      -> composition::Element;

  static auto placed(composition::Element content,
                     composition::PopoverPlacement placement)
      -> composition::Element;

 private:
  friend class ModalPresentation;

  using Measured = std::function<void(const float&)>;

  struct Modal {
    composition::OverlayLayer layer;
    composition::Alignment alignment;
    std::string exit_label;
    bool dismiss_disabled{false};
    bool scrim{true};
    bool scrim_dismisses{true};
    motion::Transition transition;
    std::optional<composition::Layer::Anchoring> anchoring;
    composition::OverlayInput input{composition::OverlayInput::modal};
    Measured measured{};
    std::optional<composition::StateHandle<std::optional<theme::ColorScheme>>>
        scheme{};
    std::optional<composition::PopoverPlacement> placement{};
  };

  struct Extras {
    std::shared_ptr<composition::Element> icon;
    composition::DialogSeverity severity{
        composition::DialogSeverity::automatic};
    std::optional<composition::PresentationOptions::Suppression> suppression;
  };

  struct Actions {
    std::vector<composition::Element> buttons;
    std::optional<composition::Element> cancel;
    std::shared_ptr<std::move_only_function<void()>> cancel_action;
    std::string cancel_label;
  };

  static auto modal(Presence presence, Modal settings,
                    composition::Element panel, Dismissed on_dismiss,
                    std::function<void()> exit) -> composition::Element;

  static auto scrim(std::function<void()> exit, bool visible, bool dismisses,
                    Measured measured) -> composition::Element;

  static auto cover_layer(
      Presence presence, composition::Element content,
      const std::optional<composition::PresentationBackground>& background,
      bool locked, Dismissed on_dismiss) -> composition::Element;

  static auto popover_layer(
      Presence presence, composition::Layer::Anchoring anchoring,
      composition::Element content,
      const std::optional<composition::PresentationBackground>& background,
      bool locked) -> composition::Element;

  static auto adapts(const composition::Element& content) -> bool;

  static auto stateful(const composition::Element& content) -> bool;

  struct ElementAccess {
    using Node = const composition::Element;

    static auto children(const composition::Element& element)
        -> std::span<const composition::Element> {
      return element.children();
    }
  };

  using DeclarationWalk = support::FullOrderWalk<ElementAccess, bool>;

  struct Declaration {
    composition::Identity key;
    bool found{false};

    auto enter(const composition::Element& element, const bool&)
        -> DeclarationWalk::Step {
      found = found || element.declares(key);
      return DeclarationWalk::Step{.state = found, .descend = !found};
    }

    void leave(const composition::Element&, bool&) {}
  };

  static auto declares_within(const composition::Element& content,
                              composition::Identity key) -> bool;

  static auto sheet_panel(
      composition::Element content,
      const std::optional<composition::PresentationBackground>& background,
      const composition::PresentationOptions& options,
      std::optional<float> height,
      std::shared_ptr<composition::Size> largest = nullptr)
      -> composition::Element;

  static auto placed(composition::Alignment alignment,
                     composition::PresentationPlacement placement) noexcept
      -> composition::Alignment;

  class Sized {
   public:
    Sized(composition::PresentationSizing sizing,
          std::shared_ptr<composition::Size> largest) noexcept
        : sizing_(std::move(sizing)), largest_(std::move(largest)) {}

    auto size_that_fits(composition::ProposedViewSize proposal,
                        const components::LayoutSubviews& subviews) const
        -> composition::Size;

    void place_subviews(composition::Rect bounds,
                        composition::ProposedViewSize proposal,
                        const components::LayoutSubviews& subviews) const;

    friend auto operator==(const Sized&, const Sized&) -> bool = default;

   private:
    composition::PresentationSizing sizing_;
    std::shared_ptr<composition::Size> largest_;
  };

  static auto extras(const std::optional<composition::Element>& first,
                     const std::optional<composition::Element>& second)
      -> Extras;

  static auto heading(std::optional<composition::Element> icon,
                      composition::DialogSeverity severity)
      -> std::optional<composition::Element>;

  class DialogPanel;

  static auto suppression_toggle(
      const composition::PresentationOptions::Suppression& suppression)
      -> composition::Element;

  static auto actions(std::optional<composition::Element> declared,
                      const Presence& presence, bool fallback) -> Actions;

  static auto styled(composition::Element button) -> composition::Element;

  using Background = std::optional<composition::PresentationBackground>;

  static auto chosen(const std::optional<composition::Element>& first,
                     const std::optional<composition::Element>& second = {})
      -> Background;

  static auto message_text(std::optional<composition::Element> message)
      -> std::optional<composition::Element>;

  static auto exit_action(const Actions& actions, const Presence& presence)
      -> std::function<void()>;

  static auto expire(std::function<void()> dismiss,
                     std::chrono::nanoseconds duration) -> async::Task<void>;

  static auto held(composition::Element element)
      -> std::shared_ptr<composition::PlaceholderContentView::Source>;

  struct HeldIcon {
    std::shared_ptr<composition::Element> from;
    std::shared_ptr<composition::PlaceholderContentView::Source> source;
  };
};

class PresentationLayer::DialogPanel final : public composition::Component {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  DialogPanel(Extras eager, std::vector<Source> leading,
              std::vector<Source> trailing, theme::Space spacing) noexcept
      : eager_(std::move(eager)),
        leading_(std::move(leading)),
        trailing_(std::move(trailing)),
        spacing_(spacing) {}

  auto operator==(const DialogPanel& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static const composition::StateKey<HeldIcon> icon_key;

 private:
  Extras eager_;
  std::vector<Source> leading_;
  std::vector<Source> trailing_;
  theme::Space spacing_;
};

}
