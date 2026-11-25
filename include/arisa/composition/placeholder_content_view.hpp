#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/node_anchor.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace arisa::composition {

class PlaceholderContentView : public Element {
 public:
  class Source {
   public:
    Source() noexcept = default;

    explicit Source(Element element) noexcept : element_(std::move(element)) {}

    auto pending() const noexcept -> bool { return element_.has_value(); }

    auto peek() const noexcept -> const std::optional<Element>& {
      return element_;
    }

    auto take() noexcept -> std::optional<Element>;

    void forward(Source& newer) noexcept;

    void replace(Source& newer);

    void shown_by(std::weak_ptr<const NodeAnchor> anchor) noexcept {
      shown_ = std::move(anchor);
    }

    void declared_by(std::weak_ptr<const NodeAnchor> anchor) noexcept {
      declared_ = std::move(anchor);
    }

    auto declared() const noexcept -> bool { return !declared_.expired(); }

    auto renew() const -> bool;

   private:
    std::optional<Element> element_;
    std::weak_ptr<const NodeAnchor> shown_;
    std::weak_ptr<const NodeAnchor> declared_;
  };

  class Content;

  explicit PlaceholderContentView(std::shared_ptr<Source> source);
};

class PlaceholderContentView::Content final : public ElementContent {
 public:
  explicit Content(std::shared_ptr<Source> source) noexcept
      : source_(std::move(source)) {}

  auto type() const noexcept -> ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(Context& context) const -> std::vector<Element> override;

  auto same_as(const ElementContent& other) const -> bool override;

  auto source() const noexcept -> const Source& { return *source_; }

  auto take() const noexcept -> std::optional<Element> {
    return source_->take();
  }

  void shown_by(std::weak_ptr<const NodeAnchor> anchor) const noexcept {
    source_->shown_by(std::move(anchor));
  }

 private:
  std::shared_ptr<Source> source_;
};

}
