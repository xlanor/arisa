#pragma once

#include <arisa/text/fitted_text.hpp>
#include <arisa/text/text_layout.hpp>
#include <arisa/text/text_size.hpp>
#include <arisa/text/typesetter_limits.hpp>
#include <text/cache_key.hpp>
#include <text/lru_cache.hpp>
#include <text/shaped_text.hpp>

#include <cstddef>
#include <memory>

namespace arisa::text {

class TypesetterCache {
 public:
  explicit TypesetterCache(TypesetterLimits limits) noexcept;

  auto layouts() noexcept
      -> LruCache<CacheKey, std::shared_ptr<const TextLayout>, CacheKey::Hash,
                  CacheKey::Equal>& {
    return layouts_;
  }

  auto shaped() noexcept
      -> LruCache<CacheKey, std::shared_ptr<const ShapedText>, CacheKey::Hash,
                  CacheKey::Equal>& {
    return shaped_;
  }

  auto measures() noexcept
      -> LruCache<CacheKey, TextSize, CacheKey::Hash, CacheKey::Equal>& {
    return measures_;
  }

  auto fits() noexcept -> LruCache<CacheKey, std::shared_ptr<const FittedText>,
                                   CacheKey::Hash, CacheKey::Equal>& {
    return fits_;
  }

  auto layout_count() const noexcept -> std::size_t { return layouts_.size(); }

  auto shaped_count() const noexcept -> std::size_t { return shaped_.size(); }

  void clear() noexcept;

 private:
  LruCache<CacheKey, std::shared_ptr<const TextLayout>, CacheKey::Hash,
           CacheKey::Equal>
      layouts_;
  LruCache<CacheKey, std::shared_ptr<const ShapedText>, CacheKey::Hash,
           CacheKey::Equal>
      shaped_;
  LruCache<CacheKey, TextSize, CacheKey::Hash, CacheKey::Equal> measures_;
  LruCache<CacheKey, std::shared_ptr<const FittedText>, CacheKey::Hash,
           CacheKey::Equal>
      fits_;
};

}
