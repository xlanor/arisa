#include <text/bidi_text.hpp>

#include <SheenBidi/SheenBidi.h>

#include <algorithm>
#include <utility>

namespace arisa::text {

void BidiText::AlgorithmRelease::release(handle_type& handle) noexcept {
  SBAlgorithmRelease(handle);
}

void BidiText::ParagraphRelease::release(handle_type& handle) noexcept {
  SBParagraphRelease(handle);
}

void BidiText::LineRelease::release(handle_type& handle) noexcept {
  SBLineRelease(handle);
}

void BidiText::LocatorRelease::release(handle_type& handle) noexcept {
  SBScriptLocatorRelease(handle);
}

auto BidiText::create(std::string_view text, bool prefer_rtl,
                      WritingDirection direction)
    -> std::expected<BidiText, TextError> {
  BidiText bidi;
  bidi.levels_.assign(text.size(), 0);
  if (text.empty()) {
    return bidi;
  }
  const SBCodepointSequence sequence{.stringEncoding = SBStringEncodingUTF8,
                                     .stringBuffer = text.data(),
                                     .stringLength = text.size()};
  bidi.algorithm_ =
      support::UniqueHandle<AlgorithmRelease>{SBAlgorithmCreate(&sequence)};
  if (!bidi.algorithm_.valid()) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  SBLevel base = prefer_rtl ? SBLevelDefaultRTL : SBLevelDefaultLTR;
  if (direction == WritingDirection::left_to_right) {
    base = 0;
  } else if (direction == WritingDirection::right_to_left) {
    base = 1;
  }
  std::size_t offset = 0;
  while (offset < text.size()) {
    support::UniqueHandle<ParagraphRelease> paragraph{
        SBAlgorithmCreateParagraph(bidi.algorithm_.get(), offset,
                                   text.size() - offset, base)};
    if (!paragraph.valid()) {
      return std::unexpected(TextError{TextErrorCode::font_engine});
    }
    const std::size_t length = SBParagraphGetLength(paragraph.get());
    if (length == 0) {
      return std::unexpected(TextError{TextErrorCode::font_engine});
    }
    const SBLevel* levels = SBParagraphGetLevelsPtr(paragraph.get());
    std::copy_n(levels, length,
                bidi.levels_.begin() + static_cast<std::ptrdiff_t>(offset));
    const bool right_to_left =
        (SBParagraphGetBaseLevel(paragraph.get()) & 1U) != 0;
    bidi.paragraphs_.push_back(Paragraph{.start = offset,
                                         .end = offset + length,
                                         .right_to_left = right_to_left,
                                         .handle = std::move(paragraph)});
    offset += length;
  }
  return bidi;
}

auto BidiText::scripts(std::string_view text)
    -> std::expected<std::vector<std::uint32_t>, TextError> {
  std::vector<std::uint32_t> tags(text.size(), 0);
  if (text.empty()) {
    return tags;
  }
  support::UniqueHandle<LocatorRelease> locator{SBScriptLocatorCreate()};
  if (!locator.valid()) {
    return std::unexpected(TextError{TextErrorCode::font_engine});
  }
  const SBCodepointSequence sequence{.stringEncoding = SBStringEncodingUTF8,
                                     .stringBuffer = text.data(),
                                     .stringLength = text.size()};
  SBScriptLocatorLoadCodepoints(locator.get(), &sequence);
  while (SBScriptLocatorMoveNext(locator.get()) != 0) {
    const SBScriptAgent* agent = SBScriptLocatorGetAgent(locator.get());
    const std::uint32_t tag = SBScriptGetUnicodeTag(agent->script);
    const std::size_t end =
        std::min(text.size(), agent->offset + agent->length);
    std::fill(tags.begin() + static_cast<std::ptrdiff_t>(agent->offset),
              tags.begin() + static_cast<std::ptrdiff_t>(end), tag);
  }
  return tags;
}

auto BidiText::level(std::size_t offset) const noexcept -> std::uint8_t {
  return offset < levels_.size() ? levels_[offset] : 0;
}

auto BidiText::paragraph(std::size_t offset) const noexcept
    -> const Paragraph* {
  const auto found = std::ranges::find_if(
      paragraphs_,
      [offset](const Paragraph& entry) { return offset < entry.end; });
  return found == paragraphs_.end() ? nullptr : &*found;
}

auto BidiText::paragraph_end(std::size_t offset) const noexcept -> std::size_t {
  const Paragraph* found = paragraph(offset);
  return found == nullptr ? levels_.size() : found->end;
}

auto BidiText::paragraph_right_to_left(std::size_t offset) const noexcept
    -> bool {
  const Paragraph* found = paragraph(offset);
  if (found == nullptr) {
    return !paragraphs_.empty() && paragraphs_.back().right_to_left;
  }
  return found->right_to_left;
}

auto BidiText::visual_runs(std::size_t start, std::size_t end) const
    -> std::vector<VisualRun> {
  std::vector<VisualRun> runs;
  const Paragraph* found = paragraph(start);
  if (found == nullptr || end <= start) {
    return runs;
  }
  end = std::min(end, found->end);
  support::UniqueHandle<LineRelease> line{
      SBParagraphCreateLine(found->handle.get(), start, end - start)};
  if (!line.valid()) {
    runs.push_back(VisualRun{.start = static_cast<std::uint32_t>(start),
                             .end = static_cast<std::uint32_t>(end),
                             .level = level(start)});
    return runs;
  }
  const SBUInteger count = SBLineGetRunCount(line.get());
  const SBRun* entries = SBLineGetRunsPtr(line.get());
  runs.reserve(count);
  for (SBUInteger index = 0; index < count; ++index) {
    runs.push_back(
        VisualRun{.start = static_cast<std::uint32_t>(entries[index].offset),
                  .end = static_cast<std::uint32_t>(entries[index].offset +
                                                    entries[index].length),
                  .level = entries[index].level});
  }
  return runs;
}

}
