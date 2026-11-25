#pragma once

#include <arisa/provider/gpu/draw_data.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <flat_map>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace arisa::rendering {

class DrawListCache {
 public:
  struct Key {
    std::uint64_t owner{0};
    std::uint32_t ordinal{0};

    friend constexpr auto operator<=>(const Key&, const Key&) = default;
  };

  struct Bounds {
    float left{0.0F};
    float top{0.0F};
    float right{0.0F};
    float bottom{0.0F};

    auto empty() const noexcept -> bool {
      return !(right > left) || !(bottom > top);
    }

    auto united(const Bounds& other) const noexcept -> Bounds;

    auto clipped(const Bounds& other) const noexcept -> Bounds;

    auto moved(std::array<float, 2> by) const noexcept -> Bounds;

    auto grown(float margin) const noexcept -> Bounds;

    auto intersects(const Bounds& other) const noexcept -> bool;

    auto area() const noexcept -> float;

    friend constexpr auto operator==(const Bounds&, const Bounds&)
        -> bool = default;
  };

  struct Run {
    Key key;
    std::uint64_t content{0};
    std::uint64_t order{0};
    std::uint32_t count{0};
    bool fresh{false};
    std::array<float, 2> offset{};
    float opacity{1.0F};
  };

  struct Extent {
    Bounds drawn{};
    Bounds local{};
    Bounds clip{};
    Bounds sense{};
    float reach{std::numeric_limits<float>::infinity()};
  };

  struct Changes {
    std::span<const Bounds> changed;
    std::span<const Bounds> departed;
  };

  struct Placement {
    std::uint32_t first{0};
    std::uint32_t count{0};
    bool write{false};
    bool damaged{false};
  };

  static constexpr std::uint32_t granule = 4;
  static constexpr std::uint32_t compaction_floor = 4096;
  static constexpr std::uint64_t seed = 0x9E37'79B9'7F4A'7C15ULL;

  static auto hash(std::uint64_t state,
                   std::span<const std::byte> bytes) noexcept -> std::uint64_t;

  template <class T>
  static auto hash_value(std::uint64_t state, const T& value) noexcept
      -> std::uint64_t {
    return hash(
        state, std::span<const std::byte>{std::as_bytes(std::span{&value, 1})});
  }

  void reserve(std::size_t runs);

  void begin(bool whole);

  auto place(const Run& run) -> std::size_t;

  auto adopt(std::uint64_t generation) -> bool;

  auto placement(std::size_t index) const noexcept -> Placement;

  void bound(std::size_t index, const Extent& extent);

  void refresh(std::size_t index);

  auto local(std::size_t index) const -> Bounds;

  auto extent() const noexcept -> std::uint32_t { return high_; }

  auto changes() -> std::optional<Changes>;

  [[nodiscard]] auto finish() -> std::optional<Bounds>;

  auto entries() const noexcept -> std::size_t { return entries_.size(); }

 private:
  struct Entry {
    std::uint64_t content{0};
    std::uint64_t order{0};
    std::uint64_t seen{0};
    std::uint32_t first{0};
    std::uint32_t count{0};
    std::uint32_t capacity{0};
    std::uint32_t rank{0};
    Bounds bounds{};
    Bounds sense{};
    Bounds local{};
    Bounds clip{};
    float reach{std::numeric_limits<float>::infinity()};
    std::array<float, 2> offset{};
    float opacity{1.0F};
  };

  struct Placed {
    Key key;
    std::uint32_t first{0};
    std::uint32_t count{0};
    bool write{false};
    bool damaged{false};
    bool known{false};
    Bounds before{};
    std::uint32_t rank{0};
    bool reordered{false};
  };

  struct Touch {
    Bounds bounds{};
    std::size_t depth{0};
    bool departed{false};
  };

  auto allocate(std::uint32_t count) -> std::uint32_t;

  void release(std::uint32_t first, std::uint32_t count);

  std::flat_map<Key, Entry> entries_;
  std::vector<Placed> frame_;
  std::vector<provider::VertexRange> free_;
  std::vector<Key> departed_;
  std::uint32_t highest_{0};
  std::uint32_t high_{0};
  std::uint32_t spare_{0};
  std::uint64_t epoch_{0};
  std::uint64_t generation_{0};
  bool whole_{true};
  std::vector<Touch> touched_;
  std::vector<Bounds> banded_;
  std::vector<Bounds> changes_;
  std::vector<Bounds> departures_;
};

}
