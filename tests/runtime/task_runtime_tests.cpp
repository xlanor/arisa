#include <arisa/async/awaitables.hpp>
#include <arisa/async/task.hpp>
#include <arisa/async/thread_pool.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/ui_error.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/text_input/absent.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/runtime/text_input.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class RootTaskTest : public ::testing::Test {
 protected:
  struct Trace {
    std::atomic<std::uint64_t> background{0};
    std::atomic<std::uint64_t> home{0};
    std::atomic<int> hops{0};
    std::atomic<int> misplaced{0};
    std::atomic<bool> done{false};
    std::optional<TextInput::Result> text;
  };

  using Shared = std::shared_ptr<Trace>;

  static auto tick(composition::StateHandle<int> seconds) -> async::Task<void> {
    while (true) {
      co_await async::sleep(1s);
      seconds.update([](int& value) { ++value; });
    }
  }

  static auto count_frames(composition::StateHandle<int> frames)
      -> async::Task<void> {
    while (true) {
      co_await async::next_frame();
      frames.update([](int& value) { ++value; });
    }
  }

  class Uptime final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> seconds{"seconds", 0};
    static constexpr composition::StateKey<int> frames{"frames", 0};

    auto operator==(const Uptime&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto elapsed = context.state(seconds);
      const auto counted = context.state(frames);
      return components::Text(
                 std::format("{} {}", elapsed.get(), counted.get()))
          .id("uptime")
          .task([elapsed] { return tick(elapsed); })
          .task([counted] { return count_frames(counted); });
    }
  };

  static auto travel(
      std::reference_wrapper<async::Executor> background,
      std::reference_wrapper<async::Executor> home,
      std::reference_wrapper<provider::ThreadingProvider> threading,
      provider::ThreadId owner, int hops, Shared trace) -> async::Task<void> {
    for (int hop = 0; hop < hops; ++hop) {
      co_await async::on(background.get());
      trace->background = threading.get().current_thread().value();
      if (threading.get().current_thread() == owner) {
        ++trace->misplaced;
      }
      co_await async::on(home.get());
      trace->home = threading.get().current_thread().value();
      if (threading.get().current_thread() != owner) {
        ++trace->misplaced;
      }
      ++trace->hops;
    }
    trace->done = true;
  }

  class Traveller final : public composition::Component {
   public:
    Traveller(Shared trace, provider::ThreadingProvider& threading,
              provider::ThreadId owner, int hops)
        : trace_(std::move(trace)),
          threading_(threading),
          owner_(owner),
          hops_(hops) {}

    auto operator==(const Traveller& other) const -> bool {
      return trace_ == other.trace_ && hops_ == other.hops_;
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const composition::RootServices& services = context.services();
      return components::Text("travelling")
          .task([background = services.executors.background,
                 home = services.root, threading = threading_, owner = owner_,
                 hops = hops_, trace = trace_] {
            return travel(background, home, threading, owner, hops, trace);
          });
    }

   private:
    Shared trace_;
    std::reference_wrapper<provider::ThreadingProvider> threading_;
    provider::ThreadId owner_;
    int hops_;
  };

  static auto ask(TextInput text_input, Shared trace) -> async::Task<void> {
    trace->text = co_await text_input.request(
        provider::TextInputRequest{.initial = "", .hint = "name"});
    trace->done = true;
  }

  class Asker final : public composition::Component {
   public:
    Asker(TextInput text_input, Shared trace,
          std::shared_ptr<composition::Observable<bool>> shown)
        : text_input_(text_input),
          trace_(std::move(trace)),
          shown_(std::move(shown)) {}

    auto operator==(const Asker&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      if (!context.read(*shown_)) {
        return components::Text("closed");
      }
      return components::Text("asking").task(
          [text_input = text_input_, trace = trace_] {
            return ask(text_input, trace);
          });
    }

   private:
    TextInput text_input_;
    Shared trace_;
    std::shared_ptr<composition::Observable<bool>> shown_;
  };

  class Broken final : public composition::Component {
   public:
    auto operator==(const Broken&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      context.fail(composition::UiError{"broken on purpose"});
      return components::Text("broken");
    }
  };

  static auto label(Root& root) -> std::string {
    const auto node = root.tree().find("uptime");
    return std::string{root.tree()
                           .node(*node)
                           ->get()
                           .content_as<components::Text::Content>()
                           ->get()
                           .text()};
  }

  static void run_until(Root& root, const std::function<bool()>& done) {
    const auto deadline = std::chrono::steady_clock::now() + 30s;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
      root.frame();
    }
  }

  provider::headless::HeadlessProviderFactory providers_;
  Shared trace_{std::make_shared<Trace>()};
};

TEST_F(RootTaskTest, SleepFollowsTheClockAndNextFrameTheFrames) {
  Root root{Uptime{}, providers_};
  root.frame();
  EXPECT_EQ(label(root), "0 0");
  ASSERT_TRUE(providers_.clock().advance(999ms).has_value());
  root.frame();
  EXPECT_EQ(label(root), "0 1");
  ASSERT_TRUE(providers_.clock().advance(1ms).has_value());
  root.frame();
  EXPECT_EQ(label(root), "1 2");
  ASSERT_TRUE(providers_.clock().advance(1s).has_value());
  root.frame();
  EXPECT_EQ(label(root), "2 3");
  EXPECT_EQ(root.tree().running_tasks(), 2U);
}

TEST_F(RootTaskTest, HopsToThePoolAndBackToTheRootThread) {
  auto pool = async::ThreadPool::create(
      providers_.threading(),
      async::ThreadPool::Config{.workers = async::WorkerCount::of<2>(),
                                .role = provider::ThreadRole::background,
                                .name = "arisa-root-pool"});
  ASSERT_TRUE(pool.has_value());
  const provider::ThreadId owner = providers_.threading().current_thread();
  Root root{Traveller{trace_, providers_.threading(), owner, 1000}, providers_,
            async::Executors{.background = *pool, .blocking = *pool}};
  run_until(root, [this] { return trace_->done.load(); });
  EXPECT_TRUE(trace_->done.load());
  EXPECT_EQ(trace_->hops.load(), 1000);
  EXPECT_EQ(trace_->misplaced.load(), 0);
  EXPECT_EQ(trace_->home.load(), owner.value());
  EXPECT_NE(trace_->background.load(), owner.value());
}

TEST_F(RootTaskTest, TextInputResolvesFromTheScriptedProvider) {
  providers_.text_input().push(std::string{"arisa"});
  auto shown = std::make_shared<composition::Observable<bool>>(true);
  Root root{Asker{TextInput{providers_.text_input()}, trace_, shown},
            providers_};
  run_until(root, [this] { return trace_->done.load(); });
  ASSERT_TRUE(trace_->text.has_value());
  ASSERT_TRUE(trace_->text->has_value());
  EXPECT_EQ(**trace_->text, "arisa");
  EXPECT_EQ(providers_.text_input().requests().front().hint, "name");
  EXPECT_EQ(providers_.text_input().live_sessions(), 0U);
}

TEST_F(RootTaskTest, TextInputReportsCancellation) {
  providers_.text_input().push(std::unexpected(
      provider::ProviderError{provider::ProviderErrorCode::cancelled}));
  auto shown = std::make_shared<composition::Observable<bool>>(true);
  Root root{Asker{TextInput{providers_.text_input()}, trace_, shown},
            providers_};
  run_until(root, [this] { return trace_->done.load(); });
  ASSERT_TRUE(trace_->text.has_value());
  ASSERT_FALSE(trace_->text->has_value());
  EXPECT_EQ(trace_->text->error().code(), TextInputErrorCode::cancelled);
}

TEST_F(RootTaskTest, TextInputWithoutAProviderIsUnsupported) {
  provider::AbsentTextInputProvider absent;
  auto shown = std::make_shared<composition::Observable<bool>>(true);
  Root root{Asker{TextInput{absent}, trace_, shown}, providers_};
  run_until(root, [this] { return trace_->done.load(); });
  ASSERT_TRUE(trace_->text.has_value());
  EXPECT_EQ(trace_->text->error().code(), TextInputErrorCode::unsupported);
}

TEST_F(RootTaskTest, DepartingCancelsTheOpenTextInputSession) {
  auto shown = std::make_shared<composition::Observable<bool>>(true);
  Root root{Asker{TextInput{providers_.text_input()}, trace_, shown},
            providers_};
  root.frame();
  root.frame();
  root.frame();
  EXPECT_EQ(providers_.text_input().live_sessions(), 1U);
  shown->set(false);
  root.frame();
  EXPECT_EQ(providers_.text_input().live_sessions(), 0U);
  EXPECT_EQ(providers_.text_input().cancelled_sessions(), 1U);
  EXPECT_FALSE(trace_->done.load());
}

TEST_F(RootTaskTest, FailuresAreReportedThroughDiagnostics) {
  Root root{Broken{}, providers_};
  root.frame();
  const auto reports = providers_.diagnostics().snapshot();
  ASSERT_EQ(reports.size(), 1U);
  EXPECT_EQ(reports.front().severity, provider::Severity::error);
  EXPECT_EQ(reports.front().category, "composition");
  EXPECT_NE(reports.front().message.find("broken on purpose"),
            std::string::npos);
  EXPECT_NE(reports.front().message.find("no boundary"), std::string::npos);
}

}
