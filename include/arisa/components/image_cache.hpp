#pragma once

#include <arisa/provider/assets/provider.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/support/strong_value.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace arisa::components {

class ImageCache {
 public:
  struct BudgetTag;
  using Budget = support::StrongValue<std::uint64_t, BudgetTag>;

  static constexpr Budget default_budget{64ULL * 1024ULL * 1024ULL};

  struct Statistics {
    std::size_t entries{0};
    std::uint64_t bytes{0};
    std::uint64_t budget{0};
    std::uint64_t hits{0};
    std::uint64_t misses{0};
    std::uint64_t insertions{0};
    std::uint64_t evictions{0};

    friend constexpr auto operator==(const Statistics&, const Statistics&)
        -> bool = default;
  };

  explicit ImageCache(const provider::AssetProvider& assets,
                      Budget budget = default_budget);

  auto find(std::string_view key) const
      -> std::shared_ptr<const provider::Image>;

  void insert(std::string key,
              std::shared_ptr<const provider::Image> image) const;

  void clear() const;

  auto assets() const noexcept -> const provider::AssetProvider&;

  auto statistics() const -> Statistics;

  static auto bytes_of(const provider::Image& image) noexcept -> std::uint64_t;

  friend auto operator==(const ImageCache& left, const ImageCache& right)
      -> bool {
    return left.store_ == right.store_;
  }

 private:
  class Store;

  std::shared_ptr<Store> store_;
};

}
