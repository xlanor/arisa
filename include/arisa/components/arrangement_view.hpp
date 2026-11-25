#pragma once

#include <arisa/components/arrangement_view_style.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <memory>
#include <vector>

namespace arisa::components {

class ArrangementView : public composition::Element {
 public:
  class Content;

  ArrangementView(composition::Element primary, composition::Element secondary);

  explicit ArrangementView(ArrangementViewStyle::Configuration configuration);

 private:
  ArrangementView(composition::Element primary, composition::Element secondary,
                  bool configured);
};

class ArrangementView::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source primary, Source secondary, bool configured) noexcept
      : primary_(std::move(primary)),
        secondary_(std::move(secondary)),
        configured_(configured) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto resolved(composition::Context& context) const -> ArrangementViewStyle;

  Source primary_;
  Source secondary_;
  bool configured_;
};

}
