#pragma once

#include <arisa/composition/proposed_view_size.hpp>
#include <arisa/composition/size.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class PresentationSizingContext {
 public:
  PresentationSizingContext() = default;

  explicit PresentationSizingContext(Size container) noexcept
      : container_(container) {}

  auto container() const noexcept -> Size { return container_; }

 private:
  Size container_{};
};

class PresentationSizingRoot {
 public:
  using Measure = std::function<Size(ProposedViewSize)>;

  explicit PresentationSizingRoot(Measure measure) noexcept
      : measure_(std::move(measure)) {}

  auto size_that_fits(ProposedViewSize proposal) const -> Size {
    return measure_ ? measure_(proposal) : Size{};
  }

 private:
  Measure measure_;
};

template <class S>
concept CustomPresentationSizing =
    requires(const S& sizing, const PresentationSizingRoot& root,
             const PresentationSizingContext& context) {
      {
        sizing.proposed_size(root, context)
      } -> std::convertible_to<ProposedViewSize>;
    };

class PresentationSizing {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    page,
    form,
    fitted,
    custom,
  };

  using Custom = std::function<ProposedViewSize(
      const PresentationSizingRoot&, const PresentationSizingContext&)>;

  static constexpr float page_width = 0.62F;
  static constexpr float page_height = 0.88F;
  static constexpr float form_width = 0.5F;
  static constexpr float form_height = 0.72F;

  static const PresentationSizing automatic;
  static const PresentationSizing page;
  static const PresentationSizing form;
  static const PresentationSizing fitted;

  template <CustomPresentationSizing S>
    requires(!std::same_as<std::remove_cvref_t<S>, PresentationSizing>)
  PresentationSizing(S sizing)
      : kind_(Kind::custom),
        custom_(std::make_shared<const Custom>(
            [held = std::move(sizing)](
                const PresentationSizingRoot& root,
                const PresentationSizingContext& context) {
              return ProposedViewSize{held.proposed_size(root, context)};
            })) {}

  auto fitting(bool horizontal, bool vertical) const -> PresentationSizing {
    PresentationSizing made = *this;
    made.fits_width_ = horizontal;
    made.fits_height_ = vertical;
    return made;
  }

  auto sticky(bool horizontal = false, bool vertical = false) const
      -> PresentationSizing {
    PresentationSizing made = *this;
    made.sticky_width_ = horizontal;
    made.sticky_height_ = vertical;
    return made;
  }

  auto proposed_size(const PresentationSizingRoot& root,
                     const PresentationSizingContext& context) const
      -> ProposedViewSize;

  auto kind() const noexcept -> Kind { return kind_; }

  auto sticky_width() const noexcept -> bool { return sticky_width_; }

  auto sticky_height() const noexcept -> bool { return sticky_height_; }

  auto plain() const noexcept -> bool {
    return kind_ != Kind::custom && !fits_width_ && !fits_height_ &&
           !sticky_width_ && !sticky_height_;
  }

  friend auto operator==(const PresentationSizing&, const PresentationSizing&)
      -> bool = default;

 private:
  explicit PresentationSizing(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  bool fits_width_{false};
  bool fits_height_{false};
  bool sticky_width_{false};
  bool sticky_height_{false};
  std::shared_ptr<const Custom> custom_;
};

}
