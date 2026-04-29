# skate3-rexglue

> **Skate 3 (Xbox 360) PC port via static recompilation.**
> First publicly-documented attempt to render Skate 3 outside Xbox 360 hardware.
> Comprehensive infrastructure + research notes for any future Xbox 360 → PC port using [rexglue](https://github.com/rexglue/rexglue-sdk).

[![Status](https://img.shields.io/badge/status-research-blue)](docs/RESULTS.md)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-green)](LICENSE)
[![rexglue](https://img.shields.io/badge/rexglue-fork-orange)](https://github.com/rexglue/rexglue-sdk)

---

## Quick navigation

📊 **[Results](docs/RESULTS.md)** — what runs, with evidence
🏗️ **[Architecture](docs/ARCHITECTURE.md)** — Skate 3's runtime, reverse-engineered
🔧 **[Cookbook](docs/cookbook/)** — reusable patterns for any Xbox 360 port
🚀 **[Upstream PRs](docs/contributions/)** — rexglue-sdk improvements ready to merge
📖 **[Research notes](docs/research/)** — cycle-by-cycle technical deep-dives
🤝 **[Contributing](docs/CONTRIBUTING.md)** — how to extend this work

## What works (verified)

| Layer | Evidence |
|-------|----------|
| Build & link | `skate3.exe` 85 MB, clean compile |
| Asset loading | 18+ `.big` files load via `NtCreateFile` |
| FE state machine | Reaches state=5 (main menu) |
| Intro VP6 video | `EA_Blackbox_english_ntsc.vp6` plays @ 29.97 fps |
| GPU pipeline | 1.4M+ packets, 178k+ draws, 7k+ swaps per run |
| EDRAM resolve | 5,351 resolves to swap chain `0x05258000` |

## What doesn't work yet

The main menu UI doesn't render. Root cause narrowed to: `sub_8258D4F8` (the screen registry walker) has zero callers in `.text` due to a XenonAnalyser jump-table detection bug ([XenonRecomp #90](https://github.com/hedge-dev/XenonRecomp/issues/90)).

## How to build

Prerequisites:
- Windows 10/11 with Visual Studio Build Tools 2022
- CMake 3.20+, Ninja
- Clang/LLVM 16+ (or MSVC `cl.exe`)
- Skate 3 NTSC ISO (or extracted XEX)
- ~10 GB free disk space

```bash
# Clone with rexglue-sdk submodule
git clone https://github.com/xdzleo/skate3-rexglue.git
cd skate3-rexglue
git submodule update --init --recursive

# Configure and build
cd build
cmake ..
cmake --build . --config Release --target skate3

# Extract assets from your Skate 3 ISO to skate3-extracted/
# Then run:
./skate3.exe --game_data_root="../skate3-extracted"
```

## Stats

- **53** guest functions hooked
- **38** custom hook bodies
- **2,419** lines of `src/hooks.cpp`
- **521** lines of `src/vp6_bridge.cpp`
- **8** rexglue-sdk diagnostic instrumentations
- **26** research notes documenting 9 cycles of investigation
- **41,429** Xenia HIR dumps captured for diff
- **40+** Opus 4.7 reverse-engineering sessions consolidated

## Upstream contributions to rexglue-sdk

| # | Type | Status | Title |
|---|------|--------|-------|
| [#300](https://github.com/rexglue/rexglue-sdk/pull/300) | PR (code) | OPEN → development | EDRAM resolve diagnostic instrumentation |
| [#301](https://github.com/rexglue/rexglue-sdk/pull/301) | PR (code) | OPEN → development | log IssueCopy_ReadbackResolvePath result |
| [#302](https://github.com/rexglue/rexglue-sdk/pull/302) | PR (code) | OPEN → development | trace RB_MODECONTROL.edram_mode per draw |
| [#306](https://github.com/rexglue/rexglue-sdk/pull/306) | PR (code) | OPEN → development | **fix dcbz to use 128-byte Xenon cache line** (correctness fix) |
| [#303](https://github.com/rexglue/rexglue-sdk/issues/303) | Issue (proposal) | OPEN | codegen: detect EA's PPC jump-table pattern |
| [#304](https://github.com/rexglue/rexglue-sdk/issues/304) | Issue (proposal) | OPEN | optional rex::video::vp6 module |
| [#305](https://github.com/rexglue/rexglue-sdk/issues/305) | Issue (proposal) | OPEN | defensive vtable-NULL skip helper |

See [docs/contributions/](docs/contributions/) for individual PR bodies, patches, and rationale.

## Acknowledgements

Built on the shoulders of:
- [rexglue](https://github.com/rexglue/rexglue-sdk) — static recompilation framework
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) — codegen patterns
- [UnleashedRecomp](https://github.com/hedge-dev/UnleashedRecomp) — hook templates
- [Xenia Canary](https://github.com/xenia-canary/xenia-canary) — reference emulator
- [librw](https://github.com/aap/librw) — RW3 reverse-engineering

## License

BSD-3-Clause for code (matching rexglue-sdk).
CC-BY-4.0 for documentation.

## Author

[@xdzleo](https://github.com/xdzleo) — Skate 3 PC port research, 2026

> Skate 3 has been publicly unsolved since 2015 ([xenia compat #201](https://github.com/xenia-project/game-compatibility/issues/201)). This work pushes deeper than any prior public attempt and contributes the toolkit needed for the next step.
