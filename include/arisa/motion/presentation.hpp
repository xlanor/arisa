#pragma once

#include <arisa/composition/environment_values.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_completion.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::motion {

class Presentation {
 public:
  enum class Property : std::uint8_t {
    frame,
    opacity,
    scale,
    offset,
    rotation,
    corner_radius,
    shadow_geometry,
    focus,
    tilt,
    path,
    scroll_phase,
    trim_start,
    trim_end,
    stroke,
    mesh,
  };

  enum class Tint : std::uint8_t {
    background,
    foreground,
    border,
    shadow,
  };

  enum class Mix : std::uint8_t {
    blur,
    colour,
    projection,
    corners,
    fill,
    foreground,
    shape,
  };

  static constexpr std::size_t property_count = 15;
  static constexpr std::size_t mix_count = 7;
  static constexpr std::size_t mix_capacity = 64;
  static constexpr std::size_t tint_count = 4;
  static constexpr double tint_precision = 5.0e-4;
  static constexpr double appearance_precision = 1.0e-3;
  static constexpr double tracking_window = 0.1;
  static constexpr double velocity_smoothing = 1.0 / 30.0;

  struct Moment {
    std::reference_wrapper<const Transaction> transaction;
    std::chrono::nanoseconds now{0};
    bool reduce_motion{false};
    bool cross_fade{false};
    std::function<composition::EnvironmentValues()> environment{};
  };

  struct Snapshot {
    AnimatableVector frame;
    AnimatableVector velocity;
    std::optional<double> corner_radius;
    std::optional<theme::Colour> background;
  };

  struct Morph {
    Snapshot source;
    MatchedGeometryProperties properties{MatchedGeometryProperties::frame};
    UnitPoint anchor{UnitPoint::center};
  };

  struct Swap {
    std::string_view outgoing;
    double progress{0.0};
    bool descending{false};
  };

  auto animate(Property property, AnimatableVector target, const Moment& moment)
      -> AnimatableVector;

  auto animate(Tint tint, theme::Colour target, const Moment& moment)
      -> theme::Colour;

  auto mix(Mix mix, std::span<const float> target, const Moment& moment,
           std::uint64_t shape = 0) -> std::span<const float>;

  auto mixing(Mix mix) const noexcept -> bool;

  struct Blend {
    std::span<const float> from;
    std::span<const float> to;
    float progress{0.0F};
  };

  void seed(Mix mix, std::span<const float> values);

  auto blend(Mix mix) const noexcept -> std::optional<Blend>;

  static auto carried(const Transition& transition,
                      const Transaction& transaction)
      -> std::optional<Animation>;

  void insert(const Transition& transition, const Transaction& transaction);

  void remove(const Transition& transition, const Transaction& transaction);

  auto appearance(const Moment& moment) -> std::optional<Transition::Effect>;

  auto exchange(std::string_view content, const Moment& moment,
                std::optional<double> value = std::nullopt)
      -> std::optional<Swap>;

  auto snapshot() const noexcept -> Snapshot;

  auto peek(Property property, std::chrono::nanoseconds now,
            const AnimatableVector& target) const noexcept
      -> std::optional<AnimatableVector>;

  void morph(const Morph& morph, std::chrono::nanoseconds now);

  auto morphing() const noexcept -> bool { return morphing_; }

  auto morph_pending() const noexcept -> bool { return morph_.has_value(); }

  void conceal() noexcept;

  auto concealed() const noexcept -> bool { return concealed_; }

  auto withhold(bool withheld) noexcept -> bool {
    return std::exchange(withheld_, withheld) != withheld;
  }

  auto withheld() const noexcept -> bool { return withheld_; }

  auto removal_finished() const noexcept -> bool;

  auto in_flight(Property property) const noexcept -> bool;

  auto in_flight(Tint tint) const noexcept -> bool;

  auto in_flight(Mix mix) const noexcept -> bool;

  auto track(Property property) const noexcept
      -> std::optional<std::reference_wrapper<const AnimationTrack>>;

  auto velocity(Property property) const noexcept -> AnimatableVector;

  auto animating() const noexcept -> bool;

  auto frame_rate() const noexcept -> std::optional<double>;

  auto unhinted_channel() const noexcept -> std::optional<std::string_view>;

  static auto name(Property property) noexcept -> std::string_view;

  static auto name(Tint tint) noexcept -> std::string_view;

  static auto name(Mix mix) noexcept -> std::string_view;

  void suspend(std::chrono::nanoseconds now) noexcept;

  void resume(std::chrono::nanoseconds now) noexcept;

  auto suspended() const noexcept -> bool { return paused_.has_value(); }

  auto observed() const noexcept -> bool;

  void finish() noexcept;

  void prime(Property property, const AnimatableVector& value,
             std::chrono::nanoseconds now) noexcept;

  void settle(std::chrono::nanoseconds now) noexcept;

  static auto precision(Property property) noexcept -> double;

  static auto moves(Property property) noexcept -> bool;

  static auto endless(const std::unique_ptr<AnimationTrack>& track) noexcept
      -> bool;

  static auto matched(const AnimatableVector& source,
                      const AnimatableVector& own,
                      MatchedGeometryProperties properties,
                      UnitPoint anchor) noexcept -> AnimatableVector;

 private:
  struct Channel {
    AnimatableVector target;
    AnimatableVector presented;
    AnimatableVector velocity;
    std::chrono::nanoseconds changed{0};
    std::unique_ptr<AnimationTrack> track;
    bool ready{false};
  };

  struct TintChannel {
    theme::Colour target;
    theme::Colour presented;
    std::unique_ptr<AnimationTrack> track;
    bool ready{false};
  };

  struct MixChannel {
    std::array<float, mix_capacity> from{};
    std::array<float, mix_capacity> to{};
    std::array<float, mix_capacity> presented{};
    std::size_t size{0};
    std::uint64_t shape{0};
    float progress{1.0F};
    std::unique_ptr<AnimationTrack> track;
  };

  using Mixes = std::array<std::unique_ptr<MixChannel>, mix_count>;

  struct Pending {
    Animation animation;
    std::shared_ptr<AnimationCompletion> completion;
    std::shared_ptr<AnimationCompletion> logical;
  };

  struct Appearance {
    Transition transition;
    double progress{0.0};
    double target{1.0};
    std::unique_ptr<AnimationTrack> track;
    std::optional<Pending> pending;
    bool removing{false};
    bool finished{false};
  };

  struct Content {
    std::string current;
    std::string outgoing;
    std::unique_ptr<AnimationTrack> track;
    bool ready{false};
    std::optional<double> value;
    bool descending{false};
  };

  static auto lab(theme::Colour colour) noexcept -> AnimatableVector;

  static auto colour(const AnimatableVector& vector) noexcept -> theme::Colour;

  static auto chosen(const Moment& moment, bool moving)
      -> std::optional<Animation>;

  static void bestow(AnimationTrack& track, const Animation& animation,
                     const Moment& moment);

  static void restart(std::unique_ptr<AnimationTrack>& track,
                      Animation::Sample settled, const AnimatableVector& target,
                      double precision,
                      const std::optional<Animation>& animation,
                      const Moment& moment);

  static auto seconds(std::chrono::nanoseconds duration) noexcept -> double;

  void begin_morph(Channel& channel, const AnimatableVector& target,
                   const Moment& moment);

  static auto moves(Mix mix) noexcept -> bool;

  template <class Visit>
  void each_mix_track(Visit&& visit);

  template <class Visit>
  void each_mix_track(Visit&& visit) const;

  std::array<Channel, property_count> channels_;
  std::array<TintChannel, tint_count> tints_;
  std::optional<Appearance> appearance_;
  Content content_;
  std::optional<Morph> morph_;
  std::unique_ptr<Mixes> mixes_;
  bool morphing_{false};
  bool concealed_{false};
  bool withheld_{false};
  std::optional<std::chrono::nanoseconds> paused_;
};

}
