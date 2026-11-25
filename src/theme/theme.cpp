#include <arisa/theme/colour_mix.hpp>
#include <arisa/theme/dynamic_type.hpp>
#include <arisa/theme/theme.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <utility>

namespace arisa::theme {

Theme::Theme(Seed seed, SemanticPalette::Variants variants) noexcept
    : seed_(seed), tokens_(std::in_place) {
  Tokens& tokens = tokens_.edit();
  tokens.variants = variants;
  tokens.palette = adapt(variants);
  tokens.typography = default_typography();
  tokens.spacing = default_spacing();
  tokens.elevations = default_elevations();
  tokens.metrics = default_metrics();
  tokens.materials = default_materials();
  tokens.liquid_glass = default_liquid_glass();
}

auto Theme::defaults() -> Theme { return from(Seed{}); }

auto Theme::from(Seed seed) -> Theme {
  return Theme{seed, SemanticPalette::derive(seed.accent)};
}

auto Theme::adapt(const SemanticPalette::Variants& variants) noexcept
    -> Palette {
  Palette palette{};
  std::ranges::transform(
      variants, palette.begin(),
      [](const AppearanceVariants& each) { return each.adaptive(); });
  return palette;
}

auto Theme::default_typography() noexcept -> Typography {
  Typography typography{};
  const auto set = [&typography](TextRole role, TypeStyle style) {
    typography[std::to_underlying(role)] = style;
  };
  set(TextRole::large_title, TypeStyle{.size = 26.0F,
                                       .weight = 400,
                                       .design = TypeDesign::display,
                                       .tracking = 0.0F,
                                       .line_height = 31.0F});
  set(TextRole::title, TypeStyle{.size = 22.0F,
                                 .weight = 400,
                                 .design = TypeDesign::display,
                                 .tracking = 0.0F,
                                 .line_height = 26.0F});
  set(TextRole::title2, TypeStyle{.size = 17.0F,
                                  .weight = 400,
                                  .design = TypeDesign::text,
                                  .tracking = 0.0F,
                                  .line_height = 21.0F});
  set(TextRole::title3, TypeStyle{.size = 15.0F,
                                  .weight = 400,
                                  .design = TypeDesign::text,
                                  .tracking = 0.0F,
                                  .line_height = 19.0F});
  set(TextRole::headline, TypeStyle{.size = 13.0F,
                                    .weight = 700,
                                    .design = TypeDesign::text,
                                    .tracking = 0.0F,
                                    .line_height = 16.0F});
  set(TextRole::subheadline, TypeStyle{.size = 11.0F,
                                       .weight = 400,
                                       .design = TypeDesign::text,
                                       .tracking = 0.0F,
                                       .line_height = 14.0F});
  set(TextRole::body, TypeStyle{.size = 13.0F,
                                .weight = 400,
                                .design = TypeDesign::text,
                                .tracking = 0.0F,
                                .line_height = 16.0F});
  set(TextRole::callout, TypeStyle{.size = 12.0F,
                                   .weight = 400,
                                   .design = TypeDesign::text,
                                   .tracking = 0.0F,
                                   .line_height = 15.0F});
  set(TextRole::footnote, TypeStyle{.size = 10.0F,
                                    .weight = 400,
                                    .design = TypeDesign::text,
                                    .tracking = 0.0F,
                                    .line_height = 13.0F});
  set(TextRole::caption, TypeStyle{.size = 10.0F,
                                   .weight = 400,
                                   .design = TypeDesign::text,
                                   .tracking = 0.0F,
                                   .line_height = 13.0F});
  set(TextRole::caption2, TypeStyle{.size = 10.0F,
                                    .weight = 400,
                                    .design = TypeDesign::text,
                                    .tracking = 0.0F,
                                    .line_height = 13.0F});
  return typography;
}

auto Theme::default_spacing() noexcept -> Spacing {
  return Spacing{0.0F, 2.0F, 4.0F, 8.0F, 12.0F, 16.0F, 24.0F, 32.0F};
}

auto Theme::default_elevations() noexcept -> Elevations {
  return Elevations{
      ElevationStyle{.ambient = Shadow{}, .key = Shadow{}},
      ElevationStyle{
          .ambient = Shadow{.offset_y = 0.0F, .blur = 2.0F, .opacity = 0.16F},
          .key = Shadow{.offset_y = 3.0F, .blur = 10.0F, .opacity = 0.30F}},
      ElevationStyle{
          .ambient = Shadow{.offset_y = 0.0F, .blur = 3.0F, .opacity = 0.10F},
          .key = Shadow{.offset_y = 6.0F, .blur = 18.0F, .opacity = 0.22F}},
      ElevationStyle{
          .ambient = Shadow{.offset_y = 0.0F, .blur = 4.0F, .opacity = 0.12F},
          .key = Shadow{.offset_y = 12.0F, .blur = 32.0F, .opacity = 0.28F}}};
}

auto Theme::default_metrics() noexcept -> Metrics {
  Metrics table{};
  const auto set = [&table](MetricToken token, float points) {
    table[std::to_underlying(token)] = points;
  };
  set(MetricToken::stack_spacing, metrics::stack.spacing);
  set(MetricToken::spacer_minimum, metrics::stack.spacer_minimum);
  set(MetricToken::button_horizontal_padding,
      metrics::button.horizontal_padding);
  set(MetricToken::button_vertical_padding, metrics::button.vertical_padding);
  set(MetricToken::toolbar_horizontal_padding,
      metrics::button.toolbar_horizontal_padding);
  set(MetricToken::toolbar_vertical_padding,
      metrics::button.toolbar_vertical_padding);
  set(MetricToken::menu_item_padding, metrics::menu.item_padding);
  set(MetricToken::menu_item_height, metrics::menu.item_height);
  set(MetricToken::menu_vertical_padding, metrics::menu.vertical_padding);
  set(MetricToken::text_field_inset, metrics::text_field.text_inset);
  set(MetricToken::text_field_vertical_inset,
      (metrics::text_field.height - metrics::stack.text_line) * 0.5F);
  set(MetricToken::text_field_height, metrics::text_field.height);
  set(MetricToken::list_plain_row_height, metrics::list.plain_row_height);
  set(MetricToken::list_plain_row_inset, metrics::list.plain_row_inset);
  set(MetricToken::list_grouped_row_height, metrics::list.grouped_row_height);
  set(MetricToken::list_grouped_margin, metrics::list.grouped_margin);
  set(MetricToken::list_grouped_row_inset, metrics::list.grouped_row_inset);
  set(MetricToken::list_grouped_header_gap, metrics::list.grouped_header_gap);
  set(MetricToken::list_grouped_footer_gap, metrics::list.grouped_footer_gap);
  set(MetricToken::list_grouped_section_gap, metrics::list.grouped_section_gap);
  set(MetricToken::sidebar_width, metrics::list.sidebar_width);
  set(MetricToken::sidebar_rail_width, metrics::list.sidebar_rail_width);
  set(MetricToken::sidebar_row_height, metrics::list.sidebar_row_height);
  set(MetricToken::sidebar_selection_inset,
      metrics::list.sidebar_selection_inset);
  set(MetricToken::sidebar_row_vertical_inset,
      (metrics::list.sidebar_row_height - metrics::stack.text_line) * 0.5F);
  set(MetricToken::navigation_bar_height, metrics::navigation.bar_height);
  set(MetricToken::alert_width, metrics::alert.width);
  set(MetricToken::alert_inset, metrics::alert.inset);
  set(MetricToken::alert_button_height, metrics::alert.button_height);
  set(MetricToken::alert_button_gap, metrics::alert.button_gap);
  set(MetricToken::toolbar_group_height, metrics::navigation.group_height);
  set(MetricToken::toolbar_group_padding, metrics::navigation.group_padding);
  set(MetricToken::toolbar_group_spacing, metrics::navigation.group_spacing);
  set(MetricToken::toolbar_group_inset, metrics::navigation.group_inset);
  set(MetricToken::toolbar_group_gap, metrics::navigation.group_gap);
  set(MetricToken::sidebar_panel_inset, metrics::list.sidebar_panel_inset);
  set(MetricToken::sidebar_panel_radius, metrics::list.sidebar_panel_radius);
  return table;
}

auto Theme::palette(ColourRole role) const noexcept -> const Adaptive<Colour>& {
  return tokens_->palette[std::to_underlying(role)];
}

auto Theme::variants(ColourRole role) const noexcept
    -> const AppearanceVariants& {
  return tokens_->variants[std::to_underlying(role)];
}

auto Theme::leaf(const ColourValue::Leaf& leaf, ColorScheme scheme,
                 ColorSchemeContrast contrast) const noexcept -> Colour {
  if (const ColourRole* role = std::get_if<ColourRole>(&leaf)) {
    if (contrast == ColorSchemeContrast::standard) {
      return palette(*role).resolve(scheme);
    }
    const AppearanceVariants& found = variants(*role);
    return scheme == ColorScheme::dark ? found.high_contrast_dark_elevated
                                       : found.high_contrast_light;
  }
  return std::get<Colour>(leaf);
}

auto Theme::composed(const ColourValue& value, Colour base, ColorScheme scheme,
                     ColorSchemeContrast contrast) const noexcept -> Colour {
  Colour resolved = base.with_alpha(base.alpha * value.opacity());
  for (const ColourValue::Step& step : value.steps()) {
    resolved = ColourMix::mix(resolved, leaf(step.colour, scheme, contrast),
                              step.amount, step.space);
  }
  return ColourMix::exposed(resolved, value.exposure());
}

auto Theme::colour(ColourValue value, ColorScheme scheme) const noexcept
    -> Colour {
  return composed(value,
                  leaf(value.leaf(), scheme, ColorSchemeContrast::standard),
                  scheme, ColorSchemeContrast::standard);
}

auto Theme::colour(ColourValue value, ColorScheme scheme,
                   ColorSchemeContrast contrast) const noexcept -> Colour {
  return composed(value, leaf(value.leaf(), scheme, contrast), scheme,
                  contrast);
}

auto Theme::colour_hdr(ColourValue value, ColorScheme scheme) const noexcept
    -> ResolvedHDR {
  return ResolvedHDR::from(colour(value, scheme), value.headroom());
}

auto Theme::type(TextRole role) const noexcept -> TypeStyle {
  return DynamicType::scaled(base_type(role), role, dynamic_type_size_);
}

auto Theme::base_type(TextRole role) const noexcept -> const TypeStyle& {
  return tokens_->typography[std::to_underlying(role)];
}

auto Theme::font_context(bool bold_text) const -> Font::Context {
  Font::Context::Styles scaled{};
  for (std::size_t index = 0; index < text_role_count; ++index) {
    scaled[index] = type(static_cast<TextRole>(index));
  }
  return Font::Context{scaled, tokens_->typography, bold_text};
}

auto Theme::sized(DynamicTypeSize size) const -> Theme {
  Theme resized = *this;
  resized.dynamic_type_size_ = size;
  return resized;
}

auto Theme::metric(MetricToken token) const noexcept -> float {
  return tokens_->metrics[std::to_underlying(token)];
}

auto Theme::scaled(ScaledMetric value) const noexcept -> float {
  const float base =
      value.token() != nullptr ? metric(*value.token()) : *value.value();
  return base * DynamicType::factor(value.relative_to(), dynamic_type_size_);
}

auto Theme::space(Space value) const noexcept -> float {
  if (const SpaceToken* token = value.token()) {
    return tokens_->spacing[std::to_underlying(*token)];
  }
  if (const ScaledMetric* metric = value.scaled()) {
    return scaled(*metric);
  }
  return *value.points();
}

auto Theme::edges(const Insets& insets) const noexcept -> Edges {
  return Edges{.top = space(insets.top),
               .leading = space(insets.leading),
               .bottom = space(insets.bottom),
               .trailing = space(insets.trailing)};
}

auto Theme::radius(Radius value) const noexcept -> float {
  if (const float* points = value.points()) {
    return std::max(*points, 0.0F);
  }
  switch (*value.role()) {
    case RadiusRole::none:
      return 0.0F;
    case RadiusRole::control:
      return tokens_->radii.control;
    case RadiusRole::card:
      return tokens_->radii.card;
    case RadiusRole::sheet:
      return tokens_->radii.sheet;
    case RadiusRole::capsule:
      return std::numeric_limits<float>::infinity();
  }
  return 0.0F;
}

auto Theme::elevation(Elevation level) const noexcept -> const ElevationStyle& {
  return tokens_->elevations[std::to_underlying(level)];
}

auto Theme::with(ColourRole role, Adaptive<Colour> colour) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().palette[std::to_underlying(role)] = colour;
  AppearanceVariants& kept =
      edited.tokens_.edit().variants[std::to_underlying(role)];
  kept.light = colour.light;
  kept.dark_elevated = colour.dark;
  return edited;
}

auto Theme::high_contrast() -> Theme {
  Theme contrasted = defaults();
  for (std::size_t index = 0; index < colour_role_count; ++index) {
    const AppearanceVariants& each = contrasted.tokens_.edit().variants[index];
    contrasted.tokens_.edit().palette[index] =
        Adaptive<Colour>{.light = each.high_contrast_light,
                         .dark = each.high_contrast_dark_elevated};
  }
  contrasted.tokens_.edit().focus_ring =
      FocusRing{.width = high_contrast_ring_width,
                .halo = true,
                .style = FocusRing::Style::highlight,
                .highlight = high_contrast_highlight};
  contrasted.name_ = std::string{high_contrast_name};
  return contrasted;
}

auto Theme::with(FocusRing ring) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().focus_ring = ring;
  return edited;
}

auto Theme::with(Radii radii) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().radii = radii;
  return edited;
}

auto Theme::with(Motion tokens) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().motion = tokens;
  return edited;
}

auto Theme::with(TextRole role, TypeStyle style) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().typography[std::to_underlying(role)] = style;
  return edited;
}

auto Theme::with(MetricToken token, float points) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().metrics[std::to_underlying(token)] =
      std::max(points, 0.0F);
  return edited;
}

auto Theme::with(Spacing spacing) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().spacing = spacing;
  return edited;
}

auto Theme::with(GlassDefaults glass) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().glass = glass;
  return edited;
}

auto Theme::with_accent(Colour accent) const -> Theme {
  const SemanticPalette::Variants previous =
      SemanticPalette::derive(seed_.accent);
  const SemanticPalette::Variants derived = SemanticPalette::derive(accent);
  Theme edited = *this;
  edited.seed_.accent = accent;
  for (std::size_t index = 0; index < colour_role_count; ++index) {
    if (tokens_->variants[index] == previous[index]) {
      edited.tokens_.edit().variants[index] = derived[index];
      edited.tokens_.edit().palette[index] = derived[index].adaptive();
    }
  }
  return edited;
}

auto Theme::lays_out_like(const Theme& other) const noexcept -> bool {
  return dynamic_type_size_ == other.dynamic_type_size_ &&
         tokens_->typography == other.tokens_->typography &&
         tokens_->spacing == other.tokens_->spacing &&
         tokens_->metrics == other.tokens_->metrics &&
         tokens_->radii == other.tokens_->radii &&
         tokens_->glass == other.tokens_->glass &&
         tokens_->preferences == other.tokens_->preferences;
}

auto Theme::paints_like(const Theme& other,
                        const ColourRoles& roles) const noexcept -> bool {
  if (!lays_out_like(other) || seed_ != other.seed_ ||
      tokens_->elevations != other.tokens_->elevations ||
      tokens_->motion != other.tokens_->motion ||
      tokens_->materials != other.tokens_->materials ||
      tokens_->liquid_glass != other.tokens_->liquid_glass ||
      tokens_->focus_ring != other.tokens_->focus_ring) {
    return false;
  }
  for (std::size_t index = 0; index < colour_role_count; ++index) {
    if (roles.test(index) &&
        (tokens_->palette[index] != other.tokens_->palette[index] ||
         tokens_->variants[index] != other.tokens_->variants[index])) {
      return false;
    }
  }
  return true;
}

auto Theme::named(std::string name) const -> Theme {
  Theme edited = *this;
  edited.name_ = std::move(name);
  return edited;
}

auto Theme::with(Preferences preferences) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().preferences = preferences;
  return edited;
}

auto Theme::with(Elevations elevations) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().elevations = elevations;
  return edited;
}

auto Theme::with(Materials materials) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().materials = materials;
  return edited;
}

auto Theme::with(LiquidGlassStyle glass) const -> Theme {
  Theme edited = *this;
  edited.tokens_.edit().liquid_glass = glass;
  return edited;
}

auto Theme::default_materials() noexcept -> Materials {
  const auto style = [](float blur, float saturation, std::uint32_t light,
                        float light_alpha, std::uint32_t dark, float dark_alpha,
                        float lift) {
    return MaterialStyle{
        .blur = blur,
        .saturation = saturation,
        .tint =
            Adaptive<Colour>{
                .light = Colour::hex(light).with_alpha(light_alpha),
                .dark = Colour::hex(dark).with_alpha(dark_alpha)},
        .luminosity = Adaptive<float>{.light = lift, .dark = -lift},
        .noise = 0.015F};
  };
  return Materials{style(12.0F, 1.6F, 0xFFFFFF, 0.22F, 0x1C1C1E, 0.28F, 0.02F),
                   style(18.0F, 1.7F, 0xFFFFFF, 0.36F, 0x1C1C1E, 0.42F, 0.02F),
                   style(24.0F, 1.8F, 0xF2F2F7, 0.52F, 0x1C1C1E, 0.58F, 0.02F),
                   style(30.0F, 1.8F, 0xF2F2F7, 0.68F, 0x1C1C1E, 0.72F, 0.02F),
                   style(40.0F, 1.9F, 0xF2F2F7, 0.82F, 0x1C1C1E, 0.84F, 0.02F),
                   style(30.0F, 2.0F, 0xF5F5F7, 0.58F, 0x26262A, 0.62F, 0.03F),
                   style(28.0F, 1.8F, 0xFFFFFF, 0.72F, 0x2C2C2E, 0.74F, 0.02F),
                   style(30.0F, 1.6F, 0x1C1C1E, 0.58F, 0x1C1C1E, 0.64F, 0.0F),
                   style(30.0F, 1.8F, 0xFFFFFF, 0.8F, 0x2D2D2D, 0.82F, 0.0F)};
}

auto Theme::default_liquid_glass() noexcept -> LiquidGlassStyle {
  return LiquidGlassStyle{
      .refraction = 43.0F,
      .rim = 0.3F,
      .blur = 4.6F,
      .clear_blur = 1.0F,
      .highlight = 1.0F,
      .tint =
          Adaptive<Colour>{.light = Colour::hex(0xFFFFFF).with_alpha(0.535F),
                           .dark = Colour::hex(0x3D3D3D).with_alpha(0.5F)},
      .shadow = ElevationStyle{
          .ambient = Shadow{.offset_y = 0.0F, .blur = 4.0F, .opacity = 0.04F},
          .key = Shadow{.offset_y = 2.0F, .blur = 28.0F, .opacity = 0.09F}}};
}

}
