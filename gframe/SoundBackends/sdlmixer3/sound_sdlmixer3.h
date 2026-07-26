#ifndef SOUND_SDL_MIXER3_H
#define SOUND_SDL_MIXER3_H
#include "../sound_threaded_backend.h"

class SoundMixer3Base;

using SoundMixer3 = SoundThreadedBackendHelper<SoundMixer3Base>;

#endif //SOUND_SDL_MIXER_H
