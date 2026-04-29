# Cookbook — Reusable Patterns for Xbox 360 → PC Static Recompilation

This folder collects battle-tested patterns developed during the Skate 3 port that apply to **any** Xbox 360 → PC static recompilation project using rexglue / XenonRecomp.

Each recipe documents:
- **When to use** — symptoms that indicate this pattern applies
- **Why it works** — root cause and mechanism
- **Code template** — copy-paste-ready snippet
- **Caveats** — known gotchas

## Recipe index

| # | Recipe | When to use |
|---|--------|-------------|
| 1 | [VP6 video integration](recipe-01-vp6-integration.md) | Title has `.vp6` cutscenes |
| 2 | [Defensive vtable-NULL skip](recipe-02-defensive-vtable.md) | Crashes during streaming-thread teardown |
| 3 | [GPU pool refill bootstrap](recipe-03-gpu-pool-refill.md) | Black screen but GPU is firing draws |
| 4 | [FE state machine override](recipe-04-fe-state-override.md) | Title stuck before menu |
| 5 | [Asset-load completion bridge](recipe-05-asset-completion.md) | Title loads files but never proceeds |
| 6 | [VP6 D3D12 IssueSwap overlay](recipe-06-vp6-d3d12-overlay.md) | VP6 plays but isn't on screen |
| 7 | [Diagnostic instrumentation](recipe-07-diagnostics.md) | Need visibility into rexglue-sdk internals |
| 8 | [Capturing a Xenia HIR baseline](recipe-08-xenia-hir-dump.md) | Need ground truth for diff |

## Project setup checklist

Before using these recipes, ensure your project has:

- [ ] rexglue-sdk built and linked
- [ ] Game `.iso` mounted or extracted to a known path
- [ ] `gameid_config.toml` with `code_base`, `code_size`, `image_base`, `image_size`
- [ ] Build successfully produces `gameid.exe`
- [ ] First boot reaches at least one log line in `build/logs/`

## Common diagnostic flags

Add to `gameid.toml` for full visibility:

```toml
log_verbose = true
log_level = "trace"        # produces many MB of log per minute
log_noisy = true           # enables NtCreateFile traces, etc.

# Critical fixes for many titles:
protect_zero = false       # if title writes at low addresses
audio_mute = true          # if audio thread crashes early
d3d12_readback_resolve = true     # for EDRAM-resolved swap chains
d3d12_readback_memexport = true   # for pixel-shader memexport titles
```

## Xenia reference build

Always have a working Xenia Canary build of the same `.iso`. Run with:

```toml
log_high_frequency_kernel_calls = true
log_string_format_kernel_calls = true
trace_function_coverage = true
trace_functions = true
enable_early_precompilation = true   # generates HIR dumps for ALL functions
dump_translated_hir_functions = true
```

This produces `hirdump_title_<id>/<addr_hex>` files you can diff against your recompile.

**WARNING:** disable these for normal play; `trace_function_data` and `trace_gpu_stream` cause black screens due to I/O overhead.

## Order of operations for a new port

1. Get the build to compile cleanly
2. Get the title to reach VP6 / pre-menu state (recipe 1, 6)
3. Get the FE state machine to advance (recipe 4)
4. Get the GPU pipeline to fire draws (recipe 3, 7)
5. Get menu UI to render (game-specific, varies wildly)

## When to give up and switch to indie XBLA

If after 5 sessions you're still on step 4-5 with no visible progress, consider switching to a smaller XBLA indie (~10-500MB) for proof-of-concept:

- Hexic HD (~10MB)
- Geometry Wars
- Limbo
- Braid

Then come back to the harder title with experience and tools.

## Contributing back

If you develop a new pattern, please contribute it back. Open a PR adding a recipe in this folder.
