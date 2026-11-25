#include <arisa/provider/horizon/ounce/ounce_pointer.hpp>

#include <algorithm>
#include <span>
#include <utility>

namespace arisa::provider::horizon::ounce {

auto OuncePointer::Session::create(SurfaceSize surface)
    -> std::expected<Session, ProviderError> {
  auto memory = std::make_unique<TransferMemory>();
  if (const Result initialised =
          hidInitializeNmouse(memory->bytes.data(), memory->bytes.size(),
                              static_cast<s32>(surface.width.value()),
                              static_cast<s32>(surface.height.value()),
                              HidNmousePointerType_Default);
      R_FAILED(initialised)) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::platform_failure, initialised});
  }
  static_cast<void>(hidSetMergedNmouseSourceTypes(sources));
  return Session{std::move(memory), surface};
}

auto OuncePointer::Session::operator=(Session&& other) noexcept -> Session& {
  if (this != &other) {
    finish();
    memory_ = std::move(other.memory_);
    surface_ = other.surface_;
  }
  return *this;
}

OuncePointer::Session::~Session() { finish(); }

void OuncePointer::Session::finish() noexcept {
  if (memory_ == nullptr) {
    return;
  }
  if (R_SUCCEEDED(hidFinalizeNmouse())) {
    memory_.reset();
    return;
  }
  static_cast<void>(memory_.release());
}

void OuncePointer::Session::resize(SurfaceSize surface) {
  if (surface == surface_ || surface.empty()) {
    return;
  }
  if (R_SUCCEEDED(hidSetNmousePointerRanges(
          static_cast<s32>(surface.width.value()),
          static_cast<s32>(surface.height.value())))) {
    surface_ = surface;
  }
}

OuncePointer::OuncePointer(SurfaceSize surface) {
  auto created = Session::create(surface);
  if (created.has_value()) {
    session_.emplace(std::move(*created));
  } else {
    failure_ = created.error();
  }
}

auto OuncePointer::acquire() -> bool {
  if (handle_.has_value()) {
    return true;
  }
  if (retry_ > 0) {
    --retry_;
    return false;
  }
  HidNmouseHandle handle{};
  if (R_FAILED(hidGetMergedNmouseHandle(&handle))) {
    retry_ = retry_interval;
    return false;
  }
  static_cast<void>(hidEnableNmousePointer(handle, true));
  handle_ = handle;
  sampled_ = 0;
  return true;
}

auto OuncePointer::presser() -> std::optional<std::size_t> {
  std::array<HidNmouseHandle, max_devices> handles{};
  s32 total = 0;
  if (R_FAILED(hidGetAllNmouseHandles(
          handles.data(), static_cast<s32>(handles.size()), &total))) {
    return std::nullopt;
  }
  const auto listed =
      std::min(static_cast<std::size_t>(std::max(total, 0)), handles.size());
  for (const HidNmouseHandle& handle : std::span{handles}.first(listed)) {
    u32 types = 0;
    if (R_FAILED(hidGetNmouseTypeSet(handle, &types)) ||
        (types & joy_cons) == 0) {
      continue;
    }
    HidNmouseState state{};
    std::size_t count = 0;
    if (R_FAILED(hidGetNmouseStates(handle, &state, 1, &count)) || count == 0 ||
        (state.buttons & (HidMouseButton_Left | HidMouseButton_Right)) == 0) {
      continue;
    }
    HidNpadIdType id{};
    if (R_FAILED(hidGetNmouseNpadIdType(handle, &id))) {
      continue;
    }
    if (id == HidNpadIdType_Handheld) {
      return 0;
    }
    if (id >= HidNpadIdType_No1 &&
        static_cast<std::size_t>(id - HidNpadIdType_No1) < max_pads) {
      return static_cast<std::size_t>(id - HidNpadIdType_No1);
    }
  }
  return std::nullopt;
}

auto OuncePointer::sample(SurfaceSize surface) -> PointerReading {
  if (!session_.has_value()) {
    return PointerReading{};
  }
  session_->resize(surface);
  if (!acquire()) {
    state_.reset();
    return PointerReading{};
  }
  std::array<HidNmouseState, history> states{};
  std::size_t count = 0;
  if (R_FAILED(
          hidGetNmouseStates(*handle_, states.data(), states.size(), &count))) {
    handle_.reset();
    state_.reset();
    return PointerReading{};
  }
  count = std::min(count, states.size());
  if (count == 0) {
    if (state_.has_value()) {
      state_->wheel_x = 0.0F;
      state_->wheel_y = 0.0F;
    }
    return PointerReading{.pointer = state_};
  }
  const HidNmouseState& newest = states.front();
  float wheel = 0.0F;
  for (const HidNmouseState& each : std::span{states}.first(count)) {
    if (sampled_ != 0 && each.sampling_number <= sampled_) {
      break;
    }
    wheel += static_cast<float>(each.wheel_delta_y);
  }
  sampled_ = newest.sampling_number;
  const bool held_before =
      state_.has_value() && (state_->left || state_->right);
  PixelPoint position{.x = static_cast<float>(newest.x),
                      .y = static_cast<float>(newest.y)};
  if ((newest.flags & tracked_flag) == 0 && state_.has_value()) {
    position = state_->position;
  }
  state_ = PointerState{.position = position,
                        .left = (newest.buttons & HidMouseButton_Left) != 0,
                        .right = (newest.buttons & HidMouseButton_Right) != 0,
                        .middle = (newest.buttons & HidMouseButton_Middle) != 0,
                        .wheel_x = 0.0F,
                        .wheel_y = wheel};
  const bool held = state_->left || state_->right;
  if (held && !held_before) {
    claimed_ = presser();
  }
  const std::optional<std::size_t> claimed = claimed_;
  if (!held) {
    claimed_.reset();
  }
  return PointerReading{.pointer = state_,
                        .claims_shoulders = held_before || held,
                        .claimed_pad = claimed,
                        .claims_sticks = wheel != 0.0F};
}

}
