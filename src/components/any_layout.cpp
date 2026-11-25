#include <arisa/components/any_layout.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

class AnyLayout::Container : public composition::Element {
 public:
  Container(std::shared_ptr<const Box> box,
            std::vector<composition::Element> children)
      : Element(std::make_unique<Content>(std::move(box)),
                std::move(children)) {}
};

auto AnyLayout::operator()(std::vector<composition::Element> children) const
    -> composition::Element {
  return std::visit(
      [&children]<class Held>(const Held& held) -> composition::Element {
        if constexpr (std::same_as<Held, std::shared_ptr<const Box>>) {
          return Container(held, std::move(children));
        } else {
          return held(std::move(children));
        }
      },
      layout_);
}

auto operator==(const AnyLayout& left, const AnyLayout& right) -> bool {
  if (left.layout_.index() != right.layout_.index()) {
    return false;
  }
  return std::visit(
      [&right]<class Held>(const Held& held) -> bool {
        const Held& other = std::get<Held>(right.layout_);
        if constexpr (std::same_as<Held,
                                   std::shared_ptr<const AnyLayout::Box>>) {
          return held == other || held->equals(*other);
        } else {
          return held == other;
        }
      },
      left.layout_);
}

auto AnyLayout::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto AnyLayout::Content::kind() const noexcept -> composition::ElementType {
  return composition::ElementType::of<AnyLayout>();
}

auto AnyLayout::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return box_ == theirs.box_ || box_->equals(*theirs.box_);
}

}
