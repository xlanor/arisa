#pragma once

#include <arisa/async/task.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tag_key.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::structure {

class DetailPresenter {
 public:
  class Commit;
  class Page;
  class Recent;

  struct Request {
    composition::TagKey shown;
    bool following{false};
    bool delivered{true};
    std::string_view space{};
  };

  struct Slot {
    composition::TagKey key;
    bool frozen{false};
  };

  using Build = std::function<composition::Element(const Slot&)>;

  static constexpr std::size_t kept_pages = 3;
  static constexpr std::chrono::milliseconds default_delay{150};
  static const composition::EnvironmentKey<std::chrono::milliseconds> delay;

  DetailPresenter(composition::Context& context, Request request);

  auto displayed() const noexcept -> const composition::TagKey& {
    return displayed_;
  }

  auto commit() const -> Commit;

  auto stack(const Build& build) const -> composition::Element;

  auto settling(composition::Element element) const -> composition::Element;

 private:
  struct Entry {
    composition::TagKey key;
    std::int64_t id{0};
  };

  auto identify(std::int64_t id) const -> composition::ElementKey;

  static auto settle(
      composition::StateHandle<std::optional<composition::TagKey>> target,
      composition::TagKey key, std::chrono::milliseconds wait)
      -> async::Task<void>;

  static const composition::StateKey<std::optional<composition::TagKey>>
      presented;
  static const composition::StateKey<Recent> kept;

  composition::StateHandle<std::optional<composition::TagKey>> target_;
  composition::TagKey shown_;
  composition::TagKey displayed_;
  std::vector<Entry> alive_;
  std::int64_t generation_{0};
  bool settled_{true};
  std::chrono::milliseconds delay_;
  std::string_view space_;
};

class DetailPresenter::Commit {
 public:
  explicit Commit(
      composition::StateHandle<std::optional<composition::TagKey>> target)
      : target_(std::move(target)) {}

  void operator()(composition::TagKey key) const {
    target_.set(std::move(key));
  }

  friend auto operator==(const Commit& left, const Commit& right) -> bool {
    return left.target_.identity() == right.target_.identity();
  }

 private:
  composition::StateHandle<std::optional<composition::TagKey>> target_;
};

class DetailPresenter::Page : public composition::Element {
 public:
  Page(composition::Element content, bool hidden);
};

class DetailPresenter::Recent {
 public:
  auto contains(const composition::TagKey& key) const -> bool;

  auto front() const -> std::optional<composition::TagKey>;

  void claim(const composition::TagKey& key);

  void remember(const composition::TagKey& key);

  auto entries() const noexcept -> const std::vector<Entry>& {
    return entries_;
  }

  auto generation(const composition::TagKey& shown) -> std::int64_t;

 private:
  std::vector<Entry> entries_;
  std::int64_t next_{0};
  std::optional<composition::TagKey> last_shown_;
  std::int64_t generation_{0};
};

}

namespace arisa::composition {

template <class Delay>
auto Element::detail_settle_delay(Delay delay) && -> Element {
  return std::move(*this).environment(
      structure::DetailPresenter::delay,
      std::chrono::duration_cast<std::chrono::milliseconds>(delay));
}

}
