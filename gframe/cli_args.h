#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include <array>
#include "text_types.h"

enum LAUNCH_PARAM {
	WORK_DIR,
	MUTE,
	CHANGELOG,
	DISCORD,
	OVERRIDE_UPDATE_URL,
	WANTS_TO_RUN_AS_ADMIN,
	REPOS_READ_ONLY,
	ONLY_CLONE_REPOS,
	COUNT,
};


struct Option {
	bool enabled{ false };
	epro::path_stringview argument;
};

using args_t = std::array<Option, LAUNCH_PARAM::COUNT>;

extern args_t cli_args;

#endif //CLI_ARGS_H
