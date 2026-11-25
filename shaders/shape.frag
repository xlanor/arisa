#version 460

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec2 v_local;
layout(location = 2) flat in vec2 v_half_size;
layout(location = 3) flat in vec4 v_shape;
layout(location = 4) in vec4 v_colour;
layout(location = 5) flat in uint v_kind;
layout(location = 6) flat in vec4 v_effect;
layout(location = 7) flat in vec4 v_detail;
layout(location = 8) in vec2 v_screen;
layout(location = 9) flat in float v_opacity;

#if defined(ARISA_PUSH_CONSTANTS)
layout(set = 0, binding = 0) uniform sampler2D atlas;
layout(set = 1, binding = 0) uniform sampler2D detail;

layout(push_constant) uniform Command
{
  layout(offset = 32) vec4 command_trim;
};
#else
#ifdef VULKAN
layout(set = 2, binding = 0) uniform sampler2D atlas;
layout(set = 2, binding = 1) uniform sampler2D detail;
#else
layout(binding = 0) uniform sampler2D atlas;
layout(binding = 1) uniform sampler2D detail;
#endif

#ifdef VULKAN
layout(std140, set = 3, binding = 0) uniform Trim
#else
layout(std140, binding = 1) uniform Trim
#endif
{
  vec4 command_trim;
};
#endif

layout(location = 0) out vec4 out_colour;

const uint kind_glyph = 0u;
const uint kind_image = 1u;
const uint kind_fill = 2u;
const uint kind_stroke = 3u;
const uint kind_shadow = 4u;
const uint kind_path = 5u;
const uint kind_gradient = 6u;
const uint kind_mesh = 7u;
const uint kind_blur_down = 8u;
const uint kind_blur_up = 9u;
const uint kind_material = 10u;
const uint kind_glass = 11u;
const uint kind_glass_path = 12u;
const uint kind_glass_union = 13u;
const uint kind_layer = 14u;
const uint kind_yuv = 15u;
const uint kind_filter = 16u;
const uint kind_blend = 17u;
const uint kind_mask = 18u;
const uint kind_gaussian = 19u;
const uint kind_shader = 20u;
const uint kind_inner_shadow = 21u;
const uint kind_gradient_fill = 22u;
const uint kind_glyph_gradient = 23u;
const int effect_arguments = 9;
const int gaussian_reach = 12;
const vec3 blend_luma = vec3(0.3, 0.59, 0.11);
const float ramp_width = 256.0;
const float tau = 6.28318530718;
const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
const float lens_zone = 18.0;
const float lens_share = 0.7;
const float lens_exponent = 2.2;
const float lens_body = 0.04;
const float lens_body_cap = 1.0;
const float lens_dispersion = 0.05;
const float rim_sharp = 0.45;
const float rim_soft = 2.5;
const float rim_sharp_weight = 0.66;
const float rim_soft_weight = 0.14;
const float rim_lower = 0.25;
const float chroma_span = 4.0;
const float chroma_bias = 0.5;
const float curve_steps = 1000.0;

float corner_length(vec2 q, float exponent)
{
  vec2 outside = max(q, vec2(0.0));
  if (exponent <= 2.0) {
    return length(outside);
  }
  float largest = max(outside.x, outside.y);
  if (largest <= 0.0) {
    return 0.0;
  }
  vec2 normalised = outside / largest;
  return largest * pow(pow(normalised.x, exponent) + pow(normalised.y, exponent), 1.0 / exponent);
}

float rounded_box(vec2 position, vec2 half_size, float radius, float smoothing)
{
  float limit = min(half_size.x, half_size.y);
  float extent = min(radius * (1.0 + 0.6 * smoothing), limit);
  float exponent = 2.0 + 3.0 * smoothing;
  vec2 q = abs(position) - half_size + vec2(extent);
  return min(max(q.x, q.y), 0.0) + corner_length(q, exponent) - extent;
}

float ellipse_distance(vec2 position, vec2 half_size)
{
  vec2 radii = max(half_size, vec2(1.0e-3));
  float stretched = length(position / radii);
  float slope = length(position / (radii * radii));
  return slope > 1.0e-6 ? stretched * (stretched - 1.0) / slope : -min(radii.x, radii.y);
}

float outline_distance(vec2 position, vec2 half_size, float radius, float smoothing, vec4 corners)
{
  if (corners.x < 0.0) {
    return ellipse_distance(position, max(half_size - vec2(radius), vec2(1.0e-3))) - radius;
  }
  if (corners != vec4(0.0)) {
    float chosen = position.x < 0.0 ? (position.y < 0.0 ? corners.x : corners.w)
                                    : (position.y < 0.0 ? corners.y : corners.z);
    return rounded_box(position, half_size, chosen, smoothing);
  }
  return rounded_box(position, half_size, radius, smoothing);
}

vec4 inset_corners(vec4 corners, float amount)
{
  return corners.x < 0.0 ? corners : max(corners - vec4(amount), vec4(0.0));
}

float gradient_position(vec2 position, float mode, vec4 points, vec2 extent)
{
  if (mode < 0.5) {
    vec2 axis = points.zw - points.xy;
    float span = dot(axis, axis);
    return span > 1.0e-6 ? dot(position - points.xy, axis) / span : 0.0;
  }
  vec2 delta = position - points.xy;
  if (mode < 1.5) {
    return (length(delta) - points.z) / max(points.w - points.z, 1.0e-4);
  }
  if (mode < 2.5) {
    float span = points.w - points.z;
    float angle = mod(atan(delta.y, delta.x) - points.z, tau);
    return abs(span) < 1.0e-4 ? 0.0 : angle / span;
  }
  return (length(delta / max(extent, vec2(1.0e-4))) - points.z) / max(points.w - points.z, 1.0e-4);
}

vec2 ramp_uv(float position)
{
  return vec2((clamp(position, 0.0, 1.0) * (ramp_width - 1.0) + 0.5) / ramp_width, 0.5);
}

vec2 box_normal(vec2 position, vec2 half_size, float radius, float smoothing)
{
  float e = 0.5;
  vec2 n = vec2(rounded_box(position + vec2(e, 0.0), half_size, radius, smoothing) -
                    rounded_box(position - vec2(e, 0.0), half_size, radius, smoothing),
                rounded_box(position + vec2(0.0, e), half_size, radius, smoothing) -
                    rounded_box(position - vec2(0.0, e), half_size, radius, smoothing));
  float span = length(n);
  return span > 1.0e-5 ? n / span : vec2(0.0);
}

float fringe(vec2 uv, float multiplier)
{
  return min(1.0, (1.0 - abs(uv.x * 2.0 - 1.0)) * multiplier) * min(1.0, uv.y);
}

float ramp_position()
{
  if (v_shape.y < 0.5) {
    return v_local.x;
  }
  if (v_shape.y < 1.5) {
    float span = max(v_half_size.y - v_half_size.x, 1.0e-4);
    return (length(v_local) - v_half_size.x) / span;
  }
  float span = v_half_size.y - v_half_size.x;
  float angle = mod(atan(v_local.y, v_local.x) - v_half_size.x, tau);
  return abs(span) < 1.0e-4 ? 0.0 : angle / span;
}

vec3 yuv_colour()
{
  vec2 luma_size = vec2(textureSize(atlas, 0));
  vec2 chroma_size = vec2(textureSize(detail, 0));
  vec2 chroma_uv = vec2((v_uv.x * luma_size.x * 0.5 + 0.25) / chroma_size.x,
                        (v_uv.y * luma_size.y * 0.5) / chroma_size.y);
  float luma = (texture(atlas, v_uv).r - v_effect.y) * v_effect.x;
  vec2 chroma = (texture(detail, chroma_uv).rg - vec2(v_effect.w)) * v_effect.z;
  return clamp(vec3(luma + v_detail.x * chroma.y, luma - v_detail.y * chroma.x - v_detail.z * chroma.y,
                    luma + v_detail.w * chroma.x),
               0.0, 1.0);
}

float trimmed()
{
  float along = v_local.y;
  float first = max(v_shape.z, command_trim.x);
  float last = min(v_shape.w, command_trim.y);
  if (along >= first && along <= last) {
    return 1.0;
  }
  float beyond = along < first ? first - along : along - last;
  if (v_half_size.y > 0.5) {
    return 1.0 - smoothstep(0.0, v_half_size.x, beyond);
  }
  float across = (v_uv.x - 0.5) * v_half_size.x;
  return clamp(v_half_size.x * 0.5 - length(vec2(beyond, across)) + 0.5, 0.0, 1.0);
}

float grain()
{
  vec2 cell = floor(gl_FragCoord.xy);
  return fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
}

vec3 saturated(vec3 colour, float amount)
{
  return clamp(mix(vec3(dot(colour, luma)), colour, amount), 0.0, 1.0);
}

vec2 backdrop_uv(vec2 offset)
{
  vec2 size = max(v_effect.zw, vec2(1.0));
  vec2 texel = 0.5 / vec2(textureSize(atlas, 0));
  return clamp((v_screen + offset - v_effect.xy) / size, texel, vec2(1.0) - texel);
}

vec3 responded(vec3 colour, vec4 veil, float response)
{
  float curve = floor(response / chroma_span) / curve_steps;
  float chroma = mod(response, chroma_span) - chroma_bias;
  float level = dot(colour, luma);
  float tone = veil.a * level - curve * level * level;
  return vec3(tone) + chroma * (colour - vec3(level)) + veil.rgb;
}

vec3 lensed(float inside, vec2 normal, float zone, float amplitude, vec2 body, float unit, float rim, vec4 veil,
           float response)
{
  float t = clamp(1.0 - inside / max(zone, 1.0), 0.0, 1.0);
  vec2 lift = body * lens_body;
  lift *= min(1.0, lens_body_cap * unit / max(length(lift), 1.0e-4));
  vec2 offset = -normal * (amplitude * pow(t, lens_exponent)) - lift;
  vec2 spread = offset * lens_dispersion;
  vec3 colour = vec3(texture(atlas, backdrop_uv(offset + spread)).r, texture(atlas, backdrop_uv(offset)).g,
                     texture(atlas, backdrop_uv(offset - spread)).b);
  colour = responded(colour, veil, response);
  float depth = max(inside, 0.0) / max(unit, 1.0e-3);
  float glow = rim * (rim_sharp_weight * exp(-depth / rim_sharp) + rim_soft_weight * exp(-depth / rim_soft));
  float facing = abs(normal.y) * (1.0 + rim_lower * max(normal.y, 0.0));
  return clamp(colour + vec3(glow * facing * facing), 0.0, 1.0);
}

float lens_zone_of(float limit, float unit)
{
  return min(lens_zone * unit, lens_share * limit);
}

vec3 highlighted(vec3 colour, vec2 position, vec2 centre, float strength, float reach)
{
  if (strength <= 0.0) {
    return colour;
  }
  vec2 delta = (position - centre) / max(reach, 1.0);
  float glow = exp(-dot(delta, delta) * 2.0) * strength;
  return clamp(colour + vec3(0.22 * glow), 0.0, 1.0);
}

float table_value(int index)
{
  int width = max(textureSize(detail, 0).x, 1);
  return uintBitsToFloat(packUnorm4x8(texelFetch(detail, ivec2(index % width, index / width), 0)));
}

vec3 box_field(vec2 position, vec2 half_size, float radius, float smoothing)
{
  float limit = min(half_size.x, half_size.y);
  float extent = min(radius * (1.0 + 0.6 * smoothing), limit);
  float exponent = 2.0 + 3.0 * smoothing;
  vec2 q = abs(position) - half_size + vec2(extent);
  vec2 outside = max(q, vec2(0.0));
  vec2 direction = vec2(position.x < 0.0 ? -1.0 : 1.0, position.y < 0.0 ? -1.0 : 1.0);
  float largest = max(outside.x, outside.y);
  if (largest <= 0.0) {
    vec2 axis = q.x > q.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
    return vec3(max(q.x, q.y) - extent, axis * direction);
  }
  vec2 normalised = outside / largest;
  float span = largest * pow(pow(normalised.x, exponent) + pow(normalised.y, exponent), 1.0 / exponent);
  vec2 gradient = vec2(pow(outside.x / span, exponent - 1.0), pow(outside.y / span, exponent - 1.0));
  return vec3(span - extent, gradient * direction);
}

float neck_blend(float gap, float thickness, float rounding, float spacing)
{
  float apart = max(gap, 0.0);
  if (apart >= spacing) {
    return 0.0;
  }
  float waist = thickness * (1.0 - apart / spacing);
  float rise = max(waist - (thickness - rounding), 0.0);
  float reach = 0.5 * apart + rounding;
  return 4.0 * (sqrt(reach * reach + rise * rise) - rounding);
}

float fusion(int member, float spacing)
{
  int base = 2 + member * 6;
  vec2 centre = vec2(table_value(base), table_value(base + 1));
  vec2 half_size = vec2(table_value(base + 2), table_value(base + 3));
  float radius = table_value(base + 4);
  float nearest = 1.0e6;
  float chosen = 0.0;
  for (int other = 0; other < member; ++other) {
    int at = 2 + other * 6;
    vec2 middle = vec2(table_value(at), table_value(at + 1));
    vec2 extent = vec2(table_value(at + 2), table_value(at + 3));
    vec2 gaps = abs(centre - middle) - (half_size + extent);
    float gap = max(gaps.x, gaps.y);
    if (gap >= nearest) {
      continue;
    }
    nearest = gap;
    float thickness = gaps.x >= gaps.y ? min(half_size.y, extent.y) : min(half_size.x, extent.x);
    float rounding = min(min(radius, table_value(at + 4)), thickness);
    chosen = neck_blend(gap, thickness, rounding, spacing);
  }
  return chosen;
}

vec3 union_field(vec2 position)
{
  int count = clamp(int(table_value(0) + 0.5), 0, 8);
  float spacing = max(table_value(1), 1.0e-3);
  vec3 united = vec3(1.0e6, 0.0, 0.0);
  for (int member = 0; member < count; ++member) {
    int base = 2 + member * 6;
    vec2 centre = vec2(table_value(base), table_value(base + 1));
    vec2 half_size = vec2(table_value(base + 2), table_value(base + 3));
    vec3 field = box_field(position - centre, half_size, table_value(base + 4), table_value(base + 5));
    if (member == 0) {
      united = field;
      continue;
    }
    float blend = max(fusion(member, spacing), 1.0e-3);
    float h = clamp(0.5 + 0.5 * (field.x - united.x) / blend, 0.0, 1.0);
    united = vec3(mix(field.x, united.x, h) - blend * h * (1.0 - h), mix(field.yz, united.yz, h));
  }
  return united;
}

vec4 unpremultiplied(vec4 texel)
{
  return texel.a > 1.0e-4 ? vec4(texel.rgb / texel.a, texel.a) : vec4(0.0);
}

vec3 srgb_decoded(vec3 c)
{
  return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

vec3 srgb_encoded(vec3 c)
{
  vec3 clamped = max(c, vec3(0.0));
  return mix(clamped * 12.92, 1.055 * pow(clamped, vec3(1.0 / 2.4)) - 0.055,
             step(vec3(0.0031308), clamped));
}

vec4 matrix_filtered(vec4 c)
{
  vec4 o = vec4(0.0);
  for (int row = 0; row < 4; ++row) {
    int at = row * 5;
    o[row] = table_value(at) * c.r + table_value(at + 1) * c.g + table_value(at + 2) * c.b +
             table_value(at + 3) * c.a + table_value(at + 4);
  }
  return clamp(o, 0.0, 1.0);
}

float dodge(float b, float s)
{
  if (b <= 0.0) {
    return 0.0;
  }
  if (s >= 1.0) {
    return 1.0;
  }
  return min(1.0, b / (1.0 - s));
}

float burn(float b, float s)
{
  if (b >= 1.0) {
    return 1.0;
  }
  if (s <= 0.0) {
    return 0.0;
  }
  return 1.0 - min(1.0, (1.0 - b) / s);
}

float hard(float b, float s)
{
  return s <= 0.5 ? b * 2.0 * s : b + (2.0 * s - 1.0) - b * (2.0 * s - 1.0);
}

float lum(vec3 c)
{
  return dot(c, blend_luma);
}

vec3 clip_colour(vec3 c)
{
  float l = lum(c);
  float n = min(min(c.r, c.g), c.b);
  float x = max(max(c.r, c.g), c.b);
  if (n < 0.0) {
    c = l + (c - l) * l / max(l - n, 1.0e-6);
  }
  if (x > 1.0) {
    c = l + (c - l) * (1.0 - l) / max(x - l, 1.0e-6);
  }
  return c;
}

vec3 set_lum(vec3 c, float l)
{
  return clip_colour(c + vec3(l - lum(c)));
}

float sat(vec3 c)
{
  return max(max(c.r, c.g), c.b) - min(min(c.r, c.g), c.b);
}

vec3 set_sat(vec3 c, float s)
{
  float n = min(min(c.r, c.g), c.b);
  float x = max(max(c.r, c.g), c.b);
  if (x <= n) {
    return vec3(0.0);
  }
  return (c - vec3(n)) * s / (x - n);
}

vec3 separable(vec3 b, vec3 s, int mode)
{
  vec3 r = s;
  for (int i = 0; i < 3; ++i) {
    float cb = b[i];
    float cs = s[i];
    float v = cs;
    if (mode == 1) {
      v = cb * cs;
    } else if (mode == 2) {
      v = cb + cs - cb * cs;
    } else if (mode == 3) {
      v = hard(cs, cb);
    } else if (mode == 4) {
      v = min(cb, cs);
    } else if (mode == 5) {
      v = max(cb, cs);
    } else if (mode == 6) {
      v = dodge(cb, cs);
    } else if (mode == 7) {
      v = burn(cb, cs);
    } else if (mode == 8) {
      v = (1.0 - 2.0 * cs) * cb * cb + 2.0 * cs * cb;
    } else if (mode == 9) {
      v = hard(cb, cs);
    } else if (mode == 10) {
      v = abs(cb - cs);
    } else if (mode == 11) {
      v = cb + cs - 2.0 * cb * cs;
    }
    r[i] = v;
  }
  return r;
}

vec4 blended(vec4 s, vec4 b, int mode)
{
  if (mode == 16) {
    return vec4(s.rgb, s.a * b.a);
  }
  if (mode == 17) {
    return vec4(s.rgb, s.a * (1.0 - b.a));
  }
  if (mode == 18) {
    return vec4(0.0, 0.0, 0.0, s.a);
  }
  if (mode == 19) {
    return s.a > 1.0e-4 ? vec4(max(vec3(0.0), b.rgb - (vec3(1.0) - s.rgb) * s.a), 1.0) : vec4(0.0);
  }
  if (mode == 20) {
    return s.a > 1.0e-4 ? vec4(min(vec3(1.0), b.rgb + s.rgb * s.a), 1.0) : vec4(0.0);
  }
  vec3 mixed = s.rgb;
  if (mode == 12) {
    mixed = set_lum(set_sat(s.rgb, sat(b.rgb)), lum(b.rgb));
  } else if (mode == 13) {
    mixed = set_lum(set_sat(b.rgb, sat(s.rgb)), lum(b.rgb));
  } else if (mode == 14) {
    mixed = set_lum(s.rgb, lum(b.rgb));
  } else if (mode == 15) {
    mixed = set_lum(b.rgb, lum(s.rgb));
  } else if (mode > 0) {
    mixed = separable(b.rgb, s.rgb, mode);
  }
  return vec4(clamp(mix(s.rgb, mixed, b.a), 0.0, 1.0), s.a);
}

vec2 arisa_size()
{
  return vec2(table_value(7), table_value(8));
}

float arisa_argument(int index)
{
  return table_value(effect_arguments + index);
}

vec4 arisa_layer(vec2 position)
{
  vec2 device = vec2(table_value(0), table_value(1)) + position * table_value(2);
  vec2 extent = max(vec2(table_value(5), table_value(6)), vec2(1.0));
  return texture(atlas, (device - vec2(table_value(3), table_value(4))) / extent);
}

vec4 arisa_effect(int index, vec2 position);

void main()
{
  float radius = v_shape.x;
  float smoothing = v_shape.y;
  float stroke = v_shape.z;
  float softness = v_shape.w;
  float coverage = 1.0;
  vec4 colour = v_colour;
  if (v_kind == kind_glyph) {
    coverage = texture(atlas, v_uv).a;
  } else if (v_kind == kind_image) {
    colour = colour * texture(atlas, v_uv);
    if (radius > 0.0 || v_detail != vec4(0.0)) {
      coverage = clamp(0.5 - outline_distance(v_local, v_half_size, radius, smoothing, v_detail), 0.0, 1.0);
    }
  } else if (v_kind == kind_fill) {
    float distance = outline_distance(v_local, v_half_size, radius, smoothing, v_detail);
    coverage = clamp(0.5 - distance, 0.0, 1.0);
  } else if (v_kind == kind_stroke) {
    float distance = outline_distance(v_local, v_half_size, radius, smoothing, v_detail);
    float inner = clamp(0.5 + distance + stroke, 0.0, 1.0);
    if (softness > 0.0) {
      float within = outline_distance(v_local, max(v_half_size - vec2(stroke), vec2(0.0)),
                                      max(radius - stroke, 0.0), smoothing, inset_corners(v_detail, stroke));
      inner = clamp(1.0 + 2.0 * within, 0.0, 1.0);
    }
    coverage = clamp(0.5 - distance, 0.0, 1.0) * inner;
  } else if (v_kind == kind_shadow) {
    float distance = outline_distance(v_local, v_half_size, radius, smoothing, v_detail);
    coverage = 1.0 - smoothstep(-softness, softness, distance);
  } else if (v_kind == kind_inner_shadow) {
    float distance = outline_distance(v_local, v_half_size, radius, smoothing, v_detail);
    float shifted = outline_distance(v_local - v_effect.xy, v_half_size, radius, smoothing, v_detail);
    coverage = clamp(0.5 - distance, 0.0, 1.0) * smoothstep(-softness, softness, shifted);
  } else if (v_kind == kind_gradient_fill) {
    float distance = outline_distance(v_local, v_half_size, radius, smoothing, v_detail);
    coverage = clamp(0.5 - distance, 0.0, 1.0);
    colour = colour * texture(atlas, ramp_uv(gradient_position(v_local, stroke, v_effect, v_uv)));
  } else if (v_kind == kind_glyph_gradient) {
    coverage = texture(atlas, v_uv).a;
    colour = colour * texture(detail, ramp_uv(gradient_position(v_local, stroke, v_effect, v_half_size)));
  } else if (v_kind == kind_path) {
    coverage = fringe(v_uv, v_shape.x) * trimmed();
  } else if (v_kind == kind_gradient) {
    coverage = fringe(v_uv, v_shape.x) * trimmed();
    float position = clamp(ramp_position(), 0.0, 1.0);
    colour = colour * texture(atlas, vec2((position * (ramp_width - 1.0) + 0.5) / ramp_width, 0.5));
  } else if (v_kind == kind_mesh) {
    colour = vec4(clamp(colour.rgb + vec3(grain() * v_shape.x), 0.0, 1.0), colour.a);
  } else if (v_kind == kind_blur_down) {
    vec2 h = 1.0 / vec2(textureSize(atlas, 0));
    vec4 sum = texture(atlas, v_uv) * 4.0;
    sum += texture(atlas, v_uv - h);
    sum += texture(atlas, v_uv + h);
    sum += texture(atlas, v_uv + vec2(h.x, -h.y));
    sum += texture(atlas, v_uv - vec2(h.x, -h.y));
    colour = vec4((sum / 8.0).rgb, 1.0);
  } else if (v_kind == kind_blur_up) {
    vec2 h = 0.5 / vec2(textureSize(atlas, 0));
    vec4 sum = texture(atlas, v_uv + vec2(-h.x * 2.0, 0.0));
    sum += texture(atlas, v_uv + vec2(-h.x, h.y)) * 2.0;
    sum += texture(atlas, v_uv + vec2(0.0, h.y * 2.0));
    sum += texture(atlas, v_uv + vec2(h.x, h.y)) * 2.0;
    sum += texture(atlas, v_uv + vec2(h.x * 2.0, 0.0));
    sum += texture(atlas, v_uv + vec2(h.x, -h.y)) * 2.0;
    sum += texture(atlas, v_uv + vec2(0.0, -h.y * 2.0));
    sum += texture(atlas, v_uv + vec2(-h.x, -h.y)) * 2.0;
    colour = vec4((sum / 12.0).rgb, 1.0);
  } else if (v_kind == kind_material) {
    float distance = rounded_box(v_local, v_half_size, radius, smoothing);
    coverage = clamp(0.5 - distance, 0.0, 1.0) * v_detail.y;
    vec3 seen = saturated(texture(atlas, backdrop_uv(vec2(0.0))).rgb, v_shape.z);
    seen = clamp(seen + vec3(v_shape.w), 0.0, 1.0);
    seen = mix(seen, v_colour.rgb, v_colour.a);
    colour = vec4(clamp(seen + vec3(grain() * v_detail.x), 0.0, 1.0), 1.0);
  } else if (v_kind == kind_glass) {
    float distance = rounded_box(v_local, v_half_size, radius, smoothing);
    vec2 normal = box_normal(v_local, v_half_size, radius, smoothing);
    coverage = clamp(0.5 - distance, 0.0, 1.0) * v_shape.w;
    float limit = min(v_half_size.x, v_half_size.y);
    vec3 seen = lensed(-distance, normal, lens_zone_of(limit, v_uv.x), v_shape.z * v_shape.w, v_local, v_uv.x,
                       v_detail.w * v_shape.w, v_colour, v_uv.y);
    seen = highlighted(seen, v_local, v_detail.xy, v_detail.z, limit * 1.6);
    colour = vec4(seen, 1.0);
  } else if (v_kind == kind_glass_path) {
    coverage = fringe(v_uv, v_shape.x) * trimmed() * v_detail.z;
    float across = v_uv.x * 2.0 - 1.0;
    vec2 gradient = vec2(dFdx(v_uv.x), dFdy(v_uv.x));
    float span = length(gradient);
    vec2 normal = span > 1.0e-6 ? (gradient / span) * sign(across) : vec2(0.0);
    float half_width = span > 1.0e-6 ? 0.5 / span : 1.0;
    float inside = (1.0 - abs(across)) * half_width;
    float unit = v_detail.w;
    vec3 seen = lensed(inside, normal, lens_zone * unit, v_detail.x * v_detail.z, normal * (half_width - inside), unit,
                       v_detail.y * v_detail.z, v_colour, v_shape.y);
    colour = vec4(seen, 1.0);
  } else if (v_kind == kind_glass_union) {
    vec3 field = union_field(v_screen);
    float slope = length(field.yz);
    vec2 normal = slope > 1.0e-5 ? field.yz / slope : vec2(0.0);
    float edge = field.x / max(slope, 0.25);
    coverage = clamp(0.5 - edge, 0.0, 1.0) * v_shape.w;
    float limit = max(v_shape.y, 8.0) * 0.625;
    vec3 seen = lensed(-field.x, normal, lens_zone_of(limit, v_uv.x), v_shape.z * v_shape.w, vec2(0.0), v_uv.x,
                       v_detail.w * v_shape.w, v_colour, v_uv.y);
    seen = highlighted(seen, v_local, v_detail.xy, v_detail.z, limit * 1.6);
    colour = vec4(seen, 1.0);
  } else if (v_kind == kind_yuv) {
    colour = colour * vec4(yuv_colour(), 1.0);
  } else if (v_kind == kind_layer) {
    vec4 texel = texture(atlas, v_uv);
    colour = texel.a > 1.0e-4 ? vec4(texel.rgb / texel.a, texel.a * v_colour.a) : vec4(0.0);
  } else if (v_kind == kind_filter) {
    vec4 seen = unpremultiplied(texture(atlas, v_uv));
    if (v_shape.x > 0.5) {
      if (v_shape.w > 0.5) {
        seen.rgb = srgb_decoded(seen.rgb);
      }
      if (v_shape.z > 0.5) {
        seen.rgb *= seen.a;
      }
      seen = matrix_filtered(seen);
      if (v_shape.z > 0.5) {
        seen = seen.a > 1.0e-4 ? vec4(seen.rgb / seen.a, seen.a) : vec4(0.0);
      }
      if (v_shape.w > 0.5) {
        seen.rgb = srgb_encoded(seen.rgb);
      }
    }
    if (v_shape.y > 0.5) {
      seen.a = 1.0;
    }
    colour = vec4(seen.rgb, seen.a * v_colour.a);
  } else if (v_kind == kind_blend) {
    vec4 seen = unpremultiplied(texture(atlas, v_uv));
    vec2 size = max(v_effect.zw, vec2(1.0));
    vec2 texel = 0.5 / vec2(textureSize(detail, 0));
    vec2 under = clamp((v_screen - v_effect.xy) / size, texel, vec2(1.0) - texel);
    vec4 result = blended(seen, texture(detail, under), int(v_shape.x + 0.5));
    colour = vec4(result.rgb, result.a * v_colour.a);
  } else if (v_kind == kind_mask) {
    vec4 seen = unpremultiplied(texture(atlas, v_uv));
    float covered = texture(detail, v_uv).a;
    if (v_shape.x > 0.5) {
      covered = 1.0 - covered;
    }
    colour = vec4(seen.rgb, seen.a * covered * v_colour.a);
  } else if (v_kind == kind_gaussian) {
    vec2 step = v_shape.yz / vec2(textureSize(atlas, 0));
    float sigma = max(v_shape.x, 1.0e-3);
    int reach = int(min(ceil(sigma * 3.0), float(gaussian_reach)));
    vec4 sum = vec4(0.0);
    float total = 0.0;
    for (int i = -gaussian_reach; i <= gaussian_reach; ++i) {
      if (abs(i) > reach) {
        continue;
      }
      float weight = exp(-0.5 * float(i * i) / (sigma * sigma));
      sum += texture(atlas, clamp(v_uv + step * float(i), v_effect.xy, v_effect.zw)) * weight;
      total += weight;
    }
    colour = unpremultiplied(sum / max(total, 1.0e-6));
  } else if (v_kind == kind_shader) {
    vec2 position = (v_screen - vec2(table_value(0), table_value(1))) / max(table_value(2), 1.0e-4);
    colour = unpremultiplied(clamp(arisa_effect(int(v_shape.x + 0.5), position), 0.0, 1.0));
    if (v_shape.y > 0.5) {
      float noise = fract(sin(dot(floor(v_screen), vec2(12.9898, 78.233))) * 43758.5453);
      colour.rgb = clamp(colour.rgb + vec3((noise - 0.5) / 255.0), 0.0, 1.0);
    }
  }
  out_colour = vec4(colour.rgb, colour.a * coverage * v_opacity);
}
