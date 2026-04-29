# Contributing to skate3-rexglue

Thanks for considering contributing! This project is research-grade infrastructure for static recompilation of Xbox 360 games to PC. Your contributions can help unlock future ports of titles that the public has never seen running outside their original hardware.

## What kind of contributions are welcome

### High value
- Implementations of game-specific hooks for **other Xbox 360 titles** that share patterns with Skate 3 (EA RW3-based games)
- New cookbook recipes documenting reusable patterns
- Verified fixes for the remaining Skate 3 main-menu blocker
- Code-review feedback on the PR proposals targeting upstream rexglue-sdk

### Medium value
- Additional diagnostic instrumentation (anything to improve visibility into rexglue-sdk internals)
- Documentation improvements
- Performance optimizations to existing hooks

### Low value (please discuss first)
- Refactoring of existing hooks unless there's a concrete bug
- Style-only changes
- New dependencies

## How to submit

1. **Open an issue first** describing what you want to do
2. Wait for maintainer response (a thumbs-up or "let's discuss")
3. Fork the repo, make changes on a feature branch
4. Submit a PR with the format:
   - **Title:** imperative one-liner, prefixed with area (e.g., `hooks: add foo`, `vp6: fix bar`)
   - **Body:** Summary → Problem statement → Changes → Test evidence → Compatibility notes
5. Be responsive to review feedback

## Code style

- Follow the existing patterns in `src/hooks.cpp`
- Use the helper macros: `IsLikelyGuestPointer()`, `REX_LOAD_U32()`, `REX_STORE_U32()`, `REXLOG_INFO()`
- Comment generously — every hook should explain WHY, citing a specific recompile.cpp:line or runtime log line
- Atomic counters for hook fire counts: `static std::atomic<int> count{0};`
- Default-off behavior: hooks should not change game behavior unless gated by a check

## Documentation expectations

Every new hook MUST include:

- A header comment explaining the hook's purpose
- Citation of supporting evidence (recompile cpp:line, runtime log line)
- The exact symptom that motivated the hook
- The expected runtime behavior change

Example:

```cpp
// sub_82F02368 -- per-frame FE poller. Per recomp.104.cpp:63675-63794, this
// dispatches `bctrl mem[mem[r3+0]+8]` (vtable[2]) which gates the
// rich-render branch. In Skate 3's recompile, this gate fires correctly
// only AFTER the FE-mgr's screen list is populated. We force-fire
// sub_825D3700 (the vtable[4] menu iterator) every frame at state==5
// to guarantee menu UI builder runs.
//
// Evidence: skate3_330.log:3565 shows fire #1, #2400+ logs at #2400, etc.
extern "C" REX_FUNC(sub_82F02368) {
    // ...
}
```

## Pull request review process

PRs are reviewed against:
- [ ] Compiles cleanly
- [ ] No new warnings
- [ ] No new dependencies (or new deps justified)
- [ ] Comprehensive comment on the change rationale
- [ ] Test evidence included
- [ ] Doesn't break existing hooks (cross-test against full run)

## Sister project: rexglue-sdk upstream contributions

Some changes here may be candidates for upstreaming to [rexglue/rexglue-sdk](https://github.com/rexglue/rexglue-sdk). If your change:

- Is game-agnostic (any title using rexglue benefits)
- Adds infrastructure (new helpers, diagnostic tools)
- Fixes a bug in the SDK itself

Then please consider also opening a PR upstream. See `docs/PR_GUIDE.md` for the workflow.

## Code of conduct

- Be respectful and patient
- No harassment, hate speech, or personal attacks
- Focus on the technical merits of contributions
- Assume good faith; if a contribution is rejected, explain why constructively

## License

Contributions are released under the same license as the existing project (BSD-3-Clause for code, CC-BY-4.0 for documentation).

## Questions?

Open a GitHub Discussion or issue. Tag @xdzleo for project-level questions.
