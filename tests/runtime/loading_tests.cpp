#include <arisa/async/manual_executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/async_image.hpp>
#include <arisa/components/async_image_phase.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/image_cache.hpp>
#include <arisa/components/image_source.hpp>
#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/system_symbols.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/url_request.hpp>
#include <arisa/components/url_session.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/image_attributes.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/redaction.hpp>
#include <arisa/composition/redaction_reasons.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/composition/symbol_variants.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/browser/url.hpp>
#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/rendering/painter.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

class LoadingTest : public ::testing::Test {
 protected:
  static constexpr std::chrono::milliseconds frame_step{16};
  static constexpr provider::SurfaceSize small{
      .width = provider::SurfaceWidth{400},
      .height = provider::SurfaceHeight{300}};

  using Build = std::function<composition::Element(composition::Context&)>;

  class Host final : public composition::Component {
   public:
    explicit Host(const Build* build) noexcept : build_(build) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return (*build_)(context);
    }

   private:
    const Build* build_;
  };

  class Reasons final : public composition::Component {
   public:
    Reasons(std::string name,
            std::map<std::string, composition::RedactionReasons>* seen)
        : name_(std::move(name)), seen_(seen) {}

    auto operator==(const Reasons&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      (*seen_)[name_] = composition::Redaction::reasons(context);
      return components::Text(name_);
    }

   private:
    std::string name_;
    std::map<std::string, composition::RedactionReasons>* seen_;
  };

  struct Spread {
    int range{0};
    std::array<std::uint8_t, 4> first{};
  };

  static auto read(const std::filesystem::path& path)
      -> std::vector<std::byte> {
    std::ifstream stream{path, std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    return bytes;
  }

  static void fixture(provider::headless::HeadlessProviderFactory& providers,
                      std::string_view file) {
    const std::string name = std::string{"arisa/images/"} + std::string{file};
    const std::vector<std::byte> bytes =
        read(std::filesystem::path{ARISA_TEST_FIXTURES} / name);
    ASSERT_FALSE(bytes.empty()) << name;
    providers.assets().add(*provider::AssetPath::create(name), bytes);
  }

  static auto pixels(std::string_view file)
      -> std::shared_ptr<const provider::Image> {
    const std::vector<std::byte> bytes = read(
        std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" / file);
    auto decoded = provider::Image::decode(bytes);
    EXPECT_TRUE(decoded.has_value()) << file;
    if (!decoded.has_value()) {
      return nullptr;
    }
    return std::make_shared<const provider::Image>(std::move(*decoded));
  }

  static void fonts(provider::headless::HeadlessProviderFactory& providers) {
    for (const std::string_view file :
         {std::string_view{"arisa/fonts/InterVariable.ttf"},
          std::string_view{"arisa/fonts/MaterialSymbolsRounded.ttf"},
          std::string_view{"arisa/fonts/MaterialSymbolsRoundedFilled.ttf"}}) {
      const std::vector<std::byte> bytes =
          read(std::filesystem::path{ARISA_SOURCE_ASSETS} / file);
      ASSERT_FALSE(bytes.empty()) << file;
      providers.assets().add(*provider::AssetPath::create(file), bytes);
    }
  }

  static void raster(provider::headless::HeadlessProviderFactory& providers) {
    providers.surface().resize(small);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(small);
  }

  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    const Build& build, ApplicationOptions options = {})
      -> Application {
    auto created =
        Application::create(providers, Host{&build}, std::move(options));
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    frames(providers, application, 2);
    return application;
  }

  static void frames(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
  }

  static auto painted(provider::headless::HeadlessProviderFactory& providers,
                      Application& application, int count) -> std::uint32_t {
    std::uint32_t most = 0;
    for (int index = 0; index < count; ++index) {
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      EXPECT_TRUE(application.frame().has_value());
      if (application.last_frame().presented) {
        most = std::max(most, application.last_frame().pictures);
      }
    }
    return most;
  }

  static auto find(Application& application, std::string_view key)
      -> std::optional<composition::Rect> {
    const auto id =
        application.root().tree().find(composition::ElementKey{key});
    if (!id.has_value()) {
      return std::nullopt;
    }
    const auto node = application.root().tree().node(*id);
    if (!node.has_value()) {
      return std::nullopt;
    }
    return node->get().frame();
  }

  static auto raster_frame(const composition::Node& node)
      -> std::optional<composition::Rect> {
    if (node.content_as<components::Image::Raster::Content>().has_value()) {
      return node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (const auto found = raster_frame(child)) {
        return found;
      }
    }
    return std::nullopt;
  }

  static auto picture(Application& application, std::string_view key)
      -> std::optional<composition::Rect> {
    const auto id =
        application.root().tree().find(composition::ElementKey{key});
    if (!id.has_value()) {
      return std::nullopt;
    }
    const auto node = application.root().tree().node(*id);
    if (!node.has_value()) {
      return std::nullopt;
    }
    return raster_frame(node->get());
  }

  static void collect(const composition::Node& node,
                      std::vector<std::string>& found) {
    if (const auto text = node.content_as<components::Text::Content>()) {
      found.emplace_back(text->get().text());
    }
    for (const composition::Node& child : node.children()) {
      collect(child, found);
    }
  }

  static auto texts(Application& application) -> std::vector<std::string> {
    std::vector<std::string> found;
    collect(application.root().tree().root(), found);
    return found;
  }

  static auto rasters(const composition::Node& node) -> int {
    int count =
        node.content_as<components::Image::Raster::Content>().has_value() ? 1
                                                                          : 0;
    for (const composition::Node& child : node.children()) {
      count += rasters(child);
    }
    return count;
  }

  static auto row(const provider::Image& image, float y, float left,
                  float right) -> Spread {
    Spread spread;
    std::array<int, 3> low{255, 255, 255};
    std::array<int, 3> high{0, 0, 0};
    bool first = true;
    for (auto x = static_cast<std::uint32_t>(left);
         x < static_cast<std::uint32_t>(right); ++x) {
      const auto seen = image.pixel(x, static_cast<std::uint32_t>(y));
      if (!seen.has_value()) {
        continue;
      }
      if (first) {
        spread.first = *seen;
        first = false;
      }
      for (std::size_t channel = 0; channel < 3; ++channel) {
        low[channel] = std::min<int>(low[channel], (*seen)[channel]);
        high[channel] = std::max<int>(high[channel], (*seen)[channel]);
      }
    }
    for (std::size_t channel = 0; channel < 3; ++channel) {
      spread.range = std::max(spread.range, high[channel] - low[channel]);
    }
    return spread;
  }

  static auto at(const provider::Image& image, float x, float y)
      -> std::array<std::uint8_t, 4> {
    return image
        .pixel(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y))
        .value_or(std::array<std::uint8_t, 4>{});
  }

  static auto ink(const provider::Image& image, const composition::Rect& box)
      -> int {
    const auto ground = at(image, box.x + box.width + 6.0F, box.y + 1.0F);
    int count = 0;
    for (auto y = static_cast<std::uint32_t>(box.y);
         y < static_cast<std::uint32_t>(box.y + box.height); ++y) {
      for (auto x = static_cast<std::uint32_t>(box.x);
           x < static_cast<std::uint32_t>(box.x + box.width); ++x) {
        const auto seen = image.pixel(x, y);
        if (seen.has_value() && std::abs((*seen)[0] - ground[0]) > 60) {
          ++count;
        }
      }
    }
    return count;
  }

  static auto phase_view(const components::AsyncImagePhase& phase)
      -> composition::Element {
    switch (phase.kind()) {
      case components::AsyncImagePhase::Kind::empty:
        return components::Text("empty").id("phase-empty");
      case components::AsyncImagePhase::Kind::success:
        return composition::Element{std::move(*phase.image())}.id(
            "phase-success");
      case components::AsyncImagePhase::Kind::failure:
        return components::Text("failure").id("phase-failure");
    }
    return components::Text("empty");
  }

  static auto counted(std::shared_ptr<std::atomic<int>> calls,
                      std::vector<std::byte> bytes)
      -> async::Task<components::ImageSource::Bytes> {
    calls->fetch_add(1);
    co_return bytes;
  }

  static auto located(std::shared_ptr<std::atomic<std::uint64_t>> where,
                      provider::ThreadingProvider* threading,
                      std::vector<std::byte> bytes)
      -> async::Task<components::ImageSource::Bytes> {
    where->store(threading->current_thread().value());
    co_return bytes;
  }

  static auto manual(async::ManualExecutor& executor) -> ApplicationOptions {
    ApplicationOptions options;
    options.executors =
        async::Executors{.background = executor, .blocking = executor};
    return options;
  }

  static auto image_of(std::uint32_t side, std::uint8_t shade)
      -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> rgba(std::size_t{side} * side * 4, std::byte{shade});
    auto created = provider::Image::create(side, side, std::move(rgba));
    EXPECT_TRUE(created.has_value());
    return std::make_shared<const provider::Image>(std::move(*created));
  }

  static auto lazy_cover(int index, std::vector<std::byte> bytes)
      -> composition::Element {
    return components::AsyncImage(
               components::ImageSource::loader(
                   std::format("cover-{}", index),
                   [bytes] {
                     return counted(std::make_shared<std::atomic<int>>(0),
                                    bytes);
                   }),
               components::AsyncImageOptions{},
               [](const components::AsyncImagePhase& phase) {
                 if (std::optional<components::Image> image = phase.image()) {
                   return composition::Element{
                       std::move(*image).resizable().scaled_to_fill()};
                 }
                 return composition::Element{components::ZStack()}.frame(
                     {.max_width = composition::infinity,
                      .max_height = composition::infinity});
               })
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .frame({.width = 40.0F, .height = 40.0F})
        .clip_shape(theme::Shape::rounded(8.0F))
        .id(std::format("cover-{}", index));
  }

  static auto red_at(const provider::Image& image,
                     const composition::Rect& box) -> bool {
    const auto seen =
        at(image, box.x + (box.width * 0.5F), box.y + (box.height * 0.5F));
    return seen[0] > 200 && seen[1] < 60 && seen[2] < 60;
  }
};

TEST_F(LoadingTest, AsyncImagesPaintInsideLazyRowsAndAfterScrollingBack) {
  provider::headless::HeadlessProviderFactory providers;
  raster(providers);
  async::ManualExecutor background;
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  auto proxy = std::make_shared<std::optional<composition::ScrollViewProxy>>();
  const Build build = [bytes, proxy](composition::Context&) {
    return components::ScrollViewReader(
        [bytes, proxy](composition::ScrollViewProxy reader) {
          *proxy = reader;
          return components::ScrollView(
                     components::LazyVStack(
                         components::LazyVStackOptions{
                             .spacing = 0.0F,
                             .overscan = 0.0F,
                             .retention = 2,
                             .estimated_extent = 50.0F},
                         components::ForEach(
                             std::views::iota(0, 60),
                             [](int index) {
                               return std::format("row-{}", index);
                             },
                             [bytes](int index) {
                               return composition::Element{
                                   components::HStack(lazy_cover(index, bytes),
                                                      components::Text(
                                                          std::format(
                                                              "Row {}",
                                                              index)))}
                                   .frame({.height = 50.0F});
                             })))
              .frame({.width = 400.0F, .height = 300.0F})
              .id("covers");
        });
  };
  Application application = start(providers, build, manual(background));
  const auto settle = [&] {
    for (int round = 0; round < 4; ++round) {
      frames(providers, application, 2);
      background.drain();
    }
    frames(providers, application, 3);
  };
  const auto shows = [&](int index) {
    const auto image = providers.gpu().capture();
    auto box = find(application, std::format("cover-{}", index));
    const auto scroller =
        application.root().tree().find(composition::ElementKey{"covers"});
    if (box.has_value() && scroller.has_value()) {
      box->y -= application.root()
                    .tree()
                    .node(*scroller)
                    ->get()
                    .scroll_presentation()
                    .offset.y;
    }
    return image.has_value() && box.has_value() && red_at(*image, *box);
  };
  settle();
  EXPECT_TRUE(shows(0));
  EXPECT_TRUE(shows(3));
  ASSERT_TRUE(proxy->has_value());
  proxy->value().scroll_to(composition::ElementKey{"row-40"},
                           motion::UnitPoint::top);
  settle();
  EXPECT_TRUE(shows(40));
  EXPECT_TRUE(shows(42));
  proxy->value().scroll_to(composition::ElementKey{"row-0"},
                           motion::UnitPoint::top);
  settle();
  EXPECT_TRUE(shows(0));
  EXPECT_TRUE(shows(2));
}

TEST_F(LoadingTest, AsyncImagesPaintInsideLazyGridCells) {
  provider::headless::HeadlessProviderFactory providers;
  raster(providers);
  async::ManualExecutor background;
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  const Build build = [bytes](composition::Context&) {
    return components::ScrollView(
               components::LazyVGrid(
                   components::LazyVGridOptions{
                       .columns = {components::GridItem{},
                                   components::GridItem{},
                                   components::GridItem{}},
                       .estimated_extent = 40.0F},
                   components::ForEach(
                       std::views::iota(0, 30),
                       [](int index) { return std::format("cell-{}", index); },
                       [bytes](int index) { return lazy_cover(index, bytes); })))
        .frame({.width = 400.0F, .height = 300.0F});
  };
  Application application = start(providers, build, manual(background));
  for (int round = 0; round < 4; ++round) {
    frames(providers, application, 2);
    background.drain();
  }
  frames(providers, application, 3);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  for (const int index : {0, 2, 3, 5}) {
    const auto box = find(application, std::format("cover-{}", index));
    ASSERT_TRUE(box.has_value()) << index;
    EXPECT_TRUE(red_at(*image, *box)) << index;
  }
}

TEST_F(LoadingTest, ImageBytesFromTheAppDecodeOnceAndCacheByKey) {
  provider::headless::HeadlessProviderFactory providers;
  async::ManualExecutor background;
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  composition::Observable<int> shown{1};
  const Build build = [&bytes, &shown](composition::Context& context) {
    std::vector<composition::Element> avatars;
    avatars.push_back(components::AsyncImage(
        components::ImageSource::bytes("avatar:ana", bytes),
        components::AsyncImageOptions{}, phase_view));
    if (context.read(shown) > 1) {
      avatars.push_back(
          components::AsyncImage(
              components::ImageSource::bytes("avatar:ana", {}),
              components::AsyncImageOptions{},
              [](const components::AsyncImagePhase& phase) {
                return phase.image().has_value()
                           ? composition::Element{components::Text("cached")}
                           : composition::Element{components::Text("missed")};
              })
              .id("second"));
    }
    return components::VStack(std::move(avatars));
  };
  Application application = start(providers, build, manual(background));
  EXPECT_TRUE(find(application, "phase-empty").has_value());
  EXPECT_EQ(rasters(application.root().tree().root()), 0);
  background.drain();
  frames(providers, application, 2);
  EXPECT_TRUE(find(application, "phase-success").has_value());
  EXPECT_EQ(rasters(application.root().tree().root()), 1);
  EXPECT_EQ(components::ImageSource::bytes("avatar:ana", {}).kind(),
            components::ImageSource::Kind::bytes);
  EXPECT_EQ(components::ImageSource::bytes("avatar:ana", {}).key(),
            "bytes:avatar:ana");
  shown.set(2);
  frames(providers, application, 1);
  const std::vector<std::string> written = texts(application);
  EXPECT_NE(std::ranges::find(written, "cached"), written.end());
  EXPECT_EQ(std::ranges::find(written, "missed"), written.end());
  EXPECT_EQ(rasters(application.root().tree().root()), 1);
}

TEST_F(LoadingTest, ImageBytesThatDoNotDecodeFail) {
  provider::headless::HeadlessProviderFactory providers;
  async::ManualExecutor background;
  const Build build = [](composition::Context&) {
    return components::AsyncImage(
        components::ImageSource::bytes(
            "broken", std::vector<std::byte>{std::byte{1}, std::byte{2}}),
        components::AsyncImageOptions{}, phase_view);
  };
  Application application = start(providers, build, manual(background));
  background.drain();
  frames(providers, application, 2);
  EXPECT_TRUE(find(application, "phase-failure").has_value());
}

TEST_F(LoadingTest, TheCacheEvictsTheLeastRecentlyUsedImagesToKeepItsBudget) {
  provider::headless::HeadlessProviderFactory providers;
  const components::ImageCache cache{
      providers.assets(), components::ImageCache::Budget{3U * 16U * 16U * 4U}};
  cache.insert("a", image_of(16, 1));
  cache.insert("b", image_of(16, 2));
  cache.insert("c", image_of(16, 3));
  EXPECT_NE(cache.find("a"), nullptr);
  cache.insert("d", image_of(16, 4));
  EXPECT_EQ(cache.find("b"), nullptr);
  EXPECT_NE(cache.find("a"), nullptr);
  EXPECT_NE(cache.find("c"), nullptr);
  EXPECT_NE(cache.find("d"), nullptr);
  const components::ImageCache::Statistics seen = cache.statistics();
  EXPECT_EQ(seen.entries, 3U);
  EXPECT_LE(seen.bytes, seen.budget);
  EXPECT_EQ(seen.evictions, 1U);
  cache.insert("huge", image_of(64, 5));
  EXPECT_EQ(cache.find("huge"), nullptr);
  EXPECT_EQ(cache.statistics().entries, 3U);
  cache.clear();
  EXPECT_EQ(cache.statistics().bytes, 0U);
}

TEST_F(LoadingTest, AsyncImageDecodesOnlyInBackgroundJobsAndWalksItsPhases) {
  provider::headless::HeadlessProviderFactory providers;
  fixture(providers, "swatch_red_4x2.png");
  async::ManualExecutor background;
  const auto calls = std::make_shared<std::atomic<int>>(0);
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  const Build build = [calls, bytes](composition::Context&) {
    return components::VStack(
        components::AsyncImage(
            components::ImageSource::asset("arisa/images/swatch_red_4x2.png"),
            components::AsyncImageOptions{}, phase_view),
        components::AsyncImage(
            components::ImageSource::loader(
                "loaded", [calls, bytes] { return counted(calls, bytes); }),
            components::AsyncImageOptions{},
            [](const components::AsyncImagePhase& phase) {
              return phase.empty()
                         ? composition::Element{components::Text("waiting")}
                         : composition::Element{components::Text("loaded")};
            }));
  };
  Application application = start(providers, build, manual(background));
  frames(providers, application, 6);
  EXPECT_TRUE(find(application, "phase-empty").has_value());
  EXPECT_EQ(calls->load(), 0);
  EXPECT_GT(background.pending(), 0U);
  EXPECT_EQ(rasters(application.root().tree().root()), 0);
  background.drain();
  EXPECT_EQ(calls->load(), 1);
  frames(providers, application, 3);
  EXPECT_FALSE(find(application, "phase-empty").has_value());
  const auto shown = find(application, "phase-success");
  ASSERT_TRUE(shown.has_value());
  EXPECT_FLOAT_EQ(shown->width, 4.0F);
  EXPECT_FLOAT_EQ(shown->height, 2.0F);
  EXPECT_EQ(rasters(application.root().tree().root()), 1);
  const std::vector<std::string> written = texts(application);
  EXPECT_NE(std::ranges::find(written, "loaded"), written.end());
  frames(providers, application, 4);
  EXPECT_EQ(background.pending(), 0U);
  EXPECT_EQ(calls->load(), 1);
}

TEST_F(LoadingTest, AsyncImageLoadsUrlsThroughTheAppSession) {
  provider::headless::HeadlessProviderFactory providers;
  async::ManualExecutor background;
  const auto calls = std::make_shared<std::atomic<int>>(0);
  const auto asked = std::make_shared<std::vector<std::string>>();
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  const auto url = provider::Url::create("https://example.com/red.png");
  ASSERT_TRUE(url.has_value());
  std::optional<provider::ProviderError> refused;
  const Build build = [calls, asked, bytes, url = *url,
                       &refused](composition::Context&) {
    const components::UrlSession session{
        [calls, asked, bytes](components::UrlRequest request)
            -> async::Task<components::UrlSession::Bytes> {
          asked->emplace_back(request.url().value());
          return counted(calls, bytes);
        }};
    return components::VStack(
        components::AsyncImage(std::optional{url},
                               components::AsyncImageOptions{}, phase_view)
            .async_image_url_session(session),
        components::AsyncImage(
            std::optional{components::UrlRequest{
                url,
                {components::UrlRequest::Header{.name = "Accept",
                                                .value = "image/png"}}}},
            [](components::Image image) {
              return composition::Element{std::move(image)}.id("requested");
            },
            [] { return composition::Element{components::Text("waiting")}; })
            .async_image_url_session(session),
        components::AsyncImage(
            std::optional{components::UrlRequest{url}},
            components::AsyncImageOptions{.scale = 1.0F,
                                          .animation = std::nullopt,
                                          .transaction = motion::Transaction{}},
            [&refused](const components::AsyncImagePhase& phase) {
              if (phase.error().has_value()) {
                refused = phase.error();
              }
              return composition::Element{components::Text("unsessioned")};
            }));
  };
  Application application = start(providers, build, manual(background));
  background.drain();
  frames(providers, application, 3);
  background.drain();
  frames(providers, application, 3);
  EXPECT_TRUE(find(application, "phase-success").has_value());
  EXPECT_TRUE(find(application, "requested").has_value());
  EXPECT_GE(calls->load(), 1);
  ASSERT_FALSE(asked->empty());
  EXPECT_EQ(asked->front(), "https://example.com/red.png");
  ASSERT_TRUE(refused.has_value());
  EXPECT_EQ(refused->code(),
            provider::ProviderErrorCode::unsupported_capability);
  EXPECT_FALSE(components::UrlSession::shared().supported());
}

TEST_F(LoadingTest, MissingAssetsAndUndecodableBytesFail) {
  provider::headless::HeadlessProviderFactory providers;
  fixture(providers, "broken.png");
  async::ManualExecutor background;
  std::optional<provider::ProviderError> missing;
  const Build build = [&missing](composition::Context&) {
    return components::VStack(
        components::AsyncImage(
            components::ImageSource::asset("arisa/images/nowhere.png"),
            components::AsyncImageOptions{},
            [&missing](const components::AsyncImagePhase& phase) {
              if (phase.error().has_value()) {
                missing = phase.error();
              }
              return phase_view(phase);
            }),
        components::AsyncImage(
            components::ImageSource::asset("arisa/images/broken.png"),
            components::AsyncImageOptions{},
            [](const components::AsyncImagePhase& phase) {
              return phase.error().has_value()
                         ? composition::Element{components::Text("broken")}
                         : composition::Element{components::Text("pending")};
            }));
  };
  Application application = start(providers, build, manual(background));
  background.drain();
  frames(providers, application, 3);
  EXPECT_TRUE(find(application, "phase-failure").has_value());
  ASSERT_TRUE(missing.has_value());
  EXPECT_EQ(missing->code(), provider::ProviderErrorCode::not_found);
  const std::vector<std::string> written = texts(application);
  EXPECT_NE(std::ranges::find(written, "broken"), written.end());
  EXPECT_EQ(rasters(application.root().tree().root()), 0);
  const std::vector<provider::Diagnostic> reported =
      providers.diagnostics().snapshot();
  const auto image_failures =
      std::ranges::count_if(reported, [](const provider::Diagnostic& each) {
        return each.category == "image" &&
               each.severity == provider::Severity::warning;
      });
  EXPECT_EQ(image_failures, 2);
  EXPECT_TRUE(
      std::ranges::any_of(reported, [](const provider::Diagnostic& each) {
        return each.message.contains("asset:arisa/images/nowhere.png");
      }));
}

TEST_F(LoadingTest, AFlexibleFrameKeepsTheAspectRatioOfItsContent) {
  provider::headless::HeadlessProviderFactory providers;
  const Build build = [](composition::Context&) {
    return components::HStack(
        components::VStack(
            components::ZStack()
                .frame({.max_width = composition::infinity})
                .aspect_ratio(2.0F / 3.0F)
                .background(theme::ColourRole::secondary_system_fill,
                            theme::Shape::rectangle())
                .id("wide"))
            .frame({.width = 100.0F}),
        components::HStack(
            components::ZStack()
                .frame({.max_height = composition::infinity})
                .aspect_ratio(2.0F)
                .background(theme::ColourRole::secondary_system_fill,
                            theme::Shape::rectangle())
                .id("tall"))
            .frame({.height = 60.0F}));
  };
  Application application = start(providers, build);
  frames(providers, application, 3);
  const auto wide = find(application, "wide");
  const auto tall = find(application, "tall");
  ASSERT_TRUE(wide.has_value() && tall.has_value());
  EXPECT_FLOAT_EQ(wide->width, 100.0F);
  EXPECT_FLOAT_EQ(wide->height, 150.0F);
  EXPECT_FLOAT_EQ(tall->width, 120.0F);
  EXPECT_FLOAT_EQ(tall->height, 60.0F);
}

TEST_F(LoadingTest, ResizableImagesTakeAnOpenAxisFromTheirRatio) {
  const std::shared_ptr<const provider::Image> red =
      pixels("swatch_red_4x2.png");
  using composition::Axes;
  using composition::ContentMode;
  using composition::Element;
  using composition::FrameSpec;
  const std::array<std::string_view, 4> variants{"ratio_fit", "ratio_fill",
                                                 "scaled_fit", "scaled_fill"};
  const auto image = [red](std::size_t variant) -> components::Image {
    components::Image resizable = components::Image(red).resizable();
    switch (variant) {
      case 0:
        return std::move(resizable).aspect_ratio(0.5F, ContentMode::fit);
      case 1:
        return std::move(resizable).aspect_ratio(0.5F, ContentMode::fill);
      case 2:
        return std::move(resizable).scaled_to_fit();
      default:
        return std::move(resizable).scaled_to_fill();
    }
  };
  using Container = std::function<Element(Element)>;
  const FrameSpec column_frame{.width = 200.0F, .height = 300.0F};
  const std::vector<std::pair<std::string_view, Container>> containers{
      {"column",
       [&](Element shown) -> Element {
         return components::ScrollView(
                    components::VStack(
                        components::VStackOptions{.spacing = 0.0F},
                        std::move(shown)))
             .frame(column_frame);
       }},
      {"row",
       [](Element shown) -> Element {
         return components::ScrollView(
                    Axes::horizontal,
                    components::HStack(
                        components::HStackOptions{.spacing = 0.0F},
                        std::move(shown)))
             .frame({.width = 300.0F, .height = 100.0F});
       }},
      {"both",
       [&](Element shown) -> Element {
         return components::ScrollView(Axes::all, std::move(shown))
             .frame(column_frame);
       }},
      {"fixed",
       [&](Element shown) -> Element {
         return components::VStack(components::VStackOptions{.spacing = 0.0F},
                                   std::move(shown).fixed_size())
             .frame(column_frame);
       }},
      {"bounded",
       [&](Element shown) -> Element {
         return components::VStack(components::VStackOptions{.spacing = 0.0F},
                                   std::move(shown))
             .frame(column_frame);
       }},
      {"framed",
       [&](Element shown) -> Element {
         return components::VStack(
                    components::VStackOptions{.spacing = 0.0F},
                    std::move(shown).frame({.width = 60.0F, .height = 20.0F}))
             .frame(column_frame);
       }},
  };
  const std::map<std::string, std::array<float, 2>> expected{
      {"column.ratio_fit", {200.0F, 400.0F}},
      {"column.ratio_fill", {200.0F, 400.0F}},
      {"column.scaled_fit", {200.0F, 100.0F}},
      {"column.scaled_fill", {200.0F, 100.0F}},
      {"row.ratio_fit", {50.0F, 100.0F}},
      {"row.ratio_fill", {50.0F, 100.0F}},
      {"row.scaled_fit", {200.0F, 100.0F}},
      {"row.scaled_fill", {200.0F, 100.0F}},
      {"both.ratio_fit", {4.0F, 2.0F}},
      {"both.ratio_fill", {4.0F, 2.0F}},
      {"both.scaled_fit", {4.0F, 2.0F}},
      {"both.scaled_fill", {4.0F, 2.0F}},
      {"fixed.ratio_fit", {4.0F, 2.0F}},
      {"fixed.ratio_fill", {4.0F, 2.0F}},
      {"fixed.scaled_fit", {4.0F, 2.0F}},
      {"fixed.scaled_fill", {4.0F, 2.0F}},
      {"bounded.ratio_fit", {150.0F, 300.0F}},
      {"bounded.ratio_fill", {200.0F, 400.0F}},
      {"bounded.scaled_fit", {200.0F, 100.0F}},
      {"bounded.scaled_fill", {600.0F, 300.0F}},
      {"framed.ratio_fit", {10.0F, 20.0F}},
      {"framed.ratio_fill", {60.0F, 120.0F}},
      {"framed.scaled_fit", {40.0F, 20.0F}},
      {"framed.scaled_fill", {60.0F, 30.0F}},
  };
  for (const auto& [container, wrap] : containers) {
    provider::headless::HeadlessProviderFactory providers;
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1000},
                              .height = provider::SurfaceHeight{400}});
    providers.surface().set_scale(provider::SurfaceScale::unit());
    const Build build = [&](composition::Context&) {
      std::vector<Element> cells;
      for (std::size_t index = 0; index < variants.size(); ++index) {
        cells.push_back(wrap(Element{image(index)}.id(composition::ElementKey{
            std::string{container} + "." + std::string{variants.at(index)}})));
      }
      return components::HStack(components::HStackOptions{.spacing = 0.0F},
                                std::move(cells));
    };
    Application application = start(providers, build);
    frames(providers, application, 3);
    for (const std::string_view variant : variants) {
      const std::string key =
          std::string{container} + "." + std::string{variant};
      const auto shown = picture(application, key);
      ASSERT_TRUE(shown.has_value()) << key;
      EXPECT_NEAR(shown->width, expected.at(key)[0], 0.01F)
          << key << " " << shown->width << "x" << shown->height;
      EXPECT_NEAR(shown->height, expected.at(key)[1], 0.01F)
          << key << " " << shown->width << "x" << shown->height;
    }
  }
}

TEST_F(LoadingTest, AsyncImageFillsTheBoxItsModifiersGiveIt) {
  provider::headless::HeadlessProviderFactory providers;
  async::ManualExecutor background;
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  const auto calls = std::make_shared<std::atomic<int>>(0);
  const Build build = [calls, bytes](composition::Context&) {
    return components::VStack(
               composition::Element{
                   components::AsyncImage(
                       components::ImageSource::loader(
                           "boxed",
                           [calls, bytes] { return counted(calls, bytes); }),
                       [](components::Image image) {
                         return composition::Element{
                             std::move(image).resizable().scaled_to_fill()}
                             .id("shown");
                       },
                       [] {
                         return components::AsyncImage::placeholder().id(
                             "waiting");
                       })}
                   .frame({.max_width = composition::infinity})
                   .aspect_ratio(2.0F / 3.0F)
                   .id("boxed"))
        .frame({.width = 100.0F});
  };
  Application application = start(providers, build, manual(background));
  frames(providers, application, 3);
  const auto boxed = find(application, "boxed");
  const auto waiting = find(application, "waiting");
  ASSERT_TRUE(boxed.has_value() && waiting.has_value());
  EXPECT_FLOAT_EQ(boxed->width, 100.0F);
  EXPECT_FLOAT_EQ(boxed->height, 150.0F);
  EXPECT_FLOAT_EQ(waiting->width, 100.0F);
  EXPECT_FLOAT_EQ(waiting->height, 150.0F);
  background.drain();
  const std::uint32_t pictures = painted(providers, application, 3);
  const auto shown = find(application, "shown");
  ASSERT_TRUE(shown.has_value());
  EXPECT_FLOAT_EQ(shown->width, 100.0F);
  EXPECT_FLOAT_EQ(shown->height, 150.0F);
  EXPECT_EQ(rasters(application.root().tree().root()), 1);
  EXPECT_EQ(pictures, 1U);
}

TEST_F(LoadingTest, AsyncImageLoadsAnAssetOnTheRealPoolWhileTheLoopIdles) {
  provider::headless::HeadlessProviderFactory providers;
  fixture(providers, "swatch_red_4x2.png");
  const Build build = [](composition::Context&) {
    return components::AsyncImage(
        components::ImageSource::asset("arisa/images/swatch_red_4x2.png"),
        components::AsyncImageOptions{}, phase_view);
  };
  Application application = start(providers, build);
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds{10};
  int idle = 0;
  std::uint32_t pictures = 0;
  ASSERT_TRUE(application
                  .run_until([&](Root&) {
                    const FrameTimings& last = application.last_frame();
                    if (last.kind == FrameKind::idle) {
                      ++idle;
                    }
                    if (last.presented) {
                      pictures = std::max(pictures, last.pictures);
                    }
                    static_cast<void>(providers.clock().advance(frame_step));
                    return (find(application, "phase-success").has_value() &&
                            pictures > 0 && idle > 0) ||
                           std::chrono::steady_clock::now() > deadline;
                  })
                  .has_value());
  ASSERT_TRUE(find(application, "phase-success").has_value());
  EXPECT_GT(idle, 0);
  EXPECT_EQ(pictures, 1U);
  EXPECT_TRUE(std::ranges::none_of(providers.diagnostics().snapshot(),
                                   [](const provider::Diagnostic& each) {
                                     return each.category == "image";
                                   }));
}

TEST_F(LoadingTest, AWarmCacheShowsTheImageOnItsFirstFrame) {
  provider::headless::HeadlessProviderFactory providers;
  fixture(providers, "swatch_red_4x2.png");
  async::ManualExecutor background;
  composition::Observable<bool> second{false};
  const Build build = [&second](composition::Context& context) {
    std::vector<composition::Element> shown;
    shown.push_back(components::AsyncImage(
        components::ImageSource::asset("arisa/images/swatch_red_4x2.png"),
        components::AsyncImageOptions{}, phase_view));
    if (context.read(second)) {
      shown.push_back(components::AsyncImage(
          components::ImageSource::asset("arisa/images/swatch_red_4x2.png"),
          components::AsyncImageOptions{},
          [](const components::AsyncImagePhase& phase) {
            return phase.empty()
                       ? composition::Element{components::Text("cold")}
                       : composition::Element{components::Text("warm")};
          }));
    }
    return components::VStack(std::move(shown));
  };
  Application application = start(providers, build, manual(background));
  background.drain();
  frames(providers, application, 3);
  ASSERT_TRUE(find(application, "phase-success").has_value());
  second.set(true);
  frames(providers, application, 1);
  const std::vector<std::string> written = texts(application);
  EXPECT_NE(std::ranges::find(written, "warm"), written.end());
  EXPECT_EQ(std::ranges::find(written, "cold"), written.end());
}

TEST_F(LoadingTest, ACachedImageEntersWithoutASecondPublish) {
  provider::headless::HeadlessProviderFactory providers;
  fixture(providers, "swatch_red_4x2.png");
  async::ManualExecutor background;
  composition::Observable<bool> second{false};
  const auto calls = std::make_shared<int>(0);
  const Build build = [&second, calls](composition::Context& context) {
    std::vector<composition::Element> shown;
    shown.push_back(components::AsyncImage(
        components::ImageSource::asset("arisa/images/swatch_red_4x2.png"),
        components::AsyncImageOptions{}, phase_view));
    if (context.read(second)) {
      shown.push_back(components::AsyncImage(
          components::ImageSource::asset("arisa/images/swatch_red_4x2.png"),
          components::AsyncImageOptions{
              .scale = 1.0F,
              .animation = motion::Animation::linear(0.4),
              .transaction = std::nullopt},
          [calls](const components::AsyncImagePhase& phase) {
            ++*calls;
            return phase.empty()
                       ? composition::Element{components::Text("cold")}
                       : composition::Element{components::Text("warm")};
          }));
    }
    return components::VStack(std::move(shown));
  };
  Application application = start(providers, build, manual(background));
  background.drain();
  frames(providers, application, 3);
  ASSERT_TRUE(find(application, "phase-success").has_value());
  second.set(true);
  frames(providers, application, 1);
  ASSERT_EQ(*calls, 1);
  const std::uint64_t evaluations = application.root().tree().evaluations();
  bool animated = false;
  bool requested = false;
  for (int frame = 0; frame < 4; ++frame) {
    background.drain();
    frames(providers, application, 1);
    animated = animated || application.last_frame().animating;
    requested = requested || application.last_frame().evaluations > 0;
  }
  EXPECT_EQ(*calls, 1);
  EXPECT_EQ(application.root().tree().evaluations(), evaluations);
  EXPECT_FALSE(requested);
  EXPECT_FALSE(animated);
  const std::vector<std::string> written = texts(application);
  EXPECT_NE(std::ranges::find(written, "warm"), written.end());
}

TEST_F(LoadingTest, AsyncImageDecodesOnAWorkerThread) {
  provider::headless::HeadlessProviderFactory providers;
  const std::uint64_t ui = providers.threading().current_thread().value();
  const auto where = std::make_shared<std::atomic<std::uint64_t>>(ui);
  provider::ThreadingProvider* threading = &providers.threading();
  const std::vector<std::byte> bytes =
      read(std::filesystem::path{ARISA_TEST_FIXTURES} / "arisa/images" /
           "swatch_red_4x2.png");
  const Build build = [where, threading, bytes](composition::Context&) {
    return components::AsyncImage(
        components::ImageSource::loader(
            "worker", [where, threading,
                       bytes] { return located(where, threading, bytes); }),
        components::AsyncImageOptions{}, phase_view);
  };
  Application application = start(providers, build);
  for (int attempt = 0; attempt < 400; ++attempt) {
    if (find(application, "phase-success").has_value()) {
      break;
    }
    frames(providers, application, 1);
  }
  ASSERT_TRUE(find(application, "phase-success").has_value());
  EXPECT_NE(where->load(), ui);
}

TEST_F(LoadingTest, RedactedTextPaintsOneBarPerLineAndKeepsItsSize) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  raster(providers);
  const Build build = [](composition::Context&) {
    return components::VStack(
        components::VStackOptions{
            .alignment = components::HorizontalAlignment::leading,
            .spacing = 20.0F},
        components::Text("Placeholder text").id("plain"),
        components::Text("Placeholder text")
            .redacted(composition::RedactionReasons::placeholder)
            .id("bar"),
        components::Text("The quick brown fox jumps over the lazy dog")
            .frame(
                {.width = 150.0F, .alignment = composition::Alignment::leading})
            .redacted(composition::RedactionReasons::placeholder)
            .id("wrapped"));
  };
  Application application = start(providers, build);
  frames(providers, application, 4);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto plain = find(application, "plain");
  const auto bar = find(application, "bar");
  ASSERT_TRUE(plain.has_value() && bar.has_value());
  EXPECT_FLOAT_EQ(plain->width, bar->width);
  EXPECT_FLOAT_EQ(plain->height, bar->height);
  const float middle = 0.6F;
  const Spread glyphs = row(*image, plain->y + (plain->height * middle),
                            plain->x + 1.0F, plain->x + plain->width - 2.0F);
  const Spread filled = row(*image, bar->y + (bar->height * middle),
                            bar->x + 2.0F, bar->x + bar->width - 3.0F);
  EXPECT_GT(glyphs.range, 60);
  EXPECT_LE(filled.range, 8);
  const auto ground = at(*image, bar->x + bar->width + 20.0F, bar->y + 8.0F);
  EXPECT_GT(std::abs(filled.first[0] - ground[0]), 15);
  EXPECT_LT(std::abs(filled.first[0] - ground[0]), 70);
  const auto above = at(*image, bar->x + 10.0F, bar->y);
  EXPECT_LE(std::abs(above[0] - ground[0]), 3);
  float first_row = -1.0F;
  float rows = 0.0F;
  for (float y = bar->y; y < bar->y + bar->height; y += 1.0F) {
    if (std::abs(at(*image, bar->x + 20.0F, y)[0] - ground[0]) > 20) {
      if (first_row < 0.0F) {
        first_row = y - bar->y;
      }
      rows += 1.0F;
    }
  }
  EXPECT_NEAR(first_row, 3.0F, 1.0F);
  EXPECT_NEAR(rows, 10.5F, 1.0F);
  const auto wrapped = find(application, "wrapped");
  ASSERT_TRUE(wrapped.has_value());
  int bands = 0;
  bool inside = false;
  for (float y = wrapped->y; y < wrapped->y + wrapped->height; y += 1.0F) {
    const auto seen = at(*image, wrapped->x + 20.0F, y);
    const bool painted = std::abs(seen[0] - ground[0]) > 10;
    if (painted && !inside) {
      ++bands;
    }
    inside = painted;
  }
  EXPECT_EQ(bands, 2);
}

TEST_F(LoadingTest, RedactionReasonsMergeAndUnredactedClearsThem) {
  provider::headless::HeadlessProviderFactory providers;
  std::map<std::string, composition::RedactionReasons> seen;
  const Build build = [&seen](composition::Context&) {
    return components::VStack(
        composition::Element{Reasons{"outside", &seen}},
        components::VStack(
            composition::Element{Reasons{"inside", &seen}},
            composition::Element{Reasons{"cleared", &seen}}.unredacted(),
            composition::Element{Reasons{"both", &seen}}.redacted(
                composition::RedactionReasons::privacy))
            .redacted(composition::RedactionReasons::placeholder));
  };
  Application application = start(providers, build);
  EXPECT_TRUE(seen["outside"].empty());
  EXPECT_EQ(seen["inside"], composition::RedactionReasons::placeholder);
  EXPECT_TRUE(seen["cleared"].empty());
  EXPECT_EQ(seen["both"], composition::RedactionReasons::placeholder |
                              composition::RedactionReasons::privacy);
  EXPECT_TRUE(seen["both"].redacts(false));
  EXPECT_FALSE(composition::RedactionReasons::privacy.redacts(false));
  EXPECT_TRUE(composition::RedactionReasons::privacy.redacts(true));
}

TEST_F(LoadingTest, PrivacyRedactionHidesOnlySensitiveContent) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  raster(providers);
  const Build build = [](composition::Context&) {
    return components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = 20.0F},
               components::Text("Public words").id("public"),
               components::Text("Secret words")
                   .privacy_sensitive()
                   .id("secret"),
               components::VStack(components::Text("Kept words").id("kept"))
                   .privacy_sensitive()
                   .unredacted())
        .redacted(composition::RedactionReasons::privacy);
  };
  Application application = start(providers, build);
  frames(providers, application, 4);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  for (const auto& [key, hidden] :
       {std::pair{"public", false}, std::pair{"secret", true},
        std::pair{"kept", false}}) {
    const auto box = find(application, key);
    ASSERT_TRUE(box.has_value()) << key;
    const Spread spread = row(*image, box->y + (box->height * 0.6F),
                              box->x + 2.0F, box->x + box->width - 3.0F);
    if (hidden) {
      EXPECT_LE(spread.range, 8) << key;
    } else {
      EXPECT_GT(spread.range, 60) << key;
    }
  }
}

TEST_F(LoadingTest, RedactedImagesFillTheirFrameAndShapesKeepTheirColour) {
  provider::headless::HeadlessProviderFactory providers;
  raster(providers);
  const std::shared_ptr<const provider::Image> red =
      pixels("swatch_red_4x2.png");
  const Build build = [red](composition::Context&) {
    return components::VStack(components::VStackOptions{.spacing = 20.0F},
                              components::Image(red).resizable().frame(
                                  {.width = 40.0F, .height = 20.0F}),
                              components::ZStack()
                                  .frame({.width = 40.0F, .height = 20.0F})
                                  .background(theme::Colour::hex(0xFF0000),
                                              theme::Shape::rectangle())
                                  .id("shape"))
        .redacted(composition::RedactionReasons::placeholder)
        .id("stack");
  };
  Application application = start(providers, build);
  frames(providers, application, 4);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  EXPECT_EQ(rasters(application.root().tree().root()), 1);
  const auto stack = find(application, "stack");
  const auto shape = find(application, "shape");
  ASSERT_TRUE(stack.has_value() && shape.has_value());
  const auto picture =
      at(*image, stack->x + (stack->width * 0.5F), stack->y + 10.0F);
  EXPECT_LT(std::abs(picture[0] - picture[1]), 6);
  EXPECT_LT(picture[0], 250);
  EXPECT_GT(picture[0], 150);
  const auto kept = at(*image, shape->x + 20.0F, shape->y + 10.0F);
  EXPECT_GT(kept[0], 200);
  EXPECT_LT(kept[1], 60);
}

TEST_F(LoadingTest,
       TemplateImagesTakeTheForegroundAndOriginalsKeepTheirColour) {
  provider::headless::HeadlessProviderFactory providers;
  raster(providers);
  const std::shared_ptr<const provider::Image> badge =
      pixels("badge_green_16.png");
  const Build build = [badge](composition::Context&) {
    return components::HStack(
        components::HStackOptions{.spacing = 20.0F},
        components::Image(badge)
            .resizable()
            .rendering_mode(components::Image::TemplateRenderingMode::template_)
            .foreground_style(theme::Colour::hex(0xFF0000))
            .frame({.width = 32.0F, .height = 32.0F})
            .id("template"),
        components::Image(badge)
            .resizable()
            .foreground_style(theme::Colour::hex(0xFF0000))
            .frame({.width = 32.0F, .height = 32.0F})
            .id("original"));
  };
  Application application = start(providers, build);
  frames(providers, application, 4);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto tinted = find(application, "template");
  const auto original = find(application, "original");
  ASSERT_TRUE(tinted.has_value() && original.has_value());
  const auto red = at(*image, tinted->x + 16.0F, tinted->y + 16.0F);
  EXPECT_GT(red[0], 200);
  EXPECT_LT(red[1], 60);
  const auto green = at(*image, original->x + 16.0F, original->y + 16.0F);
  EXPECT_LT(green[0], 100);
  EXPECT_GT(green[1], 150);
  const auto corner = at(*image, tinted->x + 1.0F, tinted->y + 1.0F);
  const auto ground = at(*image, tinted->x - 6.0F, tinted->y + 1.0F);
  EXPECT_LE(std::abs(corner[0] - ground[0]), 3);
}

TEST_F(LoadingTest, NoInterpolationSamplesTheNearestPixel) {
  provider::headless::HeadlessProviderFactory providers;
  raster(providers);
  const std::shared_ptr<const provider::Image> checker =
      pixels("checker_2x2.png");
  const Build build = [checker](composition::Context&) {
    return components::HStack(
        components::HStackOptions{.spacing = 20.0F},
        components::Image(checker)
            .resizable()
            .interpolation(components::Image::Interpolation::none)
            .frame({.width = 40.0F, .height = 40.0F})
            .id("nearest"),
        components::Image(checker)
            .resizable()
            .frame({.width = 40.0F, .height = 40.0F})
            .id("smooth"));
  };
  Application application = start(providers, build);
  frames(providers, application, 4);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto nearest = find(application, "nearest");
  const auto smooth = find(application, "smooth");
  ASSERT_TRUE(nearest.has_value() && smooth.has_value());
  const auto hard = at(*image, nearest->x + 16.0F, nearest->y + 10.0F);
  const auto soft = at(*image, smooth->x + 16.0F, smooth->y + 10.0F);
  EXPECT_LT(hard[0], 12);
  EXPECT_GT(soft[0], 30);
  EXPECT_LT(soft[0], 200);
}

TEST_F(LoadingTest, SymbolVariantsResolveToFilledShapedAndSlashedGlyphs) {
  using components::SystemSymbols;
  using composition::SymbolVariants;
  const auto index = [](std::string_view name) {
    return *SystemSymbols::codepoint(name) - SystemSymbols::first_codepoint;
  };
  EXPECT_EQ(SystemSymbols::codepoint("favorite", SymbolVariants::none),
            SystemSymbols::codepoint("favorite"));
  EXPECT_EQ(SystemSymbols::codepoint("favorite", SymbolVariants::fill),
            SystemSymbols::filled_first_codepoint + index("favorite"));
  EXPECT_EQ(SystemSymbols::codepoint("add", SymbolVariants::circle),
            SystemSymbols::codepoint("add_circle"));
  EXPECT_EQ(SystemSymbols::codepoint("add", SymbolVariants::circle.filled()),
            SystemSymbols::filled_first_codepoint + index("add_circle"));
  EXPECT_EQ(SystemSymbols::codepoint("check", SymbolVariants::square),
            SystemSymbols::codepoint("check_box"));
  EXPECT_EQ(SystemSymbols::codepoint("visibility", SymbolVariants::slash),
            SystemSymbols::codepoint("visibility_off"));
  EXPECT_EQ(SystemSymbols::codepoint("gamepad", SymbolVariants::slash),
            SystemSymbols::codepoint("gamepad"));
  EXPECT_EQ(SystemSymbols::codepoint("star", SymbolVariants::rectangle),
            SystemSymbols::codepoint("star"));
  EXPECT_FALSE(SystemSymbols::codepoint("no_such_symbol", SymbolVariants::fill)
                   .has_value());
}

TEST_F(LoadingTest, ImageScaleSizesSymbolsAndFilledGlyphsCarryMoreInk) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  raster(providers);
  const Build build = [](composition::Context&) {
    return components::VStack(
        components::VStackOptions{.spacing = 12.0F},
        components::HStack(
            components::HStackOptions{.spacing = 12.0F},
            composition::Element{components::Image::system_name("star")}
                .image_scale(composition::ImageScale::small)
                .id("small"),
            composition::Element{components::Image::system_name("star")}.id(
                "medium"),
            composition::Element{components::Image::system_name("star")}
                .image_scale(composition::ImageScale::large)
                .id("large")),
        components::HStack(
            components::HStackOptions{.spacing = 12.0F},
            composition::Element{components::Image::system_name("favorite")
                                     .font(theme::Font::system(40.0F))}
                .id("outlined"),
            composition::Element{components::Image::system_name("favorite")
                                     .font(theme::Font::system(40.0F))}
                .symbol_variant(composition::SymbolVariants::fill)
                .id("filled"),
            composition::Element{components::Image::system_name("favorite")
                                     .font(theme::Font::system(40.0F))}
                .symbol_rendering_mode(
                    composition::SymbolRenderingMode::hierarchical)
                .id("hierarchical")));
  };
  Application application = start(providers, build);
  frames(providers, application, 4);
  const auto small_box = find(application, "small");
  const auto medium_box = find(application, "medium");
  const auto large_box = find(application, "large");
  ASSERT_TRUE(small_box.has_value() && medium_box.has_value() &&
              large_box.has_value());
  ASSERT_GT(medium_box->height, 0.0F);
  EXPECT_NEAR(small_box->height / medium_box->height, 0.8F, 0.06F);
  EXPECT_NEAR(large_box->height / medium_box->height, 1.26F, 0.06F);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto outlined = find(application, "outlined");
  const auto filled = find(application, "filled");
  const auto hierarchical = find(application, "hierarchical");
  ASSERT_TRUE(outlined.has_value() && filled.has_value() &&
              hierarchical.has_value());
  const int outline_ink = ink(*image, *outlined);
  EXPECT_GT(outline_ink, 50);
  EXPECT_GT(ink(*image, *filled), outline_ink + (outline_ink / 2));
  EXPECT_EQ(ink(*image, *hierarchical), outline_ink);
}

TEST_F(LoadingTest, ContentUnavailableViewStacksLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  fonts(providers);
  providers.surface().resize(
      provider::SurfaceSize{.width = provider::SurfaceWidth{400},
                            .height = provider::SurfaceHeight{900}});
  providers.surface().set_scale(provider::SurfaceScale::unit());
  const Build build = [](composition::Context&) {
    return components::VStack(
               components::VStackOptions{.spacing = 0.0F},
               composition::Element{
                   components::ContentUnavailableView(
                       components::Text("No Games").id("label"),
                       composition::Element{
                           components::Text("Description").id("description")},
                       composition::Element{
                           components::Button("Retry", [] {}).id("action")})}
                   .id("view"),
               composition::Element{
                   components::ContentUnavailableView::search("zelda")},
               composition::Element{components::ContentUnavailableView(
                   "No Covers", "photo_library")})
        .frame({.max_width = composition::infinity});
  };
  Application application = start(providers, build);
  frames(providers, application, 3);
  const auto view = find(application, "view");
  const auto label = find(application, "label");
  const auto description = find(application, "description");
  const auto action = find(application, "action");
  ASSERT_TRUE(view.has_value() && label.has_value() &&
              description.has_value() && action.has_value());
  EXPECT_FLOAT_EQ(view->width, 400.0F);
  EXPECT_NEAR(label->y - view->y, components::ContentUnavailableView::insets,
              0.5F);
  EXPECT_NEAR(description->y - (label->y + label->height),
              components::ContentUnavailableView::description_spacing, 0.5F);
  EXPECT_NEAR(action->y - (description->y + description->height),
              components::ContentUnavailableView::actions_spacing, 0.5F);
  EXPECT_NEAR((view->y + view->height) - (action->y + action->height),
              components::ContentUnavailableView::insets, 0.5F);
  EXPECT_NEAR(label->x + (label->width * 0.5F), 200.0F, 0.5F);
  EXPECT_NEAR(description->x + (description->width * 0.5F), 200.0F, 0.5F);
  const std::vector<std::string> written = texts(application);
  EXPECT_NE(std::ranges::find(written, "No Results for “zelda”"),
            written.end());
  EXPECT_NE(std::ranges::find(written, "No Covers"), written.end());
}

TEST_F(LoadingTest, ThePictureBudgetReleasesTexturesOfImagesOffScreen) {
  constexpr std::uint32_t side = 64;
  constexpr std::uint64_t one = std::uint64_t{side} * side * 4;
  constexpr int count = 60;
  constexpr int pages = count / 4;
  std::vector<std::shared_ptr<const provider::Image>> library;
  for (int index = 0; index < count; ++index) {
    library.push_back(image_of(side, static_cast<std::uint8_t>(index * 4)));
  }
  composition::Observable<int> page{0};
  const Build build = [&library, &page](composition::Context& context) {
    const int shown = context.read(page);
    std::vector<composition::Element> tiles;
    for (int index = 0; index < 4; ++index) {
      tiles.push_back(
          components::Image(
              library[static_cast<std::size_t>(((shown * 4) + index) % count)])
              .frame({.width = 64.0F, .height = 64.0F}));
    }
    return components::HStack(std::move(tiles));
  };
  const auto cycle = [&page](
                         provider::headless::HeadlessProviderFactory& providers,
                         Application& application) {
    std::uint32_t most = 0;
    std::uint32_t last = 0;
    for (int step = 1; step < pages; ++step) {
      page.set(step);
      frames(providers, application, 1);
      const FrameTimings& timings = application.last_frame();
      if (timings.presented) {
        most = std::max(most, timings.pictures);
        last = timings.pictures;
      }
    }
    return std::pair{most, last};
  };
  provider::headless::HeadlessProviderFactory bounded_providers;
  ApplicationOptions bounded;
  bounded.images.textures = 4 * one;
  Application budgeted = start(bounded_providers, build, std::move(bounded));
  const auto [most, last] = cycle(bounded_providers, budgeted);
  EXPECT_GE(last, 4U);
  EXPECT_LE(most, 4U * (rendering::Painter::picture_grace_frames + 1U));
  EXPECT_LE(last, 4U * (rendering::Painter::picture_grace_frames + 1U));
  page.set(0);
  provider::headless::HeadlessProviderFactory roomy_providers;
  ApplicationOptions roomy;
  roomy.images.textures = std::uint64_t{count} * one * 2U;
  Application unbounded = start(roomy_providers, build, std::move(roomy));
  const auto [roomy_most, roomy_last] = cycle(roomy_providers, unbounded);
  EXPECT_EQ(roomy_last, static_cast<std::uint32_t>(count));
  EXPECT_GT(roomy_most, most);
}

}
