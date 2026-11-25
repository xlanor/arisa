#pragma once

#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_schedule.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/yuv_conversion.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace arisa::provider {

class TexturePublications {
 public:
  struct Publication {
    std::uint64_t generation{0};
    bool premultiplied{false};
    TextureId chroma{0};
    YuvConversion conversion{};

    auto planar() const noexcept -> bool { return chroma.value() != 0; }

    friend constexpr auto operator==(const Publication&, const Publication&)
        -> bool = default;
  };

  struct Listing {
    TextureId texture{0};
    std::size_t planes{0};
    std::uint64_t generation{0};
    std::uint64_t sampled{0};
    std::uint64_t completed{0};
    bool premultiplied{false};
    bool retired{false};

    friend constexpr auto operator==(const Listing&, const Listing&)
        -> bool = default;
  };

  explicit TexturePublications(std::weak_ptr<FrameSchedule> schedule) noexcept;

  TexturePublications(const TexturePublications&) = delete;
  auto operator=(const TexturePublications&) -> TexturePublications& = delete;
  TexturePublications(TexturePublications&&) = delete;
  auto operator=(TexturePublications&&) -> TexturePublications& = delete;
  ~TexturePublications() = default;

  void enlist(Texture texture, bool premultiplied,
              support::ScopedCallback released);

  void enlist(std::vector<Texture> planes, const ImportDescription& description,
              support::ScopedCallback released);

  void publish(TextureId id);

  void renew(TextureId id);

  void retire(TextureId id) noexcept;

  auto find(TextureId id) const -> std::optional<Publication>;

  auto in_use(TextureId id) const -> bool;

  auto live() const -> std::size_t;

  auto listing() const -> std::vector<Listing>;

  auto empty() const noexcept -> bool {
    return live_.load(std::memory_order_acquire) == 0;
  }

  template <class Commands, class Projection = std::identity>
  void sampled(GpuSerial serial, const Commands& commands,
               Projection projection = {}) {
    if (empty()) {
      return;
    }
    const std::scoped_lock lock{mutex_};
    for (const auto& command : commands) {
      mark(std::invoke(projection, command).texture, serial);
    }
  }

  void completed(GpuSerial serial);

  auto completed_serial() const -> GpuSerial;

 private:
  struct Entry {
    std::vector<Texture> planes;
    support::ScopedCallback released;
    std::uint64_t generation{0};
    std::uint64_t sampled{0};
    bool premultiplied{false};
    YuvConversion conversion{};
    bool retired{false};

    auto publication() const noexcept -> Publication {
      return Publication{
          .generation = generation,
          .premultiplied = premultiplied,
          .chroma = planes.size() > 1 ? planes[1].id() : TextureId{0},
          .conversion = conversion};
    }
  };

  void mark(TextureId id, GpuSerial serial);

  mutable std::mutex mutex_;
  std::flat_map<std::uint64_t, Entry> entries_;
  std::uint64_t completed_{0};
  std::atomic<std::size_t> live_{0};
  std::weak_ptr<FrameSchedule> schedule_;
};

}
