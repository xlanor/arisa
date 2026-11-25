#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::components {

class Link final : public composition::Component {
 public:
  Link(std::string title, std::string destination);

  Link(std::string destination, composition::Element label);

  auto operator==(const Link& other) const -> bool {
    return destination_ == other.destination_ && title_ == other.title_ &&
           label_ == other.label_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  auto destination() const noexcept -> std::string_view { return destination_; }

 private:
  std::string destination_;
  std::optional<std::string> title_;
  std::shared_ptr<composition::PlaceholderContentView::Source> label_;
};

}
