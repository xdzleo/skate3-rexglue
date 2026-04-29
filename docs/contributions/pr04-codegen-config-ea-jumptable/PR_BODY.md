# Codegen analyzer: detect EA's PPC jump-table pattern

## Summary

**Status:** 📝 PROPOSAL — not yet implemented as code patch.

This proposal documents a PPC jump-table pattern emitted by EA Black Box's Xbox 360 compiler that is not detected by the current rexglue function scanner. Detection would unblock several EA-published titles including Skate 3 and Burnout series.

## Problem statement

[XenonRecomp Issue #90](https://github.com/hedge-dev/XenonRecomp/issues/90) reports that Skate 3's `default.xex` contains jump tables that XenonAnalyser cannot detect. The same bug affects rexglue (forked from XenonRecomp).

The current scanner at `src/codegen/function_scanner.cpp:1502-1568` detects two canonical jump-table patterns:

1. **Sonic-Unleashed pattern** (slwi via rlwinm canonical form):
   ```
   cmplwi rN, count
   bgt default_case
   rlwinm rA, rS, 2, 0, 29   ; slwi-equivalent
   lwz rA, table(rA)
   mtctr rA
   bctr
   ```

2. **Absolute / computed / byte-offset / short-offset** layouts (4 variants).

EA's compiler emits a different pattern that the scanner misses. The exact pattern is documented below.

## Pattern documentation

### Pattern A (observed in Skate 3 sub_82720A08, sub_82822168, others)

```
cmplwi cr6, rN, count
bgt cr6, default_case
addi rA, rN, -low_bound       ; subtract base index
slwi rA, rA, 2                ; OR rlwinm rA, rA, 2, 0, 29
lis rB, table_hi
addi rB, rB, table_lo
add rB, rB, rA
lwz rB, 0(rB)
mtctr rB
bctr
```

Differences from Sonic-Unleashed pattern:
- Explicit `addi` with negative immediate to subtract a low-bound index
- `lis + addi + add` to compute the table address (not an immediate `lwz`)

### Pattern B (observed in EA RW3 vtable installers)

```
cmplwi cr6, rN, count
bgt cr6, default_case
mulli rA, rN, 16           ; 16-byte stride (entries are {fnptr,fnptr,size,flags})
addi rA, rA, table_offset
lwzx rB, rA, rTableBase
mtctr rB
bctr
```

This is used for the FE state registry walker. Stride is 16 bytes, not 4. Each entry contains four 32-bit values: {function pointer 1, function pointer 2, size, flags}. Skate 3's screen registry at `0x82FD33E8` is built this way.

## Proposed implementation

Extend `function_scanner.cpp:DetectJumpTable()` with two new pattern matchers:

```cpp
// Pattern A: addi-low-bound + slwi + lis/addi/add + lwz
// Pattern B: mulli stride 16 + lwzx
//
// Both produce a JumpTableInfo with stride/base/count/cases that downstream
// functionScan can use to enumerate target functions.
```

## Test cases

For Pattern A:
- Skate 3: `sub_82720A08` at offset 0x82720A40 (verified switch on r4 with 4 cases)
- Skate 3: `sub_82822168` at offset 0x82822180 (verified screen-id dispatch)

For Pattern B:
- Skate 3: `sub_8258D4F8` at offset 0x8258D540 (verified 16-byte stride registry walker)
- Possibly Burnout Paradise titles (untested)

## Risks and considerations

- **False positives.** Adding looser jump-table detection risks splitting parent functions incorrectly. Mitigation: require all four anchor instructions in sequence (cmplwi, bgt, slwi/mulli, lwz/lwzx).
- **Regression on non-EA titles.** Currently-supported titles must not regress. Mitigation: feature-flag the new patterns behind a config toggle (`enable_ea_jumptable_detection`) until validated.

## Implementation sketch

```cpp
// In rex/codegen/config.h:
struct CodegenConfig {
    // ... existing fields
    bool enable_ea_jumptable_detection = false;  // Pattern A + B
};

// In src/codegen/function_scanner.cpp:
bool DetectJumpTablePatternA(const InstructionStream& insns, ...);
bool DetectJumpTablePatternB(const InstructionStream& insns, ...);

// In FunctionScanner::ScanInstructionStream():
if (config_.enable_ea_jumptable_detection) {
    if (DetectJumpTablePatternA(...) || DetectJumpTablePatternB(...)) {
        // emit case targets as additional function entry candidates
    }
}
```

## Why this matters

Once EA's jump-table pattern is detected, the analyzer can correctly emit case-target functions as separate `DEFINE_REX_FUNC` entries. Currently those targets are merged into the parent function and become unreachable via indirect dispatch.

For Skate 3 specifically, the missing detection causes:
- `sub_8258D4F8` (screen registry walker) to never fire — main menu never builds
- vtable installation chains for FE-mgr to be incomplete
- Numerous indirect calls to fall through to NULL

## References

- [XenonRecomp Issue #90](https://github.com/hedge-dev/XenonRecomp/issues/90)
- [librw — RW3 reference](https://github.com/aap/librw)
- Skate 3 dumps at [skate3-rexglue/dumps/](https://github.com/xdzleo/skate3-rexglue/tree/main/dumps)

## Next steps

1. Reach out to rexglue maintainers to confirm scope acceptance
2. Build a minimal test harness with Skate 3 jump-table snippets
3. Implement Pattern A first (simpler), validate with skate3-rexglue test build
4. Iterate on Pattern B once Pattern A is stable
