#version 460

#ifdef VULKAN
#define vertex_index gl_VertexIndex
#else
#define vertex_index gl_VertexID
#endif

layout(location = 0) out vec2 v_uv;

void main()
{
  vec2 corner = vec2(float((vertex_index << 1) & 2), float(vertex_index & 2));
  v_uv = corner;
  gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
