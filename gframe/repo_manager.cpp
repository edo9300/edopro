// Copyright (c) 2019-2020 Edoardo Lolletti <edoardo762@gmail.com>
// Copyright (c) 2020 Dylam De La Torre <dyxel04@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
// Refer to the COPYING file included.

#include "repo_manager.h"
#include <fmt/format.h>
#include "game_config.h"
#include "logging.h"
#include "utils.h"
#include "libgit2.hpp"

static constexpr int MAX_HISTORY_LENGTH = 100;
static constexpr int FETCH_OBJECTS_PERCENTAGE = 60;
static constexpr int DELTA_OBJECTS_PERCENTAGE = 80;
static constexpr int CHECKOUT_PERCENTAGE = 99;

#if LIBGIT2_VER_MAJOR>0 || LIBGIT2_VER_MINOR>=99
#define git_oid_zero(oid) git_oid_is_zero(oid)
#else
#define git_oid_zero(oid) git_oid_iszero(oid)
#endif

namespace ygo {

// GitRepo

// public

bool GitRepo::Sanitize() {
	if(url.empty())
		return false;

	if(repo_path.size())
		repo_path = fmt::format("./{}", repo_path);

	if(repo_name.empty() && repo_path.empty()) {
		repo_name = Utils::GetFileName(url);
		if(repo_name.empty())
			return false;
		repo_path = fmt::format("./repositories/{}", repo_name);
	} else if(repo_name.empty())
		repo_name = Utils::GetFileName(repo_path);
	else if(repo_path.empty())
		repo_path = fmt::format("./repositories/{}", repo_name);

	data_path = Utils::NormalizePath(fmt::format("{}/{}/", repo_path, data_path));

	if(lflist_path.size())
		lflist_path = Utils::NormalizePath(fmt::format("{}/{}/", repo_path, lflist_path));
	else
		lflist_path = Utils::NormalizePath(fmt::format("{}/lflists/", repo_path));

	if(script_path.size())
		script_path = Utils::NormalizePath(fmt::format("{}/{}/", repo_path, script_path));
	else
		script_path = Utils::NormalizePath(fmt::format("{}/script/", repo_path));

	if(pics_path.size())
		pics_path = Utils::NormalizePath(fmt::format("{}/{}/", repo_path, pics_path));
	else
		pics_path = Utils::NormalizePath(fmt::format("{}/pics/", repo_path));

	if(has_core || core_path.size()) {
		has_core = true;
		core_path = Utils::NormalizePath(fmt::format("{}/{}/", repo_path, core_path));
	}
	return true;
}

// RepoManager

// public

RepoManager::RepoManager() {
	git_libgit2_init();
	if(gGameConfig->ssl_certificate_path.size() && Utils::FileExists(Utils::ToPathString(gGameConfig->ssl_certificate_path)))
		git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, gGameConfig->ssl_certificate_path.data(), "");
#ifdef _WIN32
	else
		git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, "SYSTEM", "");
#endif
	git_libgit2_opts(GIT_OPT_SET_USER_AGENT, ygo::Utils::GetUserAgent().data());
#if (LIBGIT2_VER_MAJOR>0 && LIBGIT2_VER_MINOR>=3) || LIBGIT2_VER_MAJOR>1
	// disable option introduced with https://github.com/libgit2/libgit2/pull/6266
	// due how this got backported in older libgitversion as well, and in case
	// the client is built with a dynamic version of libgit2 that could have it
	// enabled by default, always try to disable it regardless if the version
	// that's being compiled has the option available or not.
	// EDOPro only uses the library to clone and fetch repositories, so it's not
	// affected by the vulnerability that this option would potentially fix
	// also the fix could give issues with users doing random stuff on their end
	// and would only cause annoyances.
	static constexpr int OPT_SET_OWNER_VALIDATION = GIT_OPT_SET_EXTENSIONS + 2;
	git_libgit2_opts(OPT_SET_OWNER_VALIDATION, 0);
#endif
	for(int i = 0; i < 3; i++)
		cloning_threads.emplace_back(&RepoManager::CloneOrUpdateTask, this);
}

RepoManager::~RepoManager() {
	TerminateThreads();
}

size_t RepoManager::GetUpdatingReposNumber() const {
	return available_repos.size();
};

std::vector<const GitRepo*> RepoManager::GetAllRepos() const {
	std::vector<const GitRepo*> res;
	res.reserve(all_repos_count);
	for(const auto& repo : all_repos)
		res.insert(res.begin(), &repo);
	return res;
}

std::vector<const GitRepo*> RepoManager::GetReadyRepos() {
	std::vector<const GitRepo*> res;
	if(available_repos.empty())
		return res;
	auto it = available_repos.cbegin();
	{
		std::lock_guard<std::mutex> lck(syncing_mutex);
		for(; it != available_repos.cend(); it++) {
			//set internal_ready instead of ready as that's not thread safe
			//and it'll be read synchronously from the main thread after calling
			//GetAllRepos
			if(!((*it)->ready = (*it)->internal_ready))
				break;
			res.push_back(*it);
		}
	}
	if(res.size())
		available_repos.erase(available_repos.cbegin(), it);
	//by design, once repositories are added at startup, no new ones are added
	//so when they finish, the threads are no longer needed
	if(available_repos.empty())
		TerminateThreads();
	return res;
}

std::map<std::string, int> RepoManager::GetRepoStatus() {
	std::lock_guard<std::mutex> lock(syncing_mutex);
	return repos_status;
}

#define JSON_SET_IF_VALID(field, jsontype, cpptype) \
	do { auto it = obj.find(#field); \
		if(it != obj.end() && it->is_##jsontype()) \
			tmp_repo.field = it->get<cpptype>(); \
	} while(0)

void RepoManager::LoadRepositoriesFromJson(const nlohmann::json& configs) {
	auto cit = configs.find("repos");
	if(cit != configs.end() && cit->is_array()) {
		for(auto& obj : *cit) {
			{
				auto it = obj.find("should_read");
				if(it != obj.end() && it->is_boolean() && !it->get<bool>())
					continue;
			}
			GitRepo tmp_repo;
			JSON_SET_IF_VALID(url, string, std::string);
			JSON_SET_IF_VALID(should_update, boolean, bool);
			if(tmp_repo.url == "default") {
#ifdef DEFAULT_LIVE_URL
				tmp_repo.url = DEFAULT_LIVE_URL;
#ifdef YGOPRO_BUILD_DLL
				tmp_repo.has_core = true;
#endif
#else
				continue;
#endif //DEFAULT_LIVE_URL
			} else if(tmp_repo.url == "default_anime") {
#ifdef DEFAULT_LIVEANIME_URL
				tmp_repo.url = DEFAULT_LIVEANIME_URL;
#else
				continue;
#endif //DEFAULT_LIVEANIME_URL
			} else {
				JSON_SET_IF_VALID(repo_path, string, std::string);
				JSON_SET_IF_VALID(repo_name, string, std::string);
				JSON_SET_IF_VALID(data_path, string, std::string);
				JSON_SET_IF_VALID(lflist_path, string, std::string);
				JSON_SET_IF_VALID(script_path, string, std::string);
				JSON_SET_IF_VALID(pics_path, string, std::string);
				JSON_SET_IF_VALID(is_language, boolean, bool);
				if(tmp_repo.is_language)
					JSON_SET_IF_VALID(language, string, std::string);
#ifdef YGOPRO_BUILD_DLL
				JSON_SET_IF_VALID(has_core, boolean, bool);
				if(tmp_repo.has_core)
					JSON_SET_IF_VALID(core_path, string, std::string);
#endif
			}
			if(tmp_repo.Sanitize())
				AddRepo(std::move(tmp_repo));
		}
	}
}

bool RepoManager::TerminateIfNothingLoaded() {
	if(all_repos_count > 0)
		return false;
	TerminateThreads();
	return true;
}

void RepoManager::TerminateThreads() {
	if(fetchReturnValue != -1) {
		fetchReturnValue = -1;
		cv.notify_all();
		for(auto& thread : cloning_threads)
			thread.join();
		cloning_threads.clear();
		git_libgit2_shutdown();
	}
}

// private

void RepoManager::AddRepo(GitRepo&& repo) {
	std::lock_guard<std::mutex> lck(syncing_mutex);
	if(repos_status.find(repo.repo_path) != repos_status.end())
		return;
	repos_status.emplace(repo.repo_path, 0);
	all_repos.push_front(std::move(repo));
	auto* _repo = &all_repos.front();
	available_repos.push_back(_repo);
	to_sync.push(_repo);
	all_repos_count++;
	cv.notify_one();
}

void RepoManager::SetRepoPercentage(const std::string& path, int percent)
{
	std::lock_guard<std::mutex> lock(syncing_mutex);
	repos_status[path] = percent;
}

void RepoManager::CloneOrUpdateTask() {
	Utils::SetThreadName("Git update task");
	while(fetchReturnValue != -1) {
		std::unique_lock<std::mutex> lck(syncing_mutex);
		while(to_sync.empty()) {
			cv.wait(lck);
			if(fetchReturnValue == -1)
				return;
		}
		auto& _repo = *to_sync.front();
		to_sync.pop();
		lck.unlock();
		GitRepo::CommitHistory history;
		try {
			auto DoesRepoExist = [](const char* path) -> bool {
				git_repository* tmp = nullptr;
				int status = git_repository_open_ext(&tmp, path,
													 GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr);
				git_repository_free(tmp);
				return status == 0;
			};
			auto AppendCommit = [](std::vector<std::string>& v, git_commit* commit) {
				std::string message{ git_commit_message(commit) };
				message.resize(message.find_last_not_of(" \n") + 1);
				auto authorName = git_commit_author(commit)->name;
				v.push_back(fmt::format("{:s}\nAuthor: {:s}\n", message, authorName));
			};
			auto QueryFullHistory = [&](git_repository* repo, git_revwalk* walker) {
				git_revwalk_reset(walker);
				// git log HEAD~MAX_HISTORY_LENGTH..HEAD
				Git::Check(git_revwalk_push_head(walker));
				for(git_oid oid; git_revwalk_next(&oid, walker) == 0;) {
					auto commit = Git::MakeUnique(git_commit_lookup, repo, &oid);
					if(git_oid_zero(&oid) || history.full_history.size() > MAX_HISTORY_LENGTH)
						break;
					AppendCommit(history.full_history, commit.get());
				}
			};
			auto QueryPartialHistory = [&](git_repository* repo, git_revwalk* walker) {
				git_revwalk_reset(walker);
				// git log HEAD..FETCH_HEAD
				Git::Check(git_revwalk_push_range(walker, "HEAD..FETCH_HEAD"));
				for(git_oid oid; git_revwalk_next(&oid, walker) == 0;) {
					auto commit = Git::MakeUnique(git_commit_lookup, repo, &oid);
					AppendCommit(history.partial_history, commit.get());
				}
			};
			const std::string& url = _repo.url;
			const std::string& path = _repo.repo_path;
			GitCbPayload payload{ this, path };
			if(DoesRepoExist(path.data())) {
				auto repo = Git::MakeUnique(git_repository_open_ext, path.data(),
											GIT_REPOSITORY_OPEN_NO_SEARCH, nullptr);
				auto walker = Git::MakeUnique(git_revwalk_new, repo.get());
				git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
				if(_repo.should_update) {
					try {
						// git fetch
						git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
						fetchOpts.callbacks.transfer_progress = RepoManager::FetchCb;
						fetchOpts.callbacks.payload = &payload;
						auto remote = Git::MakeUnique(git_remote_lookup, repo.get(), "origin");
						Git::Check(git_remote_fetch(remote.get(), nullptr, &fetchOpts, nullptr));
						QueryPartialHistory(repo.get(), walker.get());
						SetRepoPercentage(path, DELTA_OBJECTS_PERCENTAGE);
						// git reset --hard FETCH_HEAD
						git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
						checkoutOpts.progress_cb = RepoManager::CheckoutCb;
						checkoutOpts.progress_payload = &payload;
						git_oid oid;
						Git::Check(git_reference_name_to_id(&oid, repo.get(), "FETCH_HEAD"));
						auto commit = Git::MakeUnique(git_commit_lookup, repo.get(), &oid);
						Git::Check(git_reset(repo.get(), reinterpret_cast<git_object*>(commit.get()),
											 GIT_RESET_HARD, &checkoutOpts));
					}
					catch(const std::exception& e) {
						history.partial_history.clear();
						history.warning = e.what();
						ErrorLog("Warning occurred in repo {}: {}", url, history.warning);
					}
				}
				if(history.partial_history.size() >= MAX_HISTORY_LENGTH) {
					history.full_history.swap(history.partial_history);
					history.partial_history.clear();
					history.partial_history.push_back(history.full_history.front());
					history.partial_history.push_back(history.full_history.back());
				} else
					QueryFullHistory(repo.get(), walker.get());
			} else {
				Utils::DeleteDirectory(Utils::ToPathString(path + "/"));
				// git clone <url> <path>
				git_clone_options cloneOpts = GIT_CLONE_OPTIONS_INIT;
				cloneOpts.fetch_opts.callbacks.transfer_progress = RepoManager::FetchCb;
				cloneOpts.fetch_opts.callbacks.payload = &payload;
				cloneOpts.checkout_opts.progress_cb = RepoManager::CheckoutCb;
				cloneOpts.checkout_opts.progress_payload = &payload;
				auto repo = Git::MakeUnique(git_clone, url.data(), path.data(), &cloneOpts);
				auto walker = Git::MakeUnique(git_revwalk_new, repo.get());
				git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
				QueryFullHistory(repo.get(), walker.get());
			}
			SetRepoPercentage(path, 100);
		}
		catch(const std::exception& e) {
			history.error = e.what();
			ErrorLog("Exception occurred in repo {}: {}", _repo.url, history.error);
		}
		lck.lock();
		_repo.history = std::move(history);
		_repo.internal_ready = true;
	}
}

int RepoManager::FetchCb(const git_indexer_progress* stats, void* payload) {
	int percent;
	if(stats->received_objects != stats->total_objects) {
		percent = (FETCH_OBJECTS_PERCENTAGE * stats->received_objects) / stats->total_objects;
	} else if(stats->total_deltas == 0) {
		percent = FETCH_OBJECTS_PERCENTAGE;
	} else {
		static constexpr auto DELTA_INCREMENT = DELTA_OBJECTS_PERCENTAGE - FETCH_OBJECTS_PERCENTAGE;
		percent = FETCH_OBJECTS_PERCENTAGE + ((DELTA_INCREMENT * stats->indexed_deltas) / stats->total_deltas);
	}
	auto pl = static_cast<GitCbPayload*>(payload);
	pl->rm->SetRepoPercentage(pl->path, percent);
	return pl->rm->fetchReturnValue;
}

void RepoManager::CheckoutCb(const char* path, size_t completed_steps, size_t total_steps, void* payload) {
	int percent;
	if(total_steps == 0)
		percent = CHECKOUT_PERCENTAGE;
	else {
		static constexpr auto DELTA_INCREMENT = CHECKOUT_PERCENTAGE - DELTA_OBJECTS_PERCENTAGE;
		percent = DELTA_OBJECTS_PERCENTAGE + ((DELTA_INCREMENT * completed_steps) / total_steps);
	}
	auto pl = static_cast<GitCbPayload*>(payload);
	pl->rm->SetRepoPercentage(pl->path, percent);
}

}
