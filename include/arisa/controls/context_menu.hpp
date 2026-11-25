#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_value.hpp>
#include <arisa/controls/clipboard_commands.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class ContextMenu : public composition::Element {
 public:
  class Content;
  class ForSelection;

  using Builder = std::function<composition::Element()>;

  struct Wrapped {
    bool activatable{false};
    bool button{false};
    bool discloses{false};
  };

  ContextMenu(composition::Element content, Builder items,
              std::optional<Builder> preview = std::nullopt);

  static auto content(const composition::Element& element)
      -> std::optional<std::reference_wrapper<const Content>>;

  static const composition::EnvironmentKey<bool> hosted;

  static const composition::EnvironmentKey<std::shared_ptr<const ForSelection>>
      selection;

  static auto relabel(
      composition::Element menu,
      const std::function<composition::Element(composition::Element)>& change)
      -> composition::Element;

  static auto panel(composition::Element items,
                    std::optional<composition::Element> preview,
                    composition::Binding<bool> shown,
                    const std::optional<graphics::AnyShape>& preview_shape)
      -> composition::Element;

  static auto attach(composition::Element target,
                     composition::Binding<bool> shown, Builder items,
                     std::optional<Builder> preview,
                     std::optional<graphics::AnyShape> preview_shape = {})
      -> composition::Element;

  static auto with_clipboard(
      composition::Element target,
      const std::function<void(ClipboardCommands&)>& edit)
      -> composition::Element;

  static auto with_items(composition::Element target, Builder items,
                         std::optional<Builder> preview)
      -> composition::Element;

 private:
  static auto wrapped_of(composition::Element& content) -> Wrapped;
};

class ContextMenu::ForSelection {
 public:
  using Tags = std::vector<std::shared_ptr<const composition::TagValue>>;
  using Menu = std::function<composition::Element(const Tags&)>;
  using Primary = std::function<void(const Tags&)>;

  ForSelection(Menu menu, Primary primary) noexcept
      : menu_(std::move(menu)), primary_(std::move(primary)) {}

  template <class Selected>
  static auto selected(const Tags& tags) -> std::set<Selected> {
    std::set<Selected> chosen;
    for (const std::shared_ptr<const composition::TagValue>& tag : tags) {
      if (tag == nullptr) {
        continue;
      }
      if (const Selected* value = tag->get<Selected>()) {
        chosen.insert(*value);
      }
    }
    return chosen;
  }

  auto items(Tags tags) const -> Builder;

  auto has_primary() const noexcept -> bool {
    return static_cast<bool>(primary_);
  }

  void perform(const Tags& tags) const;

 private:
  Menu menu_;
  Primary primary_;
};

class ContextMenu::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> shown{"context-menu-shown",
                                                     false};

  using Wrapped = ContextMenu::Wrapped;

  Content(Source label, Builder items, std::optional<Builder> preview,
          Wrapped wrapped) noexcept
      : label_(std::move(label)),
        items_(std::move(items)),
        preview_(std::move(preview)),
        wrapped_(wrapped) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto wrapped() const noexcept -> const Wrapped& { return wrapped_; }

  auto items() const -> Builder;

  auto clipboard() -> ClipboardCommands&;

  void list(Builder items, std::optional<Builder> preview) noexcept {
    items_ = std::move(items);
    preview_ = std::move(preview);
  }

  auto preview() const noexcept -> const std::optional<Builder>& {
    return preview_;
  }

  void hold(Source label) noexcept { label_ = std::move(label); }

  auto preview_shape() const noexcept
      -> const std::optional<graphics::AnyShape>& {
    return preview_shape_;
  }

  void set_preview_shape(std::optional<graphics::AnyShape> shape) {
    preview_shape_ = std::move(shape);
  }

  auto take_label() noexcept -> std::optional<composition::Element> {
    return label_ != nullptr ? label_->take() : std::nullopt;
  }

 private:
  Source label_;
  Builder items_;
  std::optional<Builder> preview_;
  Wrapped wrapped_;
  std::shared_ptr<ClipboardCommands> clipboard_;
  std::optional<graphics::AnyShape> preview_shape_;
};

}

namespace arisa::composition {

template <class Items>
auto Element::context_menu(Items items) && -> Element {
  return controls::ContextMenu::with_items(
      std::move(*this),
      [items = std::move(items)] { return Element{std::invoke(items)}; },
      std::nullopt);
}

template <class Items, class Preview>
auto Element::context_menu(Items items, Preview preview) && -> Element {
  return controls::ContextMenu::with_items(
      std::move(*this),
      [items = std::move(items)] { return Element{std::invoke(items)}; },
      controls::ContextMenu::Builder{[preview = std::move(preview)] {
        return Element{std::invoke(preview)};
      }});
}

template <class Selected, class Menu>
auto Element::context_menu(std::type_identity<Selected> type,
                           Menu menu) && -> Element {
  return std::move(*this).context_menu(
      type, std::move(menu), std::function<void(const std::set<Selected>&)>{});
}

template <class Selected, class Menu, class Primary>
auto Element::context_menu(std::type_identity<Selected>, Menu menu,
                           Primary primary_action) && -> Element {
  using Tags = controls::ContextMenu::ForSelection::Tags;
  controls::ContextMenu::ForSelection::Primary primary;
  if constexpr (std::is_constructible_v<bool, const Primary&>) {
    if (static_cast<bool>(primary_action)) {
      primary = [primary_action = std::move(primary_action)](const Tags& tags) {
        std::invoke(
            primary_action,
            controls::ContextMenu::ForSelection::selected<Selected>(tags));
      };
    }
  } else {
    primary = [primary_action = std::move(primary_action)](const Tags& tags) {
      std::invoke(
          primary_action,
          controls::ContextMenu::ForSelection::selected<Selected>(tags));
    };
  }
  return std::move(*this).environment(
      controls::ContextMenu::selection,
      std::make_shared<const controls::ContextMenu::ForSelection>(
          [menu = std::move(menu)](const Tags& tags) {
            return Element{std::invoke(
                menu,
                controls::ContextMenu::ForSelection::selected<Selected>(tags))};
          },
          std::move(primary)));
}

template <class Validator, class Action>
  requires std::invocable<Validator&, std::vector<TransferItem>>
auto Element::on_paste_command(std::vector<UTType> types, Validator validator,
                               Action action) && -> Element {
  return std::move(*this).on_paste_command(
      std::move(types),
      [validator = std::move(validator),
       action = std::move(action)](std::vector<TransferItem> items) {
        if (auto payload = std::invoke(validator, std::move(items))) {
          std::invoke(action, std::move(*payload));
        }
      });
}

template <class Action>
auto Element::item_provider(Action action) && -> Element {
  if constexpr (std::is_constructible_v<bool, const Action&>) {
    if (!static_cast<bool>(action)) {
      return std::move(*this);
    }
  }
  return controls::ContextMenu::with_clipboard(
      std::move(*this), [&action](controls::ClipboardCommands& commands) {
        commands.copies([action = std::move(action)] {
          std::vector<TransferItem> provided;
          if (std::optional<TransferItem> item = std::invoke(action)) {
            provided.push_back(std::move(*item));
          }
          return provided;
        });
      });
}

template <Transferable T>
auto Element::copyable(std::vector<T> items) && -> Element {
  return controls::ContextMenu::with_clipboard(
      std::move(*this),
      [items = std::move(items)](controls::ClipboardCommands& commands) {
        commands.copies([items] {
          std::vector<TransferItem> transferred;
          transferred.reserve(items.size());
          for (const T& item : items) {
            transferred.push_back(TransferItem::of(item));
          }
          return transferred;
        });
      });
}

template <Transferable T, class Action>
auto Element::cuttable(std::type_identity<T>, Action action) && -> Element {
  return controls::ContextMenu::with_clipboard(
      std::move(*this), [&action](controls::ClipboardCommands& commands) {
        commands.cuts([action = std::move(action)] {
          std::vector<TransferItem> transferred;
          for (const T& item : std::invoke(action)) {
            transferred.push_back(TransferItem::of(item));
          }
          return transferred;
        });
      });
}

template <Transferable T, class Action>
auto Element::paste_destination(std::type_identity<T> type,
                                Action action) && -> Element {
  return std::move(*this).paste_destination(
      type, std::move(action),
      [](std::vector<T> items) -> std::vector<T> { return items; });
}

template <Transferable T, class Action, class Validator>
auto Element::paste_destination(std::type_identity<T>, Action action,
                                Validator validator) && -> Element {
  return controls::ContextMenu::with_clipboard(
      std::move(*this),
      [&action, &validator](controls::ClipboardCommands& commands) {
        std::vector<UTType> types;
        types.push_back(TransferItem::content_type<T>());
        commands.pastes(std::move(types), [action = std::move(action),
                                           validator = std::move(validator)](
                                              std::vector<TransferItem> items) {
          std::vector<T> decoded;
          for (const TransferItem& item : items) {
            if (std::optional<T> value = item.template as<T>()) {
              decoded.push_back(std::move(*value));
            }
          }
          std::vector<T> accepted = std::invoke(validator, std::move(decoded));
          if (!accepted.empty()) {
            std::invoke(action, std::move(accepted));
          }
        });
      });
}

template <class Action>
auto Element::on_copy_command(Action action) && -> Element {
  return controls::ContextMenu::with_clipboard(
      std::move(*this), [&action](controls::ClipboardCommands& commands) {
        commands.copies([action = std::move(action)] {
          return std::vector<TransferItem>{std::invoke(action)};
        });
      });
}

template <class Action>
auto Element::on_cut_command(Action action) && -> Element {
  return controls::ContextMenu::with_clipboard(
      std::move(*this), [&action](controls::ClipboardCommands& commands) {
        commands.cuts([action = std::move(action)] {
          return std::vector<TransferItem>{std::invoke(action)};
        });
      });
}

template <class Action>
auto Element::on_paste_command(std::vector<UTType> types,
                               Action action) && -> Element {
  return controls::ContextMenu::with_clipboard(
      std::move(*this),
      [&types, &action](controls::ClipboardCommands& commands) {
        commands.pastes(std::move(types), [action = std::move(action)](
                                              std::vector<TransferItem> items) {
          std::invoke(action, std::move(items));
        });
      });
}

}
