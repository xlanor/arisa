#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/state_handle.hpp>

#include <optional>
#include <utility>

namespace arisa::components {

class LabelColumn {
 public:
  static constexpr float gap = 8.0F;

  static const composition::EnvironmentKey<std::optional<LabelColumn>> key;

  explicit LabelColumn(composition::StateHandle<float> width) noexcept
      : width_(std::move(width)), shown_(width_.get()) {}

  auto width() const noexcept -> float { return shown_; }

  void report(float natural) const {
    if (natural > width_.get()) {
      width_.set(natural);
    }
  }

  friend auto operator==(const LabelColumn& left, const LabelColumn& right)
      -> bool {
    return left.width_.identity() == right.width_.identity() &&
           left.shown_ == right.shown_;
  }

 private:
  composition::StateHandle<float> width_;
  float shown_;
};

}
