#include <arisa/text/text_runs.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <utility>

namespace arisa::text {

TextRuns::TextRuns(std::vector<TextRun> runs) {
  std::erase_if(runs, [](const TextRun& run) { return run.end <= run.start; });
  if (runs.empty()) {
    return;
  }
  std::ranges::stable_sort(runs, {}, &TextRun::start);
  std::uint32_t reached = 0;
  for (TextRun& run : runs) {
    run.start = std::max(run.start, reached);
    reached = std::max(reached, run.end);
  }
  std::erase_if(runs, [](const TextRun& run) { return run.end <= run.start; });
  if (runs.empty()) {
    return;
  }
  runs_ = std::make_shared<const std::vector<TextRun>>(std::move(runs));
}

auto TextRuns::runs() const noexcept -> std::span<const TextRun> {
  if (runs_ == nullptr) {
    return {};
  }
  return *runs_;
}

auto TextRuns::find(std::uint32_t offset) const noexcept
    -> std::optional<std::reference_wrapper<const TextRun>> {
  const std::span<const TextRun> all = runs();
  const auto after = std::ranges::upper_bound(all, offset, {}, &TextRun::start);
  if (after == all.begin()) {
    return std::nullopt;
  }
  const TextRun& run = *std::prev(after);
  if (offset >= run.end) {
    return std::nullopt;
  }
  return std::cref(run);
}

auto TextRuns::spliced(std::uint32_t kept_head, std::uint32_t inserted,
                       std::uint32_t tail_start, std::uint32_t size) const
    -> TextRuns {
  if (runs_ == nullptr) {
    return {};
  }
  std::vector<TextRun> made;
  made.reserve(runs_->size() + 1);
  const std::uint32_t joined = kept_head + inserted;
  for (const TextRun& run : *runs_) {
    if (run.start < kept_head) {
      TextRun head = run;
      head.end = std::min(run.end, kept_head);
      if (run.end >= kept_head && inserted > 0) {
        head.end = joined;
      }
      made.push_back(head);
    }
    if (run.end > tail_start && tail_start < size) {
      TextRun tail = run;
      const std::uint32_t from = std::max(run.start, tail_start);
      tail.start = from - tail_start + joined;
      tail.end = std::min(run.end, size) - tail_start + joined;
      if (run.start <= tail_start && kept_head == 0 && inserted > 0) {
        tail.start = 0;
      }
      made.push_back(tail);
    }
  }
  return TextRuns{std::move(made)};
}

auto TextRuns::shifted(std::uint32_t offset) const -> std::vector<TextRun> {
  std::vector<TextRun> moved{runs().begin(), runs().end()};
  for (TextRun& run : moved) {
    run.start += offset;
    run.end += offset;
  }
  return moved;
}

auto TextRuns::hash() const noexcept -> std::size_t {
  std::size_t seed = 0;
  const auto mix = [&seed](std::size_t value) {
    seed ^= value + 0x9E3779B97F4A7C15ULL + (seed << 6U) + (seed >> 2U);
  };
  const auto real = [](const std::optional<float>& value) -> std::size_t {
    return value.has_value() ? std::hash<float>{}(*value) + 1U : 0U;
  };
  for (const TextRun& run : runs()) {
    const std::size_t value =
        (static_cast<std::size_t>(run.start) << 32U) ^ run.end ^
        (static_cast<std::size_t>(run.weight.has_value() ? run.weight->value()
                                                         : 0)
         << 8U) ^
        (static_cast<std::size_t>(run.italic) << 1U) ^
        (static_cast<std::size_t>(run.typeface.has_value()
                                      ? std::to_underlying(*run.typeface) + 1U
                                      : 0U)
         << 4U) ^
        (static_cast<std::size_t>(run.underline) << 2U) ^
        (static_cast<std::size_t>(run.strikethrough) << 3U) ^
        (static_cast<std::size_t>(run.ink) << 20U) ^
        (static_cast<std::size_t>(run.link) << 40U);
    mix(value);
    mix(real(run.size.transform([](PointSize size) { return size.value(); })));
    mix(real(run.stretch));
    mix(run.name.has_value() ? run.name->value() + 1U : 0U);
    mix(real(run.kerning));
    mix(real(run.tracking));
    mix(real(run.baseline_offset));
    if (run.line_height.has_value()) {
      mix((static_cast<std::size_t>(run.line_height->kind()) + 1U) ^
          std::hash<float>{}(run.line_height->value()));
    }
    mix(run.alignment.has_value()
            ? static_cast<std::size_t>(*run.alignment) + 1U
            : 0U);
    if (run.attachment.has_value()) {
      mix(std::hash<float>{}(run.attachment->width) ^
          (std::hash<float>{}(run.attachment->height) << 1U) ^
          (std::hash<float>{}(run.attachment->descent) << 2U));
    }
    mix(static_cast<std::size_t>(run.small_caps) |
        (static_cast<std::size_t>(run.tabular_digits) << 4U) |
        (static_cast<std::size_t>(run.underline_pattern) << 8U) |
        (static_cast<std::size_t>(run.strikethrough_pattern) << 12U));
    mix(static_cast<std::size_t>(run.underline_ink) |
        (static_cast<std::size_t>(run.strikethrough_ink) << 16U) |
        (static_cast<std::size_t>(run.background) << 32U) |
        (static_cast<std::size_t>(run.font) << 48U));
    mix(static_cast<std::size_t>(run.extra) |
        (static_cast<std::size_t>(run.image) << 16U) |
        (static_cast<std::size_t>(run.glyph) << 32U) |
        (static_cast<std::size_t>(run.shade) << 48U));
  }
  return seed;
}

auto operator==(const TextRuns& left, const TextRuns& right) -> bool {
  if (left.runs_ == right.runs_) {
    return true;
  }
  return std::ranges::equal(left.runs(), right.runs());
}

}
