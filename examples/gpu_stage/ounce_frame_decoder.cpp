#include <ounce_frame_decoder.hpp>

#include <format>
#include <utility>

namespace arisa::examples {

class OunceFrameDecoder::Session {
  class Key {
    friend class Session;
    Key() = default;
  };

 public:
  explicit Session(Key) noexcept {}

  Session(const Session&) = delete;
  auto operator=(const Session&) -> Session& = delete;
  Session(Session&&) = delete;
  auto operator=(Session&&) -> Session& = delete;

  ~Session() {
    if (open_) {
      videoDecoderClose(&decoder_);
    }
  }

  [[nodiscard]] static auto create(const VideoDecoderConfig& config)
      -> std::expected<std::shared_ptr<Session>, Result> {
    auto session = std::make_shared<Session>(Key{});
    if (const Result result = videoDecoderCreate(&session->decoder_, &config);
        R_FAILED(result)) {
      return std::unexpected(result);
    }
    session->open_ = true;
    return session;
  }

  auto decoder() noexcept -> VideoDecoder* { return &decoder_; }

 private:
  VideoDecoder decoder_{};
  bool open_{false};
};

OunceFrameDecoder::OunceFrameDecoder(std::shared_ptr<Session> session) noexcept
    : session_(std::move(session)) {}

auto OunceFrameDecoder::describe(Result result) -> std::string {
  return std::format("0x{:x} (module {}, description {})", result,
                     R_MODULE(result), R_DESCRIPTION(result));
}

auto OunceFrameDecoder::waiting(Result result) noexcept -> bool {
  return R_VALUE(result) == KERNELRESULT(TimedOut) ||
         result == VIDDEC_RESULT_SUBMIT_BUSY ||
         result == VIDDEC_RESULT_NO_IMAGE;
}

auto OunceFrameDecoder::resident(const VideoDecoder& decoder,
                                 const VideoFrame& frame) noexcept
    -> std::optional<DecodedFrame::Resident> {
  if (frame.tiling != ViddecTilingMode_PitchLinear ||
      frame.slot >= decoder.image_count || frame.planes[0] == nullptr ||
      frame.planes[1] == nullptr) {
    return std::nullopt;
  }
  const auto* const backing =
      static_cast<const std::byte*>(decoder.images[frame.slot].backing);
  const auto* const luma = reinterpret_cast<const std::byte*>(frame.planes[0]);
  const auto* const chroma =
      reinterpret_cast<const std::byte*>(frame.planes[1]);
  const std::size_t size = decoder.images[frame.slot].backing_size;
  if (backing == nullptr || luma < backing || chroma < backing ||
      luma >= backing + size || chroma >= backing + size) {
    return std::nullopt;
  }
  return DecodedFrame::Resident{
      .backing = backing,
      .backing_size = size,
      .luma_offset = static_cast<std::size_t>(luma - backing),
      .luma_pitch = frame.pitches[0],
      .chroma_offset = static_cast<std::size_t>(chroma - backing),
      .chroma_pitch = frame.pitches[1]};
}

auto OunceFrameDecoder::create(const H264AnnexB& stream, Config config)
    -> std::expected<std::unique_ptr<OunceFrameDecoder>, std::string> {
  const std::span<const std::byte> parameter_sets = stream.parameter_sets();
  const VideoDecoderConfig decoder_config{
      .codec = ViddecCodec_H264,
      .width = config.width,
      .height = config.height,
      .image_count = config.image_count,
      .frame_rate = config.frame_rate,
      .tiling = ViddecTilingMode_PitchLinear,
      .color_policy = 0,
      .max_sample_size = 0,
      .codec_config = parameter_sets.data(),
      .codec_config_size = parameter_sets.size()};
  auto session = Session::create(decoder_config);
  if (!session.has_value()) {
    return std::unexpected("the video decoder did not start: " +
                           describe(session.error()));
  }
  return std::make_unique<OunceFrameDecoder>(std::move(*session));
}

auto OunceFrameDecoder::submit(std::span<const std::byte> sample,
                               std::chrono::nanoseconds timestamp)
    -> std::expected<Submission, std::string> {
  const Result result =
      videoDecoderSubmit(session_->decoder(), sample.data(), sample.size(),
                         timestamp.count(), false, 0);
  if (R_SUCCEEDED(result)) {
    return Submission::accepted;
  }
  if (waiting(result)) {
    return Submission::busy;
  }
  return std::unexpected("the video decoder refused a sample: " +
                         describe(result));
}

auto OunceFrameDecoder::next_frame()
    -> std::expected<std::optional<DecodedFrame>, std::string> {
  VideoFrame frame{};
  const Result result = videoDecoderGetFrame(session_->decoder(), &frame, 0);
  if (waiting(result)) {
    return std::nullopt;
  }
  if (R_FAILED(result)) {
    return std::unexpected("the video decoder lost a frame: " +
                           describe(result));
  }
  const DecodedFrame::Picture picture{
      .width = frame.width,
      .height = frame.height,
      .timestamp = std::chrono::nanoseconds{frame.timestamp_ns}};
  DecodedFrame::Copy copy = [session = session_,
                             frame](const DecodedFrame::Planes& planes)
      -> std::expected<void, std::string> {
    const Result copied = videoDecoderCopyFrame(
        session->decoder(), &frame,
        reinterpret_cast<std::uint8_t*>(planes.luma.data()), planes.luma_pitch,
        reinterpret_cast<std::uint8_t*>(planes.chroma.data()),
        planes.chroma_pitch);
    if (R_FAILED(copied)) {
      return std::unexpected("the video frame could not be copied: " +
                             describe(copied));
    }
    return {};
  };
  support::ScopedCallback release{
      [session = session_, frame]() mutable noexcept {
        videoDecoderReleaseFrame(session->decoder(), &frame);
      }};
  return DecodedFrame{picture, std::move(copy), std::move(release),
                      resident(*session_->decoder(), frame)};
}

}
