#include <arisa/conformance/threading_suite.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace arisa::conformance {

using provider::ProviderErrorCode;
using provider::ThreadRole;
using provider::ThreadSpec;

void ThreadingSuite::run(provider::ProviderFactory& factory, Report& report) {
  provider::ThreadingProvider& threading = factory.threading();
  check_runs(threading, report);
  check_destruction_stops_and_joins(threading, report);
  check_self_destruction(threading, report);
  check_stop_token(threading, report);
  check_concurrency(threading, report);
  check_empty_name(threading, report);
  check_thread_identity(threading, report);
  check_placement_round_trip(threading, report);
}

void ThreadingSuite::check_runs(provider::ThreadingProvider& threading,
                                Report& report) {
  static constexpr std::string_view check = "threading.runs";
  std::atomic<bool> ran{false};
  const auto thread = threading.start(
      ThreadSpec{.name = "arisa-runs", .role = ThreadRole::background},
      [&ran](const std::stop_token&) { ran.store(true); });
  if (!started(thread, check, report)) {
    return;
  }
  report.record(std::string{check}, wait_for(ran),
                "the body did not run before the deadline");
}

void ThreadingSuite::check_destruction_stops_and_joins(
    provider::ThreadingProvider& threading, Report& report) {
  static constexpr std::string_view check =
      "threading.destruction_stops_and_joins";
  std::atomic<bool> exited{false};
  {
    const auto thread = threading.start(
        ThreadSpec{.name = "arisa-join", .role = ThreadRole::background},
        [&exited](const std::stop_token& token) {
          idle_until_stopped(token);
          exited.store(true);
        });
    if (!started(thread, check, report)) {
      return;
    }
  }
  report.record(std::string{check}, exited.load(),
                "the handle was destroyed before the body returned");
}

void ThreadingSuite::check_self_destruction(
    provider::ThreadingProvider& threading, Report& report) {
  static constexpr std::string_view check = "threading.self_destruction";
  std::optional<provider::Thread> handle;
  std::atomic<bool> handed_over{false};
  std::atomic<bool> destroyed{false};
  auto thread = threading.start(
      ThreadSpec{.name = "arisa-self", .role = ThreadRole::background},
      [&handle, &handed_over, &destroyed](const std::stop_token&) {
        if (!wait_for(handed_over)) {
          return;
        }
        handle.reset();
        destroyed.store(true);
      });
  if (!started(thread, check, report)) {
    return;
  }
  handle.emplace(std::move(*thread));
  handed_over.store(true);
  report.record(std::string{check}, wait_for(destroyed),
                "destroying a thread's handle from its own body did not "
                "return");
}

void ThreadingSuite::check_stop_token(provider::ThreadingProvider& threading,
                                      Report& report) {
  static constexpr std::string_view check = "threading.stop_token_observed";
  std::atomic<bool> exited{false};
  auto thread = threading.start(
      ThreadSpec{.name = "arisa-stop", .role = ThreadRole::background},
      [&exited](const std::stop_token& token) {
        idle_until_stopped(token);
        exited.store(true);
      });
  if (!started(thread, check, report)) {
    return;
  }
  thread->request_stop();
  report.record(std::string{check}, wait_for(exited),
                "request_stop did not reach the body's stop_token");
}

void ThreadingSuite::check_concurrency(provider::ThreadingProvider& threading,
                                       Report& report) {
  static constexpr std::string_view check = "threading.concurrent";
  std::atomic<bool> first_arrived{false};
  std::atomic<bool> second_arrived{false};
  std::atomic<bool> first_met{false};
  std::atomic<bool> second_met{false};
  {
    const auto first = threading.start(
        ThreadSpec{.name = "arisa-first", .role = ThreadRole::background},
        [&](const std::stop_token&) {
          first_arrived.store(true);
          first_met.store(wait_for(second_arrived));
        });
    const auto second = threading.start(
        ThreadSpec{.name = "arisa-second", .role = ThreadRole::background},
        [&](const std::stop_token&) {
          second_arrived.store(true);
          second_met.store(wait_for(first_arrived));
        });
    if (!started(first, check, report) || !started(second, check, report)) {
      return;
    }
  }
  report.record(std::string{check}, first_met.load() && second_met.load(),
                "two threads could not run at the same time");
}

void ThreadingSuite::check_empty_name(provider::ThreadingProvider& threading,
                                      Report& report) {
  const auto thread =
      threading.start(ThreadSpec{.name = "", .role = ThreadRole::background},
                      [](const std::stop_token&) {});
  report.record("threading.empty_name_rejected",
                !thread.has_value() && thread.error().code() ==
                                           ProviderErrorCode::invalid_argument,
                "an empty thread name was not rejected with invalid_argument");
}

void ThreadingSuite::check_thread_identity(
    provider::ThreadingProvider& threading, Report& report) {
  static constexpr std::string_view check = "threading.identity";
  std::atomic<std::uint64_t> first_seen{0};
  std::atomic<std::uint64_t> second_seen{0};
  std::uint64_t first_id = 0;
  std::uint64_t second_id = 0;
  {
    const auto first = threading.start(
        ThreadSpec{.name = "arisa-id-a", .role = ThreadRole::background},
        [&threading, &first_seen](const std::stop_token&) {
          first_seen.store(threading.current_thread().value());
        });
    const auto second = threading.start(
        ThreadSpec{.name = "arisa-id-b", .role = ThreadRole::blocking},
        [&threading, &second_seen](const std::stop_token&) {
          second_seen.store(threading.current_thread().value());
        });
    if (!started(first, check, report) || !started(second, check, report)) {
      return;
    }
    first_id = first->id().value();
    second_id = second->id().value();
  }
  const std::uint64_t caller = threading.current_thread().value();
  const bool distinct = first_seen.load() != caller &&
                        second_seen.load() != caller &&
                        first_seen.load() != second_seen.load();
  const bool matches_handles =
      first_seen.load() == first_id && second_seen.load() == second_id;
  report.record(std::string{check}, distinct && matches_handles,
                "current_thread did not identify each thread uniquely");
}

void ThreadingSuite::check_placement_round_trip(
    provider::ThreadingProvider& threading, Report& report) {
  static constexpr std::string_view check = "threading.placement_round_trip";
  const provider::ThreadPlacementPolicy& policy = threading.placement();
  const auto custom = provider::ThreadPlacement::create(
      provider::CoreMask{0b0010}, provider::ThreadPriority::of<40>(),
      provider::StackSize{0x10000});
  if (!custom.has_value()) {
    report.record(std::string{check}, false, "a valid placement was rejected");
    return;
  }
  const provider::ThreadPlacementPolicy updated =
      policy.with(ThreadRole::blocking, *custom);
  bool others_kept = true;
  for (const ThreadRole role : provider::all_thread_roles) {
    if (role != ThreadRole::blocking &&
        updated.placement(role) != policy.placement(role)) {
      others_kept = false;
    }
  }
  report.record(
      std::string{check},
      updated.placement(ThreadRole::blocking) == *custom && others_kept,
      "with() did not replace exactly one role's placement");
}

auto ThreadingSuite::started(
    const std::expected<provider::Thread, provider::ProviderError>& thread,
    std::string_view check, Report& report) -> bool {
  if (!thread.has_value()) {
    report.record(std::string{check}, false,
                  std::string{"start failed: "} +
                      std::string{thread.error().description()});
  }
  return thread.has_value();
}

auto ThreadingSuite::wait_for(const std::atomic<bool>& flag) -> bool {
  static constexpr auto deadline = std::chrono::seconds{5};
  static constexpr auto tick = std::chrono::milliseconds{1};
  const auto start = std::chrono::steady_clock::now();
  while (!flag.load()) {
    if (std::chrono::steady_clock::now() - start > deadline) {
      return false;
    }
    std::this_thread::sleep_for(tick);
  }
  return true;
}

void ThreadingSuite::idle_until_stopped(const std::stop_token& token) {
  static constexpr auto tick = std::chrono::milliseconds{1};
  while (!token.stop_requested()) {
    std::this_thread::sleep_for(tick);
  }
}

}
