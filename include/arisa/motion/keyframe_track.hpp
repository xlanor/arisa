#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/keyframe_sequence.hpp>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::motion {

template <class Keyframe>
concept KeyframeSource = requires(const Keyframe& keyframe) {
  { keyframe.keyframe() } -> std::same_as<KeyframeSequence::Keyframe>;
};

template <class Root, Animatable T>
class KeyframeTrack {
  template <class Owner, bool = std::is_class_v<Owner>>
  struct Pointer {
    using type = std::nullptr_t;
  };

  template <class Owner>
  struct Pointer<Owner, true> {
    using type = T Owner::*;
  };

 public:
  using Member = typename Pointer<Root>::type;

  template <KeyframeSource... Keyframes>
    requires(!std::same_as<Root, T>)
  explicit KeyframeTrack(Member member, const Keyframes&... keyframes)
      : member_(member), keyframes_{keyframes.keyframe()...} {}

  template <KeyframeSource... Keyframes>
    requires std::same_as<Root, T>
  explicit KeyframeTrack(const Keyframes&... keyframes)
      : member_(nullptr), keyframes_{keyframes.keyframe()...} {}

  auto member() const noexcept -> Member { return member_; }

  auto whole() const noexcept -> bool { return member_ == nullptr; }

  auto keyframes() const noexcept
      -> const std::vector<KeyframeSequence::Keyframe>& {
    return keyframes_;
  }

 private:
  Member member_;
  std::vector<KeyframeSequence::Keyframe> keyframes_;
};

template <class Root, class T, class... Keyframes>
KeyframeTrack(T Root::*, const Keyframes&...) -> KeyframeTrack<Root, T>;

}
