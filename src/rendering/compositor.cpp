#include <arisa/rendering/compositor.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <utility>

namespace arisa::rendering {

Compositor::Compositor(provider::GpuProvider& gpu) : gpu_(gpu), pool_(gpu) {}

void Compositor::advance_frame() {
  ++epoch_;
  statistics_ = Statistics{};
  backdrops_.clear();
  pending_fields_.clear();
  withdrawn_.clear();
  const std::uint64_t limit = epoch_ > grace_frames ? epoch_ - grace_frames : 0;
  std::erase_if(
      blurs_, [limit](const auto& entry) { return entry.second.used < limit; });
  std::erase_if(groups_, [limit](const auto& entry) {
    return entry.second.used < limit;
  });
  const std::uint64_t kept =
      epoch_ > retention_frames ? epoch_ - retention_frames : 0;
  std::erase_if(fields_,
                [kept](const auto& entry) { return entry.second.used < kept; });
  std::erase_if(layers_, [this](const auto& entry) {
    return entry.second.used + 1 < epoch_;
  });
  std::erase_if(tables_,
                [kept](const auto& entry) { return entry.second.used < kept; });
  pool_.advance_frame();
}

void Compositor::withdraw(provider::TextureId texture) {
  if (texture.value() != 0 && !std::ranges::contains(withdrawn_, texture)) {
    withdrawn_.push_back(texture);
  }
}

void Compositor::withdraw(const Blur& entry) {
  if (const auto result = entry.result.id()) {
    withdraw(*result);
  }
  for (const RenderTargetPool::Lease& output : entry.outputs) {
    if (const auto texture = output.id()) {
      withdraw(*texture);
    }
  }
}

void Compositor::recycle_chain() noexcept {
  for (RenderTargetPool::Lease& scratch : chain_) {
    pool_.recycle(std::move(scratch));
  }
  chain_.clear();
}

void Compositor::separate(bool enabled) noexcept {
  separate_ = enabled;
  if (enabled) {
    plan_.clear();
    planned_.clear();
    consumers_.clear();
  }
}

auto Compositor::bit(std::uint32_t levels) noexcept -> std::uint8_t {
  return static_cast<std::uint8_t>(1U << std::min(levels, maximum_levels));
}

auto Compositor::united(const provider::PassRegion& first,
                        const provider::PassRegion& second) noexcept
    -> provider::PassRegion {
  const float left = std::min(first.x, second.x);
  const float top = std::min(first.y, second.y);
  const float right = std::max(first.x + first.width, second.x + second.width);
  const float bottom =
      std::max(first.y + first.height, second.y + second.height);
  return provider::PassRegion{
      .x = left, .y = top, .width = right - left, .height = bottom - top};
}

auto Compositor::contains(const provider::PassRegion& outer,
                          const provider::PassRegion& inner) noexcept -> bool {
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

auto Compositor::meets(const provider::PassRegion& first,
                       const provider::PassRegion& second) noexcept -> bool {
  return first.x <= second.x + second.width &&
         second.x <= first.x + first.width &&
         first.y <= second.y + second.height &&
         second.y <= first.y + first.height;
}

auto Compositor::meets(std::span<const DrawListCache::Bounds> damage,
                       const provider::PassRegion& region) noexcept -> bool {
  return std::ranges::any_of(
      damage, [&region](const DrawListCache::Bounds& bounds) {
        return !bounds.empty() && bounds.left <= region.x + region.width &&
               region.x <= bounds.right &&
               bounds.top <= region.y + region.height &&
               region.y <= bounds.bottom;
      });
}

auto Compositor::area(const provider::PassRegion& region) noexcept -> float {
  return std::max(region.width, 0.0F) * std::max(region.height, 0.0F);
}

auto Compositor::spread(const Field& entry) noexcept -> provider::PassRegion {
  std::optional<provider::PassRegion> covered;
  for (const FieldShape& shape : entry.shapes) {
    const float reach_x = shape.half_size[0] + entry.blend;
    const float reach_y = shape.half_size[1] + entry.blend;
    const provider::PassRegion box{.x = shape.centre[0] - reach_x,
                                   .y = shape.centre[1] - reach_y,
                                   .width = 2.0F * reach_x,
                                   .height = 2.0F * reach_y};
    covered = covered.has_value() ? united(*covered, box) : box;
  }
  return covered.value_or(provider::PassRegion{});
}

auto Compositor::levels_for(float radius_pixels) noexcept -> std::uint32_t {
  const float quarter =
      std::max(radius_pixels, 1.0F) /
      (radius_pixels >= frost_radius ? level_radius : glass_level_radius);
  const long levels = std::lround(std::log2(quarter));
  return static_cast<std::uint32_t>(
      std::clamp<long>(levels, 0, maximum_levels));
}

auto Compositor::scale_for(float radius_pixels) noexcept -> float {
  if (radius_pixels <= sharp_radius) {
    return 1.0F;
  }
  return levels_for(radius_pixels) == 0 ? light_scale : backdrop_scale;
}

auto Compositor::mix(std::uint64_t hash,
                     std::span<const std::byte> bytes) noexcept
    -> std::uint64_t {
  constexpr std::size_t word = sizeof(std::uint64_t);
  const std::size_t whole = bytes.size() / word * word;
  for (std::size_t at = 0; at < whole; at += word) {
    std::uint64_t value = 0;
    std::memcpy(&value, bytes.subspan(at, word).data(), word);
    hash = (hash * fnv_prime) ^ value;
    hash ^= hash >> 29U;
    hash *= fnv_prime;
  }
  for (const std::byte value : bytes.subspan(whole)) {
    hash ^= std::to_integer<std::uint64_t>(value);
    hash *= fnv_prime;
  }
  return hash;
}

auto Compositor::holds(const RenderTargetPool::Lease& lease,
                       std::uint32_t width, std::uint32_t height) noexcept
    -> bool {
  return lease.valid() && lease.width() >= RenderTargetPool::rounded(width) &&
         lease.height() >= RenderTargetPool::rounded(height);
}

auto Compositor::sized(RenderTargetPool::Lease& lease, std::uint32_t width,
                       std::uint32_t height, RenderTargetPool::Fit fit)
    -> std::expected<void, provider::ProviderError> {
  if (lease.valid() && lease.width() == RenderTargetPool::rounded(width) &&
      lease.height() == RenderTargetPool::rounded(height)) {
    return {};
  }
  auto acquired = pool_.acquire(width, height, fit);
  if (!acquired.has_value()) {
    lease = RenderTargetPool::Lease{};
    return std::unexpected(acquired.error());
  }
  lease = std::move(*acquired);
  return {};
}

void Compositor::refuse(std::string_view what, std::uint32_t width,
                        std::uint32_t height,
                        const provider::ProviderError& error,
                        std::string_view fallback,
                        std::optional<std::uint64_t> node) {
  report(Refusal{.what = what,
                 .node = node.value_or(0),
                 .paint = std::nullopt,
                 .cause = error.code()},
         width, height, error.explanation(), fallback);
}

void Compositor::refuse(std::string_view what, std::uint32_t width,
                        std::uint32_t height, const PaintError& error,
                        std::string_view fallback,
                        std::optional<std::uint64_t> node) {
  report(Refusal{.what = what,
                 .node = node.value_or(0),
                 .paint = error.code(),
                 .cause = error.cause().transform(
                     [](const provider::ProviderError& cause) {
                       return cause.code();
                     })},
         width, height, error.explanation(), fallback);
}

void Compositor::report(Refusal refusal, std::uint32_t width,
                        std::uint32_t height, std::string_view reason,
                        std::string_view fallback) {
  if (!diagnostics_.has_value() || std::ranges::contains(refused_, refusal)) {
    return;
  }
  if (refused_.size() >= rejected_limit) {
    refused_.clear();
  }
  refused_.push_back(refusal);
  const std::string owner = refusal.node == 0
                                ? std::string{}
                                : std::format(" for node {}", refusal.node);
  diagnostics_->get().report(provider::Diagnostic{
      .severity = provider::Severity::warning,
      .category = "glass",
      .message = std::format("{} {}x{}{} failed: {}; {}", refusal.what, width,
                             height, owner, reason, fallback)});
}

void Compositor::bound(std::flat_set<std::uint64_t>& keys, std::uint64_t key) {
  if (keys.size() >= rejected_limit) {
    keys.clear();
  }
  keys.insert(key);
}

auto Compositor::fitted(provider::PassRegion wanted, float scale,
                        std::uint32_t width, std::uint32_t height) noexcept
    -> provider::PassRegion {
  return provider::PassRegion{.x = wanted.x,
                              .y = wanted.y,
                              .width = static_cast<float>(width) / scale,
                              .height = static_cast<float>(height) / scale};
}

auto Compositor::captured_extent(float logical, float scale) noexcept
    -> std::uint32_t {
  return RenderTargetPool::rounded(
      static_cast<std::uint32_t>(std::max(std::ceil(logical * scale), 2.0F)),
      RenderTargetPool::Fit::at_least);
}

auto Compositor::capture_scale(provider::PassRegion wanted,
                               float radius_pixels) noexcept -> float {
  const float scale = scale_for(radius_pixels);
  const float pixels = wanted.width * wanted.height * scale * scale;
  if (pixels <= maximum_capture_pixels) {
    return scale;
  }
  return std::max(scale * std::sqrt(maximum_capture_pixels / pixels),
                  minimum_capture_scale);
}

auto Compositor::joined(std::uint64_t key, provider::PassRegion wanted,
                        std::uint32_t levels, float scale) const
    -> std::optional<std::uint64_t> {
  if (separate_) {
    return std::nullopt;
  }
  const auto member = plan_.find(key);
  if (member == plan_.end()) {
    return std::nullopt;
  }
  const auto group = planned_.find(member->second);
  if (group == planned_.end() || rejected_groups_.contains(member->second) ||
      group->second.scale != scale ||
      (group->second.planned & bit(levels)) == 0 ||
      !contains(group->second.region, wanted)) {
    return std::nullopt;
  }
  return member->second;
}

auto Compositor::output_of(const Blur& entry, std::uint32_t levels) noexcept
    -> std::optional<provider::TextureId> {
  return levels == entry.levels
             ? entry.result.id()
             : entry.outputs[std::min(levels, maximum_levels)].id();
}

auto Compositor::share(std::uint64_t group, std::uint32_t levels,
                       std::uint32_t first_quad) -> std::optional<Surface> {
  const Group plan = planned_.at(group);
  Blur& entry = groups_[group];
  if (entry.used != epoch_) {
    const std::uint32_t width = captured_extent(plan.region.width, plan.scale);
    const std::uint32_t height =
        captured_extent(plan.region.height, plan.scale);
    const std::optional<provider::TextureId> before = entry.result.id();
    bool moved = false;
    auto made = sized(entry.result, width, height);
    for (std::uint32_t level = 0; made.has_value() && level <= maximum_levels;
         ++level) {
      RenderTargetPool::Lease& output = entry.outputs[level];
      if ((plan.planned & bit(level)) == 0 || level == plan.levels) {
        output = RenderTargetPool::Lease{};
        continue;
      }
      const std::optional<provider::TextureId> was = output.id();
      made = sized(output, width, height);
      moved = moved || output.id() != was;
    }
    if (!made.has_value()) {
      groups_.erase(group);
      planned_.erase(group);
      refuse("shared backdrop render target", width, height, made.error(),
             "members capture their backdrops separately");
      return std::nullopt;
    }
    const provider::PassRegion region = fitted(
        plan.region, plan.scale, entry.result.width(), entry.result.height());
    if (moved || entry.result.id() != before || region != entry.region ||
        plan.levels != entry.levels || plan.scale != entry.scale ||
        plan.planned != entry.planned) {
      entry.rendered = false;
    }
    entry.region = region;
    entry.levels = plan.levels;
    entry.planned = plan.planned;
    entry.scale = plan.scale;
    entry.present = 0;
    entry.boundary = first_quad;
    entry.used = epoch_;
  }
  const std::optional<provider::TextureId> output = output_of(entry, levels);
  if (!output.has_value()) {
    return std::nullopt;
  }
  entry.boundary = std::min(entry.boundary, first_quad);
  entry.present |= bit(levels);
  return Surface{.texture = *output, .region = entry.region};
}

auto Compositor::backdrop(std::uint64_t key, provider::PassRegion wanted,
                          provider::PassRegion core, float radius_pixels,
                          std::uint32_t first_quad)
    -> std::expected<Surface, BackdropRefusal> {
  if (!(wanted.width >= 1.0F) || !(wanted.height >= 1.0F)) {
    return std::unexpected(BackdropRefusal{});
  }
  if (rejected_backdrops_.contains(key)) {
    return std::unexpected(
        BackdropRefusal{.cause = BackdropRefusal::Cause::rejected_pass,
                        .width = 0,
                        .height = 0,
                        .error = provider::ProviderError{
                            provider::ProviderErrorCode::invalid_argument}});
  }
  const std::uint32_t levels = levels_for(radius_pixels);
  const float requested = capture_scale(wanted, radius_pixels);
  if (const std::optional<std::uint64_t> group =
          joined(key, wanted, levels, requested)) {
    if (const std::optional<Surface> shared =
            share(*group, levels, first_quad)) {
      backdrops_.push_back(PendingBackdrop{.key = key,
                                           .first_quad = first_quad,
                                           .wanted = wanted,
                                           .core = core,
                                           .levels = levels,
                                           .scale = requested,
                                           .group = group});
      ++statistics_.shared;
      return Surface{
          .texture = shared->texture, .region = shared->region, .key = key};
    }
  }
  const auto earlier =
      std::ranges::find_if(backdrops_, [key](const PendingBackdrop& pending) {
        return pending.key == key && !pending.group.has_value();
      });
  if (earlier != backdrops_.end() &&
      (earlier->wanted != wanted || earlier->levels != levels)) {
    return backdrop(mix(key, std::as_bytes(std::span{&duplicate_salt, 1})),
                    wanted, core, radius_pixels, first_quad);
  }
  const auto extent = [&wanted](float scale) {
    return std::pair{captured_extent(wanted.width, scale),
                     captured_extent(wanted.height, scale)};
  };
  Blur& entry = blurs_[key];
  float scale = std::min(requested, entry.ceiling);
  const auto [first_width, first_height] = extent(scale);
  const std::optional<provider::TextureId> before = entry.result.id();
  auto made = sized(entry.result, first_width, first_height);
  const std::optional<provider::ProviderError> first_error =
      made.has_value() ? std::nullopt
                       : std::optional<provider::ProviderError>{made.error()};
  while (!made.has_value() && scale * 0.5F >= minimum_capture_scale) {
    scale *= 0.5F;
    const auto [width, height] = extent(scale);
    made = sized(entry.result, width, height);
    if (made.has_value()) {
      entry.ceiling = scale;
      refuse("full-size backdrop render target", first_width, first_height,
             *first_error,
             std::format("captured at {}x{} instead", width, height));
    }
  }
  if (!made.has_value()) {
    blurs_.erase(key);
    refuse("backdrop render target", first_width, first_height, made.error(),
           "glass draws its flat fallback");
    return std::unexpected(
        BackdropRefusal{.cause = BackdropRefusal::Cause::render_target,
                        .width = first_width,
                        .height = first_height,
                        .error = made.error()});
  }
  const provider::PassRegion region =
      fitted(wanted, scale, entry.result.width(), entry.result.height());
  if (entry.result.id() != before || region != entry.region ||
      levels != entry.levels || scale != entry.scale) {
    entry.rendered = false;
  }
  entry.region = region;
  entry.levels = levels;
  entry.planned = bit(levels);
  entry.present = bit(levels);
  entry.boundary =
      entry.used == epoch_ ? std::max(entry.boundary, first_quad) : first_quad;
  entry.scale = scale;
  entry.used = epoch_;
  backdrops_.push_back(PendingBackdrop{.key = key,
                                       .first_quad = first_quad,
                                       .wanted = wanted,
                                       .core = core,
                                       .levels = levels,
                                       .scale = scale,
                                       .group = std::nullopt});
  return Surface{.texture = *entry.result.id(), .region = region, .key = key};
}

auto Compositor::predicts(std::uint64_t key, provider::PassRegion wanted,
                          float radius_pixels, const Surface& surface) const
    -> bool {
  if (!(wanted.width >= 1.0F) || !(wanted.height >= 1.0F) ||
      rejected_backdrops_.contains(key)) {
    return false;
  }
  const std::uint32_t levels = levels_for(radius_pixels);
  if (const std::optional<std::uint64_t> group =
          joined(key, wanted, levels, capture_scale(wanted, radius_pixels))) {
    const auto shared = groups_.find(*group);
    if (shared == groups_.end() || !shared->second.result.valid()) {
      return false;
    }
    const Group& plan = planned_.at(*group);
    const Blur& entry = shared->second;
    return entry.levels == plan.levels && entry.planned == plan.planned &&
           entry.result.width() ==
               captured_extent(plan.region.width, plan.scale) &&
           entry.result.height() ==
               captured_extent(plan.region.height, plan.scale) &&
           output_of(entry, levels) == surface.texture &&
           fitted(plan.region, plan.scale, entry.result.width(),
                  entry.result.height()) == surface.region;
  }
  const auto found = blurs_.find(key);
  if (found == blurs_.end()) {
    return false;
  }
  const Blur& entry = found->second;
  if (!entry.result.valid() || entry.result.id() != surface.texture) {
    return false;
  }
  const float scale =
      std::min(capture_scale(wanted, radius_pixels), entry.ceiling);
  return entry.result.width() == captured_extent(wanted.width, scale) &&
         entry.result.height() == captured_extent(wanted.height, scale) &&
         fitted(wanted, scale, entry.result.width(), entry.result.height()) ==
             surface.region;
}

auto Compositor::field(std::uint64_t key, std::uint64_t owner,
                       std::span<const FieldShape> shapes, float blend)
    -> std::optional<provider::TextureId> {
  if (shapes.empty() || shapes.size() > field_members ||
      rejected_fields_.contains(key)) {
    return std::nullopt;
  }
  Field& entry = fields_[key];
  if (!entry.table.has_value()) {
    auto created = gpu_.get().create_texture(
        provider::TextureDescription{.width = field_width,
                                     .height = 1,
                                     .format = provider::TextureFormat::rgba8,
                                     .usage = provider::TextureUsage::sampled});
    if (!created.has_value()) {
      fields_.erase(key);
      refuse("union field table", field_width, 1, created.error(),
             "members are glazed individually");
      return std::nullopt;
    }
    entry.table = std::move(*created);
    entry.uploaded = false;
  }
  if (!std::ranges::equal(shapes, entry.shapes) || blend != entry.blend) {
    entry.uploaded = false;
    entry.shapes.assign(shapes.begin(), shapes.end());
    entry.blend = blend;
  }
  entry.owner = owner;
  entry.used = epoch_;
  if (std::ranges::find(pending_fields_, key) == pending_fields_.end()) {
    pending_fields_.push_back(key);
  }
  return entry.table->id();
}

auto Compositor::holds_field(std::uint64_t key,
                             provider::TextureId texture) const -> bool {
  const auto found = fields_.find(key);
  return found != fields_.end() && !rejected_fields_.contains(key) &&
         found->second.table.has_value() &&
         found->second.table->id() == texture;
}

void Compositor::retain_field(std::uint64_t key) {
  const auto found = fields_.find(key);
  if (found == fields_.end()) {
    return;
  }
  found->second.used = epoch_;
  if (std::ranges::find(pending_fields_, key) == pending_fields_.end()) {
    pending_fields_.push_back(key);
  }
}

auto Compositor::layerable(std::uint64_t key) const -> bool {
  const auto found = layers_.find(key);
  return found == layers_.end() || !found->second.spoiled;
}

auto Compositor::layered(std::uint64_t key) const -> bool {
  const auto found = layers_.find(key);
  return found != layers_.end() && !found->second.spoiled &&
         found->second.target.valid() && found->second.used == epoch_;
}

auto Compositor::layer(std::uint64_t key, const LayerRequest& request)
    -> std::optional<LayerPlan> {
  return planned_layer(key, request, true);
}

auto Compositor::effect_layer(std::uint64_t key, const LayerRequest& request)
    -> std::optional<LayerPlan> {
  return planned_layer(key, request, false);
}

void Compositor::redraw_layer(std::uint64_t key) {
  const auto found = layers_.find(key);
  if (found != layers_.end()) {
    found->second.rendered = false;
  }
}

auto Compositor::table(std::uint64_t key, std::span<const float> values)
    -> std::optional<provider::TextureId> {
  if (values.empty() || values.size() > table_capacity) {
    return std::nullopt;
  }
  Table& entry = tables_[key];
  if (!entry.texture.has_value()) {
    auto created = gpu_.get().create_texture(provider::TextureDescription{
        .width = table_width,
        .height = table_rows,
        .format = provider::TextureFormat::rgba8,
        .usage = provider::TextureUsage::sampled,
        .filter = provider::SamplerFilter::nearest});
    if (!created.has_value()) {
      tables_.erase(key);
      refuse("effect parameter table", table_width, table_rows, created.error(),
             "the effect is drawn without its parameters");
      return std::nullopt;
    }
    entry.texture = std::move(*created);
    entry.uploaded = false;
  }
  std::array<float, table_capacity> padded{};
  std::ranges::copy(values, padded.begin());
  if (!entry.uploaded || padded != entry.values) {
    entry.values = padded;
    if (auto updated = gpu_.get().update_texture(
            *entry.texture,
            provider::TextureRegion{
                .x = 0, .y = 0, .width = table_width, .height = table_rows},
            provider::PixelRows{std::as_bytes(std::span{entry.values})});
        !updated.has_value()) {
      tables_.erase(key);
      return std::nullopt;
    }
    entry.uploaded = true;
  }
  entry.used = epoch_;
  return entry.texture->id();
}

auto Compositor::planned_layer(std::uint64_t key, const LayerRequest& request,
                               bool spoils) -> std::optional<LayerPlan> {
  const provider::PassRegion& wanted = request.wanted;
  if (!(wanted.width >= 1.0F) || !(wanted.height >= 1.0F)) {
    return std::nullopt;
  }
  const std::optional<std::array<std::uint32_t, 2>> shared =
      request.matching.and_then(
          [this](std::uint64_t partner)
              -> std::optional<std::array<std::uint32_t, 2>> {
            const auto found = layers_.find(partner);
            if (found == layers_.end() || !found->second.target.valid()) {
              return std::nullopt;
            }
            return std::array{found->second.target.width(),
                              found->second.target.height()};
          });
  Layer& entry = layers_[key];
  if (entry.spoiled) {
    entry.used = epoch_;
    entry.target = RenderTargetPool::Lease{};
    return std::nullopt;
  }
  const std::optional<provider::TextureId> before = entry.target.id();
  const auto width = shared.has_value()
                         ? (*shared)[0]
                         : static_cast<std::uint32_t>(std::ceil(wanted.width));
  const auto height =
      shared.has_value() ? (*shared)[1]
                         : static_cast<std::uint32_t>(std::ceil(wanted.height));
  const bool kept = shared.has_value() ? entry.target.valid() &&
                                             entry.target.width() == width &&
                                             entry.target.height() == height
                                       : holds(entry.target, width, height);
  if (auto made =
          kept ? std::expected<void, provider::ProviderError>{}
               : sized(entry.target, width, height,
                       shared.has_value() ? RenderTargetPool::Fit::exact
                                          : RenderTargetPool::Fit::at_least);
      !made.has_value()) {
    layers_.erase(key);
    refuse("layer render target", width, height, made.error(),
           "the subtree is drawn directly");
    return std::nullopt;
  }
  const provider::PassRegion region =
      fitted(wanted, 1.0F, entry.target.width(), entry.target.height());
  const std::array<float, 2> moved{request.anchor[0] - entry.anchor[0],
                                   request.anchor[1] - entry.anchor[1]};
  const bool inexact = std::ranges::any_of(moved, [](float part) {
    return std::abs(part - std::round(part)) > placement_tolerance;
  });
  const bool settled = request.anchor == entry.seen;
  const bool render =
      entry.target.id() != before || !entry.rendered ||
      entry.hash != request.hash || region.width != entry.region.width ||
      region.height != entry.region.height || (inexact && settled);
  entry.used = epoch_;
  entry.seen = request.anchor;
  if (render) {
    entry.region = region;
    entry.drawn = wanted;
    entry.anchor = request.anchor;
    ++entry.streak;
    ++entry.generation;
    ++statistics_.layer_renders;
    if (spoils && entry.streak >= volatile_streak + 1) {
      entry.spoiled = true;
    }
  } else {
    entry.streak = 0;
    ++statistics_.layer_hits;
  }
  entry.hash = request.hash;
  entry.rendered = true;
  const provider::PassRegion placed =
      render ? region
             : provider::PassRegion{.x = entry.region.x + moved[0],
                                    .y = entry.region.y + moved[1],
                                    .width = entry.region.width,
                                    .height = entry.region.height};
  return LayerPlan{
      .surface =
          Surface{.texture = *entry.target.id(), .region = placed, .key = key},
      .extent = {entry.drawn.width, entry.drawn.height},
      .generation = entry.generation,
      .render = render,
      .inexact = !render && inexact};
}

void Compositor::forget_layer(std::uint64_t key) { layers_.erase(key); }

void Compositor::spoil_layer(std::uint64_t key) {
  Layer& entry = layers_[key];
  entry.spoiled = true;
  entry.used = epoch_;
}

void Compositor::count_layer(bool rendered) noexcept {
  if (rendered) {
    ++statistics_.layer_renders;
  } else {
    ++statistics_.layer_hits;
  }
}

auto Compositor::same_source(const PendingBackdrop& first,
                             const PendingBackdrop& second) noexcept -> bool {
  return first.group.has_value()
             ? first.group == second.group
             : !second.group.has_value() && first.key == second.key;
}

auto Compositor::source_of(const PendingBackdrop& pending)
    -> std::optional<std::reference_wrapper<Blur>> {
  auto& sources = pending.group.has_value() ? groups_ : blurs_;
  const auto found = sources.find(pending.group.value_or(pending.key));
  if (found == sources.end()) {
    return std::nullopt;
  }
  return std::ref(found->second);
}

auto Compositor::source_of(const PendingBackdrop& pending) const
    -> std::optional<std::reference_wrapper<const Blur>> {
  const auto& sources = pending.group.has_value() ? groups_ : blurs_;
  const auto found = sources.find(pending.group.value_or(pending.key));
  if (found == sources.end()) {
    return std::nullopt;
  }
  return std::cref(found->second);
}

auto Compositor::extra_quads() const noexcept -> std::uint32_t {
  std::uint32_t total = 0;
  for (std::size_t index = 0; index < backdrops_.size(); ++index) {
    const PendingBackdrop& pending = backdrops_[index];
    if (std::ranges::any_of(std::span{backdrops_}.first(index),
                            [&pending](const PendingBackdrop& earlier) {
                              return same_source(earlier, pending);
                            })) {
      continue;
    }
    const auto source = source_of(pending);
    if (!source.has_value()) {
      continue;
    }
    const Blur& entry = source->get();
    total += entry.levels;
    for (std::uint32_t level = 1; level <= entry.levels; ++level) {
      if ((entry.present & bit(level)) != 0) {
        total += level;
      }
    }
  }
  return total;
}

void Compositor::zoom(float factor, float centre_x, float centre_y) {
  const auto scaled = [&](provider::PassRegion region) {
    return provider::PassRegion{
        .x = centre_x + ((region.x - centre_x) * factor),
        .y = centre_y + ((region.y - centre_y) * factor),
        .width = region.width * factor,
        .height = region.height * factor};
  };
  for (const PendingBackdrop& pending : backdrops_) {
    const auto source = source_of(pending);
    if (source.has_value() && source->get().zoomed != epoch_) {
      Blur& entry = source->get();
      entry.region = scaled(entry.region);
      entry.rendered = false;
      entry.zoomed = epoch_;
    }
  }
  zoomed_ = epoch_;
  for (const std::uint64_t key : pending_fields_) {
    const auto found = fields_.find(key);
    if (found == fields_.end()) {
      continue;
    }
    Field& entry = found->second;
    entry.blend *= factor;
    for (FieldShape& shape : entry.shapes) {
      shape.centre = {centre_x + ((shape.centre[0] - centre_x) * factor),
                      centre_y + ((shape.centre[1] - centre_y) * factor)};
      shape.half_size = {shape.half_size[0] * factor,
                         shape.half_size[1] * factor};
      shape.radius *= factor;
    }
    entry.uploaded = false;
  }
}

auto Compositor::quad(Writer& writer, const provider::ShapeVertex& shape,
                      std::array<float, 4> box, std::array<float, 4> uv)
    -> std::optional<provider::DrawCommand> {
  constexpr std::uint32_t corners = 4;
  constexpr std::uint32_t indices = 6;
  if (writer.base + corners > writer.vertices.size() ||
      writer.first + indices > writer.indices.size()) {
    return std::nullopt;
  }
  const float cx = (box[0] + box[2]) * 0.5F;
  const float cy = (box[1] + box[3]) * 0.5F;
  const std::array<std::array<float, 2>, corners> points{
      {{box[0], box[1]}, {box[2], box[1]}, {box[2], box[3]}, {box[0], box[3]}}};
  const std::array<std::array<float, 2>, corners> uvs{
      {{uv[0], uv[1]}, {uv[2], uv[1]}, {uv[2], uv[3]}, {uv[0], uv[3]}}};
  for (std::uint32_t corner = 0; corner < corners; ++corner) {
    provider::ShapeVertex vertex = shape;
    vertex.position = points[corner];
    vertex.uv = uvs[corner];
    vertex.local = {points[corner][0] - cx, points[corner][1] - cy};
    writer.vertices[writer.base + corner] = vertex;
  }
  const std::array<std::uint32_t, indices> order{0, 1, 2, 2, 3, 0};
  for (std::uint32_t index = 0; index < indices; ++index) {
    writer.indices[writer.first + index] = writer.base + order[index];
  }
  const provider::DrawCommand command{.texture = provider::TextureId{0},
                                      .clip = {},
                                      .index_offset = writer.first,
                                      .index_count = indices,
                                      .vertex_offset = 0};
  writer.base += corners;
  writer.first += indices;
  return command;
}

auto Compositor::extent(
    std::span<const provider::ShapeVertex> vertices) noexcept
    -> std::optional<std::array<float, 4>> {
  if (vertices.empty()) {
    return std::nullopt;
  }
  std::array<float, 4> box{
      vertices.front().position[0], vertices.front().position[1],
      vertices.front().position[0], vertices.front().position[1]};
  for (const provider::ShapeVertex& vertex : vertices) {
    box[0] = std::min(box[0], vertex.position[0]);
    box[1] = std::min(box[1], vertex.position[1]);
    box[2] = std::max(box[2], vertex.position[0]);
    box[3] = std::max(box[3], vertex.position[1]);
  }
  return box;
}

auto Compositor::touches(const std::optional<std::array<float, 4>>& extent,
                         const provider::PassRegion& region) noexcept -> bool {
  return extent.has_value() && (*extent)[2] >= region.x &&
         (*extent)[3] >= region.y && (*extent)[0] <= region.x + region.width &&
         (*extent)[1] <= region.y + region.height;
}

auto Compositor::generation_of(provider::TextureId texture) const
    -> std::uint64_t {
  for (const auto* sources : {&blurs_, &groups_}) {
    for (const auto& [key, entry] : *sources) {
      if (entry.result.id() == texture ||
          std::ranges::any_of(entry.outputs,
                              [texture](const RenderTargetPool::Lease& output) {
                                return output.id() == texture;
                              })) {
        return entry.generation;
      }
    }
  }
  for (const auto& [key, entry] : layers_) {
    if (entry.target.id() == texture) {
      return entry.generation;
    }
  }
  for (const auto& [key, entry] : fields_) {
    if (entry.table.has_value() && entry.table->id() == texture) {
      return entry.generation;
    }
  }
  return 0;
}

auto Compositor::reaches(std::span<const QuadSpan> spans,
                         std::span<const provider::ShapeVertex> vertices,
                         std::size_t index, const provider::PassRegion& region)
    -> bool {
  const QuadSpan& span = spans[index];
  const provider::PassRegion local{.x = region.x - span.translation[0],
                                   .y = region.y - span.translation[1],
                                   .width = region.width,
                                   .height = region.height};
  if (!span.reach.empty() &&
      !touches(std::array{span.reach.left, span.reach.top, span.reach.right,
                          span.reach.bottom},
               local)) {
    return false;
  }
  if (measured_[index] == 0) {
    extents_[index] =
        span.vertex_end <= span.vertex_begin ||
                span.vertex_end > vertices.size()
            ? std::nullopt
            : extent(vertices.subspan(span.vertex_begin,
                                      span.vertex_end - span.vertex_begin));
    measured_[index] = 1;
  }
  return touches(extents_[index], local);
}

auto Compositor::clear_between(std::span<const QuadSpan> spans,
                               std::span<const provider::ShapeVertex> vertices,
                               std::uint32_t from, std::uint32_t to,
                               const provider::PassRegion& core) -> bool {
  const std::size_t last = std::min<std::size_t>(to, spans.size());
  for (std::size_t at = from; at < last; ++at) {
    if (reaches(spans, vertices, at, core)) {
      return false;
    }
  }
  return true;
}

auto Compositor::quiet(std::span<const QuadSpan> spans, std::uint32_t from,
                       std::uint32_t to) noexcept -> bool {
  const std::size_t last = std::min<std::size_t>(to, spans.size());
  const std::size_t first = std::min<std::size_t>(from, last);
  return std::ranges::none_of(spans.subspan(first, last - first),
                              &QuadSpan::damaged);
}

auto Compositor::disturbed(std::span<const QuadSpan> spans,
                           std::span<const provider::ShapeVertex> vertices,
                           std::uint32_t boundary,
                           const provider::PassRegion& region) -> bool {
  const std::size_t last = std::min<std::size_t>(boundary, spans.size());
  for (std::size_t at = 0; at < last; ++at) {
    if (spans[at].damaged && reaches(spans, vertices, at, region)) {
      return true;
    }
  }
  return false;
}

auto Compositor::members_clear(std::uint64_t group, std::uint32_t boundary,
                               std::span<const QuadSpan> spans,
                               std::span<const provider::ShapeVertex> vertices,
                               bool whole) -> bool {
  return std::ranges::all_of(backdrops_, [&](const PendingBackdrop& pending) {
    return pending.group != group || pending.first_quad <= boundary ||
           (!whole && quiet(spans, boundary, pending.first_quad)) ||
           clear_between(spans, vertices, boundary, pending.first_quad,
                         pending.core);
  });
}

auto Compositor::unchanged(
    const Blur& entry, std::uint32_t boundary, provider::Colour clear,
    std::span<const QuadSpan> spans,
    const std::optional<DrawListCache::Changes>& damage) const -> bool {
  return damage.has_value() && entry.rendered && entry.checked + 1 == epoch_ &&
         (entry.present & ~entry.computed) == 0 &&
         entry.hashed_boundary == boundary && entry.clear == clear &&
         std::ranges::none_of(changed_,
                              [&entry](const provider::PassRegion& region) {
                                return meets(region, entry.region);
                              }) &&
         (!meets(damage->changed, entry.region) ||
          (!meets(damage->departed, entry.region) &&
           quiet(spans, 0, boundary)));
}

auto Compositor::hashed(std::span<const provider::DrawCommand> prefix,
                        std::span<const QuadSpan> spans,
                        std::span<const provider::ShapeVertex> vertices,
                        const Blur& blur, provider::Colour clear)
    -> std::uint64_t {
  std::uint64_t hash = fnv_offset;
  hash = mix(hash, std::as_bytes(std::span{&blur.region, 1}));
  hash = mix(hash, std::as_bytes(std::span{&clear, 1}));
  hash = mix(hash, std::as_bytes(std::span{&blur.levels, 1}));
  std::size_t cursor = 0;
  for (const provider::DrawCommand& command : prefix) {
    const std::uint32_t first = command.index_offset;
    const std::uint32_t last = command.index_offset + command.index_count;
    while (cursor < spans.size() && spans[cursor].index_begin < first) {
      ++cursor;
    }
    if (!overlaps(command.clip, blur.region)) {
      continue;
    }
    std::uint64_t drawn = fnv_offset;
    bool reached = false;
    for (std::size_t at = cursor;
         at < spans.size() && spans[at].index_begin < last; ++at) {
      const QuadSpan& span = spans[at];
      if (reaches(spans, vertices, at, blur.region)) {
        reached = true;
        drawn = mix(drawn, std::as_bytes(std::span{&span.content, 1}));
        drawn = mix(drawn, std::as_bytes(std::span{span.translation}));
      }
    }
    if (!reached) {
      continue;
    }
    const std::array<std::uint64_t, 5> textures{
        command.texture.value(), command.detail.value(),
        generation_of(command.texture), generation_of(command.detail), drawn};
    hash = mix(hash, std::as_bytes(std::span{textures}));
    hash = mix(hash, std::as_bytes(std::span{&command.clip, 1}));
    hash = mix(hash, std::as_bytes(std::span{command.trim}));
    hash = mix(hash, std::as_bytes(std::span{&command.opacity, 1}));
  }
  return hash;
}

auto Compositor::overlaps(const provider::ClipRect& clip,
                          const provider::PassRegion& region) noexcept -> bool {
  const auto left = static_cast<float>(clip.x);
  const auto top = static_cast<float>(clip.y);
  const auto right = static_cast<float>(clip.x + clip.width);
  const auto bottom = static_cast<float>(clip.y + clip.height);
  return clip.width > 0 && clip.height > 0 && right > region.x &&
         bottom > region.y && left < region.x + region.width &&
         top < region.y + region.height;
}

auto Compositor::blur(provider::GpuFrame& frame,
                      const provider::ShapeGeometry& geometry, Writer& writer,
                      Blur& entry,
                      std::span<const provider::DrawCommand> prefix,
                      provider::Colour clear)
    -> std::expected<void, PaintError> {
  const auto submit =
      [&](std::span<const provider::DrawCommand> commands,
          const provider::RenderPass& pass) -> std::expected<void, PaintError> {
    ++statistics_.passes;
    if (auto drawn = frame.draw(
            provider::ShapeDrawData{.geometry = geometry, .commands = commands},
            pass);
        !drawn.has_value()) {
      return std::unexpected(PaintError{PaintErrorCode::draw_rejected,
                                        commands.data() == prefix.data()
                                            ? PaintStage::backdrop_capture
                                            : PaintStage::blur_level,
                                        drawn.error()});
    }
    return {};
  };
  if (auto drawn =
          submit(prefix, provider::RenderPass{.target = *entry.result.id(),
                                              .region = entry.region,
                                              .clear = clear});
      !drawn.has_value()) {
    return drawn;
  }
  std::vector<RenderTargetPool::Lease>& chain = chain_;
  recycle_chain();
  std::uint32_t width = entry.result.width();
  std::uint32_t height = entry.result.height();
  for (std::uint32_t level = 0; level < entry.levels; ++level) {
    width = std::max((width + 1) / 2, 2U);
    height = std::max((height + 1) / 2, 2U);
    auto leased = pool_.acquire(width, height);
    if (!leased.has_value()) {
      refuse("blur level render target", width, height, leased.error(),
             "the backdrop is blurred with fewer levels");
      break;
    }
    chain.push_back(std::move(*leased));
  }
  const auto pass_over =
      [&](provider::TextureId source, const RenderTargetPool::Lease& target,
          provider::ShapeKind kind) -> std::expected<void, PaintError> {
    const auto w = static_cast<float>(target.width());
    const auto h = static_cast<float>(target.height());
    std::optional<provider::DrawCommand> command =
        quad(writer, provider::ShapeVertex{.colour = 0xffffffffU, .kind = kind},
             {0.0F, 0.0F, w, h}, {0.0F, 0.0F, 1.0F, 1.0F});
    if (!command.has_value()) {
      return std::unexpected(PaintError{PaintErrorCode::geometry_unavailable,
                                        PaintStage::blur_level});
    }
    command->texture = source;
    command->clip = provider::ClipRect{
        .x = 0, .y = 0, .width = target.width(), .height = target.height()};
    return submit(std::span{&*command, 1},
                  provider::RenderPass{
                      .target = *target.id(),
                      .region = {.x = 0.0F, .y = 0.0F, .width = w, .height = h},
                      .clear = std::nullopt});
  };
  for (std::size_t level = 0; level < chain.size(); ++level) {
    const provider::TextureId source =
        level == 0 ? *entry.result.id() : *chain[level - 1].id();
    if (auto drawn =
            pass_over(source, chain[level], provider::ShapeKind::blur_down);
        !drawn.has_value()) {
      recycle_chain();
      return drawn;
    }
  }
  for (std::uint32_t wanted = 1; wanted <= entry.levels; ++wanted) {
    if ((entry.present & bit(wanted)) == 0) {
      continue;
    }
    const RenderTargetPool::Lease& output =
        wanted == entry.levels ? entry.result : entry.outputs[wanted];
    const std::size_t depth = std::min<std::size_t>(wanted, chain.size());
    if (depth == 0 && &output != &entry.result) {
      if (auto drawn = pass_over(*entry.result.id(), output,
                                 provider::ShapeKind::blur_up);
          !drawn.has_value()) {
        recycle_chain();
        return drawn;
      }
    }
    for (std::size_t level = depth; level > 0; --level) {
      const provider::TextureId source = *chain[level - 1].id();
      const RenderTargetPool::Lease& target =
          level == 1 ? output : chain[level - 2];
      if (auto drawn = pass_over(source, target, provider::ShapeKind::blur_up);
          !drawn.has_value()) {
        recycle_chain();
        return drawn;
      }
    }
  }
  recycle_chain();
  return {};
}

auto Compositor::upload(Field& entry) -> std::expected<void, PaintError> {
  std::array<float, field_width> table{};
  table[0] = static_cast<float>(entry.shapes.size());
  table[1] = entry.blend;
  std::size_t at = field_header;
  for (const FieldShape& shape : entry.shapes) {
    const std::array<float, field_stride> member{
        shape.centre[0],    shape.centre[1], shape.half_size[0],
        shape.half_size[1], shape.radius,    shape.smoothing};
    std::ranges::copy(member, table.begin() + static_cast<std::ptrdiff_t>(at));
    at += field_stride;
  }
  if (auto updated = gpu_.get().update_texture(
          *entry.table,
          provider::TextureRegion{
              .x = 0, .y = 0, .width = field_width, .height = 1},
          provider::PixelRows{std::as_bytes(std::span{table})});
      !updated.has_value()) {
    return std::unexpected(PaintError{PaintErrorCode::draw_rejected,
                                      PaintStage::union_field,
                                      updated.error()});
  }
  return {};
}

auto Compositor::repaints(std::uint64_t key) const -> bool {
  for (const PendingBackdrop& pending : backdrops_) {
    if (pending.key != key) {
      continue;
    }
    const auto source = source_of(pending);
    if (source.has_value() && source->get().refreshed == epoch_ &&
        !source->get().explained) {
      return true;
    }
  }
  return std::ranges::any_of(fields_, [this, key](const auto& entry) {
    return entry.second.owner == key && entry.second.refreshed == epoch_;
  });
}

void Compositor::emit(provider::GpuFrame& frame,
                      const provider::ShapeGeometry& geometry,
                      std::uint32_t base_vertex, std::uint32_t first_index,
                      std::span<const provider::DrawCommand> commands,
                      std::span<const QuadSpan> spans,
                      std::optional<DrawListCache::Changes> damage,
                      provider::Colour clear) {
  Writer writer{.vertices = geometry.vertices,
                .indices = geometry.indices,
                .base = base_vertex,
                .first = first_index};
  changed_.clear();
  unsettled_ = false;
  for (const std::uint64_t key : pending_fields_) {
    const auto found = fields_.find(key);
    if (found == fields_.end() || found->second.uploaded ||
        !found->second.table.has_value()) {
      continue;
    }
    if (auto sent = upload(found->second); !sent.has_value()) {
      refuse("union field table", field_width, 1, sent.error(),
             "members are glazed individually", found->second.owner);
      withdraw(found->second.table->id());
      bound(rejected_fields_, key);
      unsettled_ = true;
      continue;
    }
    found->second.uploaded = true;
    found->second.refreshed = epoch_;
    ++found->second.generation;
    ++statistics_.fields;
    changed_.push_back(spread(found->second));
  }
  if (!std::ranges::is_sorted(backdrops_, {}, &PendingBackdrop::first_quad)) {
    std::ranges::stable_sort(backdrops_, {}, &PendingBackdrop::first_quad);
  }
  std::vector<provider::DrawCommand>& prefix = prefix_;
  if (!backdrops_.empty()) {
    extents_.assign(spans.size(), std::nullopt);
    measured_.assign(spans.size(), 0);
  }
  const bool whole = !damage.has_value();
  const bool zoomed = zoomed_ == epoch_;
  bool misgrouped = false;
  for (const PendingBackdrop& pending : backdrops_) {
    const auto source = source_of(pending);
    if (!source.has_value() || source->get().handled == epoch_) {
      continue;
    }
    Blur& entry = source->get();
    entry.handled = epoch_;
    const std::uint32_t boundary = entry.boundary;
    if (pending.group.has_value() && !zoomed &&
        !members_clear(*pending.group, boundary, spans, geometry.vertices,
                       whole)) {
      misgrouped = true;
    }
    if (unchanged(entry, boundary, clear, spans, damage)) {
      entry.checked = epoch_;
      ++statistics_.blur_hits;
      continue;
    }
    const std::uint32_t cut =
        boundary < spans.size() ? spans[boundary].index_begin : first_index;
    prefix.clear();
    for (const provider::DrawCommand& command : commands) {
      if (command.index_offset >= cut) {
        break;
      }
      provider::DrawCommand trimmed = command;
      trimmed.index_count =
          std::min(trimmed.index_count, cut - trimmed.index_offset);
      prefix.push_back(trimmed);
    }
    const std::size_t reach = std::min<std::size_t>(boundary, spans.size());
    const std::uint64_t hash =
        hashed(prefix, spans.first(reach), geometry.vertices, entry, clear);
    const bool known =
        entry.rendered && damage.has_value() && entry.checked + 1 == epoch_ &&
        entry.hashed_boundary == boundary && entry.clear == clear &&
        (entry.present & ~entry.computed) == 0;
    entry.checked = epoch_;
    entry.clear = clear;
    entry.hashed_boundary = boundary;
    if (entry.rendered && entry.hash == hash &&
        (entry.present & ~entry.computed) == 0) {
      ++statistics_.blur_hits;
      continue;
    }
    entry.explained =
        known && (disturbed(spans, geometry.vertices, boundary, entry.region) ||
                  meets(damage->departed, entry.region) ||
                  std::ranges::any_of(changed_,
                                      [&entry](const provider::PassRegion& at) {
                                        return meets(at, entry.region);
                                      }));
    if (auto drawn = blur(frame, geometry, writer, entry, prefix, clear);
        !drawn.has_value()) {
      entry.rendered = false;
      unsettled_ = true;
      withdraw(entry);
      if (pending.group.has_value()) {
        bound(rejected_groups_, *pending.group);
        refuse("shared backdrop pass", entry.result.width(),
               entry.result.height(), drawn.error(),
               "members capture their backdrops separately", pending.key);
      } else {
        bound(rejected_backdrops_, pending.key);
        refuse("backdrop pass", entry.result.width(), entry.result.height(),
               drawn.error(), "glass draws its flat fallback", pending.key);
      }
      continue;
    }
    entry.hash = hash;
    entry.rendered = true;
    entry.refreshed = epoch_;
    entry.computed = entry.present;
    ++entry.generation;
    ++statistics_.blurs;
    changed_.push_back(entry.region);
  }
  if (!zoomed) {
    replan(spans, geometry.vertices, whole, misgrouped);
  }
}

void Compositor::replan(std::span<const QuadSpan> spans,
                        std::span<const provider::ShapeVertex> vertices,
                        bool whole, bool misgrouped) {
  if (separate_) {
    return;
  }
  seen_.clear();
  for (const PendingBackdrop& pending : backdrops_) {
    if (std::ranges::contains(seen_, pending.key, &Consumer::key)) {
      continue;
    }
    seen_.push_back(Consumer{.key = pending.key,
                             .wanted = pending.wanted,
                             .core = pending.core,
                             .levels = pending.levels,
                             .scale = pending.scale});
  }
  if (!whole && !misgrouped && !restless_ && seen_ == consumers_) {
    return;
  }
  std::swap(seen_, consumers_);
  form(spans, vertices);
  std::swap(plan_, next_plan_);
  std::swap(planned_, next_planned_);
  for (const PendingBackdrop& pending : backdrops_) {
    const std::optional<std::uint64_t> now =
        joined(pending.key, pending.wanted, pending.levels, pending.scale);
    if (now != pending.group) {
      unsettled_ = true;
      return;
    }
    if (!now.has_value()) {
      continue;
    }
    const auto was = next_planned_.find(*now);
    if (was == next_planned_.end() || !(was->second == planned_.at(*now))) {
      unsettled_ = true;
      return;
    }
  }
}

void Compositor::form(std::span<const QuadSpan> spans,
                      std::span<const provider::ShapeVertex> vertices) {
  forming_.clear();
  formed_.clear();
  restless_ = false;
  std::size_t index = 0;
  for (const PendingBackdrop& pending : backdrops_) {
    const std::size_t at = index++;
    if (std::ranges::any_of(std::span{backdrops_}.first(at),
                            [&pending](const PendingBackdrop& earlier) {
                              return earlier.key == pending.key;
                            })) {
      continue;
    }
    if (!std::ranges::contains(seen_, Consumer{.key = pending.key,
                                               .wanted = pending.wanted,
                                               .core = pending.core,
                                               .levels = pending.levels,
                                               .scale = pending.scale})) {
      restless_ = true;
      continue;
    }
    const float size = area(pending.wanted);
    const auto fits = [&](const Forming& group) {
      if (group.scale != pending.scale ||
          (group.levels == 0) != (pending.levels == 0) ||
          group.members >= group_members) {
        return false;
      }
      const provider::PassRegion near{
          .x = group.region.x - group_gap,
          .y = group.region.y - group_gap,
          .width = group.region.width + (2.0F * group_gap),
          .height = group.region.height + (2.0F * group_gap)};
      if (!meets(near, pending.wanted)) {
        return false;
      }
      const float joint = area(united(group.region, pending.wanted));
      return joint <= group_slack * (group.area + size) &&
             joint * pending.scale * pending.scale <= maximum_capture_pixels &&
             clear_between(spans, vertices, group.boundary, pending.first_quad,
                           pending.core);
    };
    const auto chosen = std::ranges::find_if(forming_, fits);
    if (chosen == forming_.end()) {
      formed_.emplace_back(forming_.size(), pending.key);
      forming_.push_back(Forming{.id = pending.key,
                                 .region = pending.wanted,
                                 .scale = pending.scale,
                                 .area = size,
                                 .boundary = pending.first_quad,
                                 .levels = pending.levels,
                                 .planned = bit(pending.levels),
                                 .members = 1});
      continue;
    }
    chosen->region = united(chosen->region, pending.wanted);
    chosen->area += size;
    chosen->levels = std::max(chosen->levels, pending.levels);
    chosen->planned |= bit(pending.levels);
    ++chosen->members;
    formed_.emplace_back(static_cast<std::size_t>(chosen - forming_.begin()),
                         pending.key);
  }
  next_plan_.clear();
  next_planned_.clear();
  for (const auto& [group, key] : formed_) {
    const Forming& formed = forming_[group];
    if (formed.members < 2) {
      continue;
    }
    next_plan_.insert_or_assign(key, formed.id);
    next_planned_.insert_or_assign(formed.id, Group{.region = formed.region,
                                                    .scale = formed.scale,
                                                    .levels = formed.levels,
                                                    .planned = formed.planned});
  }
}
}
