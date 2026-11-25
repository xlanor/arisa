#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/default_factory.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <format>
#include <string>

namespace arisa::runtime {

using namespace std::chrono_literals;

class ApplicationTest : public ::testing::Test {
 protected:
  class Tally final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"count", 0};

    auto operator==(const Tally&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto current = context.state(count);
      return components::VStack(
          components::Text(std::format("count {}", current.get())).id("label"),
          components::HStack(
              components::Button("add",
                                 [current] { current.set(current.get() + 1); })
                  .id("add"),
              components::Text("beside")));
    }
  };

  static auto label(Root& root) -> std::string {
    const auto node = root.tree().find("label");
    return std::string{root.tree()
                           .node(*node)
                           ->get()
                           .content_as<components::Text::Content>()
                           ->get()
                           .text()};
  }
};

TEST_F(ApplicationTest, RefusesFactoriesWithoutSurfaceInputOrGpu) {
  provider::headless::HeadlessProviderFactory headless;
  provider::DefaultProviderFactory bare;
  const auto no_surface = Application::create(bare, Tally{});
  ASSERT_FALSE(no_surface.has_value());
  EXPECT_EQ(no_surface.error().code(), ApplicationErrorCode::missing_surface);

  provider::DefaultProviderFactory surface_only{
      provider::Providers{.surface = headless.surface()}};
  const auto no_input = Application::create(surface_only, Tally{});
  ASSERT_FALSE(no_input.has_value());
  EXPECT_EQ(no_input.error().code(), ApplicationErrorCode::missing_input);

  provider::DefaultProviderFactory no_gpu_factory{provider::Providers{
      .surface = headless.surface(), .input = headless.input()}};
  const auto no_gpu = Application::create(no_gpu_factory, Tally{});
  ASSERT_FALSE(no_gpu.has_value());
  EXPECT_EQ(no_gpu.error().code(), ApplicationErrorCode::missing_gpu);
  EXPECT_FALSE(no_gpu.error().description().empty());
}

TEST_F(ApplicationTest, RendersFramesThroughTheBridgeAndActivatesThroughRoot) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Tally{});
  ASSERT_TRUE(application.has_value());
  std::uint32_t frames = 0;
  ASSERT_TRUE(application
                  ->run_until([&frames](Root& root) {
                    ++frames;
                    if (frames <= 3) {
                      EXPECT_TRUE(
                          root.activate(composition::ElementKey{"add"}));
                    }
                    return frames >= 5;
                  })
                  .has_value());
  EXPECT_EQ(frames, 5U);
  EXPECT_EQ(providers.gpu().frames_presented(), 4U);
  EXPECT_GT(providers.gpu().draws(), 0U);
  EXPECT_EQ(label(application->root()), "count 3");
  EXPECT_EQ(application->root().frames(), 5U);
  EXPECT_FALSE(application->exit_requested());
}

TEST_F(ApplicationTest, RunStopsWhenTheLifecycleAsksToExit) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Tally{});
  ASSERT_TRUE(application.has_value());
  ASSERT_TRUE(application->frame().has_value());
  providers.lifecycle().push(provider::LifecycleEvent::exit_requested);
  ASSERT_TRUE(application->run().has_value());
  EXPECT_TRUE(application->exit_requested());
  EXPECT_EQ(application->root().frames(), 2U);
}

TEST_F(ApplicationTest, MovesWithoutLosingTheTree) {
  provider::headless::HeadlessProviderFactory providers;
  auto created = Application::create(providers, Tally{});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  ASSERT_TRUE(application.frame().has_value());
  ASSERT_TRUE(providers.clock().advance(16ms).has_value());
  EXPECT_EQ(label(application.root()), "count 0");
}

}
