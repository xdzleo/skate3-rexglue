# codegen: fix `build_dcbz` to use 128-byte Xenon cache line size

## Summary

Fixes `build_dcbz` in `src/codegen/builders/system.cpp` to use 128-byte cache lines (Xenon CPU's actual L1 cache line size) instead of 32-byte. Brings it in line with the already-correct `build_dcbzl` builder.

## The bug

Xbox 360's Xenon PowerPC variant has **128-byte L1 cache lines**, not 32-byte. The PowerPC architecture mandates that `dcbz` operate on the implementation's cache-line size — Xenon defines this as 128 bytes (per the Xenon Reference Manual / IBM CBE technical docs).

Before this fix, the codegen emitted `dcbz` as a 32-byte memset. The Xenon toolchain compiler (Xenon GCC / XDK CL) emits `dcbz` expecting 128-byte clears, typically as fast object-zero in `operator new` and constructor preludes. With only the first 32 bytes cleared per cache line, the remaining 96 bytes retained stale heap content from prior allocations.

The asymmetry was already self-evident in the same file: `build_dcbzl` (the architecturally-explicit 128-byte form) at lines 160-168 is implemented as 128-byte. `build_dcbz` was the inconsistent one.

## Fix

Change `build_dcbz` to use `& ~127` alignment and `memset(..., 0, 128)`, identical to `build_dcbzl`.

```diff
 bool build_dcbz(BuilderContext& ctx) {
-  // Compute EA, align to 32-byte cache line, apply physical offset
+  // Xbox 360's Xenon CPU has 128-byte L1 cache lines, NOT 32-byte.
+  // ...
   ctx.print("\t{} = (", ctx.ea());
   if (ctx.insn.operands[0] != 0)
     ctx.print("{}.u32 + ", ctx.r(ctx.insn.operands[0]));
-  ctx.println("{}.u32) & ~31;", ctx.r(ctx.insn.operands[1]));
-  ctx.println("\tmemset((void*)REX_RAW_ADDR({}), 0, 32);", ctx.ea());
+  ctx.println("{}.u32) & ~127;", ctx.r(ctx.insn.operands[1]));
+  ctx.println("\tmemset((void*)REX_RAW_ADDR({}), 0, 128);", ctx.ea());
   return true;
 }
```

## Discovery

In a sister-project port of Final Exam (Mighty Rocket Studio, 2014, Xbox 360 XBLA), a freshly-allocated resource-bank object had its vtable slot read as non-zero garbage. The 32-byte `dcbz` left bytes 32-127 of each cache line containing previous heap content. The constructor that ran after `dcbz` only initialized some fields, so subsequent reads of "un-initialized" fields produced non-zero values. Indirect-call dispatches through the corrupted vtable slot then failed with `ctr=0` ("Indirect call to out-of-range guest address 00000000").

Diagnosis confirmed by reading the codegen output side-by-side with disassembly of the title's allocator. The XDK's Xenon-targeting compiler relies on `dcbz` as a 128-byte-clear operation in its codegen for fast object-zero patterns.

## Compatibility

- ✅ **Skate 3**: tested, no regression. Skate 3's emitted `dcbz` count is small.
- ✅ `dcbzl` was already 128-byte; this just brings `dcbz` in line.
- ✅ Compatible with the `AccessViolationCallback`'s existing 128-byte cache-line assumptions.
- ✅ No API changes, no cvar changes, no header changes.
- ✅ Compiles clean on MSVC, Clang/LLVM, GCC.

## Reviewer checklist

- [ ] Code review of changes in `src/codegen/builders/system.cpp` (lines 150-158)
- [ ] Verify Xenon ref manual cache-line size = 128 bytes
- [ ] Test with at least one title to confirm no regression
- [ ] Note: this fixes potential silent data corruption in titles that emit `dcbz` (EA RW3, custom engines, etc.)

## References

- Xenon CBE technical reference: 128 B L1 cache line size
- xenia-canary uses 128-byte alignment for `dcbz` in `xenia/cpu/ppc/ppc_emit_memory.cc`
- File:line of the asymmetry: `src/codegen/builders/system.cpp:150-158` (dcbz, was 32B) vs `src/codegen/builders/system.cpp:160-168` (dcbzl, was already 128B)

## Related work

This PR is part of a broader effort. See [skate3-rexglue](https://github.com/xdzleo/skate3-rexglue) for the Skate 3 PC port research that surfaced PRs #300/#301/#302, and the Final Exam port investigation that surfaced this issue.
