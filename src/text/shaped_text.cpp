#include <arisa/text/case_mapping.hpp>
#include <arisa/text/utf8.hpp>
#include <text/shaped_text.hpp>

#include <SheenBidi/SheenBidi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace arisa::text {

ShapedText::ShapedText(std::unique_ptr<const std::string> text,
                       std::unique_ptr<const std::string> cased,
                       TextStyle style, Chains chains, BidiText bidi,
                       TextBreaks breaks, std::vector<Run> runs) noexcept
    : text_(std::move(text)),
      cased_(std::move(cased)),
      style_(std::move(style)),
      chains_(std::move(chains)),
      bidi_(std::move(bidi)),
      breaks_(std::move(breaks)),
      runs_(std::move(runs)) {
  const Locale& shaping = style_.language.value_or(style_.locale);
  if (shaping.tag() != "und") {
    language_ = std::string{shaping.tag()};
  }
}

auto ShapedText::create(const FontCollection& fonts, std::string_view text,
                        const TextStyle& style, Shaper& shaper)
    -> std::expected<ShapedText, TextError> {
  if (text.size() >= std::numeric_limits<std::uint32_t>::max() ||
      !Utf8::valid(text)) {
    return std::unexpected(TextError{TextErrorCode::invalid_text});
  }
  if (!std::isfinite(style.tracking) || !std::isfinite(style.kerning) ||
      !std::isfinite(style.baseline_offset) ||
      !std::isfinite(style.line_spacing) || !std::isfinite(style.stretch) ||
      style.stretch <= 0.0F) {
    return std::unexpected(TextError{TextErrorCode::invalid_argument});
  }
  std::vector<FaceQuery> queries{query(style)};
  for (const TextRun& run : style.runs.runs()) {
    const FaceQuery wanted = query(style, run);
    if (std::ranges::find(queries, wanted) == queries.end()) {
      queries.push_back(wanted);
    }
  }
  Chains chains;
  chains.reserve(queries.size());
  for (const FaceQuery& wanted : queries) {
    chains.push_back(fonts.chain(style.locale, wanted));
    if (chains.back()->empty()) {
      return std::unexpected(TextError{TextErrorCode::missing_font});
    }
  }
  auto owned = std::make_unique<const std::string>(text);
  const std::string_view view = *owned;
  auto bidi =
      BidiText::create(view, style.locale.right_to_left(), style.direction);
  if (!bidi.has_value()) {
    return std::unexpected(bidi.error());
  }
  auto scripts = BidiText::scripts(view);
  if (!scripts.has_value()) {
    return std::unexpected(scripts.error());
  }
  TextBreaks breaks = TextBreaks::analyse(
      view, style.language.value_or(style.locale).language());
  std::vector<std::vector<std::uint32_t>> private_use;
  private_use.reserve(chains.size());
  for (const auto& chain : chains) {
    private_use.push_back(private_use_faces(fonts, *chain));
  }
  std::vector<Run> runs = itemise(view, style, *bidi, breaks, *scripts, queries,
                                  chains, private_use);
  std::unique_ptr<const std::string> cased = cased_text(view, style, breaks);
  ShapedText shaped{std::move(owned),  std::move(cased), style,
                    std::move(chains), std::move(*bidi), std::move(breaks),
                    std::move(runs)};
  shaped.shape_all(shaper);
  return shaped;
}

auto ShapedText::query(const TextStyle& style) noexcept -> FaceQuery {
  return FaceQuery{.weight = style.weight,
                   .design = style.design,
                   .italic = style.italic,
                   .typeface = style.typeface,
                   .stretch = style.stretch,
                   .name = style.name};
}

auto ShapedText::query(const TextStyle& style, const TextRun& run) noexcept
    -> FaceQuery {
  FaceQuery wanted = query(style);
  wanted.weight = run.weight.value_or(style.weight);
  wanted.italic = style.italic || run.italic;
  wanted.typeface = run.typeface.value_or(style.typeface);
  if (run.typeface.has_value()) {
    wanted.name.reset();
  }
  if (run.name.has_value()) {
    wanted.name = run.name;
  }
  wanted.stretch = run.stretch.value_or(style.stretch);
  return wanted;
}

auto ShapedText::chain_of(const TextStyle& style,
                          std::span<const FaceQuery> queries,
                          std::uint32_t offset) noexcept -> std::uint16_t {
  const auto run = style.runs.find(offset);
  if (!run.has_value()) {
    return 0;
  }
  const auto found = std::ranges::find(queries, query(style, run->get()));
  return found == queries.end()
             ? 0
             : static_cast<std::uint16_t>(found - queries.begin());
}

auto ShapedText::face_of(const Run& run) const noexcept -> const FontFace& {
  return *(*chains_[run.chain])[run.face];
}

auto ShapedText::shared_face_of(const Run& run) const noexcept
    -> const std::shared_ptr<const FontFace>& {
  return (*chains_[run.chain])[run.face];
}

auto ShapedText::tag(char first, char second, char third, char fourth) noexcept
    -> std::uint32_t {
  return (static_cast<std::uint32_t>(static_cast<unsigned char>(first))
          << 24U) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(second))
          << 16U) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(third)) << 8U) |
         static_cast<std::uint32_t>(static_cast<unsigned char>(fourth));
}

auto ShapedText::ignorable(char32_t codepoint) noexcept -> bool {
  if ((codepoint >= 0xFE00 && codepoint <= 0xFE0F) ||
      (codepoint >= 0xE0100 && codepoint <= 0xE01EF) || codepoint == 0x034F) {
    return true;
  }
  const SBGeneralCategory category = SBCodepointGetGeneralCategory(codepoint);
  return category == SBGeneralCategoryCC || category == SBGeneralCategoryCF ||
         category == SBGeneralCategoryZL || category == SBGeneralCategoryZP;
}

auto ShapedText::control(char32_t codepoint) noexcept -> bool {
  const SBGeneralCategory category = SBCodepointGetGeneralCategory(codepoint);
  return category == SBGeneralCategoryCC || category == SBGeneralCategoryZL ||
         category == SBGeneralCategoryZP;
}

auto ShapedText::mandatory_break(char32_t codepoint) noexcept -> bool {
  return codepoint == U'\n' || codepoint == U'\r' || codepoint == 0x0B ||
         codepoint == 0x0C || codepoint == 0x85 || codepoint == 0x2028 ||
         codepoint == 0x2029;
}

auto ShapedText::hangs(char32_t codepoint) noexcept -> bool {
  return codepoint == U' ' || codepoint == U'\t' || codepoint == 0x3000 ||
         mandatory_break(codepoint);
}

auto ShapedText::cursive(std::uint32_t script) noexcept -> bool {
  constexpr std::array<std::array<char, 4>, 7> scripts{{{'A', 'r', 'a', 'b'},
                                                        {'S', 'y', 'r', 'c'},
                                                        {'M', 'o', 'n', 'g'},
                                                        {'N', 'k', 'o', 'o'},
                                                        {'A', 'd', 'l', 'm'},
                                                        {'R', 'o', 'h', 'g'},
                                                        {'M', 'a', 'n', 'd'}}};
  return std::ranges::any_of(
      scripts, [script](const std::array<char, 4>& name) {
        return tag(name[0], name[1], name[2], name[3]) == script;
      });
}

auto ShapedText::private_use_faces(
    const FontCollection& fonts,
    const std::vector<std::shared_ptr<const FontFace>>& chain)
    -> std::vector<std::uint32_t> {
  std::vector<std::uint32_t> indices;
  for (std::uint32_t index = 0; index < chain.size(); ++index) {
    const bool symbols =
        std::ranges::any_of(fonts.entries(), [&](const FontEntry& entry) {
          return entry.role == FontRole::symbols && entry.face == chain[index];
        });
    if (symbols) {
      indices.push_back(index);
    }
  }
  return indices;
}

auto ShapedText::select_face(
    std::string_view text, std::size_t start, std::size_t end,
    std::optional<std::uint32_t> previous,
    const std::vector<std::shared_ptr<const FontFace>>& chain,
    std::span<const std::uint32_t> private_use) -> std::uint32_t {
  std::array<char32_t, 16> significant{};
  std::size_t count = 0;
  bool common = true;
  bool private_codepoint = false;
  for (std::size_t offset = start; offset < end;) {
    const Utf8::Decoded decoded = Utf8::decode(text, offset);
    offset += decoded.length;
    if (ignorable(decoded.codepoint)) {
      continue;
    }
    const SBScript script = SBCodepointGetScript(decoded.codepoint);
    if (script != SBScriptZYYY && script != SBScriptZINH) {
      common = false;
    }
    if (SBCodepointGetGeneralCategory(decoded.codepoint) ==
        SBGeneralCategoryCO) {
      private_codepoint = true;
    }
    if (count < significant.size()) {
      significant[count++] = decoded.codepoint;
    }
  }
  const std::span<const char32_t> needed{significant.data(), count};
  const auto covers_all = [needed](const FontFace& face) {
    return std::ranges::all_of(
        needed, [&face](char32_t codepoint) { return face.covers(codepoint); });
  };
  if (private_codepoint) {
    const auto symbols = std::ranges::find_if(
        private_use,
        [&](std::uint32_t index) { return covers_all(*chain[index]); });
    if (symbols != private_use.end()) {
      return *symbols;
    }
  }
  if (previous.has_value() && (common || needed.empty()) &&
      covers_all(*chain[*previous])) {
    return *previous;
  }
  if (needed.empty()) {
    return previous.value_or(0);
  }
  for (std::uint32_t index = 0; index < chain.size(); ++index) {
    if (covers_all(*chain[index])) {
      return index;
    }
  }
  for (std::uint32_t index = 0; index < chain.size(); ++index) {
    if (chain[index]->covers(needed.front())) {
      return index;
    }
  }
  return previous.value_or(0);
}

auto ShapedText::small_capital(std::string_view grapheme, SmallCaps caps)
    -> std::optional<bool> {
  if (caps == SmallCaps::none || grapheme.empty()) {
    return std::nullopt;
  }
  const std::string upper = CaseMapping::uppercased(grapheme);
  const std::string lower = CaseMapping::lowercased(grapheme);
  if (upper == lower) {
    return std::nullopt;
  }
  if (grapheme == lower && SmallCapitals::lowers(caps)) {
    return upper.size() == grapheme.size();
  }
  if (grapheme == upper && SmallCapitals::uppers(caps)) {
    return false;
  }
  return std::nullopt;
}

auto ShapedText::shaping_of(const TextStyle& style, std::string_view text,
                            std::size_t start, std::size_t end) -> Shaping {
  Shaping shaping{.size = style.size,
                  .spread = style.tracking + style.kerning,
                  .rise = 0.0F,
                  .cased = false,
                  .tabular = style.tabular_digits,
                  .attachment = std::nullopt};
  SmallCaps caps = style.small_caps;
  if (const auto found = style.runs.find(static_cast<std::uint32_t>(start))) {
    const TextRun& run = found->get();
    shaping.size = run.size.value_or(style.size);
    shaping.spread = style.tracking + run.tracking.value_or(0.0F) +
                     run.kerning.value_or(style.kerning);
    shaping.rise = run.baseline_offset.value_or(0.0F);
    shaping.tabular = style.tabular_digits || run.tabular_digits;
    shaping.attachment = run.attachment;
    caps = SmallCapitals::combined(caps, run.small_caps);
  }
  if (const std::optional<bool> small =
          small_capital(text.substr(start, end - start), caps)) {
    shaping.cased = *small;
    shaping.size =
        PointSize::create(shaping.size.value() * SmallCapitals::scale)
            .value_or(shaping.size);
  }
  return shaping;
}

auto ShapedText::cased_text(std::string_view text, const TextStyle& style,
                            const TextBreaks& breaks)
    -> std::unique_ptr<const std::string> {
  const bool any =
      style.small_caps != SmallCaps::none ||
      std::ranges::any_of(style.runs.runs(), [](const TextRun& run) {
        return run.small_caps != SmallCaps::none;
      });
  if (!any) {
    return nullptr;
  }
  std::string cased{text};
  std::size_t start = 0;
  for (std::size_t offset = 1; offset <= text.size(); ++offset) {
    if (!breaks.grapheme(offset) && offset != text.size()) {
      continue;
    }
    if (shaping_of(style, text, start, offset).cased) {
      const std::string upper =
          CaseMapping::uppercased(text.substr(start, offset - start));
      cased.replace(start, upper.size(), upper);
    }
    start = offset;
  }
  return std::make_unique<const std::string>(std::move(cased));
}

auto ShapedText::itemise(
    std::string_view text, const TextStyle& style, const BidiText& bidi,
    const TextBreaks& breaks, const std::vector<std::uint32_t>& scripts,
    std::span<const FaceQuery> queries, const Chains& chains,
    const std::vector<std::vector<std::uint32_t>>& private_use)
    -> std::vector<Run> {
  std::vector<Run> runs;
  if (text.empty()) {
    return runs;
  }
  const std::uint32_t common = tag('Z', 'y', 'y', 'y');
  std::optional<std::uint32_t> previous;
  std::uint16_t previous_chain = 0;
  std::uint32_t grapheme_start = 0;
  for (std::uint32_t offset = 1; offset <= text.size(); ++offset) {
    if (!breaks.grapheme(offset) && offset != text.size()) {
      continue;
    }
    const std::uint16_t chain = chain_of(style, queries, grapheme_start);
    if (chain != previous_chain) {
      previous.reset();
      previous_chain = chain;
    }
    const std::uint32_t face =
        select_face(text, grapheme_start, offset, previous, *chains[chain],
                    private_use[chain]);
    previous = face;
    const std::uint8_t level = bidi.level(grapheme_start);
    const std::uint32_t script =
        scripts[grapheme_start] == 0 ? common : scripts[grapheme_start];
    const Shaping shaping = shaping_of(style, text, grapheme_start, offset);
    if (runs.empty() || runs.back().face != face ||
        runs.back().chain != chain || runs.back().level != level ||
        runs.back().script != script || runs.back().shaping != shaping ||
        shaping.attachment.has_value()) {
      runs.push_back(Run{.start = grapheme_start,
                         .end = offset,
                         .face = face,
                         .chain = chain,
                         .level = level,
                         .script = script,
                         .first_glyph = 0,
                         .glyph_count = 0,
                         .shaping = shaping});
    } else {
      runs.back().end = offset;
    }
    grapheme_start = offset;
  }
  return runs;
}

void ShapedText::shape_all(Shaper& shaper) {
  const std::size_t size = text_->size();
  std::vector<float> advances(size + 1, 0.0F);
  clusters_.assign(size + 1, 0);
  for (Run& run : runs_) {
    std::vector<ShapedGlyph> shaped =
        shape_range(shaper, run, run.start, run.end, (run.level & 1U) != 0);
    run.first_glyph = static_cast<std::uint32_t>(glyphs_.size());
    run.glyph_count = static_cast<std::uint32_t>(shaped.size());
    for (const ShapedGlyph& glyph : shaped) {
      if (glyph.cluster < size) {
        advances[glyph.cluster] += glyph.advance;
        clusters_[glyph.cluster] |= cluster_start;
        if (glyph.unsafe_to_break) {
          clusters_[glyph.cluster] |= cluster_unsafe;
        }
      }
    }
    glyphs_.insert(glyphs_.end(), shaped.begin(), shaped.end());
  }
  prefix_.assign(size + 1, 0.0F);
  for (std::size_t index = 0; index < size; ++index) {
    prefix_[index + 1] = prefix_[index] + advances[index];
  }
}

auto ShapedText::shape_range(Shaper& shaper, const Run& run,
                             std::uint32_t start, std::uint32_t end,
                             bool right_to_left) const
    -> std::vector<ShapedGlyph> {
  return finish(
      shaper.shape(face_of(run),
                   ShapeRequest{.text = run.shaping.cased && cased_ != nullptr
                                            ? std::string_view{*cased_}
                                            : std::string_view{*text_},
                                .start = start,
                                .end = end,
                                .right_to_left = right_to_left,
                                .script = run.script,
                                .language = language_,
                                .size = run.shaping.size,
                                .tabular_digits = run.shaping.tabular}),
      run);
}

auto ShapedText::finish(std::vector<ShapedGlyph> glyphs, const Run& run) const
    -> std::vector<ShapedGlyph> {
  const std::uint32_t space =
      face_of(run).glyph(U' ').value_or(GlyphId{0}).value();
  for (ShapedGlyph& glyph : glyphs) {
    if (control(Utf8::decode(*text_, glyph.cluster).codepoint)) {
      glyph.glyph = space;
      glyph.advance = 0.0F;
      glyph.x_offset = 0.0F;
      glyph.y_offset = 0.0F;
    }
  }
  if (run.shaping.attachment.has_value()) {
    for (std::size_t index = 0; index < glyphs.size(); ++index) {
      glyphs[index].advance = index == 0 ? run.shaping.attachment->width : 0.0F;
      glyphs[index].x_offset = 0.0F;
      glyphs[index].y_offset = 0.0F;
    }
    return glyphs;
  }
  const float spread = run.shaping.spread;
  if (spread != 0.0F && !cursive(run.script)) {
    for (std::size_t index = 0; index < glyphs.size(); ++index) {
      const bool last = index + 1 == glyphs.size() ||
                        glyphs[index + 1].cluster != glyphs[index].cluster;
      if (last) {
        glyphs[index].advance += spread;
      }
    }
  }
  return glyphs;
}

auto ShapedText::width(std::size_t start, std::size_t end) const noexcept
    -> float {
  return prefix_[end] - prefix_[start];
}

auto ShapedText::trim(std::size_t start, std::size_t end) const noexcept
    -> std::uint32_t {
  std::size_t visible = end;
  while (visible > start) {
    const std::size_t previous = Utf8::previous(*text_, visible);
    if (!hangs(Utf8::decode(*text_, previous).codepoint)) {
      break;
    }
    visible = previous;
  }
  return static_cast<std::uint32_t>(visible);
}

auto ShapedText::emergency(std::size_t start, std::size_t limit,
                           float maximum) const noexcept -> std::size_t {
  std::size_t best = start;
  std::size_t clean = start;
  std::size_t first = 0;
  for (std::size_t offset = start + 1; offset <= limit; ++offset) {
    if (!breaks_.grapheme(offset)) {
      continue;
    }
    if (first == 0) {
      first = offset;
    }
    if (width(start, offset) > maximum + fit_tolerance) {
      break;
    }
    best = offset;
    const bool kinsoku =
        offset < text_->size() &&
        (TextBreaks::forbids_line_start(
             Utf8::decode(*text_, offset).codepoint) ||
         TextBreaks::forbids_line_end(
             Utf8::decode(*text_, Utf8::previous(*text_, offset)).codepoint));
    if (!kinsoku) {
      clean = offset;
    }
  }
  if (clean > start) {
    return clean;
  }
  if (best > start) {
    return best;
  }
  return first == 0 ? limit : first;
}

auto ShapedText::wrap(LayoutWidth constraint) const -> std::vector<LineSpan> {
  const std::size_t size = text_->size();
  std::vector<LineSpan> spans;
  if (size == 0) {
    spans.push_back(LineSpan{});
    return spans;
  }
  const float maximum = constraint.value();
  std::size_t start = 0;
  while (start < size) {
    const std::size_t paragraph_end = bidi_.paragraph_end(start);
    std::size_t fitted = start;
    std::size_t end = size;
    for (std::size_t offset = start + 1; offset <= size; ++offset) {
      LineBreak kind = breaks_.line(offset);
      if (offset == paragraph_end) {
        kind = LineBreak::mandatory;
      }
      if (kind == LineBreak::none) {
        continue;
      }
      const bool fits =
          !constraint.bounded() ||
          width(start, trim(start, offset)) <= maximum + fit_tolerance;
      if (fits) {
        if (kind == LineBreak::mandatory) {
          end = offset;
          break;
        }
        fitted = offset;
        continue;
      }
      end = fitted > start ? fitted : emergency(start, offset, maximum);
      break;
    }
    spans.push_back(LineSpan{.start = static_cast<std::uint32_t>(start),
                             .end = static_cast<std::uint32_t>(end),
                             .visible_end = trim(start, end)});
    start = end;
  }
  if (mandatory_break(
          Utf8::decode(*text_, Utf8::previous(*text_, size)).codepoint)) {
    const auto at = static_cast<std::uint32_t>(size);
    spans.push_back(LineSpan{.start = at, .end = at, .visible_end = at});
  }
  return spans;
}

auto ShapedText::safe_edge(const Run& run, std::size_t offset) const noexcept
    -> bool {
  if (offset == run.start || offset == run.end) {
    return true;
  }
  return (clusters_[offset] & cluster_start) != 0 &&
         (clusters_[offset] & cluster_unsafe) == 0;
}

auto ShapedText::run_glyphs(const Run& run) const noexcept
    -> std::span<const ShapedGlyph> {
  return std::span{glyphs_}.subspan(run.first_glyph, run.glyph_count);
}

auto ShapedText::pieces(const LineSpan& span, Shaper& shaper) const
    -> std::vector<Piece> {
  std::vector<Piece> result;
  if (span.visible_end <= span.start) {
    return result;
  }
  for (std::uint32_t index = 0; index < runs_.size(); ++index) {
    const Run& run = runs_[index];
    if (run.end <= span.start || run.start >= span.visible_end) {
      continue;
    }
    Piece piece;
    piece.run = index;
    piece.start = std::max(span.start, run.start);
    piece.end = std::min(span.visible_end, run.end);
    if (safe_edge(run, piece.start) && safe_edge(run, piece.end)) {
      piece.width = width(piece.start, piece.end);
    } else {
      piece.reshaped = true;
      piece.glyphs = shape_range(shaper, run, piece.start, piece.end,
                                 (run.level & 1U) != 0);
      for (const ShapedGlyph& glyph : piece.glyphs) {
        piece.width += glyph.advance;
      }
    }
    result.push_back(std::move(piece));
  }
  return result;
}

auto ShapedText::line_box(const std::vector<Piece>& pieces, float top,
                          provider::SurfaceScale scale) const -> LineBox {
  const FaceMetrics primary = chains_.front()->front()->metrics(style_.size);
  LineBox box{.ascent = primary.ascent, .descent = primary.descent};
  float gap = primary.line_gap;
  bool grown = false;
  for (const Piece& piece : pieces) {
    box.width += piece.width;
    const Run& run = runs_[piece.run];
    const FaceMetrics metrics = face_of(run).metrics(run.shaping.size);
    const float rise = run.shaping.rise;
    if (run.shaping.attachment.has_value()) {
      const TextAttachment& attached = *run.shaping.attachment;
      box.ascent = std::max(box.ascent, attached.height - attached.descent);
      box.descent = std::max(box.descent, attached.descent);
      grown = grown || attached.height > primary.ascent + primary.descent;
      continue;
    }
    if (run.shaping.size > style_.size || rise != 0.0F) {
      grown = true;
    }
    box.ascent = std::max(box.ascent, metrics.ascent + std::max(rise, 0.0F));
    box.descent =
        std::max(box.descent, metrics.descent + std::max(-rise, 0.0F));
    gap = std::max(gap, metrics.line_gap);
  }
  const float natural = box.ascent + box.descent + gap;
  const float base = style_.line_height.has_value() && !grown
                         ? style_.line_height->value()
                         : natural;
  box.height = line_height(pieces, base).value_or(base);
  const float within =
      ((box.height - (box.ascent + box.descent)) / 2.0F) + box.ascent;
  const float pixels = scale.value();
  box.baseline = std::round((top + within) * pixels) / pixels;
  return box;
}

auto ShapedText::line_height(const std::vector<Piece>& pieces,
                             float natural) const -> std::optional<float> {
  std::optional<float> chosen;
  const auto consider = [&](const std::optional<LineHeight>& rule,
                            PointSize size) {
    if (!rule.has_value()) {
      return;
    }
    if (const std::optional<float> points =
            rule->points(size.value(), natural)) {
      chosen = std::max(chosen.value_or(0.0F), *points);
    }
  };
  consider(style_.line_rule, style_.size);
  for (const Piece& piece : pieces) {
    const auto found = style_.runs.find(piece.start);
    if (found.has_value()) {
      consider(found->get().line_height,
               found->get().size.value_or(style_.size));
    }
  }
  return chosen;
}

auto ShapedText::alignment_at(std::uint32_t offset) const noexcept
    -> TextAlignment {
  const auto found = style_.runs.find(offset);
  if (found.has_value() && found->get().alignment.has_value()) {
    return *found->get().alignment;
  }
  return style_.alignment;
}

auto ShapedText::measure(LayoutWidth width, provider::SurfaceScale scale,
                         Shaper& shaper) const -> TextSize {
  TextSize size;
  bool first = true;
  for (const LineSpan& span : wrap(width)) {
    if (!std::exchange(first, false)) {
      size.height += style_.line_spacing;
    }
    const LineBox box = line_box(pieces(span, shaper), size.height, scale);
    size.width = std::max(size.width, box.width);
    size.height += box.height;
  }
  size.height += std::abs(style_.baseline_offset);
  return size;
}

auto ShapedText::layout(LayoutWidth width, provider::SurfaceScale scale,
                        Shaper& shaper) const -> TextLayout {
  std::vector<TextLine> lines;
  std::vector<GlyphRun> runs;
  std::vector<PositionedGlyph> glyphs;
  TextSize size;
  const float lowered = std::max(-style_.baseline_offset, 0.0F);
  for (const LineSpan& span : wrap(width)) {
    if (!lines.empty()) {
      size.height += style_.line_spacing;
    }
    const std::vector<Piece> line_pieces = pieces(span, shaper);
    const LineBox box = line_box(line_pieces, size.height, scale);
    TextLine line{.text_start = span.start,
                  .text_end = span.end,
                  .first_run = static_cast<std::uint32_t>(runs.size()),
                  .top = size.height,
                  .baseline = box.baseline + lowered,
                  .width = box.width,
                  .height = box.height,
                  .right_to_left = bidi_.paragraph_right_to_left(span.start)};
    float pen = 0.0F;
    for (const BidiText::VisualRun& visual :
         bidi_.visual_runs(span.start, span.visible_end)) {
      const bool right_to_left = (visual.level & 1U) != 0;
      std::vector<std::size_t> order;
      for (std::size_t index = 0; index < line_pieces.size(); ++index) {
        if (line_pieces[index].end > visual.start &&
            line_pieces[index].start < visual.end) {
          order.push_back(index);
        }
      }
      if (right_to_left) {
        std::ranges::reverse(order);
      }
      for (const std::size_t index : order) {
        const Piece& piece = line_pieces[index];
        const Run& run = runs_[piece.run];
        const std::uint32_t start = std::max(piece.start, visual.start);
        const std::uint32_t end = std::min(piece.end, visual.end);
        std::vector<ShapedGlyph> chunk;
        if (start == piece.start && end == piece.end &&
            right_to_left == ((run.level & 1U) != 0)) {
          if (piece.reshaped) {
            chunk = piece.glyphs;
          } else {
            for (const ShapedGlyph& glyph : run_glyphs(run)) {
              if (glyph.cluster >= start && glyph.cluster < end) {
                chunk.push_back(glyph);
              }
            }
          }
        } else {
          chunk = shape_range(shaper, run, start, end, right_to_left);
        }
        runs.push_back(
            GlyphRun{.face = shared_face_of(run),
                     .size = run.shaping.size,
                     .right_to_left = right_to_left,
                     .text_start = start,
                     .text_end = end,
                     .first_glyph = static_cast<std::uint32_t>(glyphs.size()),
                     .glyph_count = static_cast<std::uint32_t>(chunk.size())});
        for (const ShapedGlyph& glyph : chunk) {
          glyphs.push_back(PositionedGlyph{
              .glyph = GlyphId{glyph.glyph},
              .x = pen + glyph.x_offset,
              .y = box.baseline + lowered - glyph.y_offset - run.shaping.rise,
              .advance = glyph.advance,
              .cluster = glyph.cluster});
          pen += glyph.advance;
        }
      }
    }
    line.run_count = static_cast<std::uint32_t>(runs.size()) - line.first_run;
    size.width = std::max(size.width, box.width);
    size.height += box.height;
    lines.push_back(line);
  }
  size.height += std::abs(style_.baseline_offset);
  for (TextLine& line : lines) {
    float factor = 0.0F;
    switch (alignment_at(line.text_start)) {
      case TextAlignment::natural:
        factor = line.right_to_left ? 1.0F : 0.0F;
        break;
      case TextAlignment::opposite:
        factor = line.right_to_left ? 0.0F : 1.0F;
        break;
      case TextAlignment::left:
        factor = 0.0F;
        break;
      case TextAlignment::center:
        factor = 0.5F;
        break;
      case TextAlignment::right:
        factor = 1.0F;
        break;
    }
    line.x = (size.width - line.width) * factor;
    if (line.x == 0.0F) {
      continue;
    }
    for (std::uint32_t index = 0; index < line.run_count; ++index) {
      const GlyphRun& run = runs[line.first_run + index];
      for (PositionedGlyph& glyph :
           std::span{glyphs}.subspan(run.first_glyph, run.glyph_count)) {
        glyph.x += line.x;
      }
    }
  }
  return TextLayout{
      style_,           width, scale, size, std::move(lines), std::move(runs),
      std::move(glyphs)};
}

}
