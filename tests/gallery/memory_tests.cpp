#include <arisa/components/image.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/diagnostics/heap_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryMemoryTest : public GalleryFixture {
 protected:
  static auto lap(provider::headless::HeadlessProviderFactory& providers,
                  Application& application) -> std::optional<std::uint64_t> {
    for (std::size_t index = 0; index < examples::GalleryPages::all.size();
         ++index) {
      press(providers, application, provider::Button::r);
      run(providers, application, 6);
    }
    run(providers, application, 30);
    const auto heap = provider::HeapMeter::sample();
    if (!heap.has_value() || !heap->blocks.has_value()) {
      return std::nullopt;
    }
    return *heap->blocks;
  }

  class Poster final : public composition::Component {
   public:
    Poster(const composition::Observable<bool>& shown,
           std::shared_ptr<const provider::Image> pixels, float width)
        : shown_(shown), pixels_(std::move(pixels)), width_(width) {}

    auto operator==(const Poster& other) const -> bool {
      return this == &other;
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const composition::FrameSpec frame{.width = width_,
                                         .height = width_ * 1.5F};
      if (!context.read(shown_.get())) {
        return composition::Element{components::ZStack()}.frame(frame);
      }
      return composition::Element{
          components::Image{pixels_}.resizable().interpolation(
              components::Image::Interpolation::medium)}
          .frame(frame);
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> shown_;
    std::shared_ptr<const provider::Image> pixels_;
    float width_;
  };

  struct Shown {
    std::uint64_t bytes{0};
    std::optional<provider::Image> capture;
  };

  static auto poster() -> std::shared_ptr<const provider::Image> {
    constexpr std::uint32_t width = 601;
    constexpr std::uint32_t height = 899;
    std::vector<std::byte> bytes;
    bytes.reserve(std::size_t{width} * height * 4);
    for (std::uint32_t y = 0; y < height; ++y) {
      for (std::uint32_t x = 0; x < width; ++x) {
        for (const std::uint32_t channel :
             {x * 7U, y * 5U, (x + y) * 3U, (x * y) + (y / 3U)}) {
          bytes.push_back(static_cast<std::byte>(channel & 0xFFU));
        }
      }
    }
    auto made = provider::Image::create(width, height, std::move(bytes));
    EXPECT_TRUE(made.has_value());
    return std::make_shared<const provider::Image>(std::move(*made));
  }

  static auto show(const std::shared_ptr<const provider::Image>& pixels,
                   float width) -> Shown {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{640},
                                     .height = provider::SurfaceHeight{960}};
    providers.surface().resize(size);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(size);
    composition::Observable<bool> shown{false};
    auto created = Application::create(providers, Poster{shown, pixels, width});
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return {};
    }
    Application application = std::move(*created);
    run(providers, application, 4);
    shown.set(true);
    const std::uint64_t before = provider::AllocationMeter::bytes().value_or(0);
    run(providers, application, 2);
    Shown seen{.bytes = provider::AllocationMeter::bytes().value_or(0) - before,
               .capture = std::nullopt};
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (image.has_value()) {
      seen.capture = std::move(*image);
    }
    return seen;
  }
};

TEST_F(GalleryMemoryTest, PagingThroughTheGalleryKeepsLiveAllocationsFlat) {
#if defined(__SANITIZE_ADDRESS__)
  GTEST_SKIP() << "the sanitizer quarantines freed blocks";
#endif
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::typography);
  ASSERT_TRUE(provider::HeapMeter::sample().has_value());
  static_cast<void>(lap(providers, application));
  const auto settled = lap(providers, application);
  if (!settled.has_value()) {
    GTEST_SKIP() << "this platform reports bytes only";
  }
  std::uint64_t highest = *settled;
  for (int round = 0; round < 3; ++round) {
    const auto again = lap(providers, application);
    ASSERT_TRUE(again.has_value());
    highest = std::max(highest, *again);
  }
  EXPECT_LE(highest, *settled + (*settled / 100) + 32);
  EXPECT_TRUE(providers.diagnostics().snapshot().empty());
}

TEST_F(GalleryMemoryTest, AReducedPictureIsBuiltWithoutAFullSizeFloatCopy) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not metered here";
  }
  const std::shared_ptr<const provider::Image> pixels = poster();
  const std::uint64_t float_copy = std::uint64_t{pixels->width()} *
                                   pixels->height() *
                                   provider::Image::channels * sizeof(float);
  for (const float width : {150.0F, 75.0F}) {
    const Shown seen = show(pixels, width);
    EXPECT_LT(seen.bytes, float_copy) << width;
    ASSERT_TRUE(seen.capture.has_value());
  }
}

}
