#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <gallery_profile.hpp>

#include <string_view>

namespace arisa::examples {

class ProfileForm final : public composition::Component {
 public:
  static constexpr std::string_view volume_slider = "profile-volume";

  explicit ProfileForm(composition::Observable<GalleryProfile>& model) noexcept
      : model_(&model) {}

  auto operator==(const ProfileForm&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto plan_name(GalleryProfile::Plan plan) -> std::string_view;

 private:
  composition::Observable<GalleryProfile>* model_;
};

}
