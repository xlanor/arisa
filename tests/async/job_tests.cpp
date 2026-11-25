#include <arisa/async/job.hpp>
#include <arisa/async/manual_executor.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stop_token>
#include <vector>

namespace arisa::async {

TEST(JobTest, RunsOnceThroughItsTicket) {
  int runs = 0;
  auto [job, ticket] = Job::create([&runs](const std::stop_token&) { ++runs; });
  EXPECT_FALSE(job.started());
  ticket.run();
  ticket.run();
  EXPECT_EQ(runs, 1);
  EXPECT_TRUE(job.finished());
  EXPECT_FALSE(job.dropped());
}

TEST(JobTest, DestroyingAnUnstartedJobDropsTheWork) {
  auto captured = std::make_shared<int>(0);
  std::optional<Job::Ticket> kept;
  {
    auto [job, ticket] =
        Job::create([captured](const std::stop_token&) { ++*captured; });
    kept.emplace(std::move(ticket));
    EXPECT_EQ(captured.use_count(), 2);
  }
  EXPECT_EQ(captured.use_count(), 1);
  EXPECT_TRUE(kept->dropped());
  kept->run();
  EXPECT_EQ(*captured, 0);
}

TEST(JobTest, CancellingARunningJobRequestsStop) {
  std::optional<Job> held;
  bool stopped = false;
  auto [job, ticket] =
      Job::create([&held, &stopped](const std::stop_token& token) {
        held->cancel();
        stopped = token.stop_requested();
      });
  held.emplace(std::move(job));
  ticket.run();
  EXPECT_TRUE(stopped);
  EXPECT_TRUE(held->finished());
}

TEST(JobTest, MoveAssignmentCancelsTheReplacedJob) {
  int runs = 0;
  auto [first, first_ticket] =
      Job::create([&runs](const std::stop_token&) { ++runs; });
  auto [second, second_ticket] =
      Job::create([&runs](const std::stop_token&) { runs += 10; });
  first = std::move(second);
  first_ticket.run();
  second_ticket.run();
  EXPECT_EQ(runs, 10);
}

TEST(ManualExecutorTest, RunsJobsInOrderOnlyWhenStepped) {
  ManualExecutor executor;
  std::vector<int> order;
  Job first =
      executor.submit([&order](const std::stop_token&) { order.push_back(1); });
  Job second =
      executor.submit([&order](const std::stop_token&) { order.push_back(2); });
  EXPECT_TRUE(order.empty());
  EXPECT_EQ(executor.pending(), 2U);
  EXPECT_TRUE(executor.step());
  EXPECT_EQ(order, std::vector<int>{1});
  EXPECT_TRUE(executor.step());
  EXPECT_FALSE(executor.step());
  EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

TEST(ManualExecutorTest, SkipsDroppedJobsAndDrainsNewOnes) {
  ManualExecutor executor;
  std::vector<int> order;
  std::optional<Job> nested;
  {
    Job dropped = executor.submit(
        [&order](const std::stop_token&) { order.push_back(0); });
  }
  Job outer = executor.submit([&](const std::stop_token&) {
    order.push_back(1);
    nested.emplace(executor.submit(
        [&order](const std::stop_token&) { order.push_back(2); }));
  });
  EXPECT_EQ(executor.pending(), 1U);
  EXPECT_EQ(executor.drain(), 2U);
  EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

}
