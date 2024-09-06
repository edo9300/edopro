#include "repo_cloner.h"

#include "config.h"
#include "game_config.h"
#include "porting.h"
#include "repo_manager.h"
#include "text_types.h"
#include "utils.h"

#include <cstdlib>
#include <cstdio>
#include <fmt/ranges.h>
#include <map>
#include <memory>
#include <thread>

#include <IOSOperator.h>

using namespace ygo;

class CDummyOSOperator : public irr::IOSOperator {
public:
	CDummyOSOperator() = default;
	const irr::core::stringc& getOperatingSystemVersion() const override { return osVersion; }
	void copyToClipboard(const wchar_t* text) const override { return; }
	const wchar_t* getTextFromClipboard() const override { return nullptr; }
	bool getProcessorSpeedMHz(irr::u32* MHz) const override { return false; }
	bool getSystemMemory(irr::u32* Total, irr::u32* Avail) const override { return false; }
private:
	const irr::core::stringc osVersion;
};

struct GitRepoInfoToBePrinted {
	std::string name;
	std::string status; /*error, warning, ok or progress*/
	std::string warning_or_error_message;
	int percentage;
};

template<>
struct fmt::formatter<GitRepoInfoToBePrinted> {
	constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
	template <typename Context>
	constexpr auto format(const GitRepoInfoToBePrinted& repo, Context& ctx) const {
		return format_to(ctx.out(),
						 R"("name":"{}","status":"{}","warning_or_error_message":"{}","percentage":{})",
						 repo.name, repo.status, repo.warning_or_error_message, repo.percentage);
	}
};

int repo_cloner_main(const args_t& args) {
	auto configs = std::make_unique<GameConfig>();
	gGameConfig = configs.get();
	// RepoManager uses Utils::GetUserAgent, that in turn calls Utils::OSOperator::getOperatingSystemVersion
	// populate it with a dummy implementation
	CDummyOSOperator osOperator;
	Utils::OSOperator = &osOperator;
#if EDOPRO_IOS
	configs->ssl_certificate_path = epro::format("{}/cacert.pem", Utils::GetWorkingDirectory());
#elif EDOPRO_ANDROID
	configs->ssl_certificate_path = epro::format("{}/cacert.pem", porting::internal_storage);
#else
	if(configs->override_ssl_certificate_path.size()) {
		if(configs->override_ssl_certificate_path != "none" && Utils::FileExists(Utils::ToPathString(configs->override_ssl_certificate_path)))
			configs->ssl_certificate_path = configs->override_ssl_certificate_path;
	} else
		configs->ssl_certificate_path = epro::format("{}/cacert.pem", Utils::ToUTF8IfNeeded(Utils::GetWorkingDirectory()));
#endif

	auto gitManager = std::make_unique<RepoManager>();
	gitManager->LoadRepositoriesFromJson(configs->user_configs);
	gitManager->LoadRepositoriesFromJson(configs->configs);
	if(gitManager->TerminateIfNothingLoaded())
		return EXIT_SUCCESS;
	auto all_repos = gitManager->GetAllRepos();
	std::vector<GitRepoInfoToBePrinted> repos_to_clone;
	std::map<std::string, GitRepoInfoToBePrinted*> repos_to_clone_back_map;
	repos_to_clone.reserve(all_repos.size());
	for(const auto& repo : all_repos) {
		repos_to_clone.emplace_back(GitRepoInfoToBePrinted{ repo->repo_name, "progress", "", 0});
		repos_to_clone_back_map.emplace(repo->repo_path, &repos_to_clone.back());
	}
	bool there_was_any_error = false;
	while(gitManager->GetUpdatingReposNumber() > 0) {
		bool should_print = false;
		for(const auto& repo : gitManager->GetRepoStatus()) {
			const auto& path = repo.first;
			auto percentage = repo.second;
			if(std::exchange(repos_to_clone_back_map.at(path)->percentage, percentage) != percentage)
				should_print = true;
		}
		for(const auto& ready_repo : gitManager->GetReadyRepos()) {
			should_print = true;
			auto& repo = *repos_to_clone_back_map.at(ready_repo->repo_path);
			auto& status = repo.status;
			if(ready_repo->history.error.size() != 0) {
				status = "error";
				repo.warning_or_error_message = ready_repo->history.error;
				there_was_any_error = true;
			} else if(ready_repo->history.warning.size() != 0) {
				status = "warning";
				repo.warning_or_error_message = ready_repo->history.warning;
			} else {
				status = "ok";
			}
		}
		if(should_print) {
			// prints a json array contaning all the info about the repos being cloned
			epro::print("[{{{}}}]\n", fmt::join(repos_to_clone.begin(), repos_to_clone.end(), "},{"));
			std::fflush(stdout);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
	return there_was_any_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
