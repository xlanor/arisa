#include <arisa/async/awaitables.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/dispatcher.hpp>
#include <arisa/runtime/text_input.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class ShutdownTest : public ::testing::Test {
 protected:
  struct Ledger {
    std::atomic<int> destroyed{0};
    std::atomic<int> posted_while_open{0};
    std::atomic<bool> spinning{false};
  };

  using Shared = std::shared_ptr<Ledger>;
  using ExecutorRef = std::reference_wrapper<async::Executor>;

  class Witness {
   public:
    Witness(Shared ledger, std::optional<Dispatcher> dispatcher) noexcept
        : ledger_(std::move(ledger)), dispatcher_(std::move(dispatcher)) {}

    Witness(const Witness&) = delete;
    auto operator=(const Witness&) -> Witness& = delete;
    Witness(Witness&&) = delete;
    auto operator=(Witness&&) -> Witness& = delete;

    ~Witness() {
      if (dispatcher_.has_value() && dispatcher_->post([] {})) {
        ++ledger_->posted_while_open;
      }
      ++ledger_->destroyed;
    }

   private:
    Shared ledger_;
    std::optional<Dispatcher> dispatcher_;
  };

  static auto sleeper(Shared ledger, Dispatcher dispatcher)
      -> async::Task<void> {
    const Witness witness{ledger, dispatcher};
    co_await async::sleep(1h);
  }

  static auto framer(Shared ledger, Dispatcher dispatcher)
      -> async::Task<void> {
    const Witness witness{ledger, dispatcher};
    while (true) {
      co_await async::next_frame();
    }
  }

  static auto parked(Shared ledger, Dispatcher dispatcher, ExecutorRef pool)
      -> async::Task<void> {
    const Witness witness{ledger, dispatcher};
    while (true) {
      co_await async::on(pool.get());
    }
  }

  static auto posted(Shared ledger, Dispatcher dispatcher, ExecutorRef root)
      -> async::Task<void> {
    const Witness witness{ledger, dispatcher};
    while (true) {
      co_await async::on(root.get());
    }
  }

  static auto child(Shared ledger, Dispatcher dispatcher) -> async::Task<int> {
    const Witness witness{ledger, dispatcher};
    co_await async::sleep(1h);
    co_return 1;
  }

  static auto awaiting_child(Shared ledger, Dispatcher dispatcher)
      -> async::Task<void> {
    const Witness witness{ledger, dispatcher};
    static_cast<void>(co_await child(ledger, dispatcher));
  }

  static auto typing(Shared ledger, Dispatcher dispatcher, TextInput input)
      -> async::Task<void> {
    const Witness witness{ledger, dispatcher};
    static_cast<void>(co_await input.request(provider::TextInputRequest{}));
  }

  static auto spinner(Shared ledger, ExecutorRef pool) -> async::Task<void> {
    const Witness witness{ledger, std::nullopt};
    co_await async::on(pool.get());
    ledger->spinning = true;
    while (!co_await async::stop_requested()) {
      std::this_thread::yield();
    }
    co_await async::on(pool.get());
  }

  class Everything final : public composition::Component {
   public:
    Everything(Shared ledger, TextInput input)
        : ledger_(std::move(ledger)), input_(input) {}

    auto operator==(const Everything&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const composition::RootServices& services = context.services();
      const Dispatcher home =
          static_cast<const Dispatcher&>(services.root.get());
      const ExecutorRef pool = services.executors.blocking;
      const ExecutorRef root = services.root;
      return components::VStack(
          components::Text("sleep").task(
              [ledger = ledger_, home] { return sleeper(ledger, home); }),
          components::Text("frame").task(
              [ledger = ledger_, home] { return framer(ledger, home); }),
          components::Text("pool").task([ledger = ledger_, home, pool] {
            return parked(ledger, home, pool);
          }),
          components::Text("dispatcher").task([ledger = ledger_, home, root] {
            return posted(ledger, home, root);
          }),
          components::Text("child").task([ledger = ledger_, home] {
            return awaiting_child(ledger, home);
          }),
          components::Text("text").task(
              [ledger = ledger_, home, input = input_] {
                return typing(ledger, home, input);
              }),
          components::Text("spin").task(
              [ledger = ledger_, pool] { return spinner(ledger, pool); }));
    }

   private:
    Shared ledger_;
    TextInput input_;
  };

  static constexpr int suspended_witnesses = 7;
  static constexpr int all_witnesses = 8;

  provider::headless::HeadlessProviderFactory providers_;
  Shared ledger_{std::make_shared<Ledger>()};
};

TEST_F(ShutdownTest, CancelsTasksOnEveryAwaitableBeforeClosingAndJoining) {
  {
    auto application = Application::create(
        providers_, Everything{ledger_, TextInput{providers_.text_input()}});
    ASSERT_TRUE(application.has_value());
    for (int frame = 0; frame < 4; ++frame) {
      ASSERT_TRUE(application->frame().has_value());
    }
    for (int attempt = 0; attempt < 5000 && !ledger_->spinning; ++attempt) {
      std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(ledger_->spinning.load());
    EXPECT_EQ(providers_.text_input().live_sessions(), 1U);
    EXPECT_EQ(ledger_->destroyed.load(), 0);
  }
  EXPECT_EQ(ledger_->destroyed.load(), all_witnesses);
  EXPECT_EQ(ledger_->posted_while_open.load(), suspended_witnesses);
  EXPECT_EQ(providers_.text_input().live_sessions(), 0U);
  EXPECT_EQ(providers_.text_input().cancelled_sessions(), 1U);
}

}
