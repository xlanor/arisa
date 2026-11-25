#pragma once

#include <arisa/components/share_destination.hpp>
#include <arisa/components/share_preview.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/transferable.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::controls {

struct ShareLinkOptions {
  std::optional<std::string> subject{};
  std::optional<std::string> message{};
  std::optional<components::SharePreview> preview{};
};

class ShareLink : public composition::Element {
 public:
  class Content;

  using ShareDestination = components::ShareDestination;
  using SharePreview = components::SharePreview;

  using Options = ShareLinkOptions;

  static constexpr std::string_view default_title = "Share";
  static constexpr std::string_view symbol = "square.and.arrow.up";
  static constexpr std::string_view copy_title = "Copy";
  static constexpr std::string_view copy_symbol = "doc.on.doc";
  static constexpr std::string_view panel_key = "arisa.share.panel";
  static constexpr float panel_width = 280.0F;

  template <composition::Transferable T>
  explicit ShareLink(const T& item, Options options = {})
      : ShareLink(std::string{default_title},
                  std::vector{composition::TransferItem::of(item)},
                  std::move(options)) {}

  template <composition::Transferable T>
  ShareLink(std::string title, const T& item, Options options = {})
      : ShareLink(std::move(title),
                  std::vector{composition::TransferItem::of(item)},
                  std::move(options)) {}

  template <composition::Transferable T>
  ShareLink(const T& item, Options options, composition::Element label)
      : ShareLink(std::vector{composition::TransferItem::of(item)},
                  std::move(options), std::move(label)) {}

  template <composition::Transferable T>
  explicit ShareLink(const std::vector<T>& items, Options options = {})
      : ShareLink(std::string{default_title}, gathered(items),
                  std::move(options)) {}

  template <composition::Transferable T>
  ShareLink(std::string title, const std::vector<T>& items,
            Options options = {})
      : ShareLink(std::move(title), gathered(items), std::move(options)) {}

  template <composition::Transferable T>
  ShareLink(const std::vector<T>& items, Options options,
            composition::Element label)
      : ShareLink(gathered(items), std::move(options), std::move(label)) {}

  ShareLink(std::string title, std::vector<composition::TransferItem> items,
            Options options);

  ShareLink(std::vector<composition::TransferItem> items, Options options,
            composition::Element label);

  static auto panel(const ShareDestination::Request& request,
                    const std::optional<SharePreview>& preview,
                    const std::vector<ShareDestination>& destinations,
                    std::function<void()> close) -> composition::Element;

 private:
  template <composition::Transferable T>
  static auto gathered(const std::vector<T>& items)
      -> std::vector<composition::TransferItem> {
    std::vector<composition::TransferItem> made;
    made.reserve(items.size());
    for (const T& item : items) {
      made.push_back(composition::TransferItem::of(item));
    }
    return made;
  }
};

class ShareLink::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> presented{"share-presented",
                                                         false};

  Content(std::vector<composition::TransferItem> items, Options options,
          Source label) noexcept
      : items_(std::move(items)),
        options_(std::move(options)),
        label_(std::move(label)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto items() const noexcept -> const std::vector<composition::TransferItem>& {
    return items_;
  }

 private:
  std::vector<composition::TransferItem> items_;
  Options options_;
  Source label_;
};

}

namespace arisa::composition {

template <class Item>
auto Element::navigation_document(const Item& item) && -> Element {
  static_assert(Transferable<Item>, "a navigation document is Transferable");
  return std::move(*this).toolbar_title_menu([shared = TransferItem::of(item)] {
    return Element{controls::ShareLink(
        std::string{controls::ShareLink::default_title},
        std::vector<TransferItem>{shared}, controls::ShareLinkOptions{})};
  });
}

template <class Item, class Preview>
auto Element::navigation_document(const Item& item,
                                  Preview preview) && -> Element {
  static_assert(Transferable<Item>, "a navigation document is Transferable");
  return std::move(*this).toolbar_title_menu(
      [shared = TransferItem::of(item),
       shown = components::SharePreview{std::move(preview)}] {
        return Element{
            controls::ShareLink(std::string{controls::ShareLink::default_title},
                                std::vector<TransferItem>{shared},
                                controls::ShareLinkOptions{.preview = shown})};
      });
}

}
