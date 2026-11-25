#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/composition/text_selection.hpp>
#include <arisa/composition/text_selection_affinity.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::components {

class AttributedTextSelection {
 public:
  using Range = composition::TextSelection::Range;
  using InsertionPoint = composition::TextSelection::InsertionPoint;

  class Indices {
   public:
    static auto insertion_point(std::size_t index) -> Indices {
      return Indices{std::vector<Range>{Range{.lower = index, .upper = index}},
                     true};
    }

    static auto ranges(std::vector<Range> ranges) -> Indices;

    auto is_insertion_point() const noexcept -> bool { return insertion_; }

    auto insertion() const noexcept -> std::size_t {
      return ranges_.empty() ? 0 : ranges_.front().lower;
    }

    auto ranges() const noexcept -> std::span<const Range> { return ranges_; }

    auto range() const noexcept -> Range;

    friend auto operator==(const Indices&, const Indices&) -> bool = default;

   private:
    Indices(std::vector<Range> ranges, bool insertion) noexcept
        : ranges_(std::move(ranges)), insertion_(insertion) {}

    std::vector<Range> ranges_;
    bool insertion_{true};
  };

  class Attributes {
   public:
    explicit Attributes(std::vector<AttributedString::Attributes> runs)
        : runs_(std::move(runs)) {}

    auto begin() const noexcept { return runs_.begin(); }

    auto end() const noexcept { return runs_.end(); }

    auto size() const noexcept -> std::size_t { return runs_.size(); }

    template <class Attribute>
    auto values() const -> std::vector<typename Attribute::value_type> {
      std::vector<typename Attribute::value_type> found;
      found.reserve(runs_.size());
      for (const AttributedString::Attributes& run : runs_) {
        found.push_back(Attribute::of(run));
      }
      return found;
    }

    template <class Attribute>
    auto operator[](std::type_identity<Attribute>) const
        -> std::vector<typename Attribute::value_type> {
      return values<Attribute>();
    }

   private:
    std::vector<AttributedString::Attributes> runs_;
  };

  AttributedTextSelection() : indices_(Indices::insertion_point(0)) {}

  explicit AttributedTextSelection(
      InsertionPoint point,
      std::optional<AttributedString::Attributes> typing_attributes = {})
      : indices_(Indices::insertion_point(point.index)),
        typing_(std::move(typing_attributes)) {}

  explicit AttributedTextSelection(std::vector<Range> ranges)
      : indices_(Indices::ranges(std::move(ranges))) {}

  explicit AttributedTextSelection(Range range)
      : AttributedTextSelection(std::vector<Range>{range}) {}

  static auto of(const composition::TextSelection& selection,
                 std::optional<AttributedString::Attributes> typing = {})
      -> AttributedTextSelection;

  auto indices(const AttributedString& text) const -> Indices;

  auto affinity(const AttributedString& text) const
      -> composition::TextSelectionAffinity;

  auto typing_attributes(const AttributedString& text) const
      -> AttributedString::Attributes;

  auto attributes(const AttributedString& text) const -> Attributes;

  auto text_selection(const AttributedString& text) const
      -> composition::TextSelection;

  auto with_typing_attributes(AttributedString::Attributes attributes) const
      -> AttributedTextSelection;

  friend auto operator==(const AttributedTextSelection&,
                         const AttributedTextSelection&) -> bool = default;

 private:
  Indices indices_;
  std::optional<AttributedString::Attributes> typing_;
  composition::TextSelectionAffinity affinity_{
      composition::TextSelectionAffinity::automatic};
};

}
