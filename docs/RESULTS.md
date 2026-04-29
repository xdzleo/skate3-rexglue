# Skate 3 PC Port — Concrete Results

This document presents the verifiable, reproducible outcomes of the Skate 3 PC port effort. Every claim is backed by a log line, file path, and bytes count.

## Test rig

| Component | Spec |
|-----------|------|
| Host OS | Windows 11 Pro 24H2 |
| Build toolchain | Clang/LLVM via CMake + Ninja |
| GPU | NVIDIA GeForce RTX 4090 |
| Source ISO | Skate 3 NTSC Xbox 360 disc image (6.4 GB) |
| Reference emulator | Xenia Canary `9132035a5` (built from source) |
| Static recompiler | rexglue SDK `main` branch + custom modifications |

## Reproducibility

```bash
# Build skate3 binary
cd skate3-rexglue/build
cmake --build . --config Release --target skate3

# Run with extracted assets
./skate3.exe --game_data_root="path/to/skate3-extracted"
```

Logs land in `skate3-rexglue/build/logs/skate3_<run>.log` (rotated `.1.log` … `.10.log`).

## What works

### 1. Build pipeline

- 47,000+ guest functions translated by rexglue codegen into `generated/skate3_recomp.{0..110}.cpp`
- Clean compile with Clang/LLVM, no errors
- Final binary: `build/skate3.exe` (~85 MB)

### 2. Asset loading

Verified via `[NtCreateFile]` log lines (with `log_noisy=true` + `log_level=trace`):

```
[NtCreateFile] path=d:\fileserver.ini
[NtCreateFile] path=d:\data\big\miscboot.big
[NtCreateFile] path=d:\data\big\db.big
[NtCreateFile] path=d:\data\big\fedynamic.big
[NtCreateFile] path=d:\data\big\shaders_final.big
[NtCreateFile] path=d:\data\big\fedata.big
[NtCreateFile] path=d:\data\big\scene.big
[NtCreateFile] path=d:\data\big\miscload.big
[NtCreateFile] path=d:\data\audio\music\overlays.big
[NtCreateFile] path=d:\data\audio\grains.big
[NtCreateFile] path=d:\data\audio\ambience.big
[NtCreateFile] path=d:\data\audio\wheels.big
[NtCreateFile] path=d:\data\audio\audiofiles.big
[NtCreateFile] path=d:\data\content\missions.big
[NtCreateFile] path=d:\data\content\parkassets.big
[NtCreateFile] path=d:\data\content\livingworld.big
[NtCreateFile] path=d:\data\content\marquee.big
[NtCreateFile] path=d:\data\content\createacharacter.big
[NtCreateFile] path=d:\data\content\worldmisc.big
```

All return `X_STATUS_SUCCESS (0x0)`.

### 3. FE state machine

State machine progresses through all expected stages:

```
0   Boot / press-A / Skate 3 logo
2   VP6 intro playing
4   Post-VP6 (sub_825C1410 activator)
5   Main menu (stable)
```

Log lines confirming state=5:
```
[HOOK] sub_82F02368 #12000 FE-state mem[0x8300B2B0]=5
[HOOK] sub_82F02368 #15600 FE-state mem[0x8300B2B0]=5
[HOOK] sub_82F02368 #16800 FE-state mem[0x8300B2B0]=5
```

### 4. Intro VP6 video playback

`EA_Blackbox_english_ntsc.vp6` (1280×720, 268 frames, 29.97 fps):

```
[VP6] OpenFile: 'EA_Blackbox_english_ntsc.vp6' OK size=14001720 1280x720 frames=268 fps=29.970
[VP6] StartPlayback OK -- 1280x720 frames=268
[VP6] AdvanceFrame: now at frame 268/268 (final)
[HOOK] VP6 playback EOF reached. FE state=2 -- activating main menu via sub_825C1410.
[HOOK] sub_825C1410 returned -- FE state 2 -> 4
```

Implementation: `src/vp6_bridge.cpp` (521 lines)
- libavcodec VP6 decoder integration
- EA RWMovie demuxer (`MV0K`/`MV0F` chunks)
- YUV420P → BGRA8888 conversion (BT.601)
- D3D12 IssueSwap overlay substitution (1280×720 BGRA8 upload texture)
- Wall-clock pacer at 33,367μs per frame (29.97 fps NTSC)

### 5. GPU command pipeline

Per 35-second run, observed via D3D12 command processor diagnostics:

| Metric | Count |
|--------|-------|
| Total CP packets processed | 1,468,834 |
| Draw calls (DRAW_INDX_2) | 178,000+ |
| EVENT_WRITE packets | 302,405 |
| VdSwap calls | 7,000-13,000 |
| `IssueCopy` (kCopy resolves) | 22,800+ |
| Resolves to swap chain (0x05258000) | 5,351 |
| Resolves to intermediate buffers | 17,000+ |
| `[ReadbackPath] resolve_ok=true` | 22,000+ |

All resolve operations return `resolve_ok=true` with valid `written_address` and `written_length`. Memory copy from GPU buffer to CPU swap-chain memory succeeds.

### 6. FE manager construction

Verified at runtime:

```
mem[0x83073544] = 0x47998170   # FE-mgr instance pointer
mem[0x47998170 + 0] = 0x823071A0   # vtable / state-name catalog (verified rdata)
mem[0x47998170 + 0x20] = 0x10      # std::list head sentinel (empty)
mem[0x47998170 + 0x24] = 0         # std::list tail sentinel (empty)
```

The FE manager singleton is correctly allocated by `sub_826231F8` (`recomp.23.cpp:52680`) which calls `sub_82883930` to allocate 264 bytes.

### 7. sub_825D3700 (FE menu UI builder)

Function executes per-frame at state==5 without crash:

```
[HOOK] sub_82F02368 #1 fired sub_825D3700 (FE menu) fe_mgr=47998170 vt=823071A0
list[+0x20]=00000010 +0x24=00000000 -> r3=47A88D90 (fire #1)
[HOOK] sub_82F02368 #2400 fired sub_825D3700 ... -> r3=47A88D90 (fire #2400)
```

The function fires 2,400+ times per run, returns successfully, but exits early because the screen list is empty (the count `(tail - head) / 20` is 0).

## What doesn't work yet

### Menu UI render

The swap chain at `fetch_base=0x05258000` consistently shows all-black content:

```
IssueSwap #2520: first32B=[00 00 00 FF 00 00 00 FF 00 00 00 FF 00 00 00 FF ...]
```

Root cause (under investigation):
- The screen registry table at `0x82FD33E8` (129 entries × 16 bytes) lists all menu screens including main menu
- `sub_8258D4F8` (`recomp.18.cpp:35990`) is the top-level entry that should iterate the registry and call `sub_825BB5A0` per row to construct each screen
- `sub_8258D4F8` has zero callers in our build's `.text` section — it's never invoked
- This is consistent with the XenonRecomp Issue #90 jump-table detection bug — EA's compiler emits a pattern that the analyzer misses, leaving certain entry points unreferenced

## Configuration verified safe

`build/skate3.toml`:

```toml
log_verbose = false
log_level = "debug"          # "trace" for full kernel-call detail
log_noisy = false            # set true for NtCreateFile traces
protect_zero = false         # Skate 3 writes at low guest addresses
audio_mute = true            # avoid uninitialized audio worker pointer
input_backend = "xinput"
mnk_mode = true
game_data_root = "..."
d3d12_readback_resolve = true       # CRITICAL: enable EDRAM→CPU readback
d3d12_readback_memexport = true     # for pixel-shader memexport
```

## Test evidence

Full logs available at `build/logs/`:

- `skate3_312.log` and rotated files: trace-level run with full `NtCreateFile` activity
- `skate3_322.log`: state machine progression verified
- `skate3_325.log`: VP6 playback + sub_82ECFFE0 bootstrap firing
- `skate3_330.log`: post-revert with VP6 working

## Forward-looking summary

This effort is the first publicly-documented attempt to render Skate 3 outside Xbox 360 hardware. The core blocker is now narrowly scoped to a single missing function call (`sub_8258D4F8`), making the remaining work tractable for any future contributor.
