#ifndef SOUND_SDL_MIXER_H
#define SOUND_SDL_MIXER_H
#include "../sound_threaded_backend.h"

class SoundMixerBase;

using SoundMixer = SoundThreadedBackendHelper<SoundMixerBase>;

#endif //SOUND_SDL_MIXER_H
