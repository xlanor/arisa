#pragma once

#include <arisa/components/image.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::components {

class Label : public composition::Element {
 public:
  class Content;

  Label(std::string title, composition::Element icon);

  Label(composition::Element title, composition::Element icon);

  Label(std::string title, Image::SystemName system_image);

  Label(std::string title, Image::Resource image);
};

class Label::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(std::optional<std::string> title, Source written,
          Source icon) noexcept
      : title_(std::move(title)),
        written_(std::move(written)),
        icon_(std::move(icon)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto adopt(composition::ElementContent& incoming) -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::optional<std::string> title_;
  Source written_;
  Source icon_;
};

}
