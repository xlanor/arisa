#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/scroll_view_reader.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/composition/scroll_view_proxy.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/inspector/tree_panel.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::inspector {

auto TreePanel::row_key(composition::NodeId node) -> std::string {
  return std::format("{}{}", row_prefix, node.value());
}

auto TreePanel::row(const std::shared_ptr<InspectorModel>& model,
                    const InspectorModel::Row& row,
                    const InspectorModel::Indentation& indentation,
                    bool selected) -> composition::Element {
  const composition::NodeId id = row.record.id;
  const float indent = indentation.indent(row.record.depth);
  const float label_width =
      std::max(indentation.content - indent - InspectorModel::disclosure_width -
                   InspectorModel::row_spacing,
               0.0F);
  composition::Element disclosure =
      row.expandable ? composition::Element{components::Button(
                                                row.expanded ? "-" : "+",
                                                [model, id] {
                                                  model->toggle_expanded(id);
                                                })}
                           .button_style(components::ButtonStyle::borderless())
                     : composition::Element{components::Text(std::string{})};
  composition::Element label{components::Button(
      [model, id] { model->select(id); },
      composition::Element{components::Text(InspectorModel::label(row.record))
                               .font(theme::Font::footnote)
                               .line_limit(1)}
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading}))};
  composition::Element line{components::HStack(
      components::HStackOptions{.spacing = InspectorModel::row_spacing},
      std::move(disclosure).frame({.width = InspectorModel::disclosure_width}),
      std::move(label)
          .button_style(components::ButtonStyle::plain())
          .frame({.width = label_width,
                  .alignment = composition::Alignment::leading})
          .id(row_key(id)),
      components::Spacer())};
  line = std::move(line)
             .padding(theme::Insets{.leading = indent})
             .frame({.max_width = composition::infinity,
                     .alignment = composition::Alignment::leading});
  if (selected) {
    line =
        std::move(line).background(theme::ColourRole::selected_text_background,
                                   theme::RoundedRectangle{6.0F});
  }
  return line;
}

auto TreePanel::details() const -> composition::Element {
  const auto selected = model_->selected();
  if (!selected.has_value()) {
    return components::Spacer();
  }
  const auto record = model_->introspection().node(*selected);
  if (!record.has_value()) {
    return components::Spacer();
  }
  std::vector<composition::Element> rows;
  rows.push_back(InspectorStyle::value("Component", record->name));
  rows.push_back(InspectorStyle::value(
      "Key", record->key.empty() ? std::string{"none"} : record->key));
  rows.push_back(
      InspectorStyle::value("Frame", InspectorStyle::rect(record->frame)));
  rows.push_back(
      InspectorStyle::value("Bounds", InspectorStyle::rect(record->bounds)));
  rows.push_back(InspectorStyle::value(
      "Padding", InspectorStyle::insets(record->padding)));
  rows.push_back(
      InspectorStyle::value("Flags", InspectorStyle::flags(record->flags)));
  rows.push_back(InspectorStyle::value(
      "Evaluations", std::format("{}", record->cost.evaluations)));
  rows.push_back(
      InspectorStyle::value("Layout", std::format("{}", record->cost.layouts)));
  rows.push_back(
      InspectorStyle::value("Record", std::format("{}", record->cost.records)));
  rows.push_back(InspectorStyle::value(
      "Shaping", std::format("{}", record->cost.shaping)));
  rows.push_back(
      InspectorStyle::value("Children", std::format("{}", record->children)));
  for (const std::string& note : record->notes) {
    rows.push_back(InspectorStyle::value("Unhonoured", note));
  }
  return InspectorStyle::section(InspectorModel::label(*record),
                                 std::move(rows));
}

auto TreePanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  static_cast<void>(context.read(model_->structure()));
  const std::vector<InspectorModel::Row> rows = model_->rows();
  const InspectorModel::Indentation indentation = model_->indentation(rows);
  const std::optional<composition::NodeId> selected = model_->selected();
  std::shared_ptr<InspectorModel> model = model_;
  const std::optional<std::int64_t> target =
      selected.transform([](composition::NodeId id) {
        return static_cast<std::int64_t>(id.value());
      });
  composition::Element list{components::ScrollViewReader(
      [model, selected, target, rows, indentation](
          composition::ScrollViewProxy proxy) -> composition::Element {
        return composition::Element{
            components::ScrollView(composition::Element{
                components::LazyVStack(
                    components::LazyVStackOptions{
                        .alignment = components::HorizontalAlignment::leading,
                        .spacing = theme::space::none,
                        .estimated_extent = 28.0F},
                    components::ForEach(
                        rows,
                        [](const InspectorModel::Row& each) {
                          return composition::ElementKey{
                              static_cast<std::int64_t>(
                                  each.record.id.value())};
                        },
                        [model, selected,
                         indentation](const InspectorModel::Row& each) {
                          return row(model, each, indentation,
                                     selected == each.record.id);
                        }))}.frame({.max_width = composition::infinity}))}
            .on_change(target, true, [proxy, target] {
              if (target.has_value()) {
                proxy.scroll_to(composition::ElementKey{*target},
                                motion::UnitPoint::center);
              }
            });
      })};
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          std::move(list).frame(
              {.height = list_height, .max_width = composition::infinity}),
          composition::Element{
              components::ScrollView(details().frame(
                  {.max_width = composition::infinity,
                   .alignment = composition::Alignment::top_leading}))}
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity}))}
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
