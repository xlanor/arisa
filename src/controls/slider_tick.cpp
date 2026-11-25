#include <arisa/components/text.hpp>
#include <arisa/controls/slider_tick.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <utility>

namespace arisa::controls {

auto SliderTick::labelled() const -> std::optional<composition::Element> {
  if (label_) {
    return label_();
  }
  if (title_.has_value()) {
    return composition::Element{
        components::Text(text::LocalizedStringKey{*title_})};
  }
  return std::nullopt;
}

auto SliderTick::body() const -> std::vector<SliderTick> { return {*this}; }

}
