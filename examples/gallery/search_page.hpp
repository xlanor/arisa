#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::examples {

class SearchPage final : public composition::Component {
 public:
  enum class Genre : std::uint8_t {
    action,
    racing,
    puzzle,
    role_playing,
    platformer,
  };

  enum class Shelf : std::uint8_t {
    all,
    installed,
    archived,
  };

  struct Title {
    int index{0};
    std::string name;
    std::string folded;
    Genre genre{Genre::action};
    bool installed{false};
    bool archived{false};
  };

  static constexpr int title_count = 2000;
  static constexpr float cover_minimum = 150.0F;
  static constexpr std::string_view close_button = "search-close";
  static constexpr std::string_view results_view = "search-results";
  static constexpr std::string_view done_button = "search-done";
  static constexpr composition::StateKey<std::string> query_key{"search-query",
                                                                std::string{}};
  static constexpr composition::StateKey<Shelf> shelf_key{"search-shelf",
                                                          Shelf::all};
  static constexpr composition::StateKey<int> submits_key{"search-submits", 0};
  static const composition::StateKey<std::vector<Genre>> tokens_key;
  static const composition::StateKey<std::vector<Genre>> offered_key;

  explicit SearchPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const SearchPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto titles() -> const std::vector<Title>&;

  static auto matches(const std::string& query,
                      const std::vector<Genre>& genres, Shelf shelf)
      -> std::vector<int>;

  static auto genre_name(Genre genre) -> std::string_view;

 private:
  class Summary;

  static auto fold(std::string_view text) -> std::string;

  static auto cover(const Title& title) -> composition::Element;

  static auto results(const std::vector<int>& found, const std::string& query)
      -> composition::Element;

  static auto suggestions(const std::string& query) -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class SearchPage::Summary final : public composition::Component {
 public:
  explicit Summary(std::size_t count) noexcept : count_(count) {}

  auto operator==(const Summary&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::size_t count_;
};

}
