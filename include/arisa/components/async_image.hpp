#pragma once

#include <arisa/async/executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/async_image_phase.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/image_cache.hpp>
#include <arisa/components/image_source.hpp>
#include <arisa/components/url_request.hpp>
#include <arisa/components/url_session.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/provider/browser/url.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/error.hpp>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

struct AsyncImageOptions {
  float scale{1.0F};
  std::optional<motion::Animation> animation{};
  std::optional<motion::Transaction> transaction{};
};

class AsyncImage : public composition::Element {
 public:
  using PhaseContent = std::move_only_function<composition::Element(
      const AsyncImagePhase&) const>;
  using ImageContent =
      std::move_only_function<composition::Element(Image) const>;
  using PlaceholderContent =
      std::move_only_function<composition::Element() const>;

  struct Loaded {
    std::string key;
    AsyncImagePhase phase;

    friend auto operator==(const Loaded&, const Loaded&) -> bool = default;
  };

  class Content;

  static const composition::StateKey<Loaded> loaded_key;

  explicit AsyncImage(ImageSource source, AsyncImageOptions options = {});

  AsyncImage(ImageSource source, ImageContent content,
             PlaceholderContent placeholder, AsyncImageOptions options = {});

  AsyncImage(ImageSource source, AsyncImageOptions options,
             PhaseContent content);

  explicit AsyncImage(std::optional<provider::Url> url,
                      AsyncImageOptions options = {});

  AsyncImage(std::optional<provider::Url> url, ImageContent content,
             PlaceholderContent placeholder, AsyncImageOptions options = {});

  AsyncImage(std::optional<provider::Url> url, AsyncImageOptions options,
             PhaseContent content);

  explicit AsyncImage(std::optional<UrlRequest> request,
                      AsyncImageOptions options = {});

  AsyncImage(std::optional<UrlRequest> request, ImageContent content,
             PlaceholderContent placeholder, AsyncImageOptions options = {});

  AsyncImage(std::optional<UrlRequest> request, AsyncImageOptions options,
             PhaseContent content);

  static auto source_for(std::optional<UrlRequest> request) -> ImageSource;

  static auto placeholder() -> composition::Element;

  static auto standard(const AsyncImagePhase& phase) -> composition::Element;
};

class AsyncImage::Content final : public composition::ElementContent {
 public:
  Content(ImageSource source, AsyncImageOptions options,
          PhaseContent builder) noexcept
      : source_(std::move(source)),
        options_(std::move(options)),
        builder_(std::move(builder)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto source() const noexcept -> const ImageSource& { return source_; }

 private:
  using Diagnostics =
      std::optional<std::reference_wrapper<provider::DiagnosticsProvider>>;

  static auto load(ImageSource source, ImageCache cache,
                   composition::StateHandle<Loaded> loaded,
                   std::reference_wrapper<async::Executor> background,
                   std::reference_wrapper<async::Executor> root,
                   Diagnostics diagnostics, AsyncImageOptions options)
      -> async::Task<void>;

  static constexpr std::uint32_t large_image = 512;

  static void announce(const Diagnostics& diagnostics, std::string_view key,
                       const provider::Image& image);

  static void report(const Diagnostics& diagnostics, std::string_view key,
                     const provider::ProviderError& error);

  static void publish(const composition::StateHandle<Loaded>& loaded,
                      Loaded value, const AsyncImageOptions& options);

  ImageSource source_;
  AsyncImageOptions options_;
  PhaseContent builder_;
};

}
