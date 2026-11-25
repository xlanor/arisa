#include <arisa/components/image_cache.hpp>

#include <list>
#include <map>
#include <mutex>
#include <utility>

namespace arisa::components {

class ImageCache::Store {
 public:
  Store(const provider::AssetProvider& assets, Budget budget) noexcept
      : assets_(assets), budget_(budget) {}

  auto find(std::string_view key) -> std::shared_ptr<const provider::Image> {
    const std::scoped_lock lock{mutex_};
    const auto found = index_.find(key);
    if (found == index_.end()) {
      ++misses_;
      return nullptr;
    }
    ++hits_;
    order_.splice(order_.begin(), order_, found->second);
    return found->second->image;
  }

  void insert(std::string key, std::shared_ptr<const provider::Image> image) {
    if (image == nullptr) {
      return;
    }
    const std::uint64_t bytes = bytes_of(*image);
    const std::scoped_lock lock{mutex_};
    if (const auto found = index_.find(key); found != index_.end()) {
      bytes_ -= found->second->bytes;
      order_.erase(found->second);
      index_.erase(found);
    }
    if (bytes > budget_.value()) {
      return;
    }
    order_.push_front(
        Entry{.key = key, .image = std::move(image), .bytes = bytes});
    index_.emplace(std::move(key), order_.begin());
    bytes_ += bytes;
    ++insertions_;
    while (bytes_ > budget_.value() && !order_.empty()) {
      const Entry& oldest = order_.back();
      bytes_ -= oldest.bytes;
      index_.erase(oldest.key);
      order_.pop_back();
      ++evictions_;
    }
  }

  void clear() {
    const std::scoped_lock lock{mutex_};
    index_.clear();
    order_.clear();
    bytes_ = 0;
  }

  auto assets() const noexcept -> const provider::AssetProvider& {
    return assets_;
  }

  auto statistics() const -> Statistics {
    const std::scoped_lock lock{mutex_};
    return Statistics{.entries = index_.size(),
                      .bytes = bytes_,
                      .budget = budget_.value(),
                      .hits = hits_,
                      .misses = misses_,
                      .insertions = insertions_,
                      .evictions = evictions_};
  }

 private:
  struct Entry {
    std::string key;
    std::shared_ptr<const provider::Image> image;
    std::uint64_t bytes{0};
  };

  std::reference_wrapper<const provider::AssetProvider> assets_;
  Budget budget_;
  mutable std::mutex mutex_;
  std::list<Entry> order_;
  std::map<std::string, std::list<Entry>::iterator, std::less<>> index_;
  std::uint64_t bytes_{0};
  std::uint64_t hits_{0};
  std::uint64_t misses_{0};
  std::uint64_t insertions_{0};
  std::uint64_t evictions_{0};
};

ImageCache::ImageCache(const provider::AssetProvider& assets, Budget budget)
    : store_(std::make_shared<Store>(assets, budget)) {}

auto ImageCache::find(std::string_view key) const
    -> std::shared_ptr<const provider::Image> {
  return store_->find(key);
}

void ImageCache::insert(std::string key,
                        std::shared_ptr<const provider::Image> image) const {
  store_->insert(std::move(key), std::move(image));
}

void ImageCache::clear() const { store_->clear(); }

auto ImageCache::assets() const noexcept -> const provider::AssetProvider& {
  return store_->assets();
}

auto ImageCache::statistics() const -> Statistics {
  return store_->statistics();
}

auto ImageCache::bytes_of(const provider::Image& image) noexcept
    -> std::uint64_t {
  return std::uint64_t{image.width()} * image.height() *
         provider::Image::channels;
}

}
