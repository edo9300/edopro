#include "compiler_features.h"
#include "cli_args.h"
#include "text_types.h"
#include "repo_cloner.h"

#if EDOPRO_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Tchar.h> //_tmain
#define real_main _tmain
#include "winmain.inl"
#elif (EDOPRO_IOS || EDOPRO_ANDROID)
#define real_main edopro_main
#else
#define real_main main
#endif //EDOPRO_WINDOWS
#if EDOPRO_POSIX
#include <clocale>
#include <unistd.h>
#include <signal.h>
#endif //EDOPRO_POSIX

args_t cli_args;
int edopro_main(const args_t& cli_args);

namespace {
auto GetOption(epro::path_stringview option) {
	if(option.size() == 1) {
		switch(option.front()) {
		case EPRO_TEXT('C'): return LAUNCH_PARAM::WORK_DIR;
		case EPRO_TEXT('m'): return LAUNCH_PARAM::MUTE;
		case EPRO_TEXT('l'): return LAUNCH_PARAM::CHANGELOG;
		case EPRO_TEXT('D'): return LAUNCH_PARAM::DISCORD;
		case EPRO_TEXT('u'): return LAUNCH_PARAM::OVERRIDE_UPDATE_URL;
		case EPRO_TEXT('r'): return LAUNCH_PARAM::REPOS_READ_ONLY;
		case EPRO_TEXT('c'): return LAUNCH_PARAM::ONLY_CLONE_REPOS;
		default: return LAUNCH_PARAM::COUNT;
		}
	}
	if(option == EPRO_TEXT("i-want-to-be-admin"sv))
		return LAUNCH_PARAM::WANTS_TO_RUN_AS_ADMIN;
	return LAUNCH_PARAM::COUNT;
}

auto ParseArguments(int argc, epro::path_char* argv[]) {
	args_t res;
	for(int i = 1; i < argc; ++i) {
		epro::path_stringview parameter = argv[i];
		if(parameter.size() < 2)
			break;
		if(parameter[0] == EPRO_TEXT('-')) {
			auto launch_param = GetOption(parameter.substr(1));
			if(launch_param == LAUNCH_PARAM::COUNT)
				continue;
			epro::path_stringview argument;
			if(i + 1 < argc) {
				const auto* next = argv[i + 1];
				if(next[0] != EPRO_TEXT('-')) {
					argument = next;
					i++;
				}
			}
			res[launch_param] = {true,  argument};
			continue;
		}
	}
	return res;
}
}

extern "C" int real_main(int argc, epro::path_char** argv) {
#if EDOPRO_POSIX
	setlocale(LC_CTYPE, "UTF-8");
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	(void)sigaction(SIGCHLD, &sa, 0);
#endif //EDOPRO_POSIX
	cli_args = ParseArguments(argc, argv);
	if(cli_args[ONLY_CLONE_REPOS].enabled)
		return repo_cloner_main(cli_args);
	return edopro_main(cli_args);
}
