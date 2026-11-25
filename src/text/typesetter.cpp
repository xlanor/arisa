#include <arisa/text/typesetter.hpp>
#include <text/cache_key.hpp>
#include <text/shaped_text.hpp>
#include <text/shaper.hpp>
#include <text/text_breaks.hpp>
#include <text/typesetter_cache.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace arisa::text {

Typesetter::Typesetter(const FontCollection& fonts,
                       std::unique_ptr<Shaper> shaper,
                       std::unique_ptr<TypesetterCache> cache) noexcept
    : fonts_(fonts), shaper_(std::move(shaper)), cache_(std::move(cache)) {}

Typesetter::Typesetter(Typesetter&& other) noexcept = default;

auto Typesetter::operator=(Typesetter&& other) noexcept
    -> Typesetter& = default;

Typesetter::~Typesetter() = default;

auto Typesetter::create(const FontCollection& fonts, TypesetterLimits limits)
    -> std::expected<Typesetter, TextError> {
  auto shaper = Shaper::create();
  if (!shaper.has_value()) {
    return std::unexpected(shaper.error());
  }
  return Typesetter{fonts, std::make_unique<Shaper>(std::move(*shaper)),
                    std::make_unique<TypesetterCache>(limits)};
}

auto Typesetter::shaped(std::string_view text, const TextStyle& style)
    -> std::expected<std::shared_ptr<const ShapedText>, TextError> {
  const CacheKey::Probe key{text, style, 0.0F, 0.0F};
  if (auto found = cache_->shaped().find(key)) {
    ++statistics_.shaping_hits;
    return std::move(*found);
  }
  ++statistics_.shaping_misses;
  auto created = ShapedText::create(fonts_.get(), text, style, *shaper_);
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  auto shared = std::make_shared<const ShapedText>(std::move(*created));
  cache_->shaped().store(CacheKey{key}, shared);
  return shared;
}

auto Typesetter::layout(std::string_view text, const TextStyle& style,
                        LayoutWidth width, provider::SurfaceScale scale)
    -> std::expected<std::shared_ptr<const TextLayout>, TextError> {
  const CacheKey::Probe key{text, style, width.value(), scale.value()};
  if (auto found = cache_->layouts().find(key)) {
    ++statistics_.layout_hits;
    return std::move(*found);
  }
  ++statistics_.layout_misses;
  auto source = shaped(text, style);
  if (!source.has_value()) {
    return std::unexpected(source.error());
  }
  auto built = std::make_shared<const TextLayout>(
      (*source)->layout(width, scale, *shaper_));
  cache_->layouts().store(CacheKey{key}, built);
  return built;
}

auto Typesetter::measure(std::string_view text, const TextStyle& style,
                         LayoutWidth width, provider::SurfaceScale scale)
    -> std::expected<TextSize, TextError> {
  const CacheKey::Probe key{text, style, width.value(), scale.value()};
  if (auto found = cache_->layouts().find(key)) {
    ++statistics_.layout_hits;
    return (*found)->size();
  }
  if (auto found = cache_->measures().find(key)) {
    return *found;
  }
  ++statistics_.measure_misses;
  auto source = shaped(text, style);
  if (!source.has_value()) {
    return std::unexpected(source.error());
  }
  const TextSize measured = (*source)->measure(width, scale, *shaper_);
  cache_->measures().store(CacheKey{key}, measured);
  return measured;
}

auto Typesetter::fits(std::string_view text, const TextStyle& style,
                      LayoutWidth width, provider::SurfaceScale scale,
                      std::size_t lines) -> bool {
  const auto laid = layout(text, style, width, scale);
  if (!laid.has_value()) {
    return false;
  }
  return (*laid)->lines().size() <= lines &&
         (!width.bounded() || (*laid)->size().width <= width.value() + 0.01F);
}

auto Typesetter::fits(std::string_view text, const TextStyle& style,
                      LayoutWidth width, provider::SurfaceScale scale,
                      const TextFitting& fitting) -> bool {
  const auto laid = layout(text, style, width, scale);
  if (!laid.has_value()) {
    return false;
  }
  const TextLayout& made = **laid;
  if (fitting.lines.has_value() && made.lines().size() > *fitting.lines) {
    return false;
  }
  if (fitting.height.has_value() &&
      made.size().height > *fitting.height + 0.01F) {
    return false;
  }
  return !width.bounded() || made.size().width <= width.value() + 0.01F;
}

auto Typesetter::scaled(const TextStyle& style, float factor) -> TextStyle {
  TextStyle result = style;
  result.size =
      PointSize::create(style.size.value() * factor).value_or(style.size);
  if (style.line_height.has_value()) {
    result.line_height = PointSize::create(style.line_height->value() * factor);
  }
  result.tracking = style.tracking * factor;
  result.kerning = style.kerning * factor;
  result.line_spacing = style.line_spacing * factor;
  result.baseline_offset = style.baseline_offset * factor;
  if (std::ranges::any_of(style.runs.runs(), [](const TextRun& run) {
        return run.size.has_value() || run.attachment.has_value();
      })) {
    std::vector<TextRun> runs{style.runs.runs().begin(),
                              style.runs.runs().end()};
    for (TextRun& run : runs) {
      if (run.size.has_value()) {
        run.size = PointSize::create(run.size->value() * factor);
      }
      if (run.attachment.has_value()) {
        run.attachment->width *= factor;
        run.attachment->height *= factor;
        run.attachment->descent *= factor;
      }
    }
    result.runs = TextRuns{std::move(runs)};
  }
  return result;
}

auto Typesetter::tightened(std::string_view text, const TextStyle& style,
                           LayoutWidth width, provider::SurfaceScale scale,
                           const TextFitting& fitting)
    -> std::optional<TextStyle> {
  if (fitting.lines != std::optional<std::size_t>{1}) {
    return std::nullopt;
  }
  const auto natural = measure(text, style, LayoutWidth::unbounded(), scale);
  if (!natural.has_value() || natural->width <= 0.0F) {
    return std::nullopt;
  }
  const float excess = natural->width - width.value();
  if (excess <= 0.0F || excess > natural->width * maximum_tightening) {
    return std::nullopt;
  }
  const TextBreaks breaks = TextBreaks::analyse(text, style.locale.language());
  std::size_t clusters = 0;
  for (std::size_t offset = 1; offset <= text.size(); ++offset) {
    clusters += breaks.grapheme(offset) ? 1U : 0U;
  }
  if (clusters == 0) {
    return std::nullopt;
  }
  TextStyle tight = style;
  tight.tracking -= (excess + 0.02F) / static_cast<float>(clusters);
  if (!fits(text, tight, width, scale, fitting)) {
    return std::nullopt;
  }
  return tight;
}

auto Typesetter::candidate(std::string_view text, const TextStyle& style,
                           std::span<const std::size_t> boundaries,
                           std::size_t kept, TruncationMode mode)
    -> FittedText {
  const std::size_t count = boundaries.size() - 1;
  const auto trimmed_back = [](std::string_view part) {
    while (!part.empty() && part.back() == ' ') {
      part.remove_suffix(1);
    }
    return part;
  };
  const auto trimmed_front = [](std::string_view part) {
    while (!part.empty() && part.front() == ' ') {
      part.remove_prefix(1);
    }
    return part;
  };
  std::string_view head;
  std::string_view tail;
  switch (mode) {
    case TruncationMode::tail:
      head = trimmed_back(text.substr(0, boundaries[kept]));
      break;
    case TruncationMode::head:
      tail = trimmed_front(text.substr(boundaries[count - kept]));
      break;
    case TruncationMode::middle: {
      const std::size_t front = (kept + 1) / 2;
      const std::size_t back = kept / 2;
      head = trimmed_back(text.substr(0, boundaries[front]));
      tail = trimmed_front(text.substr(boundaries[count - back]));
      break;
    }
  }
  FittedText made{.text = {}, .style = style};
  made.text.reserve(head.size() + ellipsis.size() + tail.size());
  made.text.append(head);
  made.text.append(ellipsis);
  made.text.append(tail);
  if (!style.runs.empty()) {
    made.style.runs = style.runs.spliced(
        static_cast<std::uint32_t>(head.size()),
        static_cast<std::uint32_t>(ellipsis.size()),
        static_cast<std::uint32_t>(text.size() - tail.size()),
        static_cast<std::uint32_t>(text.size()));
  }
  return made;
}

auto Typesetter::truncated(std::string_view text, const TextStyle& style,
                           LayoutWidth width, provider::SurfaceScale scale,
                           std::size_t lines, TruncationMode mode)
    -> std::expected<std::string, TextError> {
  auto made = cut(text, style, width, scale, lines, mode);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  return std::move(made->text);
}

auto Typesetter::cut(std::string_view text, const TextStyle& style,
                     LayoutWidth width, provider::SurfaceScale scale,
                     std::size_t lines, TruncationMode mode)
    -> std::expected<FittedText, TextError> {
  if (lines == 0 || !width.bounded() ||
      fits(text, style, width, scale, lines)) {
    return FittedText{.text = std::string{text}, .style = style};
  }
  const TextBreaks breaks = TextBreaks::analyse(text, style.locale.language());
  std::vector<std::size_t> boundaries{0};
  for (std::size_t offset = 1; offset <= text.size(); ++offset) {
    if (breaks.grapheme(offset)) {
      boundaries.push_back(offset);
    }
  }
  if (boundaries.back() != text.size()) {
    boundaries.push_back(text.size());
  }
  std::size_t low = 0;
  std::size_t high = boundaries.size() - 1;
  while (low + 1 < high) {
    const std::size_t middle = low + ((high - low) / 2);
    const FittedText trial = candidate(text, style, boundaries, middle, mode);
    if (fits(trial.text, trial.style, width, scale, lines)) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return candidate(text, style, boundaries, low, mode);
}

auto Typesetter::fitted(std::string_view text, const TextStyle& style,
                        LayoutWidth width, provider::SurfaceScale scale,
                        const TextFitting& fitting)
    -> std::expected<std::shared_ptr<const FittedText>, TextError> {
  const CacheKey::Probe key{text, style, width.value(), scale.value(), fitting};
  if (auto found = cache_->fits().find(key)) {
    return std::move(*found);
  }
  auto made = fit(text, style, width, scale, fitting);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  auto shared = std::make_shared<const FittedText>(std::move(*made));
  cache_->fits().store(CacheKey{key}, shared);
  return shared;
}

auto Typesetter::fit(std::string_view text, const TextStyle& style,
                     LayoutWidth width, provider::SurfaceScale scale,
                     const TextFitting& fitting)
    -> std::expected<FittedText, TextError> {
  if (!width.bounded() ||
      (!fitting.lines.has_value() && !fitting.height.has_value()) ||
      fits(text, style, width, scale, fitting)) {
    return FittedText{.text = std::string{text}, .style = style};
  }
  if (fitting.allows_tightening) {
    if (auto tight = tightened(text, style, width, scale, fitting)) {
      return FittedText{.text = std::string{text}, .style = *tight};
    }
  }
  TextStyle chosen = style;
  const float smallest = std::clamp(fitting.minimum_scale_factor, 0.01F, 1.0F);
  if (smallest < 1.0F) {
    chosen = scaled(style, smallest);
    if (fits(text, chosen, width, scale, fitting)) {
      float low = smallest;
      float high = 1.0F;
      for (int step = 0; step < scale_steps; ++step) {
        const float middle = (low + high) * 0.5F;
        if (fits(text, scaled(style, middle), width, scale, fitting)) {
          low = middle;
        } else {
          high = middle;
        }
      }
      return FittedText{.text = std::string{text}, .style = scaled(style, low)};
    }
  }
  if (!fitting.lines.has_value()) {
    return FittedText{.text = std::string{text}, .style = chosen};
  }
  return cut(text, chosen, width, scale, *fitting.lines, fitting.truncation);
}

auto Typesetter::statistics() const noexcept -> CacheStatistics {
  CacheStatistics current = statistics_;
  if (cache_ != nullptr) {
    current.layouts = cache_->layout_count();
    current.shaped = cache_->shaped_count();
  }
  return current;
}

void Typesetter::clear() noexcept {
  if (cache_ != nullptr) {
    cache_->clear();
  }
}

}
