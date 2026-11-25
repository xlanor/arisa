#include <arisa/rendering/render_target_pool.hpp>

#include <algorithm>
#include <utility>

namespace arisa::rendering {

RenderTargetPool::Lease::Lease(std::shared_ptr<Shelf> shelf,
                               provider::Texture texture, Fit made) noexcept
    : shelf_(std::move(shelf)), texture_(std::move(texture)), made_(made) {
  ++shelf_->statistics.leased;
}

RenderTargetPool::Lease::Lease(Lease&& other) noexcept
    : shelf_(std::move(other.shelf_)),
      texture_(std::exchange(other.texture_, std::nullopt)),
      made_(other.made_) {}

auto RenderTargetPool::Lease::operator=(Lease&& other) noexcept -> Lease& {
  if (this != &other) {
    give_back();
    shelf_ = std::move(other.shelf_);
    texture_ = std::exchange(other.texture_, std::nullopt);
    made_ = other.made_;
  }
  return *this;
}

RenderTargetPool::Lease::~Lease() { give_back(); }

void RenderTargetPool::Lease::give_back() noexcept {
  if (!texture_.has_value() || shelf_ == nullptr) {
    texture_.reset();
    return;
  }
  --shelf_->statistics.leased;
  shelf_->returning.push_back(
      Shelf::Idle{.texture = std::move(*texture_), .made = made_, .since = 0});
  texture_.reset();
}

auto RenderTargetPool::Lease::id() const noexcept
    -> std::optional<provider::TextureId> {
  if (!texture_.has_value()) {
    return std::nullopt;
  }
  return texture_->id();
}

auto RenderTargetPool::Lease::width() const noexcept -> std::uint32_t {
  return texture_.has_value() ? texture_->description().width : 0;
}

auto RenderTargetPool::Lease::height() const noexcept -> std::uint32_t {
  return texture_.has_value() ? texture_->description().height : 0;
}

RenderTargetPool::RenderTargetPool(provider::GpuProvider& gpu)
    : gpu_(gpu), shelf_(std::make_shared<Shelf>()) {}

auto RenderTargetPool::rounded(std::uint32_t extent) noexcept -> std::uint32_t {
  const std::uint32_t at_least = std::max(extent, 1U);
  return ((at_least + granule - 1) / granule) * granule;
}

auto RenderTargetPool::idle_fitting(std::uint32_t wide, std::uint32_t tall,
                                    Fit fit) noexcept
    -> std::vector<Shelf::Idle>::iterator {
  if (fit == Fit::exact) {
    return std::ranges::find_if(
        shelf_->idle, [wide, tall](const Shelf::Idle& each) {
          return each.texture.description().width == wide &&
                 each.texture.description().height == tall;
        });
  }
  const std::uint64_t ceiling = static_cast<std::uint64_t>(rounded(wide, fit)) *
                                rounded(tall, fit) * oversize_limit;
  auto best = shelf_->idle.end();
  std::uint64_t best_area = 0;
  for (auto each = shelf_->idle.begin(); each != shelf_->idle.end(); ++each) {
    const provider::TextureDescription& held = each->texture.description();
    const std::uint64_t area =
        static_cast<std::uint64_t>(held.width) * held.height;
    if (held.width < wide || held.height < tall ||
        (each->made == Fit::exact && area > ceiling)) {
      continue;
    }
    if (best == shelf_->idle.end() || area > best_area) {
      best = each;
      best_area = area;
    }
  }
  return best;
}

auto RenderTargetPool::rounded(std::uint32_t extent, Fit fit) noexcept
    -> std::uint32_t {
  const std::uint32_t step = fit == Fit::exact ? granule : coarse_granule;
  const std::uint32_t at_least = std::max(extent, 1U);
  return ((at_least + step - 1) / step) * step;
}

auto RenderTargetPool::acquire(std::uint32_t width, std::uint32_t height,
                               Fit fit)
    -> std::expected<Lease, provider::ProviderError> {
  const std::uint32_t wide = rounded(width);
  const std::uint32_t tall = rounded(height);
  const auto found = idle_fitting(wide, tall, fit);
  if (found != shelf_->idle.end()) {
    provider::Texture texture = std::move(found->texture);
    const Fit made = found->made;
    shelf_->idle.erase(found);
    ++shelf_->statistics.reused;
    return Lease{shelf_, std::move(texture), made};
  }
  const std::uint32_t made_wide = rounded(width, fit);
  const std::uint32_t made_tall = rounded(height, fit);
  auto texture = created(made_wide, made_tall);
  if (!texture.has_value() && evict_earlier_frames() > 0) {
    texture = created(made_wide, made_tall);
  }
  if (!texture.has_value()) {
    ++shelf_->statistics.refused;
    return std::unexpected(texture.error());
  }
  ++shelf_->statistics.created;
  return Lease{shelf_, std::move(*texture), fit};
}

void RenderTargetPool::recycle(Lease scratch) noexcept {
  if (!scratch.texture_.has_value() || scratch.shelf_ != shelf_) {
    return;
  }
  --shelf_->statistics.leased;
  shelf_->idle.push_back(Shelf::Idle{.texture = std::move(*scratch.texture_),
                                     .made = scratch.made_,
                                     .since = shelf_->epoch});
  scratch.texture_.reset();
}

auto RenderTargetPool::evict_earlier_frames() -> std::size_t {
  const std::uint64_t epoch = shelf_->epoch;
  const std::size_t before = shelf_->idle.size();
  std::erase_if(shelf_->idle, [epoch](const Shelf::Idle& each) {
    return each.since < epoch;
  });
  const std::size_t evicted = before - shelf_->idle.size();
  shelf_->statistics.evicted += evicted;
  shelf_->statistics.released += evicted;
  return evicted;
}

auto RenderTargetPool::created(std::uint32_t width, std::uint32_t height)
    -> std::expected<provider::Texture, provider::ProviderError> {
  return gpu_.get().create_texture(provider::TextureDescription{
      .width = width,
      .height = height,
      .format = provider::TextureFormat::rgba8,
      .usage = provider::TextureUsage::render_target});
}

void RenderTargetPool::advance_frame() {
  const std::uint64_t closed = shelf_->epoch;
  for (Shelf::Idle& returned : shelf_->returning) {
    returned.since = closed;
    shelf_->idle.push_back(std::move(returned));
  }
  shelf_->returning.clear();
  ++shelf_->epoch;
  const std::uint64_t epoch = shelf_->epoch;
  const std::size_t before = shelf_->idle.size();
  std::erase_if(shelf_->idle, [epoch](const Shelf::Idle& each) {
    return epoch - each.since > grace_frames;
  });
  shelf_->statistics.released += before - shelf_->idle.size();
}

auto RenderTargetPool::statistics() const noexcept -> Statistics {
  Statistics current = shelf_->statistics;
  current.idle = shelf_->idle.size();
  current.returning = shelf_->returning.size();
  return current;
}

}
