#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/composition/transferable.hpp>
#include <arisa/composition/ut_type.hpp>

#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::controls {

class PasteButton : public composition::Element {
 public:
  class Content;

  using Paste = std::function<void(std::vector<composition::TransferItem>)>;

  static constexpr std::string_view label = "Paste";
  static constexpr std::string_view symbol = "content_paste";

  PasteButton(std::vector<composition::UTType> supported_content_types,
              Paste payload_action);

  PasteButton(const std::vector<std::string>& supported_types,
              Paste payload_action);

  template <class Validator, class Action>
    requires std::invocable<Validator&,
                            std::vector<composition::TransferItem>> &&
             std::invocable<
                 Action&,
                 typename std::invoke_result_t<
                     Validator&,
                     std::vector<composition::TransferItem>>::value_type>
  PasteButton(std::vector<composition::UTType> supported_content_types,
              Validator validator, Action payload_action)
      : PasteButton(
            std::move(supported_content_types),
            Paste{[validator = std::move(validator),
                   action = std::move(payload_action)](
                      std::vector<composition::TransferItem> items) mutable {
              if (auto payload = std::invoke(validator, std::move(items))) {
                std::invoke(action, std::move(*payload));
              }
            }}) {}

  template <class Validator, class Action>
    requires std::invocable<Validator&,
                            std::vector<composition::TransferItem>> &&
             std::invocable<
                 Action&,
                 typename std::invoke_result_t<
                     Validator&,
                     std::vector<composition::TransferItem>>::value_type>
  PasteButton(const std::vector<std::string>& supported_types,
              Validator validator, Action payload_action)
      : PasteButton(identified(supported_types), std::move(validator),
                    std::move(payload_action)) {}

  template <composition::Transferable T, class Action>
    requires std::invocable<Action&, std::vector<T>>
  PasteButton(std::type_identity<T>, Action on_paste)
      : PasteButton(
            std::vector<composition::UTType>{
                composition::TransferItem::content_type<T>()},
            Paste{[on_paste = std::move(on_paste)](
                      std::vector<composition::TransferItem> items) mutable {
              std::vector<T> decoded;
              for (const composition::TransferItem& item : items) {
                if (std::optional<T> value = item.template as<T>()) {
                  decoded.push_back(std::move(*value));
                }
              }
              if (!decoded.empty()) {
                std::invoke(on_paste, std::move(decoded));
              }
            }}) {}

 private:
  static auto identified(const std::vector<std::string>& identifiers)
      -> std::vector<composition::UTType>;
};

class PasteButton::Content final : public composition::ElementContent {
 public:
  Content(std::vector<composition::UTType> types, Paste paste) noexcept
      : types_(std::move(types)), paste_(std::move(paste)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::vector<composition::UTType> types_;
  Paste paste_;
};

}
