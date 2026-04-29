# Upstream Contributions to rexglue-sdk

This folder contains patches and proposals ready to submit upstream to [rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk). Each subfolder is a self-contained PR proposal.

## Contribution philosophy

1. **One concern per PR.** Each contribution addresses a single, well-defined issue.
2. **Game-agnostic.** All proposed changes benefit *any* Xbox 360 → PC port using rexglue, not just Skate 3.
3. **Minimally invasive.** Patches preserve existing behavior unless explicitly fixing a bug.
4. **Verifiable.** Every patch includes test evidence and reproduction steps.
5. **Documentation-first.** Each PR includes a clear problem statement, root-cause analysis, and rationale.

## Pending PRs

| # | Title | Files | Status |
|---|-------|-------|--------|
| [01](pr01-issuecopy-diagnostics/) | Add EDRAM resolve diagnostic instrumentation | `src/graphics/d3d12/command_processor.cpp` | ✅ Ready |
| [02](pr02-readback-path-logging/) | Log `IssueCopy_ReadbackResolvePath` results | `src/graphics/d3d12/command_processor.cpp` | ✅ Ready |
| [03](pr03-issuedraw-edram-mode-trace/) | Trace `RB_MODECONTROL.edram_mode` per draw (gated, low-overhead) | `src/graphics/d3d12/command_processor.cpp` | ✅ Ready |
| [04](pr04-codegen-config-ea-jumptable/) | Codegen analyzer: detect EA's PPC jump-table pattern | `include/rex/codegen/config.h`, `src/codegen/function_scanner.cpp` | 📝 Proposal |
| [05](pr05-vp6-bridge-template/) | VP6 decoder bridge as optional `rex::audio::vp6` module | New module | 📝 Proposal |
| [06](pr06-defensive-vtable-skip/) | Defensive vtable-NULL-skip helper macro | `include/rex/hooks/defensive.h` | 📝 Proposal |

## Submitting a PR

If you're submitting on behalf of @xdzleo:

```bash
# Authenticate first
gh auth login

# Fork the repo (one-time)
gh repo fork rexglue/rexglue-sdk --clone=false

# Clone your fork
gh repo clone xdzleo/rexglue-sdk
cd rexglue-sdk

# Apply a contribution patch
git checkout -b pr01-issuecopy-diagnostics
git apply ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/changes.patch
git commit -am "$(cat ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/COMMIT_MSG.txt)"

# Push and open PR
git push -u origin pr01-issuecopy-diagnostics
gh pr create --title "Add EDRAM resolve diagnostic instrumentation" \
             --body-file ../skate3-rexglue/docs/contributions/pr01-issuecopy-diagnostics/PR_BODY.md
```

## Why these matter

The rexglue-sdk's D3D12 backend currently has limited diagnostic visibility for the EDRAM-resolve-to-swap-chain pipeline. When a recompiled title's swap chain stays black despite high draw activity, there's no way to determine:

- Whether `IssueCopy()` is being entered at all
- Whether `RB_COPY_DEST_BASE` matches the swap-chain `fetch_base`
- Whether the `kFast` readback path actually copies bytes to CPU memory

These contributions add **non-invasive, gated instrumentation** that surfaces this information in the standard log channel. Overhead: ~16 log lines per minute at default gating (`my_n <= 32 || (my_n % 240) == 0`).

PRs 4-6 are proposals (not yet implemented patches) for larger architectural improvements:

- **PR-04 (codegen jump-table fix)** would benefit *every* Xbox 360 game using a non-Sonic-Unleashed compiler. Skate 3 is one example; many EA, Ubisoft, and Activision titles use similar patterns.
- **PR-05 (VP6 bridge)** packages our libavcodec wrapper as a reusable optional module. Any title with `.vp6` cutscenes (most Xbox 360 EA titles, plus various others) gets video playback for free.
- **PR-06 (defensive vtable skip)** formalizes the pattern we developed for handling NULL vtable slots during streaming-thread teardown.

## Code quality checklist

Each PR includes:

- [x] Clean compile, no new warnings
- [x] Default-off behavior (no impact on existing builds)
- [x] Comprehensive comment explaining the problem and rationale
- [x] Citation of supporting evidence (recompile cpp:line, runtime log lines)
- [x] No external dependencies added
- [x] No breaking API changes
- [x] No performance regression (verified via per-frame draw count parity)
