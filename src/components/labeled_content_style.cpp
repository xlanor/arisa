#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/label_column.hpp>
#include <arisa/components/labeled_content_style.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>

#include <type_traits>
#include <utility>

namespace arisa::components {

const composition::EnvironmentKey<LabeledContentStyle> LabeledContentStyle::key{
    "labeled_content_style", LabeledContentStyle::automatic()};

auto LabeledContentStyle::automatic() noexcept -> LabeledContentStyle {
  return LabeledContentStyle{Kind::automatic};
}

auto operator==(const LabeledContentStyle& left,
                const LabeledContentStyle& right) -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  return left.kind_ != LabeledContentStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto LabeledContentStyle::make_body(Configuration configuration) const
    -> composition::Element {
  if (kind_ == Kind::custom) {
    return custom_->make_body(std::move(configuration));
  }
  if (Group::is_empty(configuration.label)) {
    return std::move(configuration.content);
  }
  composition::Element row =
      HStack(HStackOptions{.alignment = VerticalAlignment::center,
                           .spacing = theme::space::md},
             std::move(configuration.label), Spacer(),
             std::move(configuration.content));
  return std::move(row).frame({.max_width = composition::infinity});
}

auto LabeledContentStyle::columned(Configuration configuration,
                                   const LabelColumn& column)
    -> composition::Element {
  composition::Element label =
      HStack(
          HStackOptions{.alignment = VerticalAlignment::center,
                        .spacing = theme::space::none},
          std::move(configuration.label)
              .fixed_size(true, false)
              .on_geometry_change(
                  std::type_identity<float>{},
                  [](const composition::GeometryProxy& proxy) {
                    return proxy.size().width;
                  },
                  [column](const float& natural) { column.report(natural); }))
          .frame({.min_width = column.width(),
                  .alignment = composition::Alignment::trailing});
  return HStack(HStackOptions{.alignment = VerticalAlignment::center,
                              .spacing = theme::Space{LabelColumn::gap}},
                std::move(label),
                HStack(HStackOptions{.alignment = VerticalAlignment::center,
                                     .spacing = theme::space::none},
                       std::move(configuration.content))
                    .frame({.max_width = composition::infinity,
                            .alignment = composition::Alignment::leading}))
      .frame({.max_width = composition::infinity});
}

}
