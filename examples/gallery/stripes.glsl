vec4 stripes(vec2 position, vec4 colour)
{
  float band = step(0.5, fract(position.x / max(arisa_argument(0), 1.0)));
  return vec4(colour.rgb * mix(1.0, 0.55, band), colour.a);
}
