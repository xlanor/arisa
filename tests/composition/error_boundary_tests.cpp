#include <arisa/async/awaitables.hpp>
#include <arisa/async/frame_timeline.hpp>
#include <arisa/async/manual_executor.hpp>
#include <arisa/async/task.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/error_boundary.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tree.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace arisa::composition {

class ErrorBoundaryTest : public ::testing::Test {
 protected:
  struct Switches {
    bool explode{false};
    bool fallback_explodes{false};
    int sibling{0};

    friend auto operator==(const Switches&, const Switches&) -> bool = default;
  };

  using Shared = std::shared_ptr<Observable<Switches>>;

  class Fragile final : public Component {
   public:
    explicit Fragile(Shared switches) : switches_(std::move(switches)) {}

    auto operator==(const Fragile&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      if (context.read(*switches_).explode) {
        context.fail(UiError{"boom"});
      }
      return components::Text("fragile").id("fragile");
    }

   private:
    Shared switches_;
  };

  class Rescue final : public Component {
   public:
    Rescue(Shared switches, std::string message)
        : switches_(std::move(switches)), message_(std::move(message)) {}

    auto operator==(const Rescue&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      if (context.read(*switches_).fallback_explodes) {
        context.fail(UiError{"fallback broke"});
      }
      return components::Text(std::format("fallback: {}", message_))
          .id("fallback");
    }

   private:
    Shared switches_;
    std::string message_;
  };

  class Sibling final : public Component {
   public:
    explicit Sibling(Shared switches) : switches_(std::move(switches)) {}

    auto operator==(const Sibling&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      return components::Text(
                 std::format("sibling {}", context.read(*switches_).sibling))
          .id("sibling");
    }

   private:
    Shared switches_;
  };

  class Screen final : public Component {
   public:
    explicit Screen(Shared switches) : switches_(std::move(switches)) {}

    auto operator==(const Screen&) const -> bool = default;

    auto body(Context&) const -> Element override {
      return components::VStack(
          ErrorBoundary(Fragile{switches_},
                        [switches = switches_](const UiError& error) {
                          return Element{
                              Rescue{switches, std::string{error.message()}}};
                        })
              .id("inner"),
          Sibling{switches_});
    }

   private:
    Shared switches_;
  };

  class Nested final : public Component {
   public:
    explicit Nested(Shared switches) : switches_(std::move(switches)) {}

    auto operator==(const Nested&) const -> bool = default;

    auto body(Context&) const -> Element override {
      return ErrorBoundary(
                 Screen{switches_},
                 [](const UiError& error) -> Element {
                   return components::Text(
                              std::format("outer: {}", error.message()))
                       .id("outer-fallback");
                 })
          .id("outer");
    }

   private:
    Shared switches_;
  };

  static auto doomed(std::reference_wrapper<async::Executor> root)
      -> async::Task<std::expected<void, UiError>> {
    co_await async::on(root.get());
    co_return std::unexpected(UiError{"task failed"});
  }

  class Worker final : public Component {
   public:
    auto operator==(const Worker&) const -> bool = default;

    auto body(Context& context) const -> Element override {
      const std::reference_wrapper<async::Executor> root =
          context.services().root;
      return ErrorBoundary(
                 components::Text("working").id("working").task(
                     [root] { return doomed(root); }),
                 [](const UiError& error) -> Element {
                   return components::Text(std::string{error.message()})
                       .id("task-fallback");
                 })
          .id("task-boundary");
    }
  };

  static auto owner() -> Tree::OwnerCheck {
    return [] { return true; };
  }

  static auto text(const Tree& tree, const ElementKey& key) -> std::string {
    const auto found = tree.find(key);
    if (!found.has_value()) {
      return {};
    }
    const auto content =
        tree.node(*found)->get().content_as<components::Text::Content>();
    return content.has_value() ? std::string{content->get().text()}
                               : std::string{};
  }

  auto services() -> RootServices {
    return RootServices{.root = manual_,
                        .frames = timeline_,
                        .executors = async::Executors{.background = manual_,
                                                      .blocking = manual_}};
  }

  async::ManualExecutor manual_;
  async::FrameTimeline timeline_{[] { return std::chrono::nanoseconds{0}; }};
  Shared switches_{std::make_shared<Observable<Switches>>()};
};

TEST_F(ErrorBoundaryTest, RendersContentUntilAFailure) {
  Tree tree{Screen{switches_}, owner()};
  tree.reconcile();
  EXPECT_EQ(text(tree, "fragile"), "fragile");
  EXPECT_TRUE(tree.take_failures().empty());
}

TEST_F(ErrorBoundaryTest, AFailingChildSwapsToTheFallbackAndIsReported) {
  Tree tree{Screen{switches_}, owner()};
  tree.reconcile();
  switches_->update([](Switches& value) { value.explode = true; });
  tree.reconcile();
  EXPECT_EQ(text(tree, "fallback"), "fallback: boom");
  EXPECT_FALSE(tree.find("fragile").has_value());
  const auto boundary = tree.find("inner");
  ASSERT_TRUE(boundary.has_value());
  ASSERT_TRUE(tree.node(*boundary)->get().boundary_failure().has_value());
  const auto failures = tree.take_failures();
  ASSERT_EQ(failures.size(), 1U);
  EXPECT_EQ(failures.front().boundary, boundary);
  EXPECT_EQ(failures.front().error, UiError{"boom"});
}

TEST_F(ErrorBoundaryTest, TheRestOfTheTreeKeepsRunning) {
  Tree tree{Screen{switches_}, owner()};
  tree.reconcile();
  switches_->update([](Switches& value) { value.explode = true; });
  tree.reconcile();
  switches_->update([](Switches& value) { value.sibling = 3; });
  tree.reconcile();
  EXPECT_EQ(text(tree, "sibling"), "sibling 3");
  EXPECT_EQ(text(tree, "fallback"), "fallback: boom");
}

TEST_F(ErrorBoundaryTest, ResetRendersTheContentAgain) {
  Tree tree{Screen{switches_}, owner()};
  tree.reconcile();
  switches_->update([](Switches& value) { value.explode = true; });
  tree.reconcile();
  switches_->update([](Switches& value) { value.explode = false; });
  tree.reconcile();
  EXPECT_EQ(text(tree, "fallback"), "fallback: boom");
  const auto boundary = tree.find("inner");
  ASSERT_TRUE(boundary.has_value());
  EXPECT_TRUE(tree.reset_boundary(*boundary));
  EXPECT_FALSE(tree.reset_boundary(*boundary));
  tree.reconcile();
  EXPECT_EQ(text(tree, "fragile"), "fragile");
  EXPECT_FALSE(tree.find("fallback").has_value());
}

TEST_F(ErrorBoundaryTest, AFailingFallbackEscalatesToTheOuterBoundary) {
  Tree tree{Nested{switches_}, owner()};
  tree.reconcile();
  switches_->update([](Switches& value) {
    value.explode = true;
    value.fallback_explodes = true;
  });
  tree.reconcile();
  EXPECT_EQ(text(tree, "outer-fallback"), "outer: fallback broke");
  EXPECT_FALSE(tree.find("sibling").has_value());
  EXPECT_EQ(tree.take_failures().size(), 2U);
}

TEST_F(ErrorBoundaryTest, AFailureWithoutABoundaryIsOnlyReported) {
  Tree tree{Fragile{switches_}, owner()};
  tree.reconcile();
  switches_->update([](Switches& value) { value.explode = true; });
  tree.reconcile();
  EXPECT_EQ(text(tree, "fragile"), "fragile");
  const auto failures = tree.take_failures();
  ASSERT_EQ(failures.size(), 1U);
  EXPECT_FALSE(failures.front().boundary.has_value());
}

TEST_F(ErrorBoundaryTest, AFailedTaskTripsTheNearestBoundary) {
  Tree tree{Worker{}, owner(), services()};
  tree.reconcile();
  EXPECT_EQ(text(tree, "working"), "working");
  EXPECT_EQ(manual_.drain(), 1U);
  tree.reconcile();
  EXPECT_EQ(text(tree, "task-fallback"), "task failed");
  EXPECT_FALSE(tree.find("working").has_value());
  EXPECT_EQ(tree.take_failures().size(), 1U);
}

}
