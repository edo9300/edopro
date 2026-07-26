#ifndef SOUND_MINIAUDIO_H
#define SOUND_MINIAUDIO_H
#include "../sound_threaded_backend.h"

class SoundMiniaudioBase;

using SoundMiniaudio = SoundThreadedBackendHelper<SoundMiniaudioBase>;

#endif //SOUND_MINIAUDIO_H
