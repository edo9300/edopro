#ifndef SOUND_SFML_H
#define SOUND_SFML_H
#include "../sound_threaded_backend.h"

class SoundSFMLBase;

using SoundSFML = SoundThreadedBackendHelper<SoundSFMLBase>;

#endif //SOUND_SFML_H
