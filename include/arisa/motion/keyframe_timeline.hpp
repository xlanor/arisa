#pragma once

#include <arisa/motion/keyframe_sequence.hpp>
#include <arisa/motion/keyframes.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace arisa::motion {

template <class Value>
class KeyframeTimeline {
 public:
  struct Progress {
    double fraction{0.0};
  };

  KeyframeTimeline(Value initial_value, const Keyframes<Value>& keyframes)
      : initial_(std::move(initial_value)) {
    for (const typename Keyframes<Value>::Track& track : keyframes.tracks()) {
      sequences_.emplace_back(track.read(initial_), track.keyframes);
      writers_.push_back(track.write);
      duration_ = std::max(duration_, sequences_.back().duration());
    }
  }

  auto value(double time) const -> Value {
    Value current = initial_;
    for (std::size_t index = 0; index < sequences_.size(); ++index) {
      writers_[index](current, sequences_[index].value(time));
    }
    return current;
  }

  auto value(Progress progress) const -> Value {
    return value(std::clamp(progress.fraction, 0.0, 1.0) * duration_);
  }

  auto duration() const noexcept -> double { return duration_; }

 private:
  Value initial_;
  std::vector<KeyframeSequence> sequences_;
  std::vector<std::function<void(Value&, const AnimatableVector&)>> writers_;
  double duration_{0.0};
};

}
