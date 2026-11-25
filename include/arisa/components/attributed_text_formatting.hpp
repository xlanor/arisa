#pragma once

#include <arisa/components/attributed_string.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace arisa::components {

class AttributedTextFormattingDefinition;

class AttributedTextFormatting {
 public:
  class AttributeContainerProxy {
   public:
    explicit AttributeContainerProxy(AttributedString::Attributes& attributes)
        : attributes_(attributes) {}

    template <class Attribute>
    auto value() const -> const typename Attribute::value_type& {
      return Attribute::of(attributes_.get());
    }

    template <class Attribute>
    void set(typename Attribute::value_type value) {
      Attribute::of(attributes_.get()) = std::move(value);
    }

    auto attributes() const noexcept -> const AttributedString::Attributes& {
      return attributes_.get();
    }

   private:
    std::reference_wrapper<AttributedString::Attributes> attributes_;
  };

  template <class Attribute>
  class ValueConstraint;
  class EmptyDefinition;
  template <class... Definitions>
  class TupleDefinition;
  class AnyDefinition;
  class DefinitionBuilder;
  class Transferable;

  struct Weight
      : AttributedString::Field<std::optional<theme::Font::Weight>,
                                &AttributedString::Attributes::weight> {};

  struct Italic
      : AttributedString::Field<bool, &AttributedString::Attributes::italic> {};

  struct Underline
      : AttributedString::Field<bool,
                                &AttributedString::Attributes::underline> {};

  struct Strikethrough
      : AttributedString::Field<bool,
                                &AttributedString::Attributes::strikethrough> {
  };

  static const composition::EnvironmentKey<
      std::shared_ptr<const AttributedTextFormattingDefinition>>
      definition;

  static auto constrained(const AttributedString& text,
                          const AttributedTextFormattingDefinition& rules)
      -> AttributedString;
};

class AttributedTextFormattingDefinition {
 public:
  AttributedTextFormattingDefinition() = default;
  AttributedTextFormattingDefinition(
      const AttributedTextFormattingDefinition&) = default;
  auto operator=(const AttributedTextFormattingDefinition&)
      -> AttributedTextFormattingDefinition& = default;
  AttributedTextFormattingDefinition(AttributedTextFormattingDefinition&&) =
      default;
  auto operator=(AttributedTextFormattingDefinition&&)
      -> AttributedTextFormattingDefinition& = default;
  virtual ~AttributedTextFormattingDefinition() = default;

  virtual void constrain(
      AttributedTextFormatting::AttributeContainerProxy& container) const = 0;
};

template <class Attribute>
class AttributedTextFormatting::ValueConstraint final
    : public AttributedTextFormattingDefinition {
 public:
  using Value = typename Attribute::value_type;

  ValueConstraint(std::vector<Value> values, Value default_value)
      : values_(std::move(values)), default_(std::move(default_value)) {}

  void constrain(AttributeContainerProxy& container) const override {
    if (!std::ranges::contains(values_, container.value<Attribute>())) {
      container.set<Attribute>(default_);
    }
  }

  friend auto operator==(const ValueConstraint& left,
                         const ValueConstraint& right) -> bool {
    return left.values_ == right.values_ && left.default_ == right.default_;
  }

 private:
  std::vector<Value> values_;
  Value default_;
};

class AttributedTextFormatting::EmptyDefinition final
    : public AttributedTextFormattingDefinition {
 public:
  EmptyDefinition() = default;

  void constrain(AttributeContainerProxy&) const override {}
};

template <class... Definitions>
class AttributedTextFormatting::TupleDefinition final
    : public AttributedTextFormattingDefinition {
 public:
  explicit TupleDefinition(Definitions... parts)
      : definitions_(std::move(parts)...) {}

  void constrain(AttributeContainerProxy& container) const override {
    std::apply(
        [&container](const auto&... each) { (each.constrain(container), ...); },
        definitions_);
  }

 private:
  std::tuple<Definitions...> definitions_;
};

class AttributedTextFormatting::AnyDefinition final
    : public AttributedTextFormattingDefinition {
 public:
  template <class Definition>
    requires std::derived_from<Definition,
                               AttributedTextFormattingDefinition> &&
             (!std::same_as<Definition, AnyDefinition>)
  explicit AnyDefinition(Definition wrapped)
      : held_(std::make_shared<const Definition>(std::move(wrapped))) {}

  void constrain(AttributeContainerProxy& container) const override {
    held_->constrain(container);
  }

  auto held() const noexcept
      -> const std::shared_ptr<const AttributedTextFormattingDefinition>& {
    return held_;
  }

 private:
  std::shared_ptr<const AttributedTextFormattingDefinition> held_;
};

class AttributedTextFormatting::DefinitionBuilder {
 public:
  static auto build_block() -> EmptyDefinition { return EmptyDefinition{}; }

  template <class... Definitions>
    requires(sizeof...(Definitions) > 0)
  static auto build_block(Definitions... definitions)
      -> TupleDefinition<Definitions...> {
    return TupleDefinition<Definitions...>{std::move(definitions)...};
  }
};

class AttributedTextFormatting::Transferable {
 public:
  Transferable(
      AttributedString text,
      std::shared_ptr<const AttributedTextFormattingDefinition> rules = nullptr)
      : text_(std::move(text)), definition_(std::move(rules)) {}

  auto resolved() const -> AttributedString;

  auto encoded() const -> std::string;

  static auto decoded(std::string_view text) -> std::optional<Transferable>;

  static auto content_type() -> std::string;

  static auto transfer_representation() -> std::string {
    return content_type();
  }

  friend auto operator==(const Transferable& left, const Transferable& right)
      -> bool {
    return left.text_ == right.text_ && left.definition_ == right.definition_;
  }

 private:
  AttributedString text_;
  std::shared_ptr<const AttributedTextFormattingDefinition> definition_;
};

}

namespace arisa::composition {

template <class Definition>
auto Element::attributed_text_formatting_definition(
    Definition definition) && -> Element {
  return std::move(*this).environment(
      components::AttributedTextFormatting::definition,
      std::shared_ptr<const components::AttributedTextFormattingDefinition>{
          std::make_shared<const Definition>(std::move(definition))});
}

}
