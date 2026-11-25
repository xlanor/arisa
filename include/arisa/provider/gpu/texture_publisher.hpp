#pragma once

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/texture_publications.hpp>

#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::provider {

struct Unfenced {
  friend constexpr auto operator==(Unfenced, Unfenced) -> bool = default;
};

template <class Fence>
class TexturePublisher : public TexturePublications {
 public:
  using TexturePublications::publish;
  using TexturePublications::TexturePublications;

  void publish(TextureId id, Fence ready) {
    if constexpr (!std::is_same_v<Fence, Unfenced>) {
      const std::scoped_lock lock{fences_mutex_};
      fences_.push_back(std::move(ready));
    }
    publish(id);
  }

  template <class Visit>
  void drain(Visit&& visit) {
    const std::scoped_lock lock{fences_mutex_};
    for (Fence& fence : fences_) {
      visit(fence);
    }
    fences_.clear();
  }

 private:
  std::mutex fences_mutex_;
  std::vector<Fence> fences_;
};

}
