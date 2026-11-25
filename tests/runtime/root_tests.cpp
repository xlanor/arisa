#include <arisa/components/button.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/dispatcher.hpp>
#include <arisa/runtime/renderer.hpp>
#include <arisa/runtime/root.hpp>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class RootTest : public ::testing::Test {
 protected:
  struct Shelf {
    std::vector<int> keys;
    bool hold{false};
    std::vector<composition::DepartureHold> holds;

    friend auto operator==(const Shelf& left, const Shelf& right) -> bool {
      return left.keys == right.keys && left.hold == right.hold;
    }
  };

  class Display final : public composition::Component {
   public:
    explicit Display(std::shared_ptr<composition::Observable<Shelf>> shelf)
        : shelf_(std::move(shelf)) {}

    auto operator==(const Display&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const Shelf& shelf = context.read(*shelf_);
      return components::VStack(components::ForEach(
          shelf.keys, [](int key) { return key; },
          [shelf = shelf_](int key) {
            composition::Element text =
                components::Text(std::format("entry {}", key));
            if (!shelf->get().hold) {
              return text;
            }
            return std::move(text).on_depart(
                [shelf](composition::DepartureHold hold) {
                  shelf->update([&hold](Shelf& value) {
                    value.holds.push_back(std::move(hold));
                  });
                });
          }));
    }

   private:
    std::shared_ptr<composition::Observable<Shelf>> shelf_;
  };

  class Clicker final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> clicks{"clicks", 0};

    auto operator==(const Clicker&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto count = context.state(clicks);
      return components::VStack(
          components::Text(std::format("{}", count.get())).id("label"),
          components::Button("go", [count] {
            count.set(count.get() + 1);
          }).id("go"));
    }
  };

  class Recorder final : public Renderer {
   public:
    void render(const composition::Node& root) override {
      ++frames_;
      nodes_ = count(root);
    }

    auto frames() const noexcept -> int { return frames_; }

    auto nodes() const noexcept -> std::size_t { return nodes_; }

   private:
    static auto count(const composition::Node& node) -> std::size_t {
      std::size_t total = 1;
      for (const composition::Node& child : node.children()) {
        total += count(child);
      }
      return total;
    }

    int frames_{0};
    std::size_t nodes_{0};
  };

  static auto shelf(std::vector<int> keys, bool hold = false) -> Shelf {
    return Shelf{.keys = std::move(keys), .hold = hold, .holds = {}};
  }

  provider::headless::HeadlessProviderFactory providers_;
};

TEST_F(RootTest, RecordsItsOwnerAndRendersAfterReconciling) {
  auto source = std::make_shared<composition::Observable<Shelf>>(shelf({1, 2}));
  Root root{Display{source}, providers_};
  EXPECT_EQ(root.owner(), providers_.threading().current_thread());
  EXPECT_TRUE(root.tree().on_owner_thread());
  Recorder recorder;
  root.frame(recorder);
  EXPECT_EQ(recorder.frames(), 1);
  EXPECT_EQ(recorder.nodes(), 6U);
  EXPECT_EQ(root.frames(), 1U);
}

TEST_F(RootTest, AppliesTasksPostedFromAnotherThreadOnTheNextFrame) {
  auto source = std::make_shared<composition::Observable<Shelf>>(shelf({1}));
  Root root{Display{source}, providers_};
  root.frame();
  const Dispatcher dispatcher = root.dispatcher();
  bool posted = false;
  {
    auto worker = providers_.threading().start(
        provider::ThreadSpec{.name = "poster",
                             .role = provider::ThreadRole::background},
        [dispatcher, source, &posted](const std::stop_token&) {
          posted = dispatcher.post([source] {
            source->update([](Shelf& value) { value.keys.push_back(2); });
          });
        });
    ASSERT_TRUE(worker.has_value());
  }
  EXPECT_TRUE(posted);
  EXPECT_EQ(source->get().keys.size(), 1U);
  const std::size_t before = root.tree().size();
  root.frame();
  EXPECT_EQ(source->get().keys.size(), 2U);
  EXPECT_EQ(root.tree().size(), before + 1);
}

TEST_F(RootTest, DropsTasksPostedAfterTheRootIsGone) {
  std::optional<Dispatcher> dispatcher;
  auto ran = std::make_shared<int>(0);
  {
    auto source = std::make_shared<composition::Observable<Shelf>>(shelf({}));
    Root root{Display{source}, providers_};
    dispatcher.emplace(root.dispatcher());
    EXPECT_TRUE(dispatcher->post([ran] { ++*ran; }));
  }
  EXPECT_EQ(ran.use_count(), 1);
  EXPECT_FALSE(dispatcher->post([ran] { ++*ran; }));
  EXPECT_EQ(*ran, 0);
}

TEST_F(RootTest, ForwardsDuplicateKeysToDiagnostics) {
  auto source = std::make_shared<composition::Observable<Shelf>>(shelf({4, 4}));
  Root root{Display{source}, providers_};
  root.frame();
  const auto reports = providers_.diagnostics().snapshot();
  ASSERT_EQ(reports.size(), 1U);
  EXPECT_EQ(reports.front().severity, provider::Severity::warning);
  EXPECT_EQ(reports.front().category, "composition");
  EXPECT_NE(reports.front().message.find("duplicate key 4"), std::string::npos);
}

TEST_F(RootTest, ActivatesNodesByKey) {
  Root root{Clicker{}, providers_};
  root.frame();
  EXPECT_TRUE(root.activate(composition::ElementKey{"go"}));
  EXPECT_FALSE(root.activate(composition::ElementKey{"missing"}));
  EXPECT_FALSE(root.activate(composition::ElementKey{"label"}));
  root.frame();
  const auto label = root.tree().find("label");
  ASSERT_TRUE(label.has_value());
  EXPECT_EQ(root.tree()
                .node(*label)
                ->get()
                .content_as<components::Text::Content>()
                ->get()
                .text(),
            "1");
}

TEST_F(RootTest, DestructionReleasesDepartingNodesAndSubscriptions) {
  auto source =
      std::make_shared<composition::Observable<Shelf>>(shelf({1, 2, 3}, true));
  std::vector<composition::DepartureHold> kept;
  {
    Root root{Display{source}, providers_};
    root.frame();
    EXPECT_EQ(source->subscribers(), 1U);
    source->update([](Shelf& value) { value.keys = {1}; });
    root.frame();
    EXPECT_EQ(root.tree().departing(), 2U);
    EXPECT_EQ(source->get().holds.size(), 2U);
    ASSERT_TRUE(root.dispatcher().post([] {}));
  }
  EXPECT_EQ(source->subscribers(), 0U);
  source->update([&kept](Shelf& value) { kept = std::move(value.holds); });
  EXPECT_EQ(kept.size(), 2U);
  kept.clear();
  source->set(shelf({9}));
}

}
