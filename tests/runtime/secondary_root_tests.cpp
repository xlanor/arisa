#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/dispatcher.hpp>
#include <arisa/runtime/root.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class SecondaryRootTest : public ::testing::Test {
 protected:
  class Mailbox {
   public:
    void deliver(Dispatcher dispatcher) {
      {
        const std::scoped_lock lock{mutex_};
        dispatcher_.emplace(std::move(dispatcher));
      }
      changed_.notify_all();
    }

    auto collect() -> Dispatcher {
      std::unique_lock lock{mutex_};
      changed_.wait(lock, [this] { return dispatcher_.has_value(); });
      return *dispatcher_;
    }

   private:
    std::mutex mutex_;
    std::condition_variable changed_;
    std::optional<Dispatcher> dispatcher_;
  };

  struct Exchange {
    std::atomic<int> main_received{0};
    std::atomic<int> overlay_received{0};
    std::atomic<int> misplaced{0};
    std::atomic<std::uint64_t> overlay_frames{0};
    std::atomic<std::uint64_t> overlay_presented{0};
    std::atomic<bool> overlay_done{false};
  };

  class Counter final : public composition::Component {
   public:
    explicit Counter(std::shared_ptr<composition::Observable<int>> value)
        : value_(std::move(value)) {}

    auto operator==(const Counter&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return components::Text(std::format("{}", context.read(*value_)))
          .id("value");
    }

   private:
    std::shared_ptr<composition::Observable<int>> value_;
  };

  static constexpr int overlay_frames = 30;

  static void intrude(provider::headless::HeadlessProviderFactory& providers) {
    auto value = std::make_shared<composition::Observable<int>>(0);
    Root root{Counter{value}, providers};
    auto intruder = providers.threading().start(
        provider::ThreadSpec{.name = "arisa-intruder",
                             .role = provider::ThreadRole::secondary_root},
        [&root](const std::stop_token&) { root.frame(); });
    static_cast<void>(intruder);
  }

  provider::headless::HeadlessProviderFactory main_providers_;
  provider::headless::HeadlessProviderFactory overlay_providers_;
};

TEST_F(SecondaryRootTest, DrivesASecondRootOnItsOwnThreadAndTalksByPost) {
  auto exchange = std::make_shared<Exchange>();
  auto main_value = std::make_shared<composition::Observable<int>>(0);
  auto application = Application::create(main_providers_, Counter{main_value});
  ASSERT_TRUE(application.has_value());
  const provider::ThreadId main_thread =
      main_providers_.threading().current_thread();
  const Dispatcher main_dispatcher = application->root().dispatcher();
  Mailbox mailbox;
  {
    auto overlay = main_providers_.threading().start(
        provider::ThreadSpec{.name = "arisa-overlay",
                             .role = provider::ThreadRole::secondary_root},
        [&, exchange](const std::stop_token&) {
          auto overlay_value =
              std::make_shared<composition::Observable<int>>(0);
          auto secondary =
              Application::create(overlay_providers_, Counter{overlay_value});
          if (!secondary.has_value()) {
            return;
          }
          const provider::ThreadId overlay_thread = secondary->root().owner();
          if (overlay_thread == main_thread) {
            ++exchange->misplaced;
          }
          mailbox.deliver(secondary->root().dispatcher());
          for (int frame = 0; frame < overlay_frames; ++frame) {
            if (!secondary->frame().has_value()) {
              ++exchange->misplaced;
            }
            static_cast<void>(
                main_dispatcher.post([&threading = main_providers_.threading(),
                                      main_thread, main_value, exchange] {
                  if (threading.current_thread() != main_thread) {
                    ++exchange->misplaced;
                  }
                  main_value->update([](int& value) { ++value; });
                  ++exchange->main_received;
                }));
            std::this_thread::sleep_for(1ms);
          }
          for (int settle = 0;
               settle < 5000 && exchange->overlay_received.load() == 0;
               ++settle) {
            static_cast<void>(secondary->frame());
            std::this_thread::sleep_for(1ms);
          }
          static_cast<void>(secondary->frame());
          exchange->overlay_frames = secondary->root().frames();
          exchange->overlay_presented =
              overlay_providers_.gpu().frames_presented();
          exchange->overlay_done = true;
        });
    ASSERT_TRUE(overlay.has_value());
    Dispatcher overlay_dispatcher = mailbox.collect();
    const provider::ThreadId overlay_owner = overlay->id();
    EXPECT_TRUE(overlay_dispatcher.post(
        [&threading = overlay_providers_.threading(), overlay_owner, exchange] {
          if (threading.current_thread() != overlay_owner) {
            ++exchange->misplaced;
          }
          ++exchange->overlay_received;
        }));
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (!exchange->overlay_done &&
           std::chrono::steady_clock::now() < deadline) {
      ASSERT_TRUE(application->frame().has_value());
      std::this_thread::sleep_for(100us);
    }
  }
  ASSERT_TRUE(application->frame().has_value());
  EXPECT_TRUE(exchange->overlay_done.load());
  EXPECT_EQ(exchange->misplaced.load(), 0);
  EXPECT_EQ(exchange->main_received.load(), overlay_frames);
  EXPECT_EQ(main_value->get(), overlay_frames);
  EXPECT_EQ(exchange->overlay_received.load(), 1);
  EXPECT_GE(exchange->overlay_frames.load(),
            static_cast<std::uint64_t>(overlay_frames));
  EXPECT_GT(exchange->overlay_presented.load(), 0U);
  EXPECT_LE(exchange->overlay_presented.load(),
            exchange->overlay_frames.load());
  EXPECT_GT(main_providers_.gpu().frames_presented(), 0U);
}

#ifndef NDEBUG
TEST_F(SecondaryRootTest, DrivingARootFromAnotherThreadIsCaughtInDebug) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(intrude(main_providers_), "");
}
#endif

}
