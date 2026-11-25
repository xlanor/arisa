#include <arisa/rendering/draw_list_cache.hpp>

#include <algorithm>
#include <array>
#include <cstring>

namespace arisa::rendering {

auto DrawListCache::Bounds::united(const Bounds& other) const noexcept
    -> Bounds {
  if (other.empty()) {
    return *this;
  }
  if (empty()) {
    return other;
  }
  return Bounds{.left = std::min(left, other.left),
                .top = std::min(top, other.top),
                .right = std::max(right, other.right),
                .bottom = std::max(bottom, other.bottom)};
}

auto DrawListCache::Bounds::clipped(const Bounds& other) const noexcept
    -> Bounds {
  return Bounds{.left = std::max(left, other.left),
                .top = std::max(top, other.top),
                .right = std::min(right, other.right),
                .bottom = std::min(bottom, other.bottom)};
}

auto DrawListCache::Bounds::moved(std::array<float, 2> by) const noexcept
    -> Bounds {
  return Bounds{.left = left + by[0],
                .top = top + by[1],
                .right = right + by[0],
                .bottom = bottom + by[1]};
}

auto DrawListCache::Bounds::grown(float margin) const noexcept -> Bounds {
  return Bounds{.left = left - margin,
                .top = top - margin,
                .right = right + margin,
                .bottom = bottom + margin};
}

auto DrawListCache::Bounds::intersects(const Bounds& other) const noexcept
    -> bool {
  return !empty() && !other.empty() && left < other.right &&
         other.left < right && top < other.bottom && other.top < bottom;
}

auto DrawListCache::Bounds::area() const noexcept -> float {
  return empty() ? 0.0F : (right - left) * (bottom - top);
}

auto DrawListCache::hash(std::uint64_t state,
                         std::span<const std::byte> bytes) noexcept
    -> std::uint64_t {
  constexpr std::uint64_t multiplier = 0xBF58'476D'1CE4'E5B9ULL;
  constexpr std::uint64_t other = 0x94D0'49BB'1331'11EBULL;
  constexpr std::size_t word = sizeof(std::uint64_t);
  const std::byte* const data = bytes.data();
  const std::size_t size = bytes.size();
  std::uint64_t left = state;
  std::uint64_t right = state ^ other;
  std::size_t at = 0;
  for (; at + (2 * word) <= size; at += 2 * word) {
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    std::memcpy(&first, data + at, word);
    std::memcpy(&second, data + at + word, word);
    left = (left ^ first) * multiplier;
    right = (right ^ second) * other;
    left ^= left >> 29U;
    right ^= right >> 31U;
  }
  std::uint64_t tail = size;
  if (at < size) {
    std::array<std::byte, 2 * word> rest{};
    std::memcpy(rest.data(), data + at, size - at);
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    std::memcpy(&first, rest.data(), word);
    std::memcpy(&second, rest.data() + word, word);
    left = (left ^ first) * multiplier;
    right = (right ^ second) * other;
  }
  state = ((left ^ (right >> 7U)) ^ tail) * multiplier;
  return state ^ (state >> 32U);
}

void DrawListCache::reserve(std::size_t runs) {
  if (entries_.keys().capacity() < runs) {
    auto containers = std::move(entries_).extract();
    containers.keys.reserve(runs);
    containers.values.reserve(runs);
    entries_.replace(std::move(containers.keys), std::move(containers.values));
  }
  frame_.reserve(runs);
  free_.reserve(runs + 1);
  departed_.reserve(runs);
  touched_.reserve((2 * runs) + 1);
  banded_.reserve(runs);
  changes_.reserve((2 * runs) + 1);
  departures_.reserve(runs + 1);
}

void DrawListCache::begin(bool whole) {
  ++epoch_;
  frame_.clear();
  highest_ = 0;
  whole_ = whole;
  if (high_ > compaction_floor && spare_ * 2 > high_) {
    free_.clear();
    high_ = 0;
    spare_ = 0;
    for (auto&& [key, entry] : entries_) {
      entry.capacity = 0;
    }
  }
}

auto DrawListCache::place(const Run& run) -> std::size_t {
  auto [found, inserted] = entries_.try_emplace(run.key);
  const auto rank = static_cast<std::uint32_t>(frame_.size() + 1);
  const bool reordered = !inserted && found->second.rank < highest_;
  if (!inserted) {
    highest_ = std::max(highest_, found->second.rank);
  }
  Entry& entry = found->second;
  Placed placed{.key = run.key,
                .first = 0,
                .count = run.count,
                .write = inserted,
                .damaged = inserted,
                .known = !inserted,
                .before = entry.bounds,
                .rank = inserted ? 0U : entry.rank,
                .reordered = reordered};
  if (inserted || entry.capacity < run.count) {
    if (!inserted) {
      release(entry.first, entry.capacity);
    }
    entry.first = allocate(run.count);
    entry.capacity = run.count;
    placed.write = true;
  }
  if (!inserted && (entry.content != run.content || entry.count != run.count)) {
    placed.write = true;
    placed.damaged = true;
  }
  if (entry.order != run.order || reordered || run.fresh) {
    placed.damaged = true;
  }
  if (!inserted && entry.offset != run.offset) {
    placed.damaged = true;
    if (!placed.write) {
      entry.bounds = entry.local.moved(run.offset).clipped(entry.clip);
    }
  }
  if (!inserted && entry.opacity != run.opacity) {
    placed.damaged = true;
  }
  entry.offset = run.offset;
  entry.opacity = run.opacity;
  entry.content = run.content;
  entry.order = run.order;
  entry.rank = rank;
  entry.count = run.count;
  entry.seen = epoch_;
  placed.first = entry.first;
  frame_.push_back(placed);
  return frame_.size() - 1;
}

auto DrawListCache::adopt(std::uint64_t generation) -> bool {
  if (generation != 0 && generation == generation_) {
    return false;
  }
  generation_ = generation;
  for (Placed& placed : frame_) {
    placed.write = true;
  }
  return true;
}

auto DrawListCache::placement(std::size_t index) const noexcept -> Placement {
  const Placed& placed = frame_[index];
  return Placement{.first = placed.first,
                   .count = placed.count,
                   .write = placed.write,
                   .damaged = placed.damaged};
}

void DrawListCache::bound(std::size_t index, const Extent& extent) {
  const auto found = entries_.find(frame_[index].key);
  if (found != entries_.end()) {
    found->second.bounds = extent.drawn;
    found->second.sense = extent.sense;
    found->second.local = extent.local;
    found->second.clip = extent.clip;
    found->second.reach = extent.reach;
  }
}

void DrawListCache::refresh(std::size_t index) { frame_[index].damaged = true; }

auto DrawListCache::local(std::size_t index) const -> Bounds {
  const auto found = entries_.find(frame_[index].key);
  return found == entries_.end() ? Bounds{} : found->second.local;
}

auto DrawListCache::changes() -> std::optional<Changes> {
  if (whole_) {
    return std::nullopt;
  }
  changes_.clear();
  departures_.clear();
  const auto note = [this](const Bounds& bounds) {
    if (!bounds.empty()) {
      changes_.push_back(bounds);
    }
  };
  for (const Placed& placed : frame_) {
    if (!placed.damaged) {
      continue;
    }
    if (placed.known) {
      note(placed.before);
    }
    if (const auto found = entries_.find(placed.key); found != entries_.end()) {
      note(found->second.bounds);
    }
  }
  for (const auto& [key, entry] : entries_) {
    if (entry.seen != epoch_ && !entry.bounds.empty()) {
      changes_.push_back(entry.bounds);
      departures_.push_back(entry.bounds);
    }
  }
  return Changes{.changed = changes_, .departed = departures_};
}

auto DrawListCache::finish() -> std::optional<Bounds> {
  Bounds damage{};
  touched_.clear();
  const auto touch = [&](const Bounds& bounds, std::size_t depth,
                         bool departed) {
    if (bounds.empty()) {
      return;
    }
    damage = damage.united(bounds);
    touched_.push_back(
        Touch{.bounds = bounds, .depth = depth, .departed = departed});
  };
  for (std::size_t index = 0; index < frame_.size(); ++index) {
    const Placed& placed = frame_[index];
    if (!placed.damaged) {
      continue;
    }
    if (placed.known) {
      touch(placed.before, placed.reordered ? 0 : index + 1, false);
    }
    const auto found = entries_.find(placed.key);
    if (found != entries_.end()) {
      touch(found->second.bounds, index + 1, false);
    }
  }
  departed_.clear();
  for (auto&& [key, entry] : entries_) {
    if (entry.seen != epoch_) {
      touch(entry.bounds, entry.rank, true);
      release(entry.first, entry.capacity);
      departed_.push_back(key);
    }
  }
  for (const Key& key : departed_) {
    entries_.erase(key);
  }
  banded_.assign(frame_.size(), Bounds{});
  bool spread = !whole_;
  while (spread) {
    spread = false;
    for (std::size_t index = 0; index < frame_.size(); ++index) {
      Placed& placed = frame_[index];
      if (placed.damaged) {
        continue;
      }
      const auto found = entries_.find(placed.key);
      if (found == entries_.end() || found->second.sense.empty()) {
        continue;
      }
      const Entry& entry = found->second;
      Bounds band{};
      for (const Touch& touched : touched_) {
        const bool beneath =
            touched.departed ? touched.depth < placed.rank || placed.rank == 0
                             : touched.depth <= index;
        if (beneath && entry.sense.intersects(touched.bounds)) {
          band = band.united(
              entry.bounds.clipped(touched.bounds.grown(entry.reach)));
        }
      }
      if (band.empty()) {
        continue;
      }
      if (band == entry.bounds) {
        touch(entry.bounds, index + 1, false);
        placed.damaged = true;
        spread = true;
        continue;
      }
      Bounds& held = banded_[index];
      if (held.united(band) == held) {
        continue;
      }
      held = held.united(band);
      touch(held, index + 1, false);
      spread = true;
    }
  }
  if (whole_) {
    return std::nullopt;
  }
  return damage;
}

auto DrawListCache::allocate(std::uint32_t count) -> std::uint32_t {
  for (auto range = free_.begin(); range != free_.end(); ++range) {
    if (range->count < count) {
      continue;
    }
    const std::uint32_t first = range->first;
    range->first += count;
    range->count -= count;
    spare_ -= count;
    if (range->count == 0) {
      free_.erase(range);
    }
    return first;
  }
  const std::uint32_t first = high_;
  high_ += count;
  return first;
}

void DrawListCache::release(std::uint32_t first, std::uint32_t count) {
  if (count == 0) {
    return;
  }
  spare_ += count;
  auto at =
      std::ranges::lower_bound(free_, first, {}, &provider::VertexRange::first);
  at = free_.insert(at, provider::VertexRange{.first = first, .count = count});
  if (at + 1 != free_.end() && at->first + at->count == (at + 1)->first) {
    at->count += (at + 1)->count;
    free_.erase(at + 1);
  }
  if (at != free_.begin() && (at - 1)->first + (at - 1)->count == at->first) {
    (at - 1)->count += at->count;
    free_.erase(at);
  }
  while (!free_.empty() && free_.back().first + free_.back().count == high_) {
    high_ -= free_.back().count;
    spare_ -= free_.back().count;
    free_.pop_back();
  }
}

}
