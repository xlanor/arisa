#pragma once

#include <arisa/conformance/report.hpp>
#include <arisa/provider/factory.hpp>

#include <atomic>
#include <expected>
#include <stop_token>
#include <string_view>

namespace arisa::conformance {

class ThreadingSuite {
 public:
  static void run(provider::ProviderFactory& factory, Report& report);

 private:
  static void check_runs(provider::ThreadingProvider& threading,
                         Report& report);
  static void check_destruction_stops_and_joins(
      provider::ThreadingProvider& threading, Report& report);
  static void check_self_destruction(provider::ThreadingProvider& threading,
                                     Report& report);
  static void check_stop_token(provider::ThreadingProvider& threading,
                               Report& report);
  static void check_concurrency(provider::ThreadingProvider& threading,
                                Report& report);
  static void check_empty_name(provider::ThreadingProvider& threading,
                               Report& report);
  static void check_thread_identity(provider::ThreadingProvider& threading,
                                    Report& report);
  static void check_placement_round_trip(provider::ThreadingProvider& threading,
                                         Report& report);

  static auto started(
      const std::expected<provider::Thread, provider::ProviderError>& thread,
      std::string_view check, Report& report) -> bool;
  static auto wait_for(const std::atomic<bool>& flag) -> bool;
  static void idle_until_stopped(const std::stop_token& token);
};

}
