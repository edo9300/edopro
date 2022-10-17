#!/usr/bin/env bash

set -euo pipefail

cd $DEPLOY_DIR
git init
git checkout --orphan $DEPLOY_BRANCH
git config user.email github-actions[bot]@users.noreply.github.com
git config user.name "github-actions[bot]"
git add -A .
git commit -qm "Deploy $DEPLOY_REPO to $DEPLOY_REPO:$DEPLOY_BRANCH"
git push -qf https://$DEPLOY_TOKEN@github.com/$DEPLOY_REPO.git $DEPLOY_BRANCH:$DEPLOY_BRANCH
