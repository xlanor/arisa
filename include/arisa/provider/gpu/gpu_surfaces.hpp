#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/surface_directory.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/gpu/surface_presence.hpp>
#include <arisa/provider/gpu/surface_underlay.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/texture_publications.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <flat_map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider {

template <class... Args>
class GpuSurfaces {
 public:
  using Function = std::move_only_function<void(Args..., const SurfaceFrame&)>;
  using Factory = std::move_only_function<std::expected<Texture, ProviderError>(
      TextureDescription)>;

  struct Due {
    SurfaceId surface{0};
    TextureId texture{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint64_t generation{0};
  };

  using Underlay = SurfaceUnderlay;

  using Hole = std::pair<std::size_t, Underlay>;

  GpuSurfaces(std::shared_ptr<FrameSchedule> schedule,
              std::shared_ptr<TexturePublications> publications,
              Factory factory)
      : state_(std::make_shared<State>(
            std::move(schedule), std::move(publications), std::move(factory))) {
  }

  GpuSurfaces(const GpuSurfaces&) = delete;
  auto operator=(const GpuSurfaces&) -> GpuSurfaces& = delete;
  GpuSurfaces(GpuSurfaces&&) noexcept = default;
  auto operator=(GpuSurfaces&&) noexcept -> GpuSurfaces& = default;

  ~GpuSurfaces() {
    if (state_ != nullptr) {
      state_->retire();
    }
  }

  [[nodiscard]] auto add(SurfaceOptions options, Function function)
      -> std::expected<GpuSurface, ProviderError> {
    if (!function) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    return state_->add(options, std::move(function));
  }

  auto installed() const noexcept -> bool { return state_->installed(); }

  auto calls() const noexcept -> std::size_t { return state_->calls(); }

  auto directory() const noexcept -> SurfaceDirectory& { return *state_; }

  template <class Command, class Projection>
  auto due(std::span<const Command> commands, GpuSerial serial,
           Projection projection) -> std::vector<Due> {
    return state_->due(commands, serial, projection);
  }

  template <class Command, class Projection>
  auto holes(std::span<const Command> commands, Projection projection) const
      -> std::vector<Hole> {
    return state_->underlays(commands, projection);
  }

  template <class Command, class Projection, class Draw, class Fill>
  void interleave(std::span<const Command> commands, Projection projection,
                  Draw&& draw, Fill&& hole) const {
    const std::vector<Hole> holes = state_->underlays(commands, projection);
    interleave(commands, std::span<const Hole>{holes}, std::forward<Draw>(draw),
               std::forward<Fill>(hole));
  }

  template <class Command, class Draw, class Fill>
  static void interleave(std::span<const Command> commands,
                         std::span<const Hole> holes, Draw&& draw,
                         Fill&& hole) {
    interleave(
        commands.size(), holes,
        [&commands, &draw](std::size_t first, std::size_t count) {
          draw(commands.subspan(first, count));
        },
        std::forward<Fill>(hole));
  }

  template <class Draw, class Fill>
  static void interleave(std::size_t commands, std::span<const Hole> holes,
                         Draw&& draw, Fill&& hole) {
    std::size_t begin = 0;
    for (const auto& [index, underlay] : holes) {
      if (index > begin) {
        draw(begin, index - begin);
      }
      hole(underlay);
      begin = index + 1;
    }
    if (begin < commands) {
      draw(begin, commands - begin);
    }
  }

  auto run(SurfaceId surface, Args... args, const SurfaceFrame& frame) -> bool {
    return state_->run(surface, args..., frame);
  }

  static auto intersect(ClipRect a, ClipRect b) noexcept -> ClipRect {
    const std::uint32_t left = std::max(a.x, b.x);
    const std::uint32_t top = std::max(a.y, b.y);
    const std::uint32_t right = std::min(a.x + a.width, b.x + b.width);
    const std::uint32_t bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) {
      return ClipRect{};
    }
    return ClipRect{
        .x = left, .y = top, .width = right - left, .height = bottom - top};
  }

  static auto mapped(ClipRect clip, PassRegion region, std::uint32_t width,
                     std::uint32_t height) noexcept -> ClipRect {
    if (region.width <= 0.0F || region.height <= 0.0F) {
      return ClipRect{};
    }
    const float across = static_cast<float>(width) / region.width;
    const float down = static_cast<float>(height) / region.height;
    const auto edge = [](float value, float origin, float factor,
                         std::uint32_t limit, bool up) {
      const float scaled = (value - origin) * factor;
      const float rounded = up ? std::ceil(scaled) : std::floor(scaled);
      return static_cast<std::uint32_t>(
          std::clamp(rounded, 0.0F, static_cast<float>(limit)));
    };
    const std::uint32_t left =
        edge(static_cast<float>(clip.x), region.x, across, width, false);
    const std::uint32_t top =
        edge(static_cast<float>(clip.y), region.y, down, height, false);
    const std::uint32_t right = edge(static_cast<float>(clip.x + clip.width),
                                     region.x, across, width, true);
    const std::uint32_t bottom = edge(static_cast<float>(clip.y + clip.height),
                                      region.y, down, height, true);
    if (right <= left || bottom <= top) {
      return ClipRect{};
    }
    return ClipRect{
        .x = left, .y = top, .width = right - left, .height = bottom - top};
  }

 private:
  static auto lease(FrameSchedule& schedule, const SurfaceOptions& options)
      -> FrameHookId {
    return schedule.lease(FrameHookOptions{.drive = options.drive,
                                           .frame_rate = options.frame_rate},
                          false);
  }

  static void release(FrameSchedule& schedule, FrameHookId id) {
    schedule.retire(id);
  }

  static void steer(FrameSchedule& schedule, FrameHookId id, FrameDrive drive) {
    schedule.set_drive(id, drive);
  }

  static auto handle(std::shared_ptr<SurfaceDirectory> directory,
                     SurfaceId surface, SurfaceMode mode) -> GpuSurface {
    return GpuSurface{std::move(directory), surface, mode};
  }

  class Entry {
   public:
    explicit Entry(Function function) noexcept
        : function_(std::move(function)) {}

    auto call(Args... args, const SurfaceFrame& frame) -> bool {
      const std::scoped_lock lock{calling_};
      if (!function_) {
        return false;
      }
      function_(args..., frame);
      return true;
    }

    void retire() noexcept {
      Function released;
      {
        const std::scoped_lock lock{calling_};
        released = std::move(function_);
        function_ = nullptr;
      }
    }

   private:
    Function function_;
    std::mutex calling_;
  };

  struct Record {
    SurfaceOptions options;
    FrameHookId lease{0};
    std::shared_ptr<Entry> entry;
    std::optional<TextureId> texture;
    std::uint32_t width{0};
    std::uint32_t height{0};
    PassRegion area{};
    std::uint64_t generation{0};
    std::uint64_t rendered{0};
    bool requested{false};
    bool fresh{false};
    std::weak_ptr<const SurfacePresence> presence;
  };

  class State final : public SurfaceDirectory,
                      public std::enable_shared_from_this<State> {
   public:
    State(std::shared_ptr<FrameSchedule> schedule,
          std::shared_ptr<TexturePublications> publications,
          Factory factory) noexcept
        : schedule_(std::move(schedule)),
          publications_(std::move(publications)),
          factory_(std::move(factory)) {}

    auto add(SurfaceOptions options, Function function) -> GpuSurface {
      const FrameHookId leased = GpuSurfaces::lease(*schedule_, options);
      std::uint64_t id = 0;
      {
        const std::scoped_lock lock{mutex_};
        id = next_id_++;
        records_.insert_or_assign(
            id, Record{.options = options,
                       .lease = leased,
                       .entry = std::make_shared<Entry>(std::move(function)),
                       .texture = std::nullopt,
                       .width = 0,
                       .height = 0,
                       .area = PassRegion{},
                       .generation = 0,
                       .rendered = 0,
                       .requested = false,
                       .fresh = false,
                       .presence = {}});
        count_.store(records_.size(), std::memory_order_release);
      }
      return GpuSurfaces::handle(this->shared_from_this(), SurfaceId{id},
                                 options.mode);
    }

    auto installed() const noexcept -> bool {
      return count_.load(std::memory_order_acquire) > 0;
    }

    auto calls() const noexcept -> std::size_t {
      return calls_.load(std::memory_order_relaxed);
    }

    void retire() noexcept {
      Factory released;
      {
        const std::scoped_lock lock{factory_mutex_};
        released = std::move(factory_);
        factory_ = nullptr;
      }
    }

    auto place(SurfaceId surface, PassRegion area)
        -> std::optional<Placement> override {
      const std::uint32_t width = extent(area.width);
      const std::uint32_t height = extent(area.height);
      TextureDescription description{};
      bool premultiplied = false;
      SurfaceMode mode = SurfaceMode::texture;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found == records_.end()) {
          return std::nullopt;
        }
        Record& record = found->second;
        record.area = area;
        mode = record.options.mode;
        if (record.texture.has_value() &&
            (mode == SurfaceMode::underlay ||
             (record.width == width && record.height == height))) {
          return Placement{.texture = *record.texture, .mode = mode};
        }
        premultiplied = record.options.premultiplied;
        description =
            mode == SurfaceMode::underlay
                ? TextureDescription{.width = 1,
                                     .height = 1,
                                     .format = TextureFormat::rgba8,
                                     .usage = TextureUsage::sampled,
                                     .filter = SamplerFilter::nearest}
                : TextureDescription{.width = width,
                                     .height = height,
                                     .format = TextureFormat::rgba8,
                                     .usage = TextureUsage::render_target,
                                     .filter = record.options.filter};
      }
      auto created = create(description);
      if (!created.has_value()) {
        return std::nullopt;
      }
      const TextureId made = created->id();
      publications_->enlist(std::move(*created),
                            premultiplied && mode == SurfaceMode::texture, {});
      std::optional<TextureId> retired;
      bool kept = false;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found != records_.end()) {
          Record& record = found->second;
          retired = std::exchange(record.texture, made);
          if (retired.has_value()) {
            textures_.erase(retired->value());
          }
          textures_.insert_or_assign(made.value(), surface.value());
          record.width = description.width;
          record.height = description.height;
          record.fresh = true;
          ++record.generation;
          kept = true;
        }
      }
      if (!kept) {
        retired = made;
      }
      if (retired.has_value()) {
        publications_->retire(*retired);
      }
      if (!kept) {
        return std::nullopt;
      }
      return Placement{.texture = made, .mode = mode};
    }

    void advance() override {
      std::vector<TextureId> renewed;
      {
        const std::scoped_lock lock{mutex_};
        for (const Record& record : records_ | std::views::values) {
          if (record.texture.has_value() &&
              (record.options.mode == SurfaceMode::underlay ||
               record.options.drive == FrameDrive::continuous)) {
            renewed.push_back(*record.texture);
          }
        }
      }
      for (const TextureId texture : renewed) {
        publications_->renew(texture);
      }
    }

    void request_frame(SurfaceId surface) override {
      std::optional<TextureId> texture;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found == records_.end()) {
          return;
        }
        found->second.requested = true;
        if (found->second.options.mode == SurfaceMode::texture) {
          texture = found->second.texture;
        }
      }
      if (texture.has_value()) {
        publications_->publish(*texture);
        return;
      }
      schedule_->request_frame();
    }

    void set_drive(SurfaceId surface, FrameDrive drive) override {
      FrameHookId leased{0};
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found == records_.end()) {
          return;
        }
        found->second.options.drive = drive;
        leased = found->second.lease;
      }
      GpuSurfaces::steer(*schedule_, leased, drive);
    }

    auto drive(SurfaceId surface) const -> FrameDrive override {
      const std::scoped_lock lock{mutex_};
      const auto found = records_.find(surface.value());
      return found == records_.end() ? FrameDrive::on_demand
                                     : found->second.options.drive;
    }

    auto presence(SurfaceId surface)
        -> std::shared_ptr<const SurfacePresence> override {
      const std::scoped_lock lock{mutex_};
      const auto found = records_.find(surface.value());
      if (found == records_.end()) {
        return nullptr;
      }
      if (std::shared_ptr<const SurfacePresence> held =
              found->second.presence.lock()) {
        return held;
      }
      auto made = std::make_shared<const SurfacePresence>(
          std::weak_ptr<SurfaceDirectory>{this->weak_from_this()}, surface);
      found->second.presence = made;
      return made;
    }

    void vacate(SurfaceId surface) noexcept override {
      std::optional<TextureId> retired;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found == records_.end() || !found->second.presence.expired()) {
          return;
        }
        Record& record = found->second;
        retired = std::exchange(record.texture, std::nullopt);
        if (retired.has_value()) {
          textures_.erase(retired->value());
        }
        record.width = 0;
        record.height = 0;
      }
      if (retired.has_value()) {
        publications_->retire(*retired);
      }
    }

    void remove(SurfaceId surface) noexcept override {
      std::optional<Record> removed;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found == records_.end()) {
          return;
        }
        removed.emplace(std::move(found->second));
        records_.erase(found);
        if (removed->texture.has_value()) {
          textures_.erase(removed->texture->value());
        }
        count_.store(records_.size(), std::memory_order_release);
      }
      if (removed->texture.has_value()) {
        publications_->retire(*removed->texture);
      }
      GpuSurfaces::release(*schedule_, removed->lease);
      removed->entry->retire();
    }

    auto listing() const -> std::vector<Listing> override {
      const std::scoped_lock lock{mutex_};
      std::vector<Listing> found;
      found.reserve(records_.size());
      for (const auto& [id, record] : records_) {
        found.push_back(Listing{.surface = SurfaceId{id},
                                .mode = record.options.mode,
                                .drive = record.options.drive,
                                .frame_rate = record.options.frame_rate,
                                .width = record.width,
                                .height = record.height,
                                .generation = record.generation});
      }
      return found;
    }

    template <class Command, class Projection>
    auto due(std::span<const Command> commands, GpuSerial serial,
             Projection& projection) -> std::vector<Due> {
      std::vector<Due> found;
      if (!installed()) {
        return found;
      }
      const std::scoped_lock lock{mutex_};
      if (textures_.empty()) {
        return found;
      }
      for (const Command& command : commands) {
        const DrawCommand& drawn = std::invoke(projection, command);
        const auto texture = textures_.find(drawn.texture.value());
        if (texture == textures_.end()) {
          continue;
        }
        const auto at = records_.find(texture->second);
        if (at == records_.end()) {
          continue;
        }
        Record& record = at->second;
        if (record.options.mode != SurfaceMode::texture ||
            record.rendered == serial.value() ||
            !(record.fresh || record.requested ||
              record.options.drive == FrameDrive::continuous)) {
          continue;
        }
        const ClipRect visible = underlay(at->first, record, drawn).scissor;
        if (visible.width == 0 || visible.height == 0) {
          continue;
        }
        record.rendered = serial.value();
        record.fresh = false;
        record.requested = false;
        found.push_back(Due{.surface = SurfaceId{at->first},
                            .texture = drawn.texture,
                            .width = record.width,
                            .height = record.height,
                            .generation = record.generation});
      }
      return found;
    }

    template <class Command, class Projection>
    auto underlays(std::span<const Command> commands,
                   Projection& projection) const
        -> std::vector<std::pair<std::size_t, Underlay>> {
      std::vector<std::pair<std::size_t, Underlay>> found;
      if (!installed()) {
        return found;
      }
      const std::scoped_lock lock{mutex_};
      if (textures_.empty()) {
        return found;
      }
      for (std::size_t index = 0; index < commands.size(); ++index) {
        const DrawCommand& drawn = std::invoke(projection, commands[index]);
        if (drawn.detail != drawn.texture) {
          continue;
        }
        const auto texture = textures_.find(drawn.texture.value());
        if (texture == textures_.end()) {
          continue;
        }
        const auto at = records_.find(texture->second);
        if (at == records_.end() ||
            at->second.options.mode != SurfaceMode::underlay ||
            std::ranges::any_of(found, [&at](const auto& seen) {
              return seen.second.surface.value() == at->first;
            })) {
          continue;
        }
        found.emplace_back(index, underlay(at->first, at->second, drawn));
      }
      return found;
    }

    auto run(SurfaceId surface, Args... args, const SurfaceFrame& frame)
        -> bool {
      std::shared_ptr<Entry> entry;
      {
        const std::scoped_lock lock{mutex_};
        const auto found = records_.find(surface.value());
        if (found == records_.end()) {
          return false;
        }
        entry = found->second.entry;
      }
      if (!entry->call(args..., frame)) {
        return false;
      }
      calls_.fetch_add(1, std::memory_order_relaxed);
      return true;
    }

   private:
    static auto extent(float length) noexcept -> std::uint32_t {
      return static_cast<std::uint32_t>(std::clamp(
          std::lround(length), 1L, static_cast<long>(Texture::maximum_extent)));
    }

    static auto underlay(std::uint64_t id, const Record& record,
                         const DrawCommand& command) noexcept -> Underlay {
      const float left = std::round(record.area.x + command.translation[0]);
      const float top = std::round(record.area.y + command.translation[1]);
      const float width = std::round(record.area.width);
      const float height = std::round(record.area.height);
      const float right = std::max(left + width, 0.0F);
      const float bottom = std::max(top + height, 0.0F);
      const float clamped_left = std::max(left, 0.0F);
      const float clamped_top = std::max(top, 0.0F);
      const ClipRect area{
          .x = static_cast<std::uint32_t>(clamped_left),
          .y = static_cast<std::uint32_t>(clamped_top),
          .width =
              static_cast<std::uint32_t>(std::max(right - clamped_left, 0.0F)),
          .height =
              static_cast<std::uint32_t>(std::max(bottom - clamped_top, 0.0F))};
      return Underlay{
          .surface = SurfaceId{id},
          .viewport =
              PassRegion{.x = left, .y = top, .width = width, .height = height},
          .scissor = GpuSurfaces::intersect(command.clip, area)};
    }

    auto create(const TextureDescription& description)
        -> std::expected<Texture, ProviderError> {
      const std::scoped_lock lock{factory_mutex_};
      if (!factory_) {
        return std::unexpected(ProviderError::unsupported_capability());
      }
      return factory_(description);
    }

    std::shared_ptr<FrameSchedule> schedule_;
    std::shared_ptr<TexturePublications> publications_;
    std::mutex factory_mutex_;
    Factory factory_;
    mutable std::mutex mutex_;
    std::flat_map<std::uint64_t, Record> records_;
    std::flat_map<std::uint64_t, std::uint64_t> textures_;
    std::uint64_t next_id_{1};
    std::atomic<std::size_t> count_{0};
    std::atomic<std::size_t> calls_{0};
  };

  std::shared_ptr<State> state_;
};

}
