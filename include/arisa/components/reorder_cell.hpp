#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <cstddef>
#include <memory>
#include <vector>

namespace arisa::components {

class ReorderCell : public composition::Element {
 public:
  class Content;
  class Source;

  struct Lanes {
    composition::Axis axis{composition::Axis::vertical};
    std::size_t count{1};

    friend constexpr auto operator==(const Lanes&, const Lanes&)
        -> bool = default;
  };

  static const composition::EnvironmentKey<Lanes> lanes;

  static constexpr float lifted_scale = 1.06F;

  ReorderCell(composition::Element content, std::size_t index,
              std::size_t count, ForEach::MoveAction move);

  static auto wrap(std::shared_ptr<const ForEach::Source> source,
                   ForEach::MoveAction move)
      -> std::shared_ptr<const ForEach::Source>;
};

class ReorderCell::Source final : public ForEach::Source {
 public:
  Source(std::shared_ptr<const ForEach::Source> inner,
         ForEach::MoveAction move) noexcept
      : inner_(std::move(inner)), move_(std::move(move)) {}

  auto count() const noexcept -> std::size_t override {
    return inner_->count();
  }

  auto key(std::size_t index) const -> composition::ElementKey override {
    return inner_->key(index);
  }

  auto build(std::size_t index) const -> composition::Element override;

  auto same_item(std::size_t index, const ForEach::Source& other,
                 std::size_t other_index) const -> bool override;

  auto tag() const noexcept -> const void* override { return &marker; }

 private:
  static constexpr char marker{};

  std::shared_ptr<const ForEach::Source> inner_;
  ForEach::MoveAction move_;
};

class ReorderCell::Content final : public composition::ElementContent {
 public:
  using Holder = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> lifted{"reorder-lifted", false};
  static constexpr composition::StateKey<composition::Size> dragged{
      "reorder-dragged", composition::Size{}};
  static constexpr composition::StateKey<composition::Size> extent{
      "reorder-extent", composition::Size{.width = 1.0F, .height = 1.0F}};

  Content(Holder content, std::size_t index, std::size_t count,
          ForEach::MoveAction move) noexcept
      : content_(std::move(content)),
        index_(index),
        count_(count),
        move_(std::move(move)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  static void shift(const ForEach::MoveAction& move, std::size_t from,
                    long steps, std::size_t count);

  Holder content_;
  std::size_t index_;
  std::size_t count_;
  ForEach::MoveAction move_;
};

}
