#version 460

layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_colour;

#if defined(ARISA_PUSH_CONSTANTS)
layout(set = 0, binding = 0) uniform sampler2D atlas;
#else
#ifdef VULKAN
layout(set = 2, binding = 0) uniform sampler2D atlas;
#else
layout(binding = 0) uniform sampler2D atlas;
#endif
#endif

layout(location = 0) out vec4 out_colour;

void main()
{
  out_colour = v_colour * texture(atlas, v_uv);
}
