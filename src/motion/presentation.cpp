#include <arisa/motion/presentation.hpp>

#include <arisa/theme/oklab.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <ranges>
#include <span>
#include <utility>

namespace arisa::motion {

auto Presentation::precision(Property property) noexcept -> double {
  switch (property) {
    case Property::frame:
    case Property::offset:
    case Property::corner_radius:
    case Property::shadow_geometry:
      return 1.0e-2;
    case Property::opacity:
    case Property::scale:
    case Property::focus:
    case Property::tilt:
    case Property::path:
    case Property::scroll_phase:
    case Property::trim_start:
    case Property::trim_end:
    case Property::mesh:
      return 1.0e-3;
    case Property::stroke:
      return 1.0e-2;
    case Property::rotation:
      return 5.0e-4;
  }
  return 1.0e-3;
}

auto Presentation::moves(Property property) noexcept -> bool {
  switch (property) {
    case Property::frame:
    case Property::offset:
    case Property::scale:
    case Property::rotation:
    case Property::corner_radius:
    case Property::shadow_geometry:
    case Property::focus:
    case Property::tilt:
    case Property::path:
    case Property::scroll_phase:
      return true;
    case Property::opacity:
    case Property::trim_start:
    case Property::trim_end:
    case Property::stroke:
    case Property::mesh:
      return false;
  }
  return false;
}

auto Presentation::matched(const AnimatableVector& source,
                           const AnimatableVector& own,
                           MatchedGeometryProperties properties,
                           UnitPoint anchor) noexcept -> AnimatableVector {
  const bool sized = properties.contains(MatchedGeometryProperties::size);
  const bool placed = properties.contains(MatchedGeometryProperties::position);
  const double width = sized ? source[2] : own[2];
  const double height = sized ? source[3] : own[3];
  const AnimatableVector& origin = placed ? source : own;
  const double anchor_x = origin[0] + (anchor.x * origin[2]);
  const double anchor_y = origin[1] + (anchor.y * origin[3]);
  return AnimatableVector{anchor_x - (anchor.x * width),
                          anchor_y - (anchor.y * height), width, height};
}

auto Presentation::seconds(std::chrono::nanoseconds duration) noexcept
    -> double {
  return std::chrono::duration<double>(duration).count();
}

auto Presentation::lab(theme::Colour colour) noexcept -> AnimatableVector {
  const theme::Oklab converted = theme::Oklab::from(colour);
  return AnimatableVector{converted.lightness(), converted.a(), converted.b(),
                          colour.alpha};
}

auto Presentation::colour(const AnimatableVector& vector) noexcept
    -> theme::Colour {
  return theme::Oklab{static_cast<float>(vector[0]),
                      static_cast<float>(vector[1]),
                      static_cast<float>(vector[2])}
      .to_colour(std::clamp(static_cast<float>(vector[3]), 0.0F, 1.0F));
}

auto Presentation::chosen(const Moment& moment, bool moving)
    -> std::optional<Animation> {
  const std::optional<Animation>& animation =
      moment.transaction.get().animation();
  if (!animation.has_value()) {
    return std::nullopt;
  }
  if (!moment.reduce_motion) {
    return animation;
  }
  if (moving) {
    return std::nullopt;
  }
  return animation->reduced();
}

void Presentation::bestow(AnimationTrack& track, const Animation& animation,
                          const Moment& moment) {
  if (animation.custom_animation() != nullptr && moment.environment) {
    track.set_environment(moment.environment());
  }
}

void Presentation::restart(std::unique_ptr<AnimationTrack>& track,
                           Animation::Sample settled,
                           const AnimatableVector& target, double precision,
                           const std::optional<Animation>& animation,
                           const Moment& moment) {
  if (!animation.has_value()) {
    track.reset();
    return;
  }
  Animation::Sample current = settled;
  std::optional<Spring> previous;
  if (track != nullptr && track->merge(*animation, target, moment.now)) {
    track->hold_until_logically_complete(
        moment.transaction.get().logical_completion());
    return;
  }
  if (track != nullptr) {
    current = track->step(moment.now);
    if (current.finished) {
      current.value = settled.value;
      current.velocity = AnimatableVector{};
    } else {
      previous = track->spring_at(moment.now);
    }
  }
  AnimationTrack fresh{*animation,
                       Animation::Span{.from = current.value,
                                       .to = target,
                                       .velocity = current.velocity,
                                       .precision = precision},
                       moment.now, moment.transaction.get().completion(),
                       previous};
  fresh.hold_until_logically_complete(
      moment.transaction.get().logical_completion());
  bestow(fresh, *animation, moment);
  if (track != nullptr) {
    *track = std::move(fresh);
    return;
  }
  track = std::make_unique<AnimationTrack>(std::move(fresh));
}

void Presentation::begin_morph(Channel& channel, const AnimatableVector& target,
                               const Moment& moment) {
  const AnimatableVector start =
      matched(morph_->source.frame, target, morph_->properties, morph_->anchor);
  channel.target = start;
  channel.presented = start;
  channel.velocity = morph_->source.velocity;
  channel.changed = moment.now;
  channel.track.reset();
  channel.ready = true;
  morph_.reset();
  morphing_ = true;
}

auto Presentation::animate(Property property, AnimatableVector target,
                           const Moment& moment) -> AnimatableVector {
  Channel& channel = channels_[static_cast<std::size_t>(property)];
  const bool framing = property == Property::frame;
  if (framing && morph_.has_value()) {
    begin_morph(channel, target, moment);
  }
  if (!channel.ready) {
    channel.target = target;
    channel.presented = target;
    channel.velocity = AnimatableVector{};
    channel.changed = moment.now;
    channel.ready = true;
    return target;
  }
  if (target != channel.target) {
    const std::optional<Animation> animation = chosen(moment, moves(property));
    const double elapsed = seconds(moment.now - channel.changed);
    const bool fresh = channel.track == nullptr && elapsed <= tracking_window;
    if (animation.has_value()) {
      restart(channel.track,
              Animation::Sample{
                  .value = channel.target,
                  .velocity = fresh ? channel.velocity : AnimatableVector{},
                  .finished = true},
              target, precision(property), animation, moment);
    } else {
      if (fresh && elapsed > 0.0) {
        const AnimatableVector measured =
            (target - channel.target) * (1.0 / elapsed);
        channel.velocity =
            moment.transaction.get().tracks_velocity()
                ? channel.velocity.interpolated(
                      measured, std::min(elapsed / velocity_smoothing, 1.0))
                : measured;
      } else if (!fresh) {
        channel.velocity = AnimatableVector{};
      }
      channel.track.reset();
    }
    channel.target = target;
    channel.changed = moment.now;
  }
  if (channel.track == nullptr) {
    channel.presented = channel.target;
    morphing_ = morphing_ && !framing;
    return channel.target;
  }
  const Animation::Sample sample = channel.track->step(moment.now);
  if (sample.finished) {
    channel.track.reset();
    channel.presented = channel.target;
    channel.velocity = AnimatableVector{};
    morphing_ = morphing_ && !framing;
    return channel.target;
  }
  channel.presented = sample.value;
  channel.velocity = sample.velocity;
  return sample.value;
}

auto Presentation::moves(Mix mix) noexcept -> bool {
  return mix == Mix::projection || mix == Mix::corners;
}

template <class Visit>
void Presentation::each_mix_track(Visit&& visit) {
  if (mixes_ == nullptr) {
    return;
  }
  for (std::unique_ptr<MixChannel>& channel : *mixes_) {
    if (channel != nullptr) {
      visit(channel->track);
    }
  }
}

template <class Visit>
void Presentation::each_mix_track(Visit&& visit) const {
  if (mixes_ == nullptr) {
    return;
  }
  for (const std::unique_ptr<MixChannel>& channel : *mixes_) {
    if (channel != nullptr) {
      visit(channel->track);
    }
  }
}

auto Presentation::mixing(Mix mix) const noexcept -> bool {
  return mixes_ != nullptr &&
         (*mixes_)[static_cast<std::size_t>(mix)] != nullptr;
}

void Presentation::seed(Mix mix, std::span<const float> values) {
  if (mixing(mix)) {
    return;
  }
  if (mixes_ == nullptr) {
    mixes_ = std::make_unique<Mixes>();
  }
  auto channel = std::make_unique<MixChannel>();
  const std::size_t size = std::min(values.size(), mix_capacity);
  std::ranges::copy(values.first(size), channel->to.begin());
  channel->presented = channel->to;
  channel->size = size;
  (*mixes_)[static_cast<std::size_t>(mix)] = std::move(channel);
}

auto Presentation::blend(Mix mix) const noexcept -> std::optional<Blend> {
  if (!in_flight(mix)) {
    return std::nullopt;
  }
  const MixChannel& channel = *(*mixes_)[static_cast<std::size_t>(mix)];
  return Blend{.from = std::span<const float>{channel.from}.first(channel.size),
               .to = std::span<const float>{channel.to}.first(channel.size),
               .progress = channel.progress};
}

auto Presentation::mix(Mix mix, std::span<const float> target,
                       const Moment& moment, std::uint64_t shape)
    -> std::span<const float> {
  const std::size_t size = std::min(target.size(), mix_capacity);
  if (mixes_ == nullptr) {
    mixes_ = std::make_unique<Mixes>();
  }
  std::unique_ptr<MixChannel>& slot = (*mixes_)[static_cast<std::size_t>(mix)];
  if (slot == nullptr) {
    slot = std::make_unique<MixChannel>();
    std::ranges::copy(target.first(size), slot->to.begin());
    slot->presented = slot->to;
    slot->size = size;
    slot->shape = shape;
    return std::span<const float>{slot->presented}.first(size);
  }
  MixChannel& channel = *slot;
  const std::span<const float> wanted = target.first(size);
  const bool reshaped = size != channel.size || shape != channel.shape;
  const bool changed =
      reshaped ||
      !std::ranges::equal(wanted, std::span{channel.to}.first(size));
  if (changed) {
    const std::optional<Animation> animation = chosen(moment, moves(mix));
    if (reshaped || !animation.has_value()) {
      channel.track.reset();
    } else {
      channel.from = channel.presented;
      AnimationTrack fresh{*animation,
                           Animation::Span{.from = AnimatableVector{0.0},
                                           .to = AnimatableVector{1.0},
                                           .velocity = {},
                                           .precision = appearance_precision},
                           moment.now, moment.transaction.get().completion()};
      fresh.hold_until_logically_complete(
          moment.transaction.get().logical_completion());
      bestow(fresh, *animation, moment);
      if (channel.track != nullptr) {
        *channel.track = std::move(fresh);
      } else {
        channel.track = std::make_unique<AnimationTrack>(std::move(fresh));
      }
    }
    std::ranges::copy(wanted, channel.to.begin());
    channel.size = size;
    channel.shape = shape;
  }
  if (channel.track == nullptr) {
    channel.presented = channel.to;
    return std::span<const float>{channel.presented}.first(size);
  }
  const Animation::Sample sample = channel.track->step(moment.now);
  if (sample.finished) {
    channel.track.reset();
    channel.presented = channel.to;
    return std::span<const float>{channel.presented}.first(size);
  }
  const auto progress = static_cast<float>(sample.value[0]);
  channel.progress = progress;
  for (std::size_t index = 0; index < size; ++index) {
    channel.presented[index] =
        channel.from[index] +
        ((channel.to[index] - channel.from[index]) * progress);
  }
  return std::span<const float>{channel.presented}.first(size);
}

auto Presentation::animate(Tint tint, theme::Colour target,
                           const Moment& moment) -> theme::Colour {
  TintChannel& channel = tints_[static_cast<std::size_t>(tint)];
  if (!channel.ready) {
    channel.target = target;
    channel.presented = target;
    channel.ready = true;
    return target;
  }
  if (target != channel.target) {
    restart(channel.track,
            Animation::Sample{
                .value = lab(channel.target), .velocity = {}, .finished = true},
            lab(target), tint_precision, chosen(moment, false), moment);
    channel.target = target;
  }
  if (channel.track == nullptr) {
    channel.presented = channel.target;
    return channel.target;
  }
  const Animation::Sample sample = channel.track->step(moment.now);
  if (sample.finished) {
    channel.track.reset();
    channel.presented = channel.target;
    return channel.target;
  }
  channel.presented = colour(sample.value);
  return channel.presented;
}

auto Presentation::carried(const Transition& transition,
                           const Transaction& transaction)
    -> std::optional<Animation> {
  if (transition.attached_animation().has_value() &&
      !transaction.disables_animations()) {
    return transition.attached_animation();
  }
  return transaction.animation();
}

void Presentation::insert(const Transition& transition,
                          const Transaction& transaction) {
  const std::optional<Animation> animation = carried(transition, transaction);
  if (!animation.has_value()) {
    return;
  }
  appearance_ = Appearance{
      .transition = transition,
      .progress = 0.0,
      .target = 1.0,
      .track = nullptr,
      .pending = Pending{.animation = *animation,
                         .completion = transaction.completion(),
                         .logical = transaction.logical_completion()},
      .removing = false,
      .finished = false};
}

void Presentation::remove(const Transition& transition,
                          const Transaction& transaction) {
  const std::optional<Animation> animation = carried(transition, transaction);
  if (!animation.has_value()) {
    return;
  }
  if (!appearance_.has_value()) {
    appearance_ = Appearance{.transition = transition,
                             .progress = 1.0,
                             .target = 0.0,
                             .track = nullptr,
                             .pending = std::nullopt,
                             .removing = true,
                             .finished = false};
  }
  appearance_->transition = transition;
  appearance_->target = 0.0;
  appearance_->removing = true;
  appearance_->finished = false;
  appearance_->pending = Pending{.animation = *animation,
                                 .completion = transaction.completion(),
                                 .logical = transaction.logical_completion()};
}

auto Presentation::appearance(const Moment& moment)
    -> std::optional<Transition::Effect> {
  if (!appearance_.has_value()) {
    return std::nullopt;
  }
  Appearance& state = *appearance_;
  if (state.pending.has_value()) {
    double from = state.progress;
    AnimatableVector velocity;
    std::optional<Spring> previous;
    if (state.track != nullptr) {
      const Animation::Sample current = state.track->sample(moment.now);
      if (!current.finished) {
        from = current.value[0];
        velocity = current.velocity;
        previous = state.track->spring_at(moment.now);
      }
    }
    const Animation animation = moment.reduce_motion
                                    ? state.pending->animation.reduced()
                                    : state.pending->animation;
    state.track = std::make_unique<AnimationTrack>(
        animation,
        Animation::Span{.from = AnimatableVector{from},
                        .to = AnimatableVector{state.target},
                        .velocity = velocity,
                        .precision = appearance_precision},
        moment.now, std::move(state.pending->completion), previous);
    state.track->hold_until_logically_complete(
        std::move(state.pending->logical));
    bestow(*state.track, animation, moment);
    state.pending.reset();
  }
  if (state.track != nullptr) {
    const Animation::Sample sample = state.track->step(moment.now);
    if (sample.finished) {
      state.track.reset();
      state.progress = state.target;
    } else {
      state.progress = sample.value[0];
    }
  }
  if (state.track == nullptr) {
    if (!state.removing) {
      appearance_.reset();
      return std::nullopt;
    }
    state.finished = true;
  }
  const Transition shown = moment.reduce_motion || moment.cross_fade
                               ? state.transition.reduced()
                               : state.transition;
  return (state.removing ? shown.removal() : shown.insertion())
      .at(state.progress);
}

auto Presentation::exchange(std::string_view content, const Moment& moment,
                            std::optional<double> value)
    -> std::optional<Swap> {
  if (!content_.ready) {
    content_.current = std::string{content};
    content_.ready = true;
    content_.value = value;
    return std::nullopt;
  }
  if (content != content_.current) {
    if (value.has_value() && content_.value.has_value()) {
      content_.descending = *value < *content_.value;
    }
    content_.value = value;
    const std::optional<Animation> animation = chosen(moment, false);
    if (animation.has_value()) {
      content_.outgoing = std::move(content_.current);
      content_.track = std::make_unique<AnimationTrack>(
          *animation,
          Animation::Span{.from = AnimatableVector{0.0},
                          .to = AnimatableVector{1.0},
                          .velocity = {},
                          .precision = appearance_precision},
          moment.now, moment.transaction.get().completion());
      content_.track->hold_until_logically_complete(
          moment.transaction.get().logical_completion());
    } else {
      content_.track.reset();
    }
    content_.current = std::string{content};
  }
  if (content_.track == nullptr) {
    return std::nullopt;
  }
  const Animation::Sample sample = content_.track->step(moment.now);
  if (sample.finished) {
    content_.track.reset();
    return std::nullopt;
  }
  return Swap{.outgoing = content_.outgoing,
              .progress = sample.value[0],
              .descending = content_.descending};
}

auto Presentation::snapshot() const noexcept -> Snapshot {
  const Channel& frame = channels_[static_cast<std::size_t>(Property::frame)];
  const Channel& corner =
      channels_[static_cast<std::size_t>(Property::corner_radius)];
  const TintChannel& fill = tints_[static_cast<std::size_t>(Tint::background)];
  Snapshot taken{.frame = frame.presented,
                 .velocity = frame.velocity,
                 .corner_radius = std::nullopt,
                 .background = std::nullopt};
  if (corner.ready) {
    taken.corner_radius = corner.presented[0];
  }
  if (fill.ready) {
    taken.background = fill.presented;
  }
  return taken;
}

auto Presentation::peek(Property property, std::chrono::nanoseconds now,
                        const AnimatableVector& target) const noexcept
    -> std::optional<AnimatableVector> {
  const Channel& channel = channels_[static_cast<std::size_t>(property)];
  if (property == Property::frame && morph_.has_value()) {
    return matched(morph_->source.frame, target, morph_->properties,
                   morph_->anchor);
  }
  if (channel.track != nullptr) {
    const Animation::Sample sample = channel.track->sample(now);
    return sample.finished ? channel.target : sample.value;
  }
  if (channel.ready) {
    return channel.presented;
  }
  return std::nullopt;
}

void Presentation::morph(const Morph& morph, std::chrono::nanoseconds now) {
  morph_ = morph;
  concealed_ = false;
  if (morph.properties.contains(MatchedGeometryProperties::corner_radius) &&
      morph.source.corner_radius.has_value()) {
    Channel& corner =
        channels_[static_cast<std::size_t>(Property::corner_radius)];
    corner.target = AnimatableVector{*morph.source.corner_radius};
    corner.presented = corner.target;
    corner.velocity = AnimatableVector{};
    corner.changed = now;
    corner.track.reset();
    corner.ready = true;
  }
  if (morph.properties.contains(MatchedGeometryProperties::colour) &&
      morph.source.background.has_value()) {
    TintChannel& fill = tints_[static_cast<std::size_t>(Tint::background)];
    fill.target = *morph.source.background;
    fill.presented = fill.target;
    fill.track.reset();
    fill.ready = true;
  }
  if (appearance_.has_value() && !appearance_->removing) {
    appearance_.reset();
  }
}

void Presentation::conceal() noexcept {
  finish();
  concealed_ = true;
}

auto Presentation::removal_finished() const noexcept -> bool {
  return appearance_.has_value() && appearance_->removing &&
         appearance_->finished;
}

auto Presentation::in_flight(Property property) const noexcept -> bool {
  return channels_[static_cast<std::size_t>(property)].track != nullptr;
}

auto Presentation::in_flight(Tint tint) const noexcept -> bool {
  return tints_[static_cast<std::size_t>(tint)].track != nullptr;
}

auto Presentation::in_flight(Mix mix) const noexcept -> bool {
  if (mixes_ == nullptr) {
    return false;
  }
  const std::unique_ptr<MixChannel>& channel =
      (*mixes_)[static_cast<std::size_t>(mix)];
  return channel != nullptr && channel->track != nullptr;
}

auto Presentation::velocity(Property property) const noexcept
    -> AnimatableVector {
  return channels_[static_cast<std::size_t>(property)].velocity;
}

auto Presentation::animating() const noexcept -> bool {
  const bool channels = std::ranges::any_of(
      channels_,
      [](const Channel& channel) { return channel.track != nullptr; });
  const bool tints = std::ranges::any_of(
      tints_,
      [](const TintChannel& channel) { return channel.track != nullptr; });
  const bool appearing =
      appearance_.has_value() &&
      (appearance_->track != nullptr || appearance_->pending.has_value());
  bool mixing = false;
  each_mix_track([&mixing](const std::unique_ptr<AnimationTrack>& track) {
    mixing = mixing || track != nullptr;
  });
  return channels || tints || appearing || mixing ||
         content_.track != nullptr || morph_.has_value();
}

void Presentation::finish() noexcept {
  for (Channel& channel : channels_) {
    channel.track.reset();
    channel.presented = channel.target;
  }
  for (TintChannel& channel : tints_) {
    channel.track.reset();
    channel.presented = channel.target;
  }
  content_.track.reset();
  if (mixes_ != nullptr) {
    for (std::unique_ptr<MixChannel>& channel : *mixes_) {
      if (channel != nullptr) {
        channel->track.reset();
        channel->presented = channel->to;
      }
    }
  }
  morph_.reset();
  morphing_ = false;
  if (!appearance_.has_value()) {
    return;
  }
  if (appearance_->removing) {
    appearance_->track.reset();
    appearance_->pending.reset();
    appearance_->progress = 0.0;
    appearance_->finished = true;
    return;
  }
  appearance_.reset();
}

void Presentation::prime(Property property, const AnimatableVector& value,
                         std::chrono::nanoseconds now) noexcept {
  Channel& channel = channels_[static_cast<std::size_t>(property)];
  if (channel.ready) {
    return;
  }
  channel.target = value;
  channel.presented = value;
  channel.velocity = AnimatableVector{};
  channel.changed = now;
  channel.ready = true;
}

auto Presentation::endless(
    const std::unique_ptr<AnimationTrack>& track) noexcept -> bool {
  if (track == nullptr) {
    return false;
  }
  const auto& repetition = track->animation().repetition();
  return repetition.has_value() && !repetition->count.has_value();
}

void Presentation::settle(std::chrono::nanoseconds now) noexcept {
  bool kept = false;
  for (Channel& channel : channels_) {
    if (endless(channel.track)) {
      kept = true;
      continue;
    }
    channel.track.reset();
    channel.presented = channel.target;
  }
  for (TintChannel& channel : tints_) {
    if (endless(channel.track)) {
      kept = true;
      continue;
    }
    channel.track.reset();
    channel.presented = channel.target;
  }
  if (endless(content_.track)) {
    kept = true;
  } else {
    content_.track.reset();
  }
  if (mixes_ != nullptr) {
    for (std::unique_ptr<MixChannel>& channel : *mixes_) {
      if (channel == nullptr) {
        continue;
      }
      if (endless(channel->track)) {
        kept = true;
        continue;
      }
      channel->track.reset();
      channel->presented = channel->to;
    }
  }
  morph_.reset();
  morphing_ = false;
  if (appearance_.has_value()) {
    if (appearance_->removing) {
      appearance_->track.reset();
      appearance_->pending.reset();
      appearance_->progress = 0.0;
      appearance_->finished = true;
    } else {
      appearance_.reset();
    }
  }
  if (kept) {
    suspend(now);
  }
}

auto Presentation::name(Property property) noexcept -> std::string_view {
  switch (property) {
    case Property::frame:
      return "frame";
    case Property::opacity:
      return "opacity";
    case Property::scale:
      return "scale";
    case Property::offset:
      return "offset";
    case Property::rotation:
      return "rotation";
    case Property::corner_radius:
      return "corner_radius";
    case Property::shadow_geometry:
      return "shadow_geometry";
    case Property::focus:
      return "focus";
    case Property::tilt:
      return "tilt";
    case Property::path:
      return "path";
    case Property::scroll_phase:
      return "scroll_phase";
    case Property::trim_start:
      return "trim_start";
    case Property::trim_end:
      return "trim_end";
    case Property::stroke:
      return "stroke";
    case Property::mesh:
      return "mesh";
  }
  return "frame";
}

auto Presentation::name(Tint tint) noexcept -> std::string_view {
  switch (tint) {
    case Tint::background:
      return "background";
    case Tint::foreground:
      return "foreground";
    case Tint::border:
      return "border";
    case Tint::shadow:
      return "shadow";
  }
  return "background";
}

auto Presentation::name(Mix mix) noexcept -> std::string_view {
  switch (mix) {
    case Mix::blur:
      return "blur";
    case Mix::colour:
      return "colour";
    case Mix::projection:
      return "projection";
    case Mix::corners:
      return "corners";
    case Mix::fill:
      return "fill";
    case Mix::foreground:
      return "foreground_style";
    case Mix::shape:
      return "shape_fill";
  }
  return "blur";
}

auto Presentation::unhinted_channel() const noexcept
    -> std::optional<std::string_view> {
  const auto unhinted = [](const std::unique_ptr<AnimationTrack>& track) {
    return track != nullptr && !track->animation().frame_rate().has_value();
  };
  for (std::size_t index = 0; index < property_count; ++index) {
    if (unhinted(channels_[index].track)) {
      return name(static_cast<Property>(index));
    }
  }
  for (std::size_t index = 0; index < tint_count; ++index) {
    if (unhinted(tints_[index].track)) {
      return name(static_cast<Tint>(index));
    }
  }
  if (appearance_.has_value() &&
      (unhinted(appearance_->track) || appearance_->pending.has_value())) {
    return "appearance";
  }
  if (unhinted(content_.track)) {
    return "content";
  }
  if (mixes_ != nullptr) {
    for (std::size_t index = 0; index < mix_count; ++index) {
      const std::unique_ptr<MixChannel>& channel = (*mixes_)[index];
      if (channel != nullptr && unhinted(channel->track)) {
        return name(static_cast<Mix>(index));
      }
    }
  }
  if (morph_.has_value()) {
    return "morph";
  }
  return std::nullopt;
}

auto Presentation::frame_rate() const noexcept -> std::optional<double> {
  std::optional<double> fastest;
  bool unhinted = false;
  const auto consider =
      [&fastest, &unhinted](const std::unique_ptr<AnimationTrack>& track) {
        if (track == nullptr) {
          return;
        }
        const std::optional<double> rate = track->animation().frame_rate();
        if (!rate.has_value()) {
          unhinted = true;
          return;
        }
        fastest = std::max(fastest.value_or(0.0), *rate);
      };
  for (const Channel& channel : channels_) {
    consider(channel.track);
  }
  for (const TintChannel& channel : tints_) {
    consider(channel.track);
  }
  if (appearance_.has_value()) {
    consider(appearance_->track);
    unhinted = unhinted || appearance_->pending.has_value();
  }
  consider(content_.track);
  each_mix_track(consider);
  unhinted = unhinted || morph_.has_value();
  if (unhinted) {
    return std::nullopt;
  }
  return fastest;
}

auto Presentation::observed() const noexcept -> bool {
  return channels_[static_cast<std::size_t>(Property::frame)].ready;
}

auto Presentation::track(Property property) const noexcept
    -> std::optional<std::reference_wrapper<const AnimationTrack>> {
  const std::unique_ptr<AnimationTrack>& found =
      channels_[static_cast<std::size_t>(property)].track;
  if (found == nullptr || paused_.has_value()) {
    return std::nullopt;
  }
  return std::cref(*found);
}

void Presentation::suspend(std::chrono::nanoseconds now) noexcept {
  if (!paused_.has_value()) {
    paused_ = now;
  }
}

void Presentation::resume(std::chrono::nanoseconds now) noexcept {
  if (!paused_.has_value()) {
    return;
  }
  const std::chrono::nanoseconds gap = now - *paused_;
  paused_.reset();
  if (gap <= std::chrono::nanoseconds::zero()) {
    return;
  }
  const auto postpone = [gap](std::unique_ptr<AnimationTrack>& track) {
    if (track != nullptr) {
      track->postpone(gap);
    }
  };
  for (Channel& channel : channels_) {
    postpone(channel.track);
    channel.changed += gap;
  }
  for (TintChannel& channel : tints_) {
    postpone(channel.track);
  }
  if (appearance_.has_value()) {
    postpone(appearance_->track);
  }
  postpone(content_.track);
  each_mix_track(postpone);
}

}
