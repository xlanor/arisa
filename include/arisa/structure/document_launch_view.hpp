#pragma once

#include <arisa/components/document_error.hpp>
#include <arisa/components/new_document_action.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/file_url.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/ut_type.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/structure/document_launch_geometry_proxy.hpp>

#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace arisa::structure {

class DocumentLaunchView : public composition::Element {
 public:
  class Content;

  using Build = std::function<composition::Element()>;
  using OnDocumentOpen =
      std::function<composition::Element(const composition::FileURL&)>;
  using Accessory =
      std::function<composition::Element(const DocumentLaunchGeometryProxy&)>;
  using Create = std::function<
      std::expected<composition::FileURL, components::DocumentError>(
          components::NewDocumentAction::Request)>;

  struct Options {
    std::optional<std::string> subtitle{};
    std::optional<graphics::ShapeStyle> background_style{};
    Build background{};
    Accessory background_accessory{};
    Accessory overlay_accessory{};
    Create create{};
  };

  static constexpr composition::StateKey<std::string> opened{
      "arisa.document-launch.opened", std::string{}};
  static constexpr composition::StateKey<std::string> failure{
      "arisa.document-launch.failure", std::string{}};
  static constexpr composition::StateKey<composition::Rect> title_frame{
      "arisa.document-launch.title-frame", composition::Rect{}};

  static const composition::EnvironmentKey<bool> creates_documents;

  DocumentLaunchView(std::string title,
                     std::vector<composition::UTType> content_types,
                     Build actions, OnDocumentOpen on_document_open);

  DocumentLaunchView(std::string title,
                     std::vector<composition::UTType> content_types,
                     Build actions, OnDocumentOpen on_document_open,
                     Options options);

  DocumentLaunchView(std::string title,
                     std::vector<composition::UTType> content_types,
                     graphics::ShapeStyle background_style, Build actions,
                     OnDocumentOpen on_document_open);

  DocumentLaunchView(std::string title,
                     std::vector<composition::UTType> content_types,
                     graphics::ShapeStyle background_style, Build actions,
                     OnDocumentOpen on_document_open, Options options);

  static auto title_key() -> composition::ElementKey;

  static auto browser_key() -> composition::ElementKey;

  static auto document_key() -> composition::ElementKey;
};

class DocumentLaunchView::Content final : public composition::ElementContent {
 public:
  struct Parts {
    std::string title;
    std::vector<composition::UTType> content_types;
    Build actions;
    OnDocumentOpen on_document_open;
    Options options;
  };

  explicit Content(Parts parts) noexcept : parts_(std::move(parts)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto launch(composition::Context& context) const -> composition::Element;

  auto opening(composition::Context& context) const
      -> components::NewDocumentAction;

  Parts parts_;
};

}
