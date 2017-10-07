#!/bin/bash
set -e # Exit with nonzero exit code if anything fails

SOURCE_BRANCH="master"
TARGET_BRANCH="gh-pages"
PAGES_DIR="_builds/docs/html"
# GITHUB_TOKEN - set as private variable
echo "Starting deployment"
echo "Target: ${TARGET_BRANCH} branch"

CURRENT_COMMIT=`git rev-parse HEAD`
ORIGIN_URL=`git config --get remote.origin.url`
ORIGIN_URL_WITH_CREDENTIALS=${ORIGIN_URL/\/\/github.com/\/\/$GITHUB_TOKEN@github.com}

# Clone the existing gh-pages for this repo into gh-pages-deploy/
# Create a new empty branch if gh-pages doesn't exist yet (should only happen on first deply)
echo "Checking out ${TARGET_BRANCH} branch"
git clone ${ORIGIN_URL} gh-pages-deploy
cd gh-pages-deploy
git checkout ${TARGET_BRANCH} || git checkout --orphan ${TARGET_BRANCH}

echo "Removing old static content"
git rm -rf .

echo "Copying pages content to root"
cp -r ../${PAGES_DIR}/* .

echo "Pushing new content to $ORIGIN_URL"
git config user.name "Travis CI" || exit 1
git config user.email "$COMMIT_AUTHOR_EMAIL" || exit 1

# If there are no changes to the compiled out (e.g. this is a README update) then just bail.
if git diff --quiet; then
    echo "No changes to the output on this push; exiting."
    exit 0
fi

git add -A .
git commit -m "Deploy to GitHub Pages: ${CURRENT_COMMIT}"
git push --quiet "${ORIGIN_URL_WITH_CREDENTIALS}" gh-pages > /dev/null 2>&1

echo "Deployed successfully."
exit 0
