#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/provider.hpp>
#include <arisa/provider/gpu/texture.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::rendering {

class RenderTargetPool {
 public:
  struct Statistics {
    std::size_t created{0};
    std::size_t reused{0};
    std::size_t released{0};
    std::size_t evicted{0};
    std::size_t refused{0};
    std::size_t leased{0};
    std::size_t idle{0};
    std::size_t returning{0};

    friend constexpr auto operator==(const Statistics&, const Statistics&)
        -> bool = default;
  };

  enum class Fit : std::uint8_t {
    exact,
    at_least,
  };

 private:
  struct Shelf {
    struct Idle {
      provider::Texture texture;
      Fit made{Fit::exact};
      std::uint64_t since{0};
    };

    std::vector<Idle> idle;
    std::vector<Idle> returning;
    std::uint64_t epoch{1};
    Statistics statistics;
  };

 public:
  class Lease {
   public:
    Lease() noexcept = default;
    Lease(const Lease&) = delete;
    auto operator=(const Lease&) -> Lease& = delete;
    Lease(Lease&& other) noexcept;
    auto operator=(Lease&& other) noexcept -> Lease&;
    ~Lease();

    auto valid() const noexcept -> bool { return texture_.has_value(); }

    auto id() const noexcept -> std::optional<provider::TextureId>;

    auto width() const noexcept -> std::uint32_t;

    auto height() const noexcept -> std::uint32_t;

   private:
    friend class RenderTargetPool;

    Lease(std::shared_ptr<Shelf> shelf, provider::Texture texture,
          Fit made) noexcept;

    void give_back() noexcept;

    std::shared_ptr<Shelf> shelf_;
    std::optional<provider::Texture> texture_;
    Fit made_{Fit::exact};
  };

  static constexpr std::uint32_t granule = 8;
  static constexpr std::uint32_t coarse_granule = 64;
  static constexpr std::uint64_t oversize_limit = 2;
  static constexpr std::uint64_t grace_frames = 30;

  explicit RenderTargetPool(provider::GpuProvider& gpu);

  [[nodiscard]] auto acquire(std::uint32_t width, std::uint32_t height,
                             Fit fit = Fit::exact)
      -> std::expected<Lease, provider::ProviderError>;

  void recycle(Lease scratch) noexcept;

  void advance_frame();

  auto statistics() const noexcept -> Statistics;

  static auto rounded(std::uint32_t extent) noexcept -> std::uint32_t;

  static auto rounded(std::uint32_t extent, Fit fit) noexcept -> std::uint32_t;

 private:
  auto idle_fitting(std::uint32_t wide, std::uint32_t tall, Fit fit) noexcept
      -> std::vector<Shelf::Idle>::iterator;

  auto created(std::uint32_t width, std::uint32_t height)
      -> std::expected<provider::Texture, provider::ProviderError>;

  auto evict_earlier_frames() -> std::size_t;

  std::reference_wrapper<provider::GpuProvider> gpu_;
  std::shared_ptr<Shelf> shelf_;
};

}
