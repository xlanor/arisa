vec4 bands(vec2 position, vec4 colour)
{
  int weights = int(arisa_argument(0));
  int palette = 1 + weights;
  int count = int(arisa_argument(palette));
  int bounds = palette + 1 + count * 4;
  float width = max(arisa_argument(bounds + 2), 1.0);
  float total = 0.0;
  for (int index = 0; index < weights; ++index) {
    total += arisa_argument(1 + index);
  }
  float along = clamp(position.x / width, 0.0, 0.9999) * max(total, 1.0e-4);
  int chosen = 0;
  float reached = 0.0;
  for (int index = 0; index < weights; ++index) {
    reached += arisa_argument(1 + index);
    if (along < reached) {
      chosen = index;
      break;
    }
  }
  chosen = min(chosen, max(count - 1, 0));
  int at = palette + 1 + chosen * 4;
  return vec4(arisa_argument(at), arisa_argument(at + 1), arisa_argument(at + 2), colour.a);
}
