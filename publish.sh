#!/usr/bin/env bash
# publish.sh -- one-shot publication of skate3-rexglue to GitHub.
#
# Prereq (run once, manually, in your terminal):
#   gh auth login -h github.com
#   # choose HTTPS, "Login with a web browser", paste the device code
#
# Then:
#   bash publish.sh
#
# This script is idempotent: re-running it is safe.

set -euo pipefail

REPO_NAME="skate3-rexglue"
REPO_DESC="Skate 3 (Xbox 360) PC port research via rexglue static recompilation. First public attempt to render the main menu outside Xbox 360 hardware."
GITHUB_USER="xdzleo"

echo "=== Step 1: Verify gh auth ==="
if ! gh auth status >/dev/null 2>&1; then
    echo "ERROR: gh CLI not authenticated."
    echo "Run:  gh auth login -h github.com"
    echo "Then re-run this script."
    exit 1
fi
echo "OK: authenticated as $(gh api user -q .login)"

echo ""
echo "=== Step 2: Create GitHub repo (skip if exists) ==="
if gh repo view "${GITHUB_USER}/${REPO_NAME}" >/dev/null 2>&1; then
    echo "OK: repo already exists at https://github.com/${GITHUB_USER}/${REPO_NAME}"
else
    gh repo create "${REPO_NAME}" --public --description "${REPO_DESC}" \
        --homepage "https://github.com/${GITHUB_USER}/${REPO_NAME}"
    echo "OK: repo created"
fi

echo ""
echo "=== Step 3: Configure origin remote ==="
if git remote get-url origin >/dev/null 2>&1; then
    echo "OK: origin already configured ($(git remote get-url origin))"
else
    git remote add origin "https://github.com/${GITHUB_USER}/${REPO_NAME}.git"
    echo "OK: origin set to https://github.com/${GITHUB_USER}/${REPO_NAME}.git"
fi

echo ""
echo "=== Step 4: Push to main ==="
git push -u origin main
echo "OK: pushed"

echo ""
echo "=== Step 5: Fork rexglue/rexglue-sdk ==="
if gh repo view "${GITHUB_USER}/rexglue-sdk" >/dev/null 2>&1; then
    echo "OK: fork already exists"
else
    gh repo fork rexglue/rexglue-sdk --clone=false
    echo "OK: forked"
fi

echo ""
echo "=== ALL DONE ==="
echo ""
echo "Public repo:  https://github.com/${GITHUB_USER}/${REPO_NAME}"
echo "Your fork:    https://github.com/${GITHUB_USER}/rexglue-sdk"
echo ""
echo "Next manual steps (PR submission to rexglue-sdk upstream):"
echo "  See docs/PR_GUIDE.md  Part 3 onward."
echo ""
echo "Quick PR-01 launch (run from rexglue-sdk dir):"
echo "  cd ../rexglue-sdk"
echo "  git remote add fork https://github.com/${GITHUB_USER}/rexglue-sdk.git"
echo "  git checkout -b pr01-issuecopy-diagnostics"
echo "  # apply the diff from docs/contributions/pr01-issuecopy-diagnostics/changes.patch"
echo "  git apply ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/changes.patch"
echo "  git add -A && git commit -F ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/COMMIT_MSG.txt"
echo "  git push fork pr01-issuecopy-diagnostics"
echo "  gh pr create --repo rexglue/rexglue-sdk \\"
echo "      --base main --head ${GITHUB_USER}:pr01-issuecopy-diagnostics \\"
echo "      --title 'graphics/d3d12: add EDRAM resolve diagnostic instrumentation' \\"
echo "      --body-file ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/PR_BODY.md"
