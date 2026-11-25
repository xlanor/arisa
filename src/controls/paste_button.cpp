#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/controls/paste_button.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::controls {

PasteButton::PasteButton(
    std::vector<composition::UTType> supported_content_types,
    Paste payload_action)
    : Element(std::make_unique<Content>(std::move(supported_content_types),
                                        std::move(payload_action))) {}

PasteButton::PasteButton(const std::vector<std::string>& supported_types,
                         Paste payload_action)
    : PasteButton(identified(supported_types), std::move(payload_action)) {}

auto PasteButton::identified(const std::vector<std::string>& identifiers)
    -> std::vector<composition::UTType> {
  std::vector<composition::UTType> types;
  types.reserve(identifiers.size());
  for (const std::string& identifier : identifiers) {
    types.emplace_back(identifier);
  }
  return types;
}

auto PasteButton::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto PasteButton::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const std::optional<composition::Pasteboard>& board =
      context.environment(components::Environment::pasteboard);
  std::vector<composition::Element> produced;
  produced.push_back(components::Button(
      [board, accepted = types_, paste = paste_] {
        if (!board.has_value() || !paste) {
          return;
        }
        std::vector<composition::TransferItem> matching;
        for (composition::TransferItem& item : board->items()) {
          if (std::ranges::contains(accepted, item.type())) {
            matching.push_back(std::move(item));
          }
        }
        if (!matching.empty()) {
          paste(std::move(matching));
        }
      },
      components::Label(std::string{label},
                        components::Image::system_name(std::string{symbol}))));
  return produced;
}

}
