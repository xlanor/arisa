#include <arisa/provider/gpu/frame_planner.hpp>
#include <arisa/provider/gpu/gpu_surfaces.hpp>

#include <algorithm>
#include <ranges>

namespace arisa::provider::gpu {

using Rectangles = GpuSurfaces<>;

void FramePlanner::created(TextureId texture,
                           const TextureDescription& description) {
  fresh_.insert_or_assign(texture, Extent{.width = description.width,
                                          .height = description.height});
}

auto FramePlanner::upload(TextureId texture, TextureRegion region)
    -> std::optional<ClearTexture> {
  const std::optional<Extent> pending = settle(texture);
  if (!pending.has_value()) {
    return std::nullopt;
  }
  const Extent extent = *pending;
  if (region.x == 0 && region.y == 0 && region.width >= extent.width &&
      region.height >= extent.height) {
    return std::nullopt;
  }
  return ClearTexture{.texture = texture};
}

auto FramePlanner::settle(TextureId texture) -> std::optional<Extent> {
  for (std::flat_map<TextureId, Extent>* owing : {&fresh_, &sealed_}) {
    if (const auto found = owing->find(texture); found != owing->end()) {
      const Extent extent = found->second;
      owing->erase(found);
      return extent;
    }
  }
  return std::nullopt;
}

void FramePlanner::seal() {
  for (const auto& [texture, extent] : fresh_) {
    sealed_.insert_or_assign(texture, extent);
  }
  fresh_.clear();
}

auto FramePlanner::plan(const RecordedFrame& frame) -> std::span<const Step> {
  steps_.clear();
  segments_.clear();
  segments_.reserve(bound(frame));
  plan_clears(frame.passes);
  steps_.emplace_back(RenderSurfaces{});
  for (const auto& [index, pass] : std::views::enumerate(frame.passes)) {
    plan_pass(static_cast<std::size_t>(index), pass);
  }
  if (frame.screen.has_value()) {
    plan_screen(*frame.screen);
  }
  return steps_;
}

auto FramePlanner::owed() -> std::span<const Step> {
  steps_.clear();
  segments_.clear();
  plan_clears({});
  return steps_;
}

void FramePlanner::invalidate() noexcept {
  std::ranges::fill(pending_, Pending{});
}

auto FramePlanner::writes(const RecordedPass& pass) noexcept -> bool {
  return pass.target.has_value() && !pass.target->empty() &&
         pass.pass.region.width > 0.0F && pass.pass.region.height > 0.0F;
}

auto FramePlanner::unite(ClipRect a, ClipRect b) noexcept -> ClipRect {
  if (a.width == 0 || a.height == 0) {
    return b;
  }
  if (b.width == 0 || b.height == 0) {
    return a;
  }
  const std::uint32_t left = std::min(a.x, b.x);
  const std::uint32_t top = std::min(a.y, b.y);
  const std::uint32_t right = std::max(a.x + a.width, b.x + b.width);
  const std::uint32_t bottom = std::max(a.y + a.height, b.y + b.height);
  return ClipRect{
      .x = left, .y = top, .width = right - left, .height = bottom - top};
}

auto FramePlanner::covered(TextureId texture,
                           std::span<const RecordedPass> passes) const noexcept
    -> bool {
  const auto first =
      std::ranges::find_if(passes, [texture](const RecordedPass& pass) {
        return pass.pass.target == texture && writes(pass);
      });
  return first != passes.end() && first->pass.clear.has_value();
}

void FramePlanner::plan_clears(std::span<const RecordedPass> passes) {
  for (const TextureId texture : sealed_ | std::views::keys) {
    if (!covered(texture, passes)) {
      steps_.emplace_back(ClearTexture{.texture = texture});
    }
  }
  sealed_.clear();
}

void FramePlanner::plan_pass(std::size_t index, const RecordedPass& pass) {
  if (!writes(pass)) {
    steps_.emplace_back(SkippedPass{.pass = index});
    return;
  }
  steps_.emplace_back(OffscreenPass{.pass = index,
                                    .target = pass.pass.target,
                                    .size = *pass.target,
                                    .region = pass.pass.region,
                                    .clear = pass.pass.clear,
                                    .segments = segments(pass)});
}

void FramePlanner::plan_screen(const RecordedScreen& screen) {
  const auto [region, whole] = damaged(screen);
  const bool painted = region.width > 0 && region.height > 0;
  steps_.emplace_back(ScreenPass{
      .region = region,
      .whole = whole,
      .scissor = whole ? std::nullopt : std::optional<ClipRect>{region},
      .segments = painted ? segments(screen.batches, region)
                          : std::span<const Segment>{}});
}

auto FramePlanner::damaged(const RecordedScreen& screen)
    -> std::pair<ClipRect, bool> {
  const std::size_t images = std::max<std::size_t>(screen.images, 1);
  if (pending_.size() != images) {
    pending_.assign(images, Pending{});
  }
  const ClipRect bounds{.x = 0,
                        .y = 0,
                        .width = screen.size.width.value(),
                        .height = screen.size.height.value()};
  for (Pending& each : pending_) {
    if (screen.damage.has_value()) {
      each.region =
          unite(each.region, Rectangles::intersect(*screen.damage, bounds));
    } else {
      each.whole = true;
    }
  }
  Pending& mine = pending_[std::min(screen.image, images - 1)];
  const bool whole = mine.whole || screen.whole;
  const ClipRect region = whole ? bounds : mine.region;
  mine = Pending{.whole = false, .region = {}};
  return {region, whole};
}

auto FramePlanner::bound(const RecordedFrame& frame) noexcept -> std::size_t {
  const auto size = [](const Batch& batch) {
    return (2 * batch.holes.size()) + 1;
  };
  std::size_t total = 0;
  for (const RecordedPass& pass : frame.passes) {
    total += size(pass.batch);
  }
  if (frame.screen.has_value()) {
    for (const Batch& batch : frame.screen->batches) {
      total += size(batch);
    }
  }
  return total;
}

auto FramePlanner::segments(const RecordedPass& pass)
    -> std::span<const Segment> {
  const std::size_t first = segments_.size();
  const SurfaceSize target = *pass.target;
  Rectangles::interleave(
      pass.batch.commands, pass.batch.holes,
      [this](std::size_t begin, std::size_t count) {
        segments_.emplace_back(
            DrawRange{.batch = 0, .first = begin, .count = count});
      },
      [this, &pass, target](const SurfaceUnderlay& underlay) {
        const ClipRect area =
            Rectangles::mapped(underlay.scissor, pass.pass.region,
                               target.width.value(), target.height.value());
        if (area.width > 0 && area.height > 0) {
          segments_.emplace_back(
              UnderlayFill{.underlay = underlay, .scissor = area});
        }
      });
  return std::span<const Segment>{segments_}.subspan(first);
}

auto FramePlanner::segments(std::span<const Batch> batches, ClipRect region)
    -> std::span<const Segment> {
  const std::size_t first = segments_.size();
  for (const auto& [index, batch] : std::views::enumerate(batches)) {
    const auto at = static_cast<std::size_t>(index);
    Rectangles::interleave(
        batch.commands, batch.holes,
        [this, at](std::size_t begin, std::size_t count) {
          segments_.emplace_back(
              DrawRange{.batch = at, .first = begin, .count = count});
        },
        [this, region](const SurfaceUnderlay& underlay) {
          const ClipRect scissor =
              Rectangles::intersect(underlay.scissor, region);
          if (scissor.width > 0 && scissor.height > 0) {
            segments_.emplace_back(
                UnderlayFill{.underlay = underlay, .scissor = scissor});
          }
        });
  }
  return std::span<const Segment>{segments_}.subspan(first);
}

}
