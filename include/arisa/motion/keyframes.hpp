#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/keyframe_sequence.hpp>
#include <arisa/motion/keyframe_track.hpp>

#include <concepts>
#include <functional>
#include <utility>
#include <vector>

namespace arisa::motion {

template <class Value>
class Keyframes {
 public:
  struct Track {
    std::function<AnimatableVector(const Value&)> read;
    std::function<void(Value&, const AnimatableVector&)> write;
    std::vector<KeyframeSequence::Keyframe> keyframes;
  };

  template <class... Members>
  Keyframes(KeyframeTrack<Value, Members>... tracks) {
    tracks_.reserve(sizeof...(Members));
    (adopt(std::move(tracks)), ...);
  }

  auto tracks() const noexcept -> const std::vector<Track>& { return tracks_; }

 private:
  template <class T>
  void adopt(KeyframeTrack<Value, T> track) {
    if constexpr (std::same_as<Value, T>) {
      tracks_.push_back(
          Track{.read =
                    [](const Value& value) {
                      return AnimatableTraits<T>::vector(value);
                    },
                .write =
                    [](Value& value, const AnimatableVector& vector) {
                      value = AnimatableTraits<T>::value(vector);
                    },
                .keyframes = track.keyframes()});
    } else {
      const typename KeyframeTrack<Value, T>::Member member = track.member();
      tracks_.push_back(
          Track{.read =
                    [member](const Value& value) {
                      return AnimatableTraits<T>::vector(value.*member);
                    },
                .write =
                    [member](Value& value, const AnimatableVector& vector) {
                      value.*member = AnimatableTraits<T>::value(vector);
                    },
                .keyframes = track.keyframes()});
    }
  }

  std::vector<Track> tracks_;
};

template <class Root, class... Members>
Keyframes(KeyframeTrack<Root, Members>...) -> Keyframes<Root>;

}
