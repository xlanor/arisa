#include <arisa/provider/gpu/frame_packet.hpp>

#include <utility>

namespace arisa::provider {

FramePacket::FramePacket(std::shared_ptr<Submitter> submitter,
                         Contents contents) noexcept
    : submitter_(std::move(submitter)), contents_(contents) {}

FramePacket::FramePacket(FramePacket&& other) noexcept
    : submitter_(std::move(other.submitter_)), contents_(other.contents_) {}

auto FramePacket::operator=(FramePacket&& other) noexcept -> FramePacket& {
  if (this != &other) {
    abandon();
    submitter_ = std::move(other.submitter_);
    contents_ = other.contents_;
  }
  return *this;
}

FramePacket::~FramePacket() { abandon(); }

auto FramePacket::submit(Clock clock)
    -> std::expected<FrameReceipt, ProviderError> {
  const std::shared_ptr<Submitter> submitter = std::exchange(submitter_, {});
  if (submitter == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return submitter->submit(*this, clock);
}

void FramePacket::abandon() noexcept {
  if (const std::shared_ptr<Submitter> submitter =
          std::exchange(submitter_, {})) {
    submitter->abandon(*this);
  }
}

}
