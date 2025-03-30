// Copyright (c) 2019-2020 Edoardo Lolletti <edoardo762@gmail.com>
// Copyright (c) 2020 Dylam De La Torre <dyxel04@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
// Refer to the COPYING file included.

#ifndef REPOMANAGER_H
#define REPOMANAGER_H
#include <atomic>
#include <map>
#include <forward_list>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <queue>
#include "epro_thread.h"
#include "epro_mutex.h"
#include "epro_condition_variable.h"

namespace ygo {

class GitRepo {
public:
	// first = all changes history, second = only HEAD..FETCH_HEAD changes
	struct CommitHistory {
		std::vector<std::string> full_history;
		std::vector<std::string> partial_history;
		std::string error;
		std::string warning;
	};
	std::string url{};
	std::string repo_name{};
	std::string repo_path{};
	std::string data_path{};
	std::string lflist_path{"lflists"};
	std::string script_path{"script"};
	std::string pics_path{"pics"};
	std::string core_path{};
	std::string language{};
	bool not_git_repo{false};
	bool should_update{true};
	bool has_core{false};
	bool ready{false};
	bool is_language{false};
	CommitHistory history;
	bool Sanitize();
	friend class RepoManager;
private:
	bool internal_ready{ false };
};

class RepoManager {
public:

	RepoManager();
	// Cancel fetching of repos and synchronize with futures
	~RepoManager();

	size_t GetAllReposNumber() const;
	size_t GetUpdatingReposNumber() const;
	std::vector<const GitRepo*> GetAllRepos() const;
	std::vector<const GitRepo*> GetReadyRepos(); // changes available_repos
	std::map<std::string, int> GetRepoStatus(); // locks mutex

	void ToggleReadOnly(bool readOnly) {
		read_only = readOnly;
	}

	bool IsReadOnly() const { return read_only; }

	void LoadRepositoriesFromJson(const nlohmann::json& configs);
	bool TerminateIfNothingLoaded();
private:
	void TerminateThreads();
	bool read_only{false};
	std::forward_list<GitRepo> all_repos{};
	size_t all_repos_count{};
	std::vector<GitRepo*> available_repos{};
	std::map<std::string, int> repos_status{};
	std::queue<GitRepo*> to_sync;
	epro::mutex syncing_mutex;
	epro::condition_variable cv;
	std::vector<epro::thread> cloning_threads;
	// Initialized with GIT_OK (0), changed to cancel fetching
	std::atomic<int> fetchReturnValue{0};

	void AddRepo(GitRepo&& repo);
	void SetRepoPercentage(const std::string& path, int percent);

	// Will be started on a new thread
	void CloneOrUpdateTask();

	// libgit2 Callbacks stuff
	struct GitCbPayload
	{
		RepoManager* rm;
		const std::string& path;
	};
	template<typename git_indexer_progress>
	static int FetchCb(const git_indexer_progress* stats, void* payload);
	static void CheckoutCb(const char* path, size_t completed_steps, size_t total_steps, void* payload);
};

extern RepoManager* gRepoManager;

}

#endif // REPOMANAGER_H
