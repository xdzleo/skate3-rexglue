# Log `IssueCopy_ReadbackResolvePath` returns

## Summary

Adds a single log line at the start of `D3D12CommandProcessor::IssueCopy_ReadbackResolvePath()` capturing the result of `RenderTargetCache::Resolve()` (success flag + `written_address` + `written_length`).

## Problem statement

`IssueCopy_ReadbackResolvePath()` is the kFast/kSome path entered when `d3d12_readback_resolve` is `"true"` (the recommended setting for many titles). The function calls `render_target_cache_->Resolve(...)` and proceeds with various early-returns — but until this PR there was zero visibility into what the resolve actually wrote.

When a title is suspected of "resolves happen but content is empty", you can't tell from existing logs whether:

- `Resolve()` returned `false` (failure)
- `Resolve()` returned `true` but `written_length == 0` (early exit path at line 3198)
- `Resolve()` returned `true` with bytes written but to a wrong address

## Changes

**File:** `src/graphics/d3d12/command_processor.cpp`
**Lines added:** 9

After `Resolve()` returns, log:

```cpp
if (rn <= 32 || (rn % 240) == 0) {
  REXGPU_INFO("[ReadbackPath] #{} resolve_ok={} written_addr={:08X} written_len={}",
              rn, resolve_ok, written_address, written_length);
}
```

## Test evidence

Skate 3 port log sample:

```
[ReadbackPath] #1 resolve_ok=true written_addr=05258000 written_len=3768320
[ReadbackPath] #240 resolve_ok=true written_addr=05019000 written_len=1474560
[ReadbackPath] #480 resolve_ok=true written_addr=04EB1000 written_len=294912
```

Surfaced that the title resolves to multiple destinations (intermediate render targets at `0x05019000`, `0x04EB1000`) AND the swap chain (`0x05258000`). Without this, we could not determine the resolve dest distribution.

## Compatibility

Same as PR 01 — INFO-level, low-overhead, no API change.

## Pairs with

- PR 01 (IssueCopy entry diagnostic) — together they give complete picture of EDRAM resolve pipeline
- PR 03 (IssueDraw edram_mode trace) — provides upstream context about which mode triggers resolve
