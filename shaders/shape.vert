#version 460

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec2 in_local;
layout(location = 3) in vec2 in_half_size;
layout(location = 4) in vec4 in_shape;
layout(location = 5) in vec4 in_colour;
layout(location = 6) in uint in_kind;
layout(location = 7) in vec4 in_effect;
layout(location = 8) in vec4 in_detail;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec2 v_local;
layout(location = 2) flat out vec2 v_half_size;
layout(location = 3) flat out vec4 v_shape;
layout(location = 4) out vec4 v_colour;
layout(location = 5) flat out uint v_kind;
layout(location = 6) flat out vec4 v_effect;
layout(location = 7) flat out vec4 v_detail;
layout(location = 8) out vec2 v_screen;
layout(location = 9) flat out float v_opacity;

#if defined(ARISA_PUSH_CONSTANTS)
layout(push_constant) uniform Command
{
  vec2 scale;
  vec2 translation;
  vec4 command_placement;
};
#else
#ifdef VULKAN
layout(std140, set = 1, binding = 0) uniform Transform
#else
layout(std140, binding = 0) uniform Transform
#endif
{
  vec2 scale;
  vec2 translation;
};

#ifdef VULKAN
layout(std140, set = 1, binding = 1) uniform Placement
#else
layout(std140, binding = 1) uniform Placement
#endif
{
  vec4 command_placement;
};
#endif

const uint kind_glyph = 0u;
const uint kind_glyph_gradient = 23u;

void main()
{
  v_uv = in_uv;
  v_local = in_local;
  v_half_size = in_half_size;
  v_shape = in_shape;
  v_colour = in_colour;
  v_opacity = command_placement.z;
  v_kind = in_kind;
  v_effect = in_effect;
  v_detail = in_detail;
  vec2 shift = command_placement.xy;
  if (in_kind == kind_glyph || in_kind == kind_glyph_gradient) {
    shift = sign(shift) * floor(abs(shift) + 0.5);
  }
  vec2 placed = in_position + shift;
  v_screen = placed;
  gl_Position = vec4(placed * scale + translation, 0.0, 1.0);
}
