#include <arisa/inspector/frame_history.hpp>

#include <algorithm>
#include <utility>

namespace arisa::inspector {

FrameHistory::FrameHistory(std::size_t capacity)
    : entries_(std::max<std::size_t>(capacity, 1)) {}

void FrameHistory::record(const runtime::FrameTimings& timings,
                          std::vector<CostLedger::Ranked> top) {
  Entry& slot = entries_[head_];
  slot.timings = timings;
  slot.number = ++recorded_;
  slot.top = std::move(top);
  head_ = (head_ + 1) % entries_.size();
  size_ = std::min(size_ + 1, entries_.size());
}

void FrameHistory::clear() noexcept {
  head_ = 0;
  size_ = 0;
}

auto FrameHistory::at(std::size_t index) const -> const Entry& {
  const std::size_t oldest =
      (head_ + entries_.size() - size_) % entries_.size();
  return entries_[(oldest + index) % entries_.size()];
}

auto FrameHistory::index_of(std::uint64_t number) const
    -> std::optional<std::size_t> {
  if (size_ == 0) {
    return std::nullopt;
  }
  const std::uint64_t oldest = at(0).number;
  if (number < oldest || number > recorded_) {
    return std::nullopt;
  }
  const std::size_t offset = number - oldest;
  return offset;
}

}
