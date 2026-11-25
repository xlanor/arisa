#version 460

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_colour;

layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_colour;

#if defined(ARISA_PUSH_CONSTANTS)
layout(push_constant) uniform Command
#else
#ifdef VULKAN
layout(std140, set = 1, binding = 0) uniform Transform
#else
layout(std140, binding = 0) uniform Transform
#endif
#endif
{
  vec2 scale;
  vec2 translation;
};

void main()
{
  v_uv = in_uv;
  v_colour = in_colour;
  gl_Position = vec4(in_position * scale + translation, 0.0, 1.0);
}
