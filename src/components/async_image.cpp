#include <arisa/async/awaitables.hpp>
#include <arisa/components/async_image.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/root_services.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>

#include <format>
#include <memory>
#include <utility>

namespace arisa::components {

const composition::StateKey<AsyncImage::Loaded> AsyncImage::loaded_key{
    "async-image-loaded", Loaded{}};

AsyncImage::AsyncImage(ImageSource source, AsyncImageOptions options)
    : AsyncImage(std::move(source), std::move(options),
                 PhaseContent{[](const AsyncImagePhase& phase) {
                   return standard(phase);
                 }}) {}

AsyncImage::AsyncImage(ImageSource source, ImageContent content,
                       PlaceholderContent placeholder,
                       AsyncImageOptions options)
    : AsyncImage(std::move(source), std::move(options),
                 PhaseContent{[content = std::move(content),
                               placeholder = std::move(placeholder)](
                                  const AsyncImagePhase& phase) {
                   if (std::optional<Image> image = phase.image()) {
                     return content ? content(std::move(*image))
                                    : composition::Element{std::move(*image)};
                   }
                   return placeholder ? placeholder()
                                      : AsyncImage::placeholder();
                 }}) {}

AsyncImage::AsyncImage(ImageSource source, AsyncImageOptions options,
                       PhaseContent content)
    : Element(std::make_unique<Content>(std::move(source), std::move(options),
                                        std::move(content))) {}

AsyncImage::AsyncImage(std::optional<provider::Url> url,
                       AsyncImageOptions options)
    : AsyncImage(source_for(url.transform([](provider::Url value) {
                   return UrlRequest{std::move(value)};
                 })),
                 std::move(options)) {}

AsyncImage::AsyncImage(std::optional<provider::Url> url, ImageContent content,
                       PlaceholderContent placeholder,
                       AsyncImageOptions options)
    : AsyncImage(source_for(url.transform([](provider::Url value) {
                   return UrlRequest{std::move(value)};
                 })),
                 std::move(content), std::move(placeholder),
                 std::move(options)) {}

AsyncImage::AsyncImage(std::optional<provider::Url> url,
                       AsyncImageOptions options, PhaseContent content)
    : AsyncImage(source_for(url.transform([](provider::Url value) {
                   return UrlRequest{std::move(value)};
                 })),
                 std::move(options), std::move(content)) {}

AsyncImage::AsyncImage(std::optional<UrlRequest> request,
                       AsyncImageOptions options)
    : AsyncImage(source_for(std::move(request)), std::move(options)) {}

AsyncImage::AsyncImage(std::optional<UrlRequest> request, ImageContent content,
                       PlaceholderContent placeholder,
                       AsyncImageOptions options)
    : AsyncImage(source_for(std::move(request)), std::move(content),
                 std::move(placeholder), std::move(options)) {}

AsyncImage::AsyncImage(std::optional<UrlRequest> request,
                       AsyncImageOptions options, PhaseContent content)
    : AsyncImage(source_for(std::move(request)), std::move(options),
                 std::move(content)) {}

auto AsyncImage::source_for(std::optional<UrlRequest> request) -> ImageSource {
  if (!request.has_value()) {
    return ImageSource::loader("absent-url", nullptr);
  }
  return ImageSource::url(std::move(*request));
}

auto AsyncImage::placeholder() -> composition::Element {
  return ZStack()
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .background(theme::ColourRole::secondary_system_fill,
                  theme::Shape::rectangle());
}

auto AsyncImage::standard(const AsyncImagePhase& phase)
    -> composition::Element {
  if (std::optional<Image> image = phase.image()) {
    return std::move(*image);
  }
  return placeholder();
}

auto AsyncImage::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto AsyncImage::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const std::optional<ImageCache>& cache =
      context.environment(Environment::image_cache);
  const composition::StateHandle<Loaded> loaded = context.state(loaded_key);
  const ImageSource source =
      source_.kind() == ImageSource::Kind::url
          ? source_.resolved(context.environment(UrlSession::key))
          : source_;
  const std::string_view key = source.key();
  AsyncImagePhase phase;
  bool cached = false;
  if (loaded.get().key == key) {
    phase = loaded.get().phase;
  } else if (cache.has_value()) {
    if (auto hit = cache->find(key)) {
      phase = AsyncImagePhase::success(std::move(hit), options_.scale);
      cached = true;
    }
  }
  composition::Element shown = builder_ ? builder_(phase) : standard(phase);
  std::vector<composition::Element> produced;
  if (!cache.has_value() || cached) {
    produced.push_back(Group(std::move(shown)));
    return produced;
  }
  const composition::RootServices& services = context.services();
  produced.push_back(
      Group(std::move(shown))
          .task(
              composition::ElementKey{std::string{key}},
              [source, cache = *cache, loaded,
               background = services.executors.background, root = services.root,
               diagnostics = services.diagnostics, options = options_] {
                return load(source, cache, loaded, background, root,
                            diagnostics, options);
              }));
  return produced;
}

void AsyncImage::Content::report(const Diagnostics& diagnostics,
                                 std::string_view key,
                                 const provider::ProviderError& error) {
  if (!diagnostics.has_value()) {
    return;
  }
  diagnostics->get().report(provider::Diagnostic{
      .severity = provider::Severity::warning,
      .category = "image",
      .message = std::format("async image {} failed to load: {}", key,
                             error.explanation())});
}

void AsyncImage::Content::announce(const Diagnostics& diagnostics,
                                   std::string_view key,
                                   const provider::Image& image) {
  if (!diagnostics.has_value()) {
    return;
  }
  diagnostics->get().report(provider::Diagnostic{
      .severity = provider::Severity::info,
      .category = "image",
      .message = std::format("async image {} decoded {}x{}", key, image.width(),
                             image.height())});
}

void AsyncImage::Content::publish(
    const composition::StateHandle<Loaded>& loaded, Loaded value,
    const AsyncImageOptions& options) {
  if (options.transaction.has_value()) {
    motion::with_transaction(*options.transaction,
                             [&] { loaded.set(std::move(value)); });
    return;
  }
  if (options.animation.has_value()) {
    motion::with_animation(options.animation,
                           [&] { loaded.set(std::move(value)); });
    return;
  }
  loaded.set(std::move(value));
}

auto AsyncImage::Content::load(
    ImageSource source, ImageCache cache,
    composition::StateHandle<Loaded> loaded,
    std::reference_wrapper<async::Executor> background,
    std::reference_wrapper<async::Executor> root, Diagnostics diagnostics,
    AsyncImageOptions options) -> async::Task<void> {
  std::string key{source.key()};
  if (loaded.get().key == key && !loaded.get().phase.empty()) {
    co_return;
  }
  if (auto hit = cache.find(key)) {
    publish(loaded,
            Loaded{.key = std::move(key),
                   .phase =
                       AsyncImagePhase::success(std::move(hit), options.scale)},
            options);
    co_return;
  }
  co_await async::on(background.get(), async::JobPriority::low);
  ImageSource::Decoded decoded = co_await ImageSource::load(
      std::move(source), ImageSource::Assets{std::cref(cache.assets())});
  if (decoded.has_value()) {
    cache.insert(key, *decoded);
  }
  co_await async::on(root.get());
  if (!decoded.has_value()) {
    report(diagnostics, key, decoded.error());
  } else if ((*decoded)->width() > large_image ||
             (*decoded)->height() > large_image) {
    announce(diagnostics, key, **decoded);
  }
  publish(loaded,
          Loaded{.key = std::move(key),
                 .phase = decoded.has_value()
                              ? AsyncImagePhase::success(std::move(*decoded),
                                                         options.scale)
                              : AsyncImagePhase::failure(decoded.error())},
          options);
}

}
