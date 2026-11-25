#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/modifiers.hpp>
#include <arisa/inspector/cost_panel.hpp>
#include <arisa/inspector/inspector_style.hpp>
#include <arisa/theme/typography.hpp>

#include <format>
#include <utility>
#include <vector>

namespace arisa::inspector {

auto CostPanel::cells(std::string name, std::array<std::string, 5> counts)
    -> composition::Element {
  std::vector<composition::Element> row;
  row.push_back(composition::Element{
      components::Text(std::move(name)).font(theme::Font::footnote)}
                    .frame({.width = name_width,
                            .alignment = composition::Alignment::leading}));
  for (std::string& count : counts) {
    row.push_back(composition::Element{
        components::Text(std::move(count)).font(theme::Font::footnote)}
                      .frame({.width = count_width,
                              .alignment = composition::Alignment::trailing}));
  }
  return composition::Element{
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::top,
              .spacing = theme::space::xs},
          std::move(row))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto CostPanel::body(composition::Context& context) const
    -> composition::Element {
  static_cast<void>(context.read(model_->state()));
  static_cast<void>(context.read(model_->frames()));
  std::shared_ptr<InspectorModel> model = model_;
  const InspectorModel::CostSpan span = model_->cost_span();
  std::vector<composition::Element> rows;
  rows.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::space::xs},
      InspectorStyle::tab(
          "Window", span == InspectorModel::CostSpan::window,
          [model] { model->set_cost_span(InspectorModel::CostSpan::window); }),
      InspectorStyle::tab(
          "Frame", span == InspectorModel::CostSpan::frame,
          [model] { model->set_cost_span(InspectorModel::CostSpan::frame); })));
  rows.push_back(cells("Node", {"Eval", "Layout", "Record", "Shape", "Total"}));
  for (const CostLedger::Ranked& entry : model_->ranked()) {
    rows.push_back(cells(model_->describe(entry.id),
                         {std::format("{}", entry.cost.evaluations),
                          std::format("{}", entry.cost.layouts),
                          std::format("{}", entry.cost.records),
                          std::format("{}", entry.cost.shaping),
                          std::format("{}", entry.cost.total())}));
  }
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          std::move(rows))}
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::top_leading});
}

}
