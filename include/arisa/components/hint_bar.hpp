#pragma once

#include <arisa/composition/command_hint.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <string_view>

namespace arisa::components {

class HintBar final : public composition::Component {
 public:
  auto operator==(const HintBar&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto glyph(provider::Button button) noexcept -> std::string_view;

  static auto name(provider::Button button) noexcept -> std::string_view;

 private:
  static auto hint(const composition::CommandHint& command, bool glyphs)
      -> composition::Element;
};

}
