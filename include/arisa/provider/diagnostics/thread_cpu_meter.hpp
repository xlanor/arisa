#pragma once

#include <arisa/support/unique_handle.hpp>

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::provider {

struct ThreadCpu {
  std::uint64_t id{0};
  std::string name;
  std::chrono::nanoseconds cpu{0};
};

class ThreadCpuMeter {
 public:
  struct EnrolmentTraits {
    using handle_type = std::uint64_t;

    static auto empty() noexcept -> handle_type { return 0; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle != 0;
    }

    static void release(handle_type& handle) noexcept {
      ThreadCpuMeter::withdraw(handle);
    }
  };

  using Enrolment = support::UniqueHandle<EnrolmentTraits>;

  static auto enrol(std::string_view name) -> Enrolment;

  static auto sample() -> std::vector<ThreadCpu>;

  static auto measuring() noexcept -> bool;

  static auto since(const std::vector<ThreadCpu>& before,
                    const std::vector<ThreadCpu>& after)
      -> std::vector<ThreadCpu>;

 private:
  struct Entry {
    std::uint64_t id{0};
    std::string name;
    std::uint64_t native{0};
  };

  struct Registry {
    std::mutex mutex;
    std::vector<Entry> entries;
    std::uint64_t next{1};
  };

  static auto registry() -> Registry&;

  static void withdraw(std::uint64_t id) noexcept;

  static auto current_native() noexcept -> std::uint64_t;

  static auto read(std::uint64_t native) noexcept -> std::chrono::nanoseconds;
};

}
