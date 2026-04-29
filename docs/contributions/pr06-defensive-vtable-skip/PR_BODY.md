# Defensive vtable-NULL-skip helper macro

## Summary

**Status:** 📝 PROPOSAL — not yet implemented as code patch.

This proposal formalizes a pattern used in Skate 3, Sonic Unleashed, and likely other Xbox 360 titles: defensively skipping vtable-method dispatch when the vtable slot is NULL during streaming-thread teardown or dynamic-linkage transitions.

## Problem statement

Many Xbox 360 game engines use plugin-style architectures with vtable dispatch through dynamically-loaded subsystems. During streaming-thread teardown (e.g. when a level transitions, or when audio/IO subsystems shut down), vtable slots can transiently become NULL. The recompiled body issues `bctrl` through that NULL slot, and the runtime traps with "indirect call to out-of-range guest address 00000000".

The standard mitigation is a hook on the function containing the bctrl that:
1. Reads the vtable pointer from the relevant offset
2. Reads the slot index
3. If the slot is NULL, performs the function's natural early-return path
4. Otherwise, calls `__imp__sub_XXXXXX` to run the real recompiled body

This pattern is used in `skate3-rexglue/src/hooks.cpp` for at least 4 functions (`sub_828F47D0`, `sub_828F6B00`, `sub_82F02368`, `sub_82F010B0`). UnleashedRecomp uses similar patterns.

## Proposed helper

Add a helper macro to `rex/hooks/defensive.h`:

```cpp
// rex/hooks/defensive.h

namespace rex::hooks {

// Returns true iff `addr` looks like a valid guest pointer.
inline bool IsLikelyGuestPointer(uint32_t addr) {
    return addr >= 0x40000000u && addr < 0xA0000000u;
}

// Walks a chain of pointer dereferences, returning 0 on any NULL.
// Usage: PointerChain(base, ptr, +20, +4, +0, +16) reads
//   mem[mem[mem[mem[ptr+20]+4]+0]+16]
template <typename... Offsets>
uint32_t PointerChain(uint8_t* base, uint32_t ptr, Offsets... offsets) {
    if (!IsLikelyGuestPointer(ptr)) return 0;
    auto step = [base](uint32_t p, uint32_t off) -> uint32_t {
        if (!IsLikelyGuestPointer(p)) return 0;
        return REX_LOAD_U32(p + off);
    };
    uint32_t result = ptr;
    ((result = step(result, offsets)), ...);
    return result;
}

}  // namespace rex::hooks

// Convenience macro for the common pattern:
//   uint32_t method = REX_VTABLE_METHOD_OR_NULL(ctx.r3.u32, 20, 4, 16);
//   if (method == 0) {
//       // take early-exit path
//       ctx.r3.s64 = early_exit_value;
//       return;
//   }
//
// Reads:
//   container = mem[r3 + 20]
//   element   = mem[container + 4]
//   vtable    = mem[element + 0]
//   method    = mem[vtable + 16]
#define REX_VTABLE_METHOD_OR_NULL(this_ptr, ...) \
    rex::hooks::PointerChain(base, (this_ptr), __VA_ARGS__)
```

## Refactored Skate 3 hook example

Before:

```cpp
extern "C" REX_FUNC(sub_828F47D0) {
  uint32_t obj = static_cast<uint32_t>(ctx.r3.u32);
  if (IsLikelyGuestPointer(obj)) {
    uint32_t container = LoadU32IfPointer(base, obj, 20);
    uint32_t element = LoadU32IfPointer(base, container, 4);
    uint32_t vt = LoadU32IfPointer(base, element);
    uint32_t method4 =
        IsLikelyGuestPointer(vt) ? static_cast<uint32_t>(REX_LOAD_U32(vt + 16)) : 0;
    if (method4 == 0) {
      ctx.r3.s64 = 1;
      return;
    }
  }
  __imp__sub_828F47D0(ctx, base);
}
```

After:

```cpp
extern "C" REX_FUNC(sub_828F47D0) {
  uint32_t method4 = REX_VTABLE_METHOD_OR_NULL(ctx.r3.u32, 20, 4, 0, 16);
  if (method4 == 0) {
    ctx.r3.s64 = 1;
    return;
  }
  __imp__sub_828F47D0(ctx, base);
}
```

5 lines of boilerplate per defensive hook → 1 line.

## Bounded fields

The macro pattern handles vtable dispatch with arbitrary depth (typical: 2-4 levels). It does not generalize to:
- Conditional dispatches (e.g., "if state==X dispatch via vtable[N], else vtable[M]")
- Multi-argument bctrl preparation

For those, the existing manual hook pattern stays.

## Test cases

In Skate 3:
- `sub_828F47D0` (linked-list iterator with broken-vtable skip)
- `sub_828F6B00` (companion list method)
- `sub_82F02368` (per-frame poller with multiple vtable dispatches)

## Related: RAII guard for ctx save/restore

Common pattern:

```cpp
PPCContext saved = ctx;
ctx.r3.u32 = some_addr;
__imp__sub_XXX(ctx, base);
ctx = saved;
```

Could be wrapped:

```cpp
{
    rex::hooks::ContextGuard guard(ctx);
    ctx.r3.u32 = some_addr;
    __imp__sub_XXX(ctx, base);
}  // ctx restored here
```

This is OPTIONAL bonus refactoring, can be a follow-up PR.

## Why this matters

Reduces hook boilerplate by ~80%. Makes hooks smaller, more reviewable, less error-prone. Consolidates the pattern used across multiple ports into one well-tested helper.
