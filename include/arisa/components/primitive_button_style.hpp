#pragma once

#include <arisa/components/button_role.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::components {

class PrimitiveButtonStyle {
 public:
  struct Configuration {
    composition::Element label;
    std::optional<ButtonRole> role;
    std::function<void()> trigger;
  };

  static const composition::EnvironmentKey<std::optional<PrimitiveButtonStyle>>
      key;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, PrimitiveButtonStyle> &&
             StyleOf<Style, Configuration>)
  PrimitiveButtonStyle(Style style)
      : custom_(StyleBox<Configuration>::of(std::move(style))) {}

  auto make_body(Configuration configuration) const -> composition::Element {
    return custom_->make_body(std::move(configuration));
  }

  friend auto operator==(const PrimitiveButtonStyle& left,
                         const PrimitiveButtonStyle& right) -> bool {
    return left.custom_->equals(*right.custom_);
  }

 private:
  std::shared_ptr<const StyleBox<Configuration>> custom_;
};

}
