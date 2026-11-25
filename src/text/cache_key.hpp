#pragma once

#include <arisa/text/text_fitting.hpp>
#include <arisa/text/text_style.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace arisa::text {

class CacheKey {
 public:
  class Probe {
   public:
    Probe(std::string_view text, const TextStyle& style, float width,
          float scale, TextFitting fitting = {}) noexcept;

   private:
    friend class CacheKey;

    std::string_view text_;
    std::reference_wrapper<const TextStyle> style_;
    float width_;
    float scale_;
    TextFitting fitting_;
    std::size_t hash_;
  };

  struct Hash {
    using is_transparent = void;

    auto operator()(const CacheKey& key) const noexcept -> std::size_t {
      return key.hash_;
    }

    auto operator()(const Probe& probe) const noexcept -> std::size_t {
      return probe.hash_;
    }
  };

  struct Equal {
    using is_transparent = void;

    auto operator()(const CacheKey& left, const CacheKey& right) const -> bool {
      return left == right;
    }

    auto operator()(const CacheKey& key, const Probe& probe) const -> bool {
      return key.matches(probe);
    }

    auto operator()(const Probe& probe, const CacheKey& key) const -> bool {
      return key.matches(probe);
    }
  };

  explicit CacheKey(const Probe& probe);

  friend auto operator==(const CacheKey&, const CacheKey&) -> bool = default;

 private:
  static auto digest(std::string_view text, const TextStyle& style, float width,
                     float scale, const TextFitting& fitting) noexcept
      -> std::size_t;

  static auto combine(std::size_t seed, std::size_t value) noexcept
      -> std::size_t;

  auto matches(const Probe& probe) const -> bool;

  std::string text_;
  TextStyle style_;
  float width_;
  float scale_;
  TextFitting fitting_;
  std::size_t hash_;
};

}
