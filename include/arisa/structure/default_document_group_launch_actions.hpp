#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>

#include <vector>

namespace arisa::structure {

class DefaultDocumentGroupLaunchActions : public composition::Element {
 public:
  class Content;

  DefaultDocumentGroupLaunchActions();

  static auto new_key() -> composition::ElementKey;
};

class DefaultDocumentGroupLaunchActions::Content final
    : public composition::ElementContent {
 public:
  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;
};

}
