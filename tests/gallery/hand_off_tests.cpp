#include <arisa/async/executor.hpp>
#include <arisa/async/job.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/dispatcher.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <numeric>
#include <stop_token>
#include <vector>

namespace arisa::runtime {

class GalleryHandOffTest : public GalleryFixture {
 protected:
  struct Ledger {
    std::atomic<int> worked{0};
    std::atomic<std::uint64_t> metered{0};
    int landed{0};
    std::uint64_t sum{0};
  };

  static constexpr int rounds = 6;
  static constexpr int jobs_per_round = 24;

  static auto dispatch(async::Executor& executor, const Dispatcher& home,
                       const std::shared_ptr<Ledger>& ledger, int seed)
      -> async::Job {
    return executor.submit([home, ledger, seed](std::stop_token) {
      std::vector<std::uint64_t> scratch(64 + static_cast<std::size_t>(seed));
      std::iota(scratch.begin(), scratch.end(), std::uint64_t{0});
      const std::uint64_t total =
          std::accumulate(scratch.begin(), scratch.end(), std::uint64_t{0});
      ledger->metered.store(provider::AllocationMeter::count().value_or(0),
                            std::memory_order_relaxed);
      ledger->worked.fetch_add(1, std::memory_order_relaxed);
      static_cast<void>(home.post([ledger, total] {
        ++ledger->landed;
        ledger->sum += total;
      }));
    });
  }
};

TEST_F(GalleryHandOffTest, WorkerResultsLandOnTheUiThreadWhileGlassRenders) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::materials);
  const Dispatcher home = application.root().dispatcher();
  const auto ledger = std::make_shared<Ledger>();
  std::vector<async::Job> jobs;
  std::uint64_t expected = 0;
  for (int round = 0; round < rounds; ++round) {
    for (int index = 0; index < jobs_per_round; ++index) {
      const int seed = (round * jobs_per_round) + index;
      async::Executor& executor = index % 2 == 0
                                      ? application.executors().background.get()
                                      : application.executors().blocking.get();
      jobs.push_back(dispatch(executor, home, ledger, seed));
      const std::uint64_t count = 64 + static_cast<std::uint64_t>(seed);
      expected += count * (count - 1) / 2;
    }
    if (round % 2 == 1) {
      press(providers, application, provider::Button::r);
    }
    run(providers, application, 8);
  }
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds{60};
  while (ledger->landed < rounds * jobs_per_round &&
         std::chrono::steady_clock::now() < deadline) {
    run(providers, application, 1);
  }
  EXPECT_EQ(ledger->worked.load(), rounds * jobs_per_round);
  EXPECT_EQ(ledger->landed, rounds * jobs_per_round);
  EXPECT_EQ(ledger->sum, expected);
}

}
