# Skate 3 PC Port via rexglue — Research & Contribution Hub

> **Contributor:** [@xdzleo](https://github.com/xdzleo)
> **Status:** Research & infrastructure complete. Game-specific blockers under active investigation.
> **Last update:** 2026-04-29

This repository documents a deep-dive port attempt of **Skate 3 (Xbox 360, EA Black Box, 2010)** to PC via static recompilation using the [rexglue SDK](https://github.com/rexglue/rexglue-sdk). The work surfaces multiple reusable infrastructure patterns, codegen issues, and engine-architecture discoveries that benefit *any* Xbox 360 → PC port using rexglue/XenonRecomp.

Skate 3 has been **publicly unsolved** since 2015 ([xenia-project/game-compatibility#201](https://github.com/xenia-project/game-compatibility/issues/201)). No emulator or recompile has rendered its main menu. This work pushes deeper than any prior public attempt and contributes the toolkit needed for the next step.

## TL;DR — What was achieved

| Layer | Status |
|-------|--------|
| Build & link | ✅ skate3.exe builds clean with rexglue codegen |
| Asset loading (`.big` files) | ✅ 18+ files loaded via `NtCreateFile` |
| FE state machine (0→2→4→5) | ✅ Reaches main-menu state |
| Intro VP6 video | ✅ Plays via libavcodec bridge + D3D12 overlay |
| GPU command pipeline | ✅ 4M+ draws, 40k+ swaps per run |
| EDRAM resolve to swap chain | ✅ 5,351 resolves to swap address |
| FE manager allocation | ✅ Singleton constructed, vtable installed |
| Main-menu UI render | ❌ Screen list never populated (root cause TBD) |

## Repository layout

```
skate3-rexglue/
├── docs/                        ← YOU ARE HERE (research + contribution docs)
│   ├── contributions/           ← PR-ready patches for rexglue-sdk upstream
│   ├── cookbook/                ← Reusable patterns for future XB360 ports
│   └── research/                ← Deep-dive technical findings
├── src/
│   ├── hooks.cpp                ← 2,419 lines of game-specific hooks (53 functions)
│   └── vp6_bridge.cpp           ← 521 lines of libavcodec VP6 decoder bridge
├── generated/                   ← rexglue codegen output (47k recompiled functions)
├── build/                       ← CMake build dir
├── dumps/                       ← Section dumps from default.xex
└── skate3.toml                  ← Runtime config
```

## Reading order

If you're new to this work, read in order:

1. **[RESULTS.md](RESULTS.md)** — concrete deliverables and test evidence
2. **[ARCHITECTURE.md](ARCHITECTURE.md)** — Skate 3 engine architecture as discovered
3. **[contributions/README.md](contributions/README.md)** — patches ready for rexglue upstream
4. **[cookbook/README.md](cookbook/README.md)** — reusable recipes for next ports
5. **[research/](research/)** — per-cycle deep-dives (cycle16-cycle24)

## Quick stats

- **53 guest functions hooked** (extern declarations)
- **38 custom hook bodies** (active behavior overrides)
- **2,419 lines** of `hooks.cpp` original code
- **521 lines** of VP6 decoder bridge (libavcodec wrapper)
- **8 SDK instrumentation points** added to `rexglue-sdk/src/graphics/d3d12/command_processor.cpp`
- **26 research notes** documenting cycles 16-24
- **41,429 HIR dumps** captured from working Xenia Canary reference build
- **40+ Opus 4.7 reverse-engineering agent runs** consolidated

## Rexglue-SDK upstream contributions

The following improvements to `rexglue-sdk` are independent of Skate 3 and ready for upstream PR:

| File | Improvement | Status |
|------|-------------|--------|
| `src/graphics/d3d12/command_processor.cpp` | EDRAM resolve diagnostic instrumentation (`IssueDraw`, `IssueCopy`, `IssueCopy_ReadbackResolvePath`) | ✅ Ready |
| `include/rex/codegen/config.h` | Codegen config additions | ✅ Ready |
| `CMakeLists.txt` | Build improvements | ✅ Ready |

See [contributions/README.md](contributions/README.md) for PR-ready details.

## Key findings benefiting other Xbox 360 ports

1. **EA's RW3 fork uses non-standard vtable patterns** (parent + offset stored, not whole function addresses). XenonAnalyser misses these — see [research/xenonanalyse_jumptable_issue.md](research/xenonanalyse_jumptable_issue.md).
2. **Skate 3's `.big` archives load cleanly** via standard XEX file path resolution — patterns reusable for any EA RenderWare game.
3. **VP6 video integration via libavcodec** is portable to *any* Xbox 360 game with `.vp6` cutscenes.
4. **The two-phase RW3 engine init** (OPEN allocates, START+FINALIZESTART wires) is the same for Skate 1/2, Burnout, GTA III/VC/SA Xbox versions.
5. **Defensive vtable-NULL skip pattern** (sub_828F47D0/sub_828F6B00) handles dynamic linkage in any RW-based title.

## Game-specific blockers (Skate 3 only)

The remaining blocker is **menu screen-list population** — the `sub_8258D4F8` top-level entry that iterates the screen registry table at `0x82FD33E8` (129 entries × 16 bytes) is never invoked. Force-firing it is the next experimental step. See [research/cycle24_menu_screen_registry.md](research/cycle24_menu_screen_registry.md).

## How to contribute

This research is open. To extend or apply to other games:

- See [cookbook/](cookbook/) for reusable patterns
- See [contributions/](contributions/) for upstream-ready patches
- Open issues at [github.com/xdzleo/skate3-rexglue/issues](https://github.com/xdzleo/skate3-rexglue/issues)

## License

Code modifications follow the same license as their parent project (BSD-3-Clause for rexglue-sdk).

## Acknowledgements

- [rexglue-sdk](https://github.com/rexglue/rexglue-sdk) — static recompilation framework
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — codegen patterns
- [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) — hook/patch templates
- [Xenia Canary](https://github.com/xenia-canary/xenia-canary) — reference emulator
- [librw](https://github.com/aap/librw) — RenderWare 3 reverse-engineering
- Anthropic Claude Opus 4.7 — reverse-engineering agent runs
