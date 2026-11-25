#pragma once

#include <arisa/components/document_creation_source.hpp>
#include <arisa/components/new_document_action.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/ut_type.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::structure {

class NewDocumentButton : public composition::Element {
 public:
  class Content;

  using Prepare = components::NewDocumentAction::Prepare;

  static constexpr std::string_view default_title = "Create Document";

  explicit NewDocumentButton(
      std::optional<std::string> label = std::nullopt,
      std::optional<composition::UTType> content_type = std::nullopt);

  NewDocumentButton(std::optional<std::string> label,
                    std::optional<composition::UTType> content_type,
                    components::DocumentCreationSource source);

  NewDocumentButton(std::optional<std::string> label,
                    composition::UTType content_type,
                    components::DocumentCreationSource source,
                    Prepare prepare_document_url);

  NewDocumentButton(std::optional<std::string> label,
                    composition::UTType content_type,
                    Prepare prepare_document_url);
};

class NewDocumentButton::Content final : public composition::ElementContent {
 public:
  Content(std::string label,
          components::NewDocumentAction::Request request) noexcept
      : label_(std::move(label)), request_(std::move(request)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto activate(composition::Context& context, const composition::Node& node,
                std::optional<composition::Point> at) -> bool override;

 private:
  std::string label_;
  components::NewDocumentAction::Request request_;
};

}
