#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/gpu/render_queue.hpp>
#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class GpuTakeoverTest : public ::testing::Test {
 protected:
  using NullGpu = provider::headless::NullGpu;

  static constexpr std::uint32_t width = 160;
  static constexpr std::uint32_t height = 90;

  class Tinted final : public composition::Component {
   public:
    explicit Tinted(const composition::Observable<int>& shade)
        : shade_(shade) {}

    auto operator==(const Tinted& other) const -> bool {
      return &shade_.get() == &other.shade_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto level =
          static_cast<std::uint32_t>(16 + (context.read(shade_.get()) * 9));
      return components::VStack()
          .frame({.width = 160.0F, .height = 90.0F})
          .background(theme::Colour::hex((level << 16U) | 0x40U));
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> shade_;
  };

  void SetUp() override {
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{width},
        .height = provider::SurfaceHeight{height}};
    providers_.surface().resize(extent);
    providers_.surface().set_scale(provider::SurfaceScale::unit());
    providers_.gpu().rasterize(extent);
    providers_.gpu().swapchain(2, true);
  }

  auto gpu() -> NullGpu& { return providers_.gpu(); }

  auto start() -> Application {
    auto created = Application::create(providers_, Tinted{shade_});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    frames(application, 2);
    return application;
  }

  void frames(Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
    application.drain();
  }

  auto presents_over(Application& application, int count) -> std::size_t {
    const std::size_t before = gpu().frames_presented();
    frames(application, count);
    return gpu().frames_presented() - before;
  }

  void take_over_once_a_frame_waits() {
    provider::RenderQueue& queue = gpu().render_queue()->get();
    const std::uint64_t waits = queue.counters().submission_waits;
    blocker_ = queue.lease();
    auto started = providers_.threading().start(
        provider::ThreadSpec{.name = "test-taker",
                             .role = provider::ThreadRole::app},
        [this, &queue, waits](const std::stop_token&) {
          const auto deadline = std::chrono::steady_clock::now() + 3s;
          while (queue.counters().submission_waits == waits &&
                 std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(100us);
          }
          auto taken = gpu().take_over();
          held_.store(taken.has_value());
          blocker_.reset();
          while (!finish_.load() &&
                 std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(100us);
          }
          if (taken.has_value()) {
            taken->release();
          }
          handed_.store(true);
        });
    ASSERT_TRUE(started.has_value());
    taker_.emplace(std::move(*started));
  }

  void hand_back() {
    finish_.store(true);
    taker_.reset();
    EXPECT_TRUE(handed_.load());
  }

  provider::headless::HeadlessProviderFactory providers_;
  composition::Observable<int> shade_{0};
  std::optional<provider::RenderQueue::Lease> blocker_;
  std::atomic<bool> held_{false};
  std::atomic<bool> handed_{false};
  std::atomic<bool> finish_{false};
  std::optional<provider::Thread> taker_;
};

TEST_F(GpuTakeoverTest, ArisaStopsPresentingWhileTheAppHoldsTheGpu) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  const auto queue = gpu().render_queue();
  ASSERT_TRUE(queue.has_value());
  EXPECT_TRUE(queue->get().parked());
  std::size_t presented = 0;
  for (int index = 0; index < 6; ++index) {
    shade_.set(index + 1);
    presented += presents_over(application, 1);
    EXPECT_TRUE(application.last_frame().taken_over);
  }
  EXPECT_EQ(presented, 0U);
  EXPECT_FALSE(gpu().take_over().has_value());
  EXPECT_EQ(gpu().takeover_counters().refused, 1U);
}

TEST_F(GpuTakeoverTest, TheAppAcquiresAndPresentsTheLentImages) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  EXPECT_EQ(taken->image_count(), 2U);
  for (int index = 0; index < 4; ++index) {
    const auto first = taken->acquire();
    ASSERT_TRUE(first.has_value());
    const auto second = taken->acquire();
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(*first, *second);
    const auto third = taken->acquire();
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error().detail(),
              provider::ProviderErrorDetail::image_unavailable);
    EXPECT_TRUE(taken->target(*first).has_value());
    ASSERT_TRUE(taken->present(*first).has_value());
    EXPECT_FALSE(taken->present(*first).has_value());
    ASSERT_TRUE(taken->present(*second).has_value());
  }
  EXPECT_EQ(gpu().takeover_presents(), 8U);
  frames(application, 2);
  EXPECT_EQ(gpu().takeover_presents(), 8U);
}

TEST_F(GpuTakeoverTest, PresentingAnImageThatWasNeverAcquiredIsRefused) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  EXPECT_FALSE(taken->present(0).has_value());
  EXPECT_FALSE(taken->present(1).has_value());
  EXPECT_FALSE(taken->present(7).has_value());
  EXPECT_EQ(gpu().takeover_presents(), 0U);
}

TEST_F(GpuTakeoverTest, HandBackSettlesTheImagesTheAppLeftAcquired) {
  Application application = start();
  {
    auto taken = gpu().take_over();
    ASSERT_TRUE(taken.has_value());
    ASSERT_TRUE(taken->acquire().has_value());
    ASSERT_TRUE(taken->acquire().has_value());
  }
  EXPECT_EQ(gpu().takeover_settled(), 2U);
  EXPECT_EQ(gpu().takeover_presents(), 0U);
  auto again = gpu().take_over();
  ASSERT_TRUE(again.has_value());
  const auto first = again->acquire();
  ASSERT_TRUE(first.has_value());
  EXPECT_TRUE(again->acquire().has_value());
}

TEST_F(GpuTakeoverTest, AReleasedTakeoverLendsNothing) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  const auto image = taken->acquire();
  ASSERT_TRUE(image.has_value());
  taken->release();
  EXPECT_EQ(taken->image_count(), 0U);
  EXPECT_FALSE(taken->target(*image).has_value());
  EXPECT_FALSE(taken->acquire().has_value());
}

TEST_F(GpuTakeoverTest, TheAppSetsItsOwnSwapIntervalAndHandBackRestoresIt) {
  Application application = start();
  const std::uint32_t before = gpu().swap_interval();
  {
    auto taken = gpu().take_over();
    ASSERT_TRUE(taken.has_value());
    taken->set_swap_interval(provider::SwapInterval::of<0>());
    EXPECT_EQ(gpu().swap_interval(), 0U);
  }
  EXPECT_EQ(gpu().swap_interval(), before);
}

TEST_F(GpuTakeoverTest, HandingBackRepaintsTheWholeUiOnce) {
  Application application = start();
  frames(application, 4);
  {
    auto taken = gpu().take_over();
    ASSERT_TRUE(taken.has_value());
    frames(application, 3);
  }
  EXPECT_FALSE(gpu().render_queue()->get().parked());
  EXPECT_EQ(presents_over(application, 1), 1U);
  EXPECT_FALSE(application.last_frame().taken_over);
  EXPECT_FALSE(gpu().last_damage().has_value());
  EXPECT_EQ(gpu().takeover_counters().returned, 1U);
  EXPECT_EQ(gpu().render_queue()->get().counters().overlaps, 0U);
}

TEST_F(GpuTakeoverTest, ATakeoverBetweenTwoFramesStillRepaintsTheWholeUi) {
  Application application = start();
  frames(application, 4);
  {
    auto taken = gpu().take_over();
    ASSERT_TRUE(taken.has_value());
  }
  EXPECT_EQ(presents_over(application, 1), 1U);
  EXPECT_FALSE(gpu().last_damage().has_value());
  EXPECT_EQ(gpu().takeover_counters().returned, 1U);
}

TEST_F(GpuTakeoverTest, ALongTakeoverLetsTheUiLoopWait) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  shade_.set(3);
  ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_TRUE(application.last_frame().taken_over);
  EXPECT_TRUE(application.waiting());
  int framed = 0;
  const auto began = std::chrono::steady_clock::now();
  ASSERT_TRUE(application
                  .run_until([&](Root&) {
                    ++framed;
                    return std::chrono::steady_clock::now() - began >= 160ms;
                  })
                  .has_value());
  EXPECT_LE(framed, 40);
  EXPECT_TRUE(application.last_frame().taken_over);
  EXPECT_TRUE(application.waiting());
}

TEST_F(GpuTakeoverTest, UiWorkKeepsRunningDuringATakeover) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  std::atomic<int> posted{0};
  ASSERT_TRUE(
      application.root().dispatcher().post([&posted] { posted.fetch_add(1); }));
  frames(application, 2);
  EXPECT_EQ(posted.load(), 1);
}

TEST_F(GpuTakeoverTest, AnotherThreadPresentsWhileTheUiTicks) {
  Application application = start();
  std::atomic<int> presented{0};
  std::atomic<bool> failed{false};
  std::atomic<bool> held{false};
  std::optional<provider::Thread> presenter;
  auto started = providers_.threading().start(
      provider::ThreadSpec{.name = "test-presenter",
                           .role = provider::ThreadRole::app},
      [&](const std::stop_token& stop) {
        auto taken = gpu().take_over();
        if (!taken.has_value()) {
          failed.store(true);
          return;
        }
        held.store(true);
        taken->set_swap_interval(provider::SwapInterval::of<0>());
        while (!stop.stop_requested() && presented.load() < 40) {
          const auto image = taken->acquire();
          if (!image.has_value() || !taken->present(*image).has_value()) {
            failed.store(true);
            return;
          }
          presented.fetch_add(1);
          std::this_thread::sleep_for(200us);
        }
      });
  ASSERT_TRUE(started.has_value());
  presenter.emplace(std::move(*started));
  for (int index = 0; index < 60 && presented.load() < 40; ++index) {
    shade_.set(index);
    ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
    ASSERT_TRUE(application.frame().has_value());
    std::this_thread::sleep_for(1ms);
  }
  presenter->request_stop();
  presenter.reset();
  EXPECT_FALSE(failed.load());
  EXPECT_TRUE(held.load());
  frames(application, 2);
  EXPECT_FALSE(gpu().render_queue()->get().parked());
  EXPECT_EQ(gpu().takeover_counters().returned, 1U);
  EXPECT_EQ(gpu().render_queue()->get().counters().overlaps, 0U);
  EXPECT_FALSE(application.last_frame().taken_over);
}

TEST_F(GpuTakeoverTest, AParkLandingOnAFrameInFlightLeavesTheUiThreadFree) {
  Application application = start();
  take_over_once_a_frame_waits();
  shade_.set(5);
  ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_TRUE(held_.load());
  EXPECT_FALSE(handed_.load());
  for (int index = 0; index < 3; ++index) {
    ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
    ASSERT_TRUE(application.frame().has_value());
    EXPECT_TRUE(application.last_frame().taken_over);
  }
  EXPECT_FALSE(handed_.load());
  hand_back();
  application.drain();
  EXPECT_EQ(presents_over(application, 1), 1U);
  EXPECT_FALSE(application.last_frame().taken_over);
  EXPECT_EQ(gpu().render_queue()->get().counters().overlaps, 0U);
}

TEST_F(GpuTakeoverTest, DestroyingTheApplicationDuringATakeoverDoesNotHang) {
  std::optional<Application> application{start()};
  take_over_once_a_frame_waits();
  shade_.set(5);
  ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
  ASSERT_TRUE(application->frame().has_value());
  EXPECT_TRUE(held_.load());
  application.reset();
  EXPECT_FALSE(handed_.load());
  hand_back();
  EXPECT_FALSE(gpu().render_queue()->get().parked());
  EXPECT_EQ(gpu().takeover_counters().returned, 1U);
}

TEST_F(GpuTakeoverTest, ReleasedTakeoverRefusesFurtherPresents) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  const auto image = taken->acquire();
  ASSERT_TRUE(image.has_value());
  taken->release();
  EXPECT_FALSE(taken->held());
  const auto refused = taken->present(*image);
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().detail(),
            provider::ProviderErrorDetail::not_taken_over);
}

}
