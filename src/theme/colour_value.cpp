#include <arisa/theme/colour_mix.hpp>
#include <arisa/theme/colour_value.hpp>

#include <charconv>
#include <format>
#include <ranges>
#include <vector>

namespace arisa::theme {

auto ColourValue::content_type() -> std::string { return "public.color"; }

auto ColourValue::mix(ColourValue other, float amount,
                      ColourInterpolation space) const noexcept -> ColourValue {
  const auto plain = [](const ColourValue& value) -> std::optional<Colour> {
    const Colour* colour = value.literal();
    if (colour == nullptr || value.step_count_ != 0 ||
        value.exposure_ != 0.0F) {
      return std::nullopt;
    }
    return colour->with_alpha(colour->alpha * value.opacity_);
  };
  const std::optional<Colour> from = plain(*this);
  const std::optional<Colour> to = plain(other);
  if (from.has_value() && to.has_value()) {
    ColourValue mixed{ColourMix::mix(*from, *to, amount, space)};
    mixed.headroom_ = headroom_;
    return mixed;
  }
  ColourValue mixed = *this;
  const Leaf colour = to.has_value() ? Leaf{*to}
                      : other.literal() != nullptr
                          ? Leaf{other.literal()->with_alpha(
                                other.literal()->alpha * other.opacity_)}
                          : other.value_;
  const Step step{.colour = colour, .amount = amount, .space = space};
  if (mixed.step_count_ < maximum_steps) {
    mixed.steps_[mixed.step_count_++] = step;
  } else {
    mixed.steps_.back() = step;
  }
  return mixed;
}

auto ColourValue::encoded() const -> std::string {
  std::string text;
  if (const ColourRole* named = role()) {
    text =
        std::format("role {} {}", static_cast<std::size_t>(*named), opacity_);
  } else {
    const Colour& colour = *literal();
    text = std::format("rgba {} {} {} {} {}", colour.red, colour.green,
                       colour.blue, colour.alpha, opacity_);
  }
  for (const Step& step : steps()) {
    if (const ColourRole* named = std::get_if<ColourRole>(&step.colour)) {
      text +=
          std::format(" mix role {} {} {}", static_cast<std::size_t>(*named),
                      step.amount, static_cast<unsigned>(step.space));
    } else {
      const Colour& colour = std::get<Colour>(step.colour);
      text += std::format(" mix rgba {} {} {} {} {} {}", colour.red,
                          colour.green, colour.blue, colour.alpha, step.amount,
                          static_cast<unsigned>(step.space));
    }
  }
  if (headroom_ != 1.0F || exposure_ != 0.0F) {
    text += std::format(" hdr {} {}", headroom_, exposure_);
  }
  return text;
}

auto ColourValue::decoded(std::string_view text) -> std::optional<ColourValue> {
  std::vector<std::string_view> parts;
  for (const auto part : text | std::views::split(' ')) {
    parts.emplace_back(part.begin(), part.end());
  }
  const auto number = [](std::string_view field) -> std::optional<float> {
    float value = 0.0F;
    const auto [end, failure] =
        std::from_chars(field.data(), field.data() + field.size(), value);
    if (failure != std::errc{} || end != field.data() + field.size()) {
      return std::nullopt;
    }
    return value;
  };
  const auto index_of =
      [](std::string_view field) -> std::optional<std::size_t> {
    std::size_t index = 0;
    const auto [end, failure] =
        std::from_chars(field.data(), field.data() + field.size(), index);
    if (failure != std::errc{} || end != field.data() + field.size()) {
      return std::nullopt;
    }
    return index;
  };
  std::size_t at = 0;
  const auto leaf = [&]() -> std::optional<Leaf> {
    if (at + 2 <= parts.size() && parts[at] == "role") {
      const std::optional<std::size_t> index = index_of(parts[at + 1]);
      if (!index.has_value() || *index >= colour_role_count) {
        return std::nullopt;
      }
      at += 2;
      return Leaf{static_cast<ColourRole>(*index)};
    }
    if (at + 5 <= parts.size() && parts[at] == "rgba") {
      const std::optional<float> red = number(parts[at + 1]);
      const std::optional<float> green = number(parts[at + 2]);
      const std::optional<float> blue = number(parts[at + 3]);
      const std::optional<float> alpha = number(parts[at + 4]);
      if (!red || !green || !blue || !alpha) {
        return std::nullopt;
      }
      at += 5;
      return Leaf{
          Colour{.red = *red, .green = *green, .blue = *blue, .alpha = *alpha}};
    }
    return std::nullopt;
  };
  const std::optional<Leaf> base = leaf();
  if (!base.has_value() || at >= parts.size()) {
    return std::nullopt;
  }
  const std::optional<float> faded = number(parts[at++]);
  if (!faded.has_value()) {
    return std::nullopt;
  }
  ColourValue value =
      std::holds_alternative<ColourRole>(*base)
          ? ColourValue{std::get<ColourRole>(*base)}.opacity(*faded)
          : ColourValue{std::get<Colour>(*base)}.opacity(*faded);
  while (at < parts.size()) {
    if (parts[at] == "mix") {
      ++at;
      const std::optional<Leaf> other = leaf();
      if (!other.has_value() || at + 2 > parts.size()) {
        return std::nullopt;
      }
      const std::optional<float> amount = number(parts[at]);
      const std::optional<std::size_t> space = index_of(parts[at + 1]);
      if (!amount.has_value() || !space.has_value() || *space > 2 ||
          value.step_count_ >= maximum_steps) {
        return std::nullopt;
      }
      at += 2;
      value.steps_[value.step_count_++] =
          Step{.colour = *other,
               .amount = *amount,
               .space = static_cast<ColourInterpolation>(*space)};
    } else if (parts[at] == "hdr" && at + 3 <= parts.size()) {
      const std::optional<float> room = number(parts[at + 1]);
      const std::optional<float> stops = number(parts[at + 2]);
      if (!room.has_value() || !stops.has_value()) {
        return std::nullopt;
      }
      value.headroom_ = *room;
      value.exposure_ = *stops;
      at += 3;
    } else {
      return std::nullopt;
    }
  }
  return value;
}

}
