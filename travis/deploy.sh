#!/usr/bin/env bash

set -euo pipefail

cd $DEPLOY_DIR
git init
git remote add origin https://$DEPLOY_TOKEN@github.com/$DEPLOY_REPO.git
git config user.email github-actions[bot]@users.noreply.github.com
git config user.name "github-actions[bot]"
echo $GITHUB_SHA > commit
if ! git fetch -q origin $DEPLOY_BRANCH --depth 1; then
	git checkout --orphan $DEPLOY_BRANCH
else
	mkdir ./.tmp
	mv ./* ./.tmp
	git checkout $DEPLOY_BRANCH
	rm -r ./*
	mv ./.tmp/* .
	rm -r ./.tmp
fi
git add -A .
git commit -qm "$GITHUB_SHA $DEPLOY_BRANCH"
git push -qf origin $DEPLOY_BRANCH:$DEPLOY_BRANCH
