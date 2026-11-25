#include <arisa/provider/gpu/texture_publications.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace arisa::provider {

TexturePublications::TexturePublications(
    std::weak_ptr<FrameSchedule> schedule) noexcept
    : schedule_(std::move(schedule)) {}

void TexturePublications::enlist(Texture texture, bool premultiplied,
                                 support::ScopedCallback released) {
  std::vector<Texture> planes;
  planes.push_back(std::move(texture));
  enlist(std::move(planes), ImportDescription{.premultiplied = premultiplied},
         std::move(released));
}

void TexturePublications::enlist(std::vector<Texture> planes,
                                 const ImportDescription& description,
                                 support::ScopedCallback released) {
  if (planes.empty()) {
    return;
  }
  const std::scoped_lock lock{mutex_};
  const std::uint64_t key = planes.front().id().value();
  entries_.insert_or_assign(
      key, Entry{.planes = std::move(planes),
                 .released = std::move(released),
                 .generation = 0,
                 .sampled = 0,
                 .premultiplied = description.premultiplied,
                 .conversion = YuvConversion{description.colour_matrix,
                                             description.range},
                 .retired = false});
  live_.store(entries_.size(), std::memory_order_release);
}

void TexturePublications::publish(TextureId id) {
  {
    const std::scoped_lock lock{mutex_};
    const auto found = entries_.find(id.value());
    if (found == entries_.end() || found->second.retired) {
      return;
    }
    ++found->second.generation;
  }
  if (const std::shared_ptr<FrameSchedule> schedule = schedule_.lock()) {
    schedule->request_frame();
  }
}

void TexturePublications::renew(TextureId id) {
  const std::scoped_lock lock{mutex_};
  const auto found = entries_.find(id.value());
  if (found != entries_.end() && !found->second.retired) {
    ++found->second.generation;
  }
}

void TexturePublications::retire(TextureId id) noexcept {
  std::optional<Entry> released;
  {
    const std::scoped_lock lock{mutex_};
    const auto found = entries_.find(id.value());
    if (found == entries_.end()) {
      return;
    }
    found->second.retired = true;
    if (found->second.sampled > completed_) {
      return;
    }
    released.emplace(std::move(found->second));
    entries_.erase(found);
    live_.store(entries_.size(), std::memory_order_release);
  }
}

auto TexturePublications::find(TextureId id) const
    -> std::optional<Publication> {
  if (empty()) {
    return std::nullopt;
  }
  const std::scoped_lock lock{mutex_};
  const auto found = entries_.find(id.value());
  if (found == entries_.end() || found->second.retired) {
    return std::nullopt;
  }
  return found->second.publication();
}

auto TexturePublications::in_use(TextureId id) const -> bool {
  const std::scoped_lock lock{mutex_};
  const auto found = entries_.find(id.value());
  return found != entries_.end() && found->second.sampled > completed_;
}

auto TexturePublications::live() const -> std::size_t {
  const std::scoped_lock lock{mutex_};
  return entries_.size();
}

auto TexturePublications::listing() const -> std::vector<Listing> {
  const std::scoped_lock lock{mutex_};
  std::vector<Listing> found;
  found.reserve(entries_.size());
  for (const auto& [id, entry] : entries_) {
    found.push_back(Listing{.texture = TextureId{id},
                            .planes = entry.planes.size(),
                            .generation = entry.generation,
                            .sampled = entry.sampled,
                            .completed = completed_,
                            .premultiplied = entry.premultiplied,
                            .retired = entry.retired});
  }
  return found;
}

void TexturePublications::completed(GpuSerial serial) {
  std::vector<Entry> released;
  {
    const std::scoped_lock lock{mutex_};
    completed_ = std::max(completed_, serial.value());
    const auto done = [this](const auto& entry) {
      return entry.second.retired && entry.second.sampled <= completed_;
    };
    if (std::ranges::none_of(entries_, done)) {
      return;
    }
    for (auto&& entry : entries_) {
      if (done(entry)) {
        released.push_back(std::move(entry.second));
      }
    }
    std::erase_if(entries_, done);
    live_.store(entries_.size(), std::memory_order_release);
  }
}

auto TexturePublications::completed_serial() const -> GpuSerial {
  const std::scoped_lock lock{mutex_};
  return GpuSerial{completed_};
}

void TexturePublications::mark(TextureId id, GpuSerial serial) {
  const auto found = entries_.find(id.value());
  if (found != entries_.end()) {
    found->second.sampled = std::max(found->second.sampled, serial.value());
  }
}

}
