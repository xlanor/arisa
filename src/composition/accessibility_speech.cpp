#include <arisa/composition/accessibility_speech.hpp>

namespace arisa::composition {

auto AccessibilitySpeech::over(const AccessibilitySpeech& inherited) const
    -> AccessibilitySpeech {
  return AccessibilitySpeech{
      .spells_out_characters = spells_out_characters.has_value()
                                   ? spells_out_characters
                                   : inherited.spells_out_characters,
      .always_includes_punctuation =
          always_includes_punctuation.has_value()
              ? always_includes_punctuation
              : inherited.always_includes_punctuation,
      .announcements_queued = announcements_queued.has_value()
                                  ? announcements_queued
                                  : inherited.announcements_queued,
      .adjusted_pitch = adjusted_pitch.has_value() ? adjusted_pitch
                                                   : inherited.adjusted_pitch};
}

}
