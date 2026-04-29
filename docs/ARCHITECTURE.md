# Skate 3 Architecture (Reverse-Engineered)

This document describes the runtime architecture of Skate 3 (Xbox 360, default.xex) as discovered through static + dynamic analysis. None of this is officially documented; everything below is reverse-engineered from disassembly, runtime tracing, and cross-referencing with the leaked RenderWare 3.7 PC SDK.

> **Caveat:** EA's Black Box studio fork of RenderWare 3 (RW3) is *not* the same as the public RW3.7 SDK. Where they diverge, the divergence is documented inline. Confidence levels: 🟢 verified by runtime trace, 🟡 inferred from disassembly, 🔴 hypothesis only.

## Layer 1 — XEX boot

🟢 **Code section:** `0x82370000–0x82F734D4` (~12 MB, 47k functions per pdata)
🟢 **rdata section:** `0x82000000–0x8232A204` (3.3 MB)
🟢 **Title ID:** `454108E6` (EA-2278)
🟢 **Entry point:** `xstart` at `0x82EB4F80` → calls title init chain

The XEX has standard PE-style sections plus a `.CRT` static-initializer section that the rexglue runtime currently runs only via the explicit guest entry, not pre-main.

## Layer 2 — Engine init (RW3-derived)

The engine follows the canonical RW3 four-state lifecycle (per librw `Engine::init/open/start`):

```
IDLE → INITED → OPENED → STARTED
                  ↓         ↓
              RwMalloc  _rwPluginRegistryInitObject
                       + DEVICEFINALIZESTART
```

🟢 **Memory pool allocator:** `sub_82883930` (`recomp.45.cpp:2784`) — a named-singleton allocator. Takes a string name (e.g. `"Singleton"`), vtable pointer, type-tag, alignment, size. Returns a heap pointer.

🟢 **264-byte FE-mgr instance:** allocated by `sub_826231F8` (`recomp.23.cpp:52680`)
- Stored at `mem[0x83073544]`
- Sets `obj+0` to `0x823071A0` (the state-name catalog table — see Layer 3)
- Initial fields: `+0x20=0x10`, `+0x24=0` (empty std::list head sentinel: head==tail==self-link offset)

🟡 **168-byte sibling singleton:** at `mem[0x8307353C]` with vtable `0x822FF2C8`
- Constructor: `sub_82CDE548` (`recomp.84.cpp:18461`)
- Vtable contains real function pointers: dtor + class methods including `sub_825D3700` (menu iterator)
- Possibly `TheFrontEndStateManager` per agent analysis

🔴 **Two-singleton hypothesis:** Skate 3 may have a "FE manager" base + a wrapper sub-class. Confirming this requires more cross-reference work.

🟢 **ScreenManager:** at `mem[0x830734B4]`, allocated by `sub_825CE8D8` (`recomp.20.cpp:43508`), 312 bytes.

🟢 **FE root subsystem:** 25,368-byte object at `mem[0x830737F4]/mem[0x830737F8]`, constructed by `sub_826B8620` (the FE-mgr's vtable[5] init method).

## Layer 3 — State-name catalog at 0x823071A0

🟢 **Confirmed:** `0x823071A0` in `.rdata` contains a packed list of null-terminated state-name strings:

```
"erPhotoSaveFlow"        (tail of "GamerPhotoSaveFlow")
"StateOnlineDownload"
"StateOnlineFreeskateHere"
... (continues for ~256 bytes)
```

This is **not a C++ vtable**. It's a string lookup table. The FE-mgr stores its address at `+0` for use by state-name dispatch (e.g. `goto state by string id`).

🔴 **Why this layout:** EA's RW3 fork appears to combine traditional vtables with name-based dispatch. The C++ vtable for the FE-mgr class is at `0x822FF2C8` (16 verified function pointers, agent a4b80fc165f5c48f8 finding) but the engine instance stores `0x823071A0` (the names) at `+0`. This is unusual.

## Layer 4 — Screen registry table at 0x82FD33E8

🟢 **Confirmed:** `0x82FD33E8` in `.rdata` is a table of **129 entries × 16 bytes** = 2,064 bytes total.

Each entry contains screen metadata (name string ptr, factory function ptr, flags, etc.) — exact layout TBD but verified dimensionally.

🟡 **Iterator:** `sub_825C0E98` (`recomp.20.cpp:11221`) walks the table and calls `sub_825BB5A0` per row.

🟡 **Top-level entry:** `sub_8258D4F8` (`recomp.18.cpp:35990`) — calls `sub_825C0E98` plus pre/post setup.

🔴 **Critical finding:** `sub_8258D4F8` has **zero callers in `.text`**. It's recompiled (because rexglue codegen emitted it) but never invoked. This is the smoking-gun root cause of the empty screen list.

## Layer 5 — Per-frame state machine

🟢 **State global:** `mem[0x8300B2B0]` (single 32-bit value)

🟢 **State writer:** `sub_824EBBB8` (`recomp.13.cpp:2581`) — single canonical setter.

🟢 **State driver:** `sub_825C0BB0` (`recomp.20.cpp:10764`) — runs every frame, has a switch over current state. Transitions:

| From | Setup before transition | To |
|------|--------------------------|------|
| 1 | `vtable[12]` of obj+44 | 2 |
| 3 | `vtable[36]` of obj+44 | 0 |
| **4** | **(none — bare advance)** | **5** |
| 6 | (none) | 0 |

🔴 **The 4→5 transition has no setup vtable call.** It just sets state=5. This means the vtable[N] dispatches in the body of `sub_825C0BB0` (specifically `vtable[16]` of `mem[0x83006788+N]` for various N) are responsible for populating menu state. If those vtables are broken, the screen list stays empty.

## Layer 6 — Per-frame submission

🟢 **Per-frame poller:** `sub_82F02368` (`recomp.104.cpp:63675`) — runs at ~60 Hz in the title's main thread.

Body sequence (verified):
1. Read `mem[this+80]` and walk vtables for inner objects
2. Read `mem[this+252]` (scene-graph buffer) and dispatch `vtable[11]` if non-zero
3. Read `mem[this+8]` and dispatch `vtable[11]` if non-zero
4. Check `mem[this+264]` and `mem[this+124]`; if both `+264 != 0 && +124 == 0`, call `sub_82F010B0`
5. **bctrl `mem[this+0]+8`** (vtable[2] dispatch — the build-chain gate)
6. If bctrl returns non-zero AND `mem[+124] == 0` AND (`+184 != 0` OR `+176 != 0`): proceed to `sub_82F02178` (the draw-list builder)
7. Otherwise skip the rich-render branch

🟢 **Draw-list builder:** `sub_82F02178` (`recomp.104.cpp:63377`) — when fired, populates `mem[+184]` via `sub_82F01848` → `sub_82F011E0` (descriptor pool pop), then dispatches the per-batch builder `sub_82ED0900`.

🟢 **Per-batch appender:** `sub_82ED0900` (`recomp.103.cpp:20883`) — only caller of `sub_82ED07E0` (size-based allocator). Writes the count to `mem[+184]`.

🟢 **Allocator:** `sub_82ED07E0` (`recomp.103.cpp:20554`) — best-fit splitter from the buffer pool at `+252`.

🟢 **Pool carver:** `sub_82ECFFE0` (`recomp.103.cpp:19238`) — RW3 `AllocSceneGraphBuffer`. Writes new pool slab to `+252`, links tracking node into `+124`.

## Layer 7 — GPU pipeline

🟢 **GPU dispatcher:** `sub_82B5A648` (`recomp.70.cpp:39922`) — fires 60 Hz, calls `__imp__VdGetSystemCommandBuffer` then `__imp__VdSwap`.

🟢 **Render frame coordinator:** `sub_82A52BF8` (`recomp.60.cpp:4624`) — calls per-frame setup including `sub_82F01038` (GPU engine event kick).

🟢 **EDRAM resolve:** triggered by `RB_MODECONTROL.edram_mode = kCopy(6)` + a screen-aligned `DRAW_INDX_2`. Handled by `D3D12CommandProcessor::IssueCopy` → `IssueCopy_ReadbackResolvePath`.

🟢 **Swap chain:** `fetch_base = 0x05258000` (verified across 7,000+ swaps). 1280×720 BGRA8 (`fmt=6`), pitch 5120, height 1280.

🟢 **Render targets:** EDRAM base 0 (color + depth), 4xMSAA + 1xMSAA variants (640×1024 / 1280×2048).

## Layer 8 — Threading model

🟢 **Threads spawned:**
- `Main XThread` (handle F8000008)
- `XMA Decoder`, `Audio Worker` (host threads)
- `GPU Commands`, `GPU VSync` (host threads)
- `load_thread` (handle F8000058 / F8000060) — calls `NtCreateFile`
- `timer_thread` (F8000040)
- `rwfilesys` ×2 (F8000050, F8000060) — file I/O workers
- `presence_thread`
- 3× `Job Manager - Job Thread`
- `dlc_enumerator`
- `decompress_thread`
- `render_thread`
- `sk8_memcard_helper`
- `RwAudioCore Dac`
- `MoviePlayer2 Decode Thread` (suspended in our build to avoid recompiled VP6 decoder bug)

🟢 **All thread entrypoints are `sub_82EC44F8`** (the dispatcher wrapper), which dispatches to `sub_82F2A108` (thread-init bookkeeping). Per-thread context determines actual work via `mem[ctx+84]` indirect call.

## Layer 9 — Asset loading

🟢 **`.big` archive format:** EA's RW container. Each `.big` is a manifest + multiple sub-files.

🟢 **Loader path:** rwfilesys thread → `NtCreateFile` → `NtReadFile` (synchronous) → guest-side `sub_8293F018` (asset-completion broadcast — calls `NtSetEvent` + `vtable[24]` broadcast).

🔴 **Suspected gap:** in our build, `sub_8293F018` never fires (no `NtSetEvent` calls observed in our log vs. dozens in Xenia's reference log). The synchronous `NtReadFile` returns success but doesn't schedule the worker thread into the broadcast path.

## Layer 10 — Frontend (FE) manager

🟢 **FE manager singleton:** `mem[0x83073544]`
🟢 **State catalog:** `0x823071A0` (string list)
🟢 **Screen registry:** `0x82FD33E8` (129 entries)
🟢 **State machine driver:** `sub_825C0BB0`
🟢 **Per-frame poller / submit:** `sub_82F02368`
🟢 **Menu iterator:** `sub_825D3700` (iterates `+0x20`/`+0x24` list, dispatches per-item vtable)
🔴 **Screen registry walker:** `sub_8258D4F8` (NEVER FIRES — bug)

## Confidence summary

| Layer | Confidence |
|-------|------------|
| 1. XEX boot | 🟢 high |
| 2. Engine init | 🟢 high |
| 3. State-name catalog | 🟢 high |
| 4. Screen registry | 🟡 medium |
| 5. State machine | 🟢 high |
| 6. Per-frame submission | 🟢 high |
| 7. GPU pipeline | 🟢 high |
| 8. Threading | 🟢 high |
| 9. Asset loading | 🟡 medium |
| 10. Frontend manager | 🟡 medium |

## Diagrams

```
                          ┌─────────────────┐
                          │  default.xex     │
                          │  (Skate 3 boot)  │
                          └────────┬─────────┘
                                   │
                    ┌──────────────┴──────────────┐
                    │                             │
            ┌───────▼─────────┐         ┌─────────▼──────────┐
            │  RW3 Engine init │         │  FE manager init   │
            │  (sub_826B8620)  │         │  (sub_826231F8)    │
            └───────┬──────────┘         └─────────┬──────────┘
                    │                              │
                    ▼                              ▼
             [25368-byte FE root]            [264-byte FE-mgr]
             mem[0x830737F4]                 mem[0x83073544]
                                                   │
                              ┌────────────────────┴───┐
                              ▼                        ▼
                       sub_82F02368                sub_825D3700
                       (per-frame poller)          (menu iterator)
                              │                        │
                              ▼                        ▼
                       sub_82F02178               iterates +0x20/+0x24
                       (draw-list builder)         list (currently empty)
                              │
                              ▼
                       sub_82ED0900
                       (per-batch appender)
                              │
                              ▼
                       writes mem[+184]
                       (descriptor pool head)
```

## References

- [librw — Engine state machine reference](https://github.com/aap/librw/blob/master/src/engine.cpp)
- [RenderWare 3.7 PC SDK (sigmaco/rwsrc-v37-pc)](https://github.com/sigmaco/rwsrc-v37-pc)
- [Xenia compat issue #201 — Skate 3](https://github.com/xenia-project/game-compatibility/issues/201)
- [XenonRecomp Issue #90 — Jump tables in Skate 3](https://github.com/hedge-dev/XenonRecomp/issues/90)
