# Guide — How to Publish This Work to GitHub

This guide walks you (@xdzleo) through publishing the project to your GitHub and submitting the PRs to rexglue-sdk upstream. **Follow the steps in order.** Each step is verifiable.

## Part 1 — GitHub authentication (one-time)

Your `gh` CLI is installed but token is expired. Re-auth:

```bash
gh auth login -h github.com
```

Choose:
- HTTPS (or SSH if you have keys set up)
- Authenticate with browser (easiest)
- Open the URL it shows in your browser, paste the device code

Verify:

```bash
gh auth status
# Expected: "Logged in to github.com account xdzleo"
```

## Part 2 — Initialize the skate3-rexglue project as your own repo

The skate3-rexglue folder is currently NOT a git repo. Initialize it:

```bash
cd C:/Users/admin/Downloads/xenia-analysis/skate3-rexglue

# Initialize repo
git init
git checkout -b main

# Add a .gitignore to avoid committing huge build artifacts
cat > .gitignore <<'EOF'
build/
out/
generated/
dumps/
*.iso
*.xex
*.log
*.bin
*.pdb
*.obj
*.exp
*.lib
*.exe
.cache/
node_modules/
EOF

# Initial commit — code + docs only
git add docs/ src/ build/skate3.toml CMakeLists.txt CMakePresets.json README.md .gitignore
git commit -m "Initial commit: Skate 3 PC port research + rexglue contributions

- 53 hooked guest functions, 38 custom hook bodies
- VP6 decoder bridge (libavcodec, 521 lines)
- 2,419 lines of game-specific hooks
- 26 research notes documenting cycles 16-24
- 6 PR proposals ready for rexglue-sdk upstream
- Comprehensive ARCHITECTURE.md, RESULTS.md, cookbook"
```

Create the repo on GitHub and push:

```bash
# Create a public repo
gh repo create skate3-rexglue --public --description "Skate 3 (Xbox 360) PC port research via rexglue static recompilation. First public attempt to render the main menu."

# Add the remote and push
git remote add origin https://github.com/xdzleo/skate3-rexglue.git
git push -u origin main
```

Verify at: https://github.com/xdzleo/skate3-rexglue

## Part 3 — Submit PRs to rexglue-sdk

The rexglue-sdk repo is at `github.com/rexglue/rexglue-sdk`. Your local clone is at `C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk` and is 20 commits ahead of origin/main with uncommitted changes.

### 3.1 — Fork rexglue-sdk to your account (one-time)

```bash
gh repo fork rexglue/rexglue-sdk --clone=false
```

### 3.2 — Set your fork as a remote

```bash
cd C:/Users/admin/Downloads/xenia-analysis/rexglue-sdk
git remote add fork https://github.com/xdzleo/rexglue-sdk.git
git remote -v
# Expected:
#   origin  https://github.com/rexglue/rexglue-sdk.git (fetch)
#   origin  https://github.com/rexglue/rexglue-sdk.git (push)
#   fork    https://github.com/xdzleo/rexglue-sdk.git (fetch)
#   fork    https://github.com/xdzleo/rexglue-sdk.git (push)
```

### 3.3 — Save your local work

Your local rexglue-sdk has 20 unpushed commits + uncommitted changes. Save them to a backup branch:

```bash
git checkout -b backup/full-skate3-research
git add -A
git commit -m "WIP: full Skate 3 research state (uncurated combined diff)"
git push fork backup/full-skate3-research
```

This preserves all your work. You'll cherry-pick clean commits for each PR.

### 3.4 — Submit PR 01 (IssueCopy diagnostics)

Branch off main:

```bash
git checkout main
git fetch origin
git reset --hard origin/main  # discard local commits temporarily; they're saved in backup branch

git checkout -b pr01-issuecopy-diagnostics
```

Apply only the IssueCopy entry diagnostic from your saved branch:

```bash
# Cherry-pick the specific commit, OR manually re-create the change.
# Since the changes are interleaved with other patches in the backup branch,
# easier to re-apply manually using the PR_BODY.md as guide:

# 1. Open src/graphics/d3d12/command_processor.cpp
# 2. Find IssueCopy() at line ~3115
# 3. Add the diagnostic block at function entry (see docs/contributions/pr01-issuecopy-diagnostics/PR_BODY.md)
# 4. Save

git add src/graphics/d3d12/command_processor.cpp
git commit -F ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/COMMIT_MSG.txt
git push fork pr01-issuecopy-diagnostics
```

Open the PR:

```bash
gh pr create \
    --repo rexglue/rexglue-sdk \
    --base main \
    --head xdzleo:pr01-issuecopy-diagnostics \
    --title "graphics/d3d12: add EDRAM resolve diagnostic instrumentation" \
    --body-file ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/PR_BODY.md
```

### 3.5 — Repeat for PRs 02 and 03

Same workflow:

```bash
git checkout main
git checkout -b pr02-readback-path-logging
# ... apply PR 02 changes ...
git commit -F ../skate3-rexglue/docs/contributions/pr02-readback-path-logging/COMMIT_MSG.txt
git push fork pr02-readback-path-logging
gh pr create --repo rexglue/rexglue-sdk --head xdzleo:pr02-readback-path-logging \
             --title "graphics/d3d12: log IssueCopy_ReadbackResolvePath result" \
             --body-file ../skate3-rexglue/docs/contributions/pr02-readback-path-logging/PR_BODY.md

git checkout main
git checkout -b pr03-issuedraw-edram-mode-trace
# ... apply PR 03 changes ...
git commit -F ../skate3-rexglue/docs/contributions/pr03-issuedraw-edram-mode-trace/COMMIT_MSG.txt
git push fork pr03-issuedraw-edram-mode-trace
gh pr create --repo rexglue/rexglue-sdk --head xdzleo:pr03-issuedraw-edram-mode-trace \
             --title "graphics/d3d12: trace RB_MODECONTROL.edram_mode per draw" \
             --body-file ../skate3-rexglue/docs/contributions/pr03-issuedraw-edram-mode-trace/PR_BODY.md
```

### 3.6 — PRs 04, 05, 06 are PROPOSALS

These don't have full code patches yet — they're design proposals. Submit as **GitHub Discussions** or **issue + draft PR** so the maintainers can comment before you implement:

```bash
gh issue create --repo rexglue/rexglue-sdk \
    --title "[Proposal] Codegen: detect EA's PPC jump-table pattern (Skate 3, Burnout)" \
    --body-file ../skate3-rexglue/docs/contributions/pr04-codegen-config-ea-jumptable/PR_BODY.md

gh issue create --repo rexglue/rexglue-sdk \
    --title "[Proposal] Optional rex::video::vp6 module from Skate 3 port" \
    --body-file ../skate3-rexglue/docs/contributions/pr05-vp6-bridge-template/PR_BODY.md

gh issue create --repo rexglue/rexglue-sdk \
    --title "[Proposal] Defensive vtable-NULL skip helper macro" \
    --body-file ../skate3-rexglue/docs/contributions/pr06-defensive-vtable-skip/PR_BODY.md
```

## Part 4 — Cross-reference

Once your skate3-rexglue repo is up and PRs are open, edit `docs/contributions/README.md` in your skate3-rexglue repo to add direct PR/issue links:

```bash
cd C:/Users/admin/Downloads/xenia-analysis/skate3-rexglue
# Edit docs/contributions/README.md, add a "Live PRs" section:
#   PR 01: https://github.com/rexglue/rexglue-sdk/pull/<N>
#   PR 02: https://github.com/rexglue/rexglue-sdk/pull/<N>
#   ...

git add docs/contributions/README.md
git commit -m "docs: link to live rexglue-sdk PRs and issues"
git push origin main
```

## Part 5 — Engaging the maintainers

After submitting, ping the rexglue maintainers on whatever channel they prefer (Discord, GitHub Discussions). Be prepared to:

- Respond to code-review feedback
- Iterate on patches
- Possibly split a PR if a maintainer requests narrower scope

## Tips for professional PRs

1. **Title format:** `<area>: <imperative one-liner>` (e.g., "graphics/d3d12: add foo")
2. **Body structure:** Summary → Problem → Changes → Testing → Compatibility
3. **Link evidence:** Always include a log line, file:line, or screenshot
4. **Be honest about scope:** Don't oversell. If it's a diagnostic-only patch, say so.
5. **One concern per PR:** Maintainers reject "big ball" PRs with multiple unrelated changes
6. **Ask before rewriting:** If you want to refactor existing code, open a discussion first

## Notes on the 20 unpushed commits in your local rexglue-sdk

Your local clone has 20 commits ahead of origin/main that aren't curated. These represent the iterative development of the Skate 3 patches. **Do not push them as-is to your fork's main** — they'll create a noisy diff. Instead:

- The cleanly-curated PRs above are the publishable subset
- The full backup branch (Part 3.3) preserves the raw history for your own reference
- Don't worry about losing work — git keeps everything

## Troubleshooting

**`gh auth login` fails:**
- Make sure you're using HTTPS not SSH if you don't have keys set up
- If you have 2FA, use a personal access token: https://github.com/settings/tokens

**`gh pr create` fails with "must be on a branch":**
- Make sure `git status` shows you're on a branch, not detached HEAD

**Push rejected because remote has more commits:**
- Use `git pull --rebase` then re-push, OR `--force-with-lease` if your fork is solo

## Summary checklist

- [ ] gh CLI authenticated
- [ ] skate3-rexglue is a git repo, pushed to your account
- [ ] rexglue-sdk forked to your account
- [ ] Backup branch saved on your fork
- [ ] PR 01 submitted
- [ ] PR 02 submitted
- [ ] PR 03 submitted
- [ ] Issues 04, 05, 06 submitted as proposals
- [ ] docs/contributions/README.md updated with live links
