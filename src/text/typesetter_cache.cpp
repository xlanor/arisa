#include <text/typesetter_cache.hpp>

namespace arisa::text {

TypesetterCache::TypesetterCache(TypesetterLimits limits) noexcept
    : layouts_(limits.layouts.value()),
      shaped_(limits.shaped.value()),
      measures_(limits.layouts.value()),
      fits_(limits.layouts.value()) {}

void TypesetterCache::clear() noexcept {
  layouts_.clear();
  shaped_.clear();
  measures_.clear();
  fits_.clear();
}

}
