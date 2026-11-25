#include <arisa/structure/form_style.hpp>

#include <utility>

namespace arisa::structure {

const composition::EnvironmentKey<FormStyle> FormStyle::key{
    "form_style", FormStyle::automatic()};

auto FormStyle::automatic() noexcept -> FormStyle {
  return FormStyle{Kind::automatic};
}

auto FormStyle::grouped() noexcept -> FormStyle {
  return FormStyle{Kind::grouped};
}

auto FormStyle::columns() noexcept -> FormStyle {
  return FormStyle{Kind::columns};
}

auto FormStyle::cards() noexcept -> FormStyle { return FormStyle{Kind::cards}; }

auto operator==(const FormStyle& left, const FormStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != FormStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto FormStyle::make_body(Configuration configuration) const
    -> composition::Element {
  if (kind_ == Kind::custom) {
    return custom_->make_body(std::move(configuration));
  }
  return std::move(configuration.content);
}

}
