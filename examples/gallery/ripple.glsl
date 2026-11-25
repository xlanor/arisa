vec2 ripple(vec2 position)
{
  vec2 centre = arisa_size() * 0.5;
  vec2 away = position - centre;
  float distance = length(away);
  float wave = sin(distance * 0.12 - arisa_argument(0) * 6.0) * arisa_argument(1);
  return position + (distance > 0.001 ? away / distance : vec2(0.0)) * wave;
}
