#include <text/cache_key.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace arisa::text {

CacheKey::Probe::Probe(std::string_view text, const TextStyle& style,
                       float width, float scale, TextFitting fitting) noexcept
    : text_(text),
      style_(style),
      width_(width),
      scale_(scale),
      fitting_(fitting),
      hash_(digest(text, style, width, scale, fitting)) {}

CacheKey::CacheKey(const Probe& probe)
    : text_(probe.text_),
      style_(probe.style_.get()),
      width_(probe.width_),
      scale_(probe.scale_),
      fitting_(probe.fitting_),
      hash_(probe.hash_) {}

auto CacheKey::matches(const Probe& probe) const -> bool {
  return hash_ == probe.hash_ && width_ == probe.width_ &&
         scale_ == probe.scale_ && fitting_ == probe.fitting_ &&
         text_ == probe.text_ && style_ == probe.style_.get();
}

auto CacheKey::digest(std::string_view text, const TextStyle& style,
                      float width, float scale,
                      const TextFitting& fitting) noexcept -> std::size_t {
  std::size_t hash = std::hash<std::string_view>{}(text);
  hash = combine(hash, std::hash<float>{}(style.size.value()));
  hash = combine(hash, std::hash<std::uint16_t>{}(style.weight.value()));
  hash = combine(hash, static_cast<std::size_t>(style.design));
  hash = combine(hash, std::hash<float>{}(style.tracking));
  hash = combine(hash, std::hash<float>{}(style.kerning));
  hash = combine(hash, std::hash<float>{}(style.baseline_offset));
  hash = combine(hash, std::hash<float>{}(style.line_spacing));
  hash = combine(hash, static_cast<std::size_t>(style.tabular_digits));
  hash = combine(hash, std::hash<float>{}(style.line_height
                                              .transform([](PointSize size) {
                                                return size.value();
                                              })
                                              .value_or(0.0F)));
  hash = combine(hash, std::hash<std::string_view>{}(style.locale.tag()));
  hash = combine(hash, static_cast<std::size_t>(style.alignment));
  hash = combine(hash, static_cast<std::size_t>(style.italic));
  hash = combine(hash, static_cast<std::size_t>(style.typeface));
  hash = combine(hash, std::hash<float>{}(style.stretch));
  hash = combine(hash, style.name.has_value() ? style.name->value() : 0U);
  hash = combine(hash, static_cast<std::size_t>(style.underline));
  hash = combine(hash, static_cast<std::size_t>(style.strikethrough));
  hash = combine(hash, style.runs.hash());
  hash = combine(hash, static_cast<std::size_t>(style.small_caps));
  hash = combine(hash, static_cast<std::size_t>(style.underline_pattern));
  hash = combine(hash, static_cast<std::size_t>(style.strikethrough_pattern));
  hash = combine(
      hash, style.line_rule.has_value()
                ? (static_cast<std::size_t>(style.line_rule->kind()) + 1U) ^
                      std::hash<float>{}(style.line_rule->value())
                : 0U);
  hash =
      combine(hash, style.language.has_value()
                        ? std::hash<std::string_view>{}(style.language->tag())
                        : 0U);
  hash = combine(hash, static_cast<std::size_t>(style.direction));
  hash = combine(hash, std::hash<float>{}(width));
  hash = combine(hash, std::hash<float>{}(scale));
  hash = combine(hash, fitting.lines.value_or(0));
  hash = combine(hash, std::hash<float>{}(fitting.height.value_or(0.0F)));
  hash = combine(hash, static_cast<std::size_t>(fitting.truncation));
  hash = combine(hash, std::hash<float>{}(fitting.minimum_scale_factor));
  hash = combine(hash, static_cast<std::size_t>(fitting.allows_tightening));
  return hash;
}

auto CacheKey::combine(std::size_t seed, std::size_t value) noexcept
    -> std::size_t {
  return seed ^ (value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U));
}

}
