#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <gallery_profile.hpp>

#include <string_view>

namespace arisa::examples {

class ProfileSummary final : public composition::Component {
 public:
  static constexpr std::string_view summary_text = "profile-summary";

  explicit ProfileSummary(
      composition::Observable<GalleryProfile>& model) noexcept
      : model_(&model) {}

  auto operator==(const ProfileSummary&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  composition::Observable<GalleryProfile>* model_;
};

}
