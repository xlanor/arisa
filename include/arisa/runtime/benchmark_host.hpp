#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_error.hpp>
#include <arisa/runtime/introspection.hpp>
#include <arisa/runtime/root.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <functional>
#include <optional>

namespace arisa::runtime {

class BenchmarkHost {
 public:
  struct Measurement {
    std::size_t frames{0};
    std::size_t rendered{0};
    std::chrono::nanoseconds total{0};
    std::chrono::nanoseconds worst{0};

    auto mean() const noexcept -> std::chrono::nanoseconds {
      return frames == 0 ? std::chrono::nanoseconds{0}
                         : total / static_cast<std::int64_t>(frames);
    }
  };

  static constexpr std::chrono::nanoseconds nominal_frame{16'666'667};
  static constexpr std::size_t settle_limit = 240;

  explicit BenchmarkHost(Application& application) noexcept
      : application_(application) {}

  [[nodiscard]] auto measure_render(std::chrono::nanoseconds interval)
      -> std::expected<Measurement, ApplicationError>;

  [[nodiscard]] auto measure_renders(std::chrono::nanoseconds duration)
      -> std::expected<Measurement, ApplicationError>;

  [[nodiscard]] auto measure_renders(double seconds)
      -> std::expected<Measurement, ApplicationError>;

  [[nodiscard]] auto measure_action(std::move_only_function<void()> action)
      -> std::expected<Measurement, ApplicationError>;

  auto view_for_identifier(const composition::ElementKey& key) const
      -> std::optional<Introspection::NodeRecord>;

  template <class T>
  auto state_for_identifier(const composition::ElementKey& key,
                            const composition::StateKey<T>& state) const
      -> std::optional<T> {
    const composition::Tree& tree = application_.get().root().tree();
    const auto id = tree.find(key);
    if (!id.has_value()) {
      return std::nullopt;
    }
    const auto node = tree.node(*id);
    if (!node.has_value()) {
      return std::nullopt;
    }
    const auto held = node->get().stored(state);
    if (!held.has_value()) {
      return std::nullopt;
    }
    return held->get();
  }

 private:
  auto run(std::size_t frames, bool until_idle)
      -> std::expected<Measurement, ApplicationError>;

  std::reference_wrapper<Application> application_;
};

}
