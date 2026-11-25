#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>

#include <string>
#include <utility>
#include <vector>

namespace arisa::structure {

class SectionIndex final : public composition::Component {
 public:
  struct Entry {
    std::string label;
    composition::ElementKey key;

    friend auto operator==(const Entry&, const Entry&) -> bool = default;
  };

  static constexpr float width = 28.0F;
  static constexpr float row_height = 16.0F;

  SectionIndex(std::vector<Entry> entries, composition::ScrollViewProxy proxy)
      : entries_(std::move(entries)), proxy_(std::move(proxy)) {}

  static auto key_of(std::size_t section) -> composition::ElementKey;

  auto operator==(const SectionIndex& other) const -> bool {
    return entries_ == other.entries_ && proxy_ == other.proxy_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  void jump(std::size_t index) const;

  std::vector<Entry> entries_;
  composition::ScrollViewProxy proxy_;
};

}
