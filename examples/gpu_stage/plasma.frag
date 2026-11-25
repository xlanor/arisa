#version 460

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_colour;

#ifdef VULKAN
layout(std140, set = 3, binding = 0) uniform Stage
#else
layout(std140, binding = 0) uniform Stage
#endif
{
  vec4 params;
  vec4 encode;
};

const float tau = 6.28318530718;

vec3 palette(float t)
{
  return 0.45 + 0.35 * cos(tau * (vec3(t) + vec3(0.0, 0.1, 0.2)));
}

vec3 plasma(vec2 p, float time)
{
  float v = sin(p.x * 3.0 + time);
  v += sin((p.y * 3.0 + time) * 0.7);
  v += sin((p.x * 2.0 + p.y * 2.0 + time) * 0.6);
  vec2 c = p + vec2(sin(time * 0.33) * 1.5, cos(time * 0.5) * 1.5);
  v += sin(sqrt(dot(c, c) + 1.0) * 2.5 + time);
  return palette(v * 0.25 + time * 0.05);
}

vec3 ripples(vec2 p, float time)
{
  float r = length(p);
  float wave = sin(r * 9.0 - time * 3.0) * exp(-r * 0.6);
  float light = 0.55 + 0.45 * wave;
  return palette(r * 0.15 - time * 0.04) * light;
}

vec3 tunnel(vec2 p, float time)
{
  float r = max(length(p), 1.0e-3);
  float a = atan(p.y, p.x) / tau;
  float depth = 1.0 / r + time * 0.6;
  float bands = step(0.5, fract(depth * 2.0)) * 0.35 + step(0.5, fract(a * 8.0 + time * 0.1)) * 0.25;
  return palette(depth * 0.1) * (0.4 + bands) * clamp(r * 1.6, 0.0, 1.0);
}

vec2 luma_weights(float matrix)
{
  if (matrix < 0.5) {
    return vec2(0.299, 0.114);
  }
  if (matrix < 1.5) {
    return vec2(0.2126, 0.0722);
  }
  return vec2(0.2627, 0.0593);
}

vec4 encoded(vec3 rgb)
{
  vec2 k = luma_weights(encode.y);
  float y = dot(rgb, vec3(k.x, 1.0 - k.x - k.y, k.y));
  vec2 difference = vec2((rgb.b - y) / (2.0 * (1.0 - k.y)), (rgb.r - y) / (2.0 * (1.0 - k.x)));
  bool full = encode.z > 0.5;
  float luma = full ? y : (16.0 + 219.0 * y) / 255.0;
  vec2 chroma = vec2(128.0 / 255.0) + difference * (full ? 1.0 : 224.0 / 255.0);
  if (encode.x < 1.5) {
    return vec4(luma, 0.0, 0.0, 1.0);
  }
  return vec4(chroma, 0.0, 1.0);
}

void main()
{
  float time = params.x;
  float mode = params.y;
  float aspect = params.z;
  vec2 uv = v_uv;
  if (encode.x > 1.5) {
    uv.x -= 0.5 / max(encode.w, 1.0);
  }
  vec2 p = (uv * 2.0 - 1.0) * vec2(aspect, 1.0) * 2.0;
  vec3 colour;
  if (mode < 0.5) {
    colour = plasma(p, time);
  } else if (mode < 1.5) {
    colour = ripples(p, time);
  } else {
    colour = tunnel(p, time);
  }
  out_colour = encode.x < 0.5 ? vec4(colour, 1.0) : encoded(clamp(colour, 0.0, 1.0));
}
