/**
 * @file        src/hooks.cpp
 * @brief       Skate 3 -- runtime hooks of recompiled functions.
 */

#include "generated/skate3_init.h"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>

#include <rex/logging.h>
#include <rex/ppc.h>

extern "C" REX_FUNC(__imp__sub_82883B10);
extern "C" REX_FUNC(__imp__sub_827FBC28);
extern "C" REX_FUNC(__imp__sub_82C71988);
extern "C" REX_FUNC(__imp__sub_8247E9A0);
extern "C" REX_FUNC(__imp__sub_825A3D20);
extern "C" REX_FUNC(__imp__sub_826B5690);
extern "C" REX_FUNC(__imp__sub_82966438);
extern "C" REX_FUNC(__imp__sub_82966700);
extern "C" REX_FUNC(__imp__sub_82966948);
extern "C" REX_FUNC(__imp__sub_82966970);
extern "C" REX_FUNC(__imp__sub_82966990);
extern "C" REX_FUNC(__imp__sub_82945B88);
extern "C" REX_FUNC(__imp__sub_82931578);
extern "C" REX_FUNC(__imp__sub_82933740);
extern "C" REX_FUNC(__imp__sub_82933810);
extern "C" REX_FUNC(__imp__sub_8293F0E8);
extern "C" REX_FUNC(__imp__sub_828E3F30);
extern "C" REX_FUNC(__imp__sub_82F02368);
extern "C" REX_FUNC(__imp__sub_828F47D0);
extern "C" REX_FUNC(__imp__sub_828F6B00);
extern "C" REX_FUNC(__imp__sub_82EB7610);
extern "C" REX_FUNC(__imp__sub_82EB8CC8);
extern "C" REX_FUNC(__imp__sub_82CDF8E8);
extern "C" REX_FUNC(__imp__sub_82F01848);
extern "C" REX_FUNC(__imp__sub_82F01240);
extern "C" REX_FUNC(__imp__sub_82B5A648);
extern "C" REX_FUNC(__imp__sub_82A52BF8);
extern "C" REX_FUNC(__imp__sub_82B62540);
extern "C" REX_FUNC(__imp__sub_82F01038);
extern "C" REX_FUNC(__imp__sub_82B62078);
extern "C" REX_FUNC(__imp__sub_82B4C5A8);
extern "C" REX_FUNC(__imp__sub_82B4CC28);
extern "C" REX_FUNC(__imp__sub_82B60FA0);
extern "C" REX_FUNC(__imp__sub_82B616D8);
extern "C" REX_FUNC(__imp__KeSetEvent);
extern "C" REX_FUNC(__imp__sub_82A68EE0);
extern "C" REX_FUNC(__imp__sub_82A6A038);
extern "C" REX_FUNC(__imp__sub_82A674D8);
extern "C" REX_FUNC(__imp__sub_82A67400);
extern "C" REX_FUNC(__imp__sub_827DB100);
extern "C" REX_FUNC(__imp__sub_827DB6C8);
extern "C" REX_FUNC(__imp__sub_825C1410);  // FE MainMenu activator (sets state=4)
extern "C" REX_FUNC(__imp__sub_826C3508);  // FE EnterMainMenu wrapper
extern "C" REX_FUNC(__imp__sub_82F2A108);  // rwfilesys thread-init dispatcher
extern "C" REX_FUNC(__imp__sub_8293F018);  // Asset-load completion broadcaster
extern "C" REX_FUNC(__imp__NtClose);       // Kernel handle close
extern "C" REX_FUNC(__imp__sub_82ECFFE0);  // RW3 AllocSceneGraphBuffer (sets +252)
extern "C" REX_FUNC(__imp__sub_825D3438);  // FE-mgr vtable[4] readiness gate
extern "C" REX_FUNC(__imp__sub_82F02178);  // Draw-list builder (writes +184 via sub_82F01848 internal)
extern "C" REX_FUNC(__imp__sub_825D3700);  // FE menu UI builder (vtable[4] of FE manager)
extern "C" REX_FUNC(__imp__sub_82822168);  // CYCLE 24: PushScreen 13 onto FE-mgr stack
extern "C" REX_FUNC(__imp__sub_82720A08);  // CYCLE 24: Register screen flag set 1|4|16 + push node
extern "C" REX_FUNC(__imp__sub_82720B00);  // CYCLE 24: Deregister screen flag

// VP6 bridge -- libavcodec wrapper for Skate 3 intro/movies. The C entry
// points are in src/vp6_bridge.cpp.
extern "C" bool Skate3VP6_ProbeAvailable();
extern "C" int Skate3VP6_DecodeFrame(const uint8_t* data, int size,
                                      uint8_t* out_rgba, int out_width,
                                      int out_height);
extern "C" void Skate3VP6_Reset();
extern "C" int Skate3VP6_FrameCount();
extern "C" bool Skate3VP6_OpenFile(const char* path);
extern "C" int Skate3VP6_NextPacket(const uint8_t** out_data, int* out_size, int* out_is_key);
extern "C" void Skate3VP6_CloseFile();
extern "C" void Skate3VP6_GetMovieInfo(uint32_t* w, uint32_t* h, uint32_t* frame_count);
extern "C" int Skate3VP6_TestDecode(const char* path);
extern "C" bool Skate3VP6_StartPlayback(const char* path);
extern "C" int Skate3VP6_AdvanceFrame();
extern "C" bool Skate3VP6_PlaybackActive();

// ----------------------------------------------------------------------------
// MoviePlayer2 play-list drain stub (substitutes for libavcodec/VP6 hook).
//
// Original plan: hook MoviePlayer2 decode-thread proc at sub_82A68E78,
// decode VP6 packets via libavcodec, pop drained PlaybackRequest entries
// from the play list at mem[mp+40]. That symbol does NOT exist in our
// codegen (closest entries: sub_82A68E68 / sub_82A68EC0 in skate3_init.h);
// 0x82A68E78 is a mid-function address Ghidra labelled, not a function
// start the recompiler emitted. Without a generated entry we can't attach
// a REX_FUNC override. Additionally, libavcodec.lib is only linked into
// rex::audio in the current SDK CMake, not the modules our target pulls
// in (rex::core/system/kernel/graphics/ui), so even a libavcodec call
// would fail to link without further CMake changes.
//
// Per the user's fallback spec: write the SIMPLEST possible stub that
// reads the play list head from mem[mp+40], pops it (writes mem[mp+40] =
// mem[head+next_offset]), increments a counter, and returns. We invoke it
// from sub_82F02368 (already a per-frame hook running post-language-menu)
// so the title's polling sites that fetch the list head via
// vtable[5] -> `return mem[r3+40]` eventually see "list empties over
// time" and advance state.
//
// PlaybackRequest layout (per the surrounding decode-driver loop body
// sub_82A68EE0 in skate3_recomp.60.cpp ~57040: it does
//   lwz r11,128(r30); addi r11,r11,-1; stw r11,128(r30)   <- frame counter
//   lwz r11,32(r30);  addi r11,r11,-1; stw r11,32(r30)    <- refcount
// and uses r30 = (queue header).head_iter):
//   +0    vtable
//   +4    next pointer (singly-linked list)  -- conservative guess
//   +32   refcount-ish word
//   +128  frame counter
//   +132  state byte (0 = active)
namespace {
uint32_t DrainMoviePlayer2PlayList(uint8_t* base) {
  // MoviePlayer2 singleton globals -- existing comments call out
  // 0x8302844C and 0x8302853C. Try both.
  static constexpr uint32_t kMpSingletonGlobals[] = {0x8302844Cu, 0x8302853Cu};
  uint32_t drained = 0;
  for (uint32_t mp_global : kMpSingletonGlobals) {
    uint32_t mp = static_cast<uint32_t>(REX_LOAD_U32(mp_global));
    if (mp < 0x10000u) {
      continue;
    }
    // Play list head at mem[mp+40]; pop one node by relinking head to
    // mem[head+4] (most common slist layout). One pop per frame is
    // enough -- the queue is small and the title polls per-frame.
    uint32_t head = static_cast<uint32_t>(REX_LOAD_U32(mp + 40));
    if (head < 0x10000u) {
      continue;
    }
    uint32_t next = static_cast<uint32_t>(REX_LOAD_U32(head + 4));
    if (next == head) {
      // Self-link sentinel -- treat as empty.
      next = 0;
    }
    REX_STORE_U32(mp + 40, next);
    ++drained;
  }
  return drained;
}
}  // namespace

namespace {

std::array<std::atomic<uint16_t>, 4> g_last_pad_mask{};
std::atomic<int> g_interesting_action_logs{0};

float LoadGuestFloat(uint8_t* base, uint32_t address) {
  return std::bit_cast<float>(static_cast<uint32_t>(REX_LOAD_U32(address)));
}

bool IsLikelyGuestPointer(uint32_t address) {
  return address >= 0x10000;
}

uint32_t LoadU32IfPointer(uint8_t* base, uint32_t address, uint32_t offset = 0) {
  return IsLikelyGuestPointer(address) ? static_cast<uint32_t>(REX_LOAD_U32(address + offset)) : 0;
}

uint16_t ReadDigitalMaskFromBuffer(uint8_t* base, uint32_t buffer) {
  if (!buffer) {
    return 0;
  }

  uint16_t mask = 0;
  auto set_if = [&](uint32_t offset, uint16_t bit) {
    if (LoadGuestFloat(base, buffer + offset) > 0.5f) {
      mask |= bit;
    }
  };

  set_if(0, 0x0001);
  set_if(4, 0x0002);
  set_if(8, 0x0004);
  set_if(12, 0x0008);
  set_if(16, 0x0010);
  set_if(20, 0x0020);
  set_if(24, 0x0040);
  set_if(28, 0x0080);
  set_if(32, 0x0100);
  set_if(36, 0x0200);
  set_if(48, 0x1000);
  set_if(52, 0x2000);
  set_if(56, 0x4000);
  set_if(60, 0x8000);
  return mask;
}

uint16_t CombinedPadMask() {
  uint16_t mask = 0;
  for (const auto& slot_mask : g_last_pad_mask) {
    mask |= slot_mask.load(std::memory_order_relaxed);
  }
  return mask;
}

bool ShouldLogActionQuery(int n, uint16_t raw_mask, uint32_t result) {
  (void)result;
  if (n <= 120 || (n % 5000) == 0) {
    return true;
  }
  if (raw_mask &&
      g_interesting_action_logs.fetch_add(1, std::memory_order_relaxed) < 500) {
    return true;
  }
  return false;
}

void LogManagerQuery(const char* name, uint8_t* base, int n, uint32_t manager,
                     uint32_t action, uint32_t flags, uint32_t r7, uint32_t r8,
                     uint32_t result) {
  uint16_t raw_mask = CombinedPadMask();
  if (!ShouldLogActionQuery(n, raw_mask, result)) {
    return;
  }

  uint32_t slot = manager ? static_cast<uint32_t>(REX_LOAD_U32(manager + 8)) : 0xFFFFFFFFu;
  uint32_t and_mask = manager ? static_cast<uint32_t>(REX_LOAD_U32(manager + 12)) : 0;
  uint32_t or_mask = manager ? static_cast<uint32_t>(REX_LOAD_U32(manager + 16)) : 0;
  uint32_t input_root = manager ? static_cast<uint32_t>(REX_LOAD_U32(manager + 20)) : 0;
  REXLOG_INFO("[HOOK] {} #{} mgr={:08X} slot={} action={} flags={:08X} r7={:08X} "
              "r8={:08X} and={:08X} or={:08X} root={:08X} raw=0x{:04X} ret={:08X}",
              name, n, manager, slot, action, flags, r7, r8, and_mask, or_mask,
              input_root, raw_mask, result);
}

void LogLowLevelQuery(const char* name, uint8_t* base, int n, uint32_t player,
                      uint32_t binding, uint32_t action, uint32_t flags, uint32_t min_value,
                      uint32_t extra, uint32_t result) {
  uint16_t raw_mask = CombinedPadMask();
  if (!ShouldLogActionQuery(n, raw_mask, result)) {
    return;
  }

  uint32_t binding_object = binding ? static_cast<uint32_t>(REX_LOAD_U32(binding)) : 0;
  uint32_t binding_vtable =
      binding_object ? static_cast<uint32_t>(REX_LOAD_U32(binding_object)) : 0;
  uint32_t binding_method8 =
      binding_vtable ? static_cast<uint32_t>(REX_LOAD_U32(binding_vtable + 8)) : 0;
  uint32_t player_user = player ? static_cast<uint32_t>(REX_LOAD_U32(player + 44)) : 0;
  uint32_t result_vtable = result ? static_cast<uint32_t>(REX_LOAD_U32(result)) : 0;
  uint32_t result_04 = result ? static_cast<uint32_t>(REX_LOAD_U32(result + 4)) : 0;
  uint32_t result_08 = result ? static_cast<uint32_t>(REX_LOAD_U32(result + 8)) : 0;
  uint32_t result_0c = result ? static_cast<uint32_t>(REX_LOAD_U32(result + 12)) : 0;
  uint32_t result_10 = result ? static_cast<uint32_t>(REX_LOAD_U32(result + 16)) : 0;
  REXLOG_INFO("[HOOK] {} #{} player={:08X} bind={:08X}->{:08X} vt={:08X} m8={:08X} "
              "action={} flags={:08X} min={} extra={:08X} playerUser={} raw=0x{:04X} "
              "ret={:08X} retvt={:08X} r04={:08X} r08={:08X} r0c={:08X} r10={:08X}",
              name, n, player, binding, binding_object, binding_vtable, binding_method8,
              action, flags, min_value, extra, player_user, raw_mask, result, result_vtable,
              result_04, result_08, result_0c, result_10);
}

}  // namespace

extern "C" REX_FUNC(sub_82883B10) {
  uint32_t caller_lr = static_cast<uint32_t>(ctx.lr);
  __imp__sub_82883B10(ctx, base);

  static std::atomic<int> count{0};
  static std::atomic<int> low_result_logs{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t manager = static_cast<uint32_t>(ctx.r3.u32);
  bool low_result = manager != 0 && manager < 0x10000;
  bool should_log = n <= 16 || (n % 100000 == 0);
  if (low_result) {
    should_log =
        should_log || low_result_logs.fetch_add(1, std::memory_order_relaxed) < 32;
  }
  if (should_log) {
    uint32_t singleton = static_cast<uint32_t>(REX_LOAD_U32(0x8307360C));
    uint32_t registry =
        IsLikelyGuestPointer(singleton) ? static_cast<uint32_t>(REX_LOAD_U32(singleton + 4)) : 0;
    uint32_t cat_count =
        IsLikelyGuestPointer(registry) ? static_cast<uint32_t>(REX_LOAD_U32(registry + 0xC1B0)) : 0;
    uint32_t cat_base =
        IsLikelyGuestPointer(registry) ? static_cast<uint32_t>(REX_LOAD_U32(registry + 0xC1B4)) : 0;
    uint32_t current_index = static_cast<uint32_t>(REX_LOAD_U32(0x83060D20));
    uint32_t vtable =
        IsLikelyGuestPointer(manager) ? static_cast<uint32_t>(REX_LOAD_U32(manager)) : 0;
    uint32_t method4 =
        IsLikelyGuestPointer(vtable) ? static_cast<uint32_t>(REX_LOAD_U32(vtable + 4)) : 0;
    uint32_t method8 =
        IsLikelyGuestPointer(vtable) ? static_cast<uint32_t>(REX_LOAD_U32(vtable + 8)) : 0;
    uint32_t method12 =
        IsLikelyGuestPointer(vtable) ? static_cast<uint32_t>(REX_LOAD_U32(vtable + 12)) : 0;
    REXLOG_INFO("[HOOK] sub_82883B10 #{} lr={:08X} manager={:08X} singleton={:08X} "
                "registry={:08X} cat_count={} cat_base={:08X} idx={} vt={:08X} "
                "m4={:08X} m8={:08X} m12={:08X}",
                n, caller_lr, manager, singleton, registry, cat_count, cat_base,
                current_index, vtable, method4, method8, method12);
  }
}

extern "C" REX_FUNC(sub_827FBC28) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t caller_lr = static_cast<uint32_t>(ctx.lr);
  uint32_t object = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t field_10 = LoadU32IfPointer(base, object, 16);
  uint32_t field_28 = LoadU32IfPointer(base, object, 40);
  uint32_t field_c0 = LoadU32IfPointer(base, object, 192);
  uint32_t field_c4 = LoadU32IfPointer(base, object, 196);
  uint32_t field_10_vt = LoadU32IfPointer(base, field_10);
  uint32_t field_28_vt = LoadU32IfPointer(base, field_28);
  uint32_t method_10_20 = LoadU32IfPointer(base, field_10_vt, 32);
  uint32_t method_28_58 = LoadU32IfPointer(base, field_28_vt, 88);
  uint32_t method_28_60 = LoadU32IfPointer(base, field_28_vt, 96);
  bool suspicious = field_c0 != 0 && (method_28_60 == 0 || method_10_20 == 0);

  if (n <= 32 || suspicious) {
    REXLOG_INFO("[HOOK] sub_827FBC28 #{} lr={:08X} obj={:08X} f10={:08X} "
                "f28={:08X} fC0={:08X} fC4={:08X} vt10={:08X} vt28={:08X} "
                "m10+20={:08X} m28+58={:08X} m28+60={:08X}",
                n, caller_lr, object, field_10, field_28, field_c0, field_c4,
                field_10_vt, field_28_vt, method_10_20, method_28_58, method_28_60);
  }

  __imp__sub_827FBC28(ctx, base);

  if (n <= 32 || suspicious) {
    REXLOG_INFO("[HOOK] sub_827FBC28 #{} ret={:08X}", n,
                static_cast<uint32_t>(ctx.r3.u32));
  }
}

extern "C" REX_FUNC(sub_82C71988) {
  static std::atomic<int> count{0};
  static std::atomic<int> dropped_count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t object = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t old_index = LoadU32IfPointer(base, object, 48);
  uint32_t active_index = (old_index + 1) % 3;
  uint32_t queue_index = (active_index + 1) % 3;
  uint32_t queue = object + 52 + queue_index * 16;
  uint32_t begin = LoadU32IfPointer(base, queue);
  uint32_t end = LoadU32IfPointer(base, queue, 4);
  uint32_t byte_count = end - begin;
  uint32_t entry_count = byte_count >> 2;
  bool corrupt_queue =
      !IsLikelyGuestPointer(object) ||
      (end > begin &&
       (!IsLikelyGuestPointer(begin) || !IsLikelyGuestPointer(end) || entry_count > 4096 ||
        (byte_count & 3) != 0));

  if (n <= 16 || corrupt_queue) {
    REXLOG_INFO("[HOOK] sub_82C71988 #{} obj={:08X} old_idx={} active={} queue_idx={} "
                "queue={:08X} begin={:08X} end={:08X} bytes={} entries={} corrupt={}",
                n, object, old_index, active_index, queue_index, queue, begin, end,
                byte_count, entry_count, corrupt_queue ? 1 : 0);
  }

  if (corrupt_queue) {
    int dropped = dropped_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (dropped <= 32 || (dropped % 256) == 0) {
      REXLOG_INFO("[HOOK] sub_82C71988 dropping corrupt queue #{} obj={:08X} queue={:08X} "
                  "begin={:08X} end={:08X} entries={}",
                  dropped, object, queue, begin, end, entry_count);
    }
    if (IsLikelyGuestPointer(queue)) {
      REX_STORE_U32(queue + 4, begin);
    }
  }

  __imp__sub_82C71988(ctx, base);
}

extern "C" REX_FUNC(sub_8247E9A0) {
  static std::atomic<int> skipped_count{0};
  uint32_t object = static_cast<uint32_t>(ctx.r3.u32);
  if (!IsLikelyGuestPointer(object)) {
    int skipped = skipped_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (skipped <= 32 || (skipped % 256) == 0) {
      REXLOG_INFO("[HOOK] sub_8247E9A0 skipping invalid object #{} obj={:08X} "
                  "lr={:08X} r4={:08X} r5={:08X}",
                  skipped, object, static_cast<uint32_t>(ctx.lr),
                  static_cast<uint32_t>(ctx.r4.u32), static_cast<uint32_t>(ctx.r5.u32));
    }
    ctx.r3.u64 = 0;
    return;
  }

  __imp__sub_8247E9A0(ctx, base);
}

extern "C" REX_FUNC(sub_826B5690) {
  constexpr uint32_t kListenerListGlobal = 0x830734AC;

  static std::atomic<int> repaired_count{0};
  uint32_t list_owner = static_cast<uint32_t>(REX_LOAD_U32(kListenerListGlobal));
  uint32_t list = LoadU32IfPointer(base, list_owner);
  uint32_t begin = LoadU32IfPointer(base, list, 4);
  uint32_t end = LoadU32IfPointer(base, list, 8);
  uint32_t byte_count = end - begin;
  uint32_t entry_count = byte_count >> 2;
  bool corrupt_list =
      IsLikelyGuestPointer(list_owner) && IsLikelyGuestPointer(list) && begin != end &&
      (end < begin || !IsLikelyGuestPointer(begin) || !IsLikelyGuestPointer(end) ||
       (byte_count & 3) != 0 || entry_count > 4096);

  if (corrupt_list) {
    int repaired = repaired_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (repaired <= 32 || (repaired % 256) == 0) {
      REXLOG_INFO("[HOOK] sub_826B5690 closing corrupt listener list #{} owner={:08X} "
                  "list={:08X} begin={:08X} end={:08X} entries={}",
                  repaired, list_owner, list, begin, end, entry_count);
    }
    REX_STORE_U32(list + 8, begin);
  }

  __imp__sub_826B5690(ctx, base);
}

extern "C" REX_FUNC(sub_825A3D20) {
  static std::atomic<int> skipped_count{0};
  uint32_t stack = static_cast<uint32_t>(ctx.r1.u32);
  if (!IsLikelyGuestPointer(stack)) {
    int skipped = skipped_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (skipped <= 32 || (skipped % 256) == 0) {
      REXLOG_INFO("[HOOK] sub_825A3D20 skipping invalid PPC stack #{} sp={:08X} "
                  "lr={:08X} r3={:08X} r4={:08X} r5={:08X} ctr={:08X}",
                  skipped, stack, static_cast<uint32_t>(ctx.lr),
                  static_cast<uint32_t>(ctx.r3.u32), static_cast<uint32_t>(ctx.r4.u32),
                  static_cast<uint32_t>(ctx.r5.u32), static_cast<uint32_t>(ctx.ctr.u32));
    }
    ctx.r3.u64 = 0;
    return;
  }

  __imp__sub_825A3D20(ctx, base);
}

// Auto-press the A button + START on user 0 in pulses for the first ~45s
// of gameplay. The intro video pipeline is broken (MoviePlayer2 thread is
// suspended by the runtime to dodge the VP6 decoder bug), so the title
// stays at the post-logo black screen forever waiting for a "video done"
// signal. Most Skate 3 menu/cutscene screens treat A or START as a skip,
// so injecting synthetic presses fast-forwards through the EA / Black
// Box / intro_movie sequence and lands us at the main menu without
// having to mock up the entire video state machine.
//
// Pulse pattern: 30-frame press, 30-frame release. At ~60 Hz that's a
// half-second-on / half-second-off rhythm -- fast enough to register on
// any "press A to skip" listener but slow enough that single-press menu
// actions don't double-fire.
void MaybeInjectAutoSkip(uint8_t* base, uint32_t buffer, uint32_t user, int call_n) {
  // Disabled again to bisect: in run 213 with auto-skip on we saw VP6 open
  // and MoviePlayer2 spawn; in runs 220/221 with auto-skip on we don't.
  // Need to see if turning auto-skip off restores MoviePlayer2 spawn.
  static constexpr bool kEnableAutoSkip = false;
  if (!kEnableAutoSkip) {
    return;
  }
  if (user != 0 || !buffer) {
    return;
  }
  constexpr int kAutoSkipFrames = 45 * 60;  // ~45 seconds at 60 Hz
  if (call_n > kAutoSkipFrames) {
    return;
  }
  // 60-frame cycle: hold A for the first 30 frames, release for the last 30.
  bool press_a = (call_n % 60) < 30;
  if (!press_a) {
    return;
  }
  // Buffer layout: 16x 4-byte digital floats (>0.5 == pressed) at offsets
  // 0..60, then 4 bytes of analog at +40/+44 (LT/RT). Offsets we care
  // about: +48 = A, +52 = B, +60 = Y/Start (actually buffer+0..3 for the
  // first slot; A is at +48). Match the offsets in
  // ReadDigitalMaskFromBuffer above.
  uint32_t a_offset = buffer + 48;
  uint32_t start_offset = buffer + 16;
  uint32_t one = std::bit_cast<uint32_t>(1.0f);
  REX_STORE_U32(a_offset, one);
  REX_STORE_U32(start_offset, one);
  static std::atomic<int> log_count{0};
  int lc = log_count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (lc <= 4 || (lc % 60) == 0) {
    REXLOG_INFO("[HOOK] auto-skip injecting A+START press call={} buf={:08X}",
                call_n, buffer);
  }
}

extern "C" REX_FUNC(sub_82945B88) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t object = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t buffer = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t user = object ? static_cast<uint32_t>(REX_LOAD_U32(object + 8)) : 0xFFFFFFFFu;
  if (n <= 30 || (n % 1200 == 0)) {
    REXLOG_INFO("[HOOK] sub_82945B88 called #{} obj={:08X} buf={:08X} user={}", n, object,
                buffer, user);
  }
  __imp__sub_82945B88(ctx, base);

  // Inject auto-skip presses AFTER __imp__ has filled the buffer.
  MaybeInjectAutoSkip(base, buffer, user, n);

  if (buffer) {
      static std::array<uint16_t, 4> last_mask{};
    uint16_t mask = ReadDigitalMaskFromBuffer(base, buffer);
    uint32_t slot = user < last_mask.size() ? user : 0;
    if (user < g_last_pad_mask.size()) {
      g_last_pad_mask[user].store(mask, std::memory_order_relaxed);
    }
    if (n <= 30 || mask != last_mask[slot]) {
      last_mask[slot] = mask;
      float lt = LoadGuestFloat(base, buffer + 40);
      float rt = LoadGuestFloat(base, buffer + 44);
      REXLOG_INFO("[HOOK] pad user={} buf={:08X} mask=0x{:04X} A={} B={} X={} Y={} Start={} "
                  "Back={} LT={:.3f} RT={:.3f}",
                  user, buffer, mask, (mask & 0x1000) ? 1 : 0, (mask & 0x2000) ? 1 : 0,
                  (mask & 0x4000) ? 1 : 0, (mask & 0x8000) ? 1 : 0,
                  (mask & 0x0010) ? 1 : 0, (mask & 0x0020) ? 1 : 0, lt, rt);
    }
  }
}

extern "C" REX_FUNC(sub_82966948) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t manager = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t action = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t flags = static_cast<uint32_t>(ctx.r6.u32);
  uint32_t r7 = static_cast<uint32_t>(ctx.r7.u32);
  uint32_t r8 = static_cast<uint32_t>(ctx.r8.u32);
  __imp__sub_82966948(ctx, base);
  LogManagerQuery("sub_82966948", base, n, manager, action, flags, r7, r8,
                  static_cast<uint32_t>(ctx.r3.u32));
}

extern "C" REX_FUNC(sub_82966970) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t manager = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t action = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t flags = static_cast<uint32_t>(ctx.r6.u32);
  uint32_t r7 = static_cast<uint32_t>(ctx.r7.u32);
  uint32_t r8 = static_cast<uint32_t>(ctx.r8.u32);
  __imp__sub_82966970(ctx, base);
  LogManagerQuery("sub_82966970", base, n, manager, action, flags, r7, r8,
                  static_cast<uint32_t>(ctx.r3.u32));
}

extern "C" REX_FUNC(sub_82966990) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t manager = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t action = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t flags = static_cast<uint32_t>(ctx.r6.u32);
  uint32_t r7 = static_cast<uint32_t>(ctx.r7.u32);
  uint32_t r8 = static_cast<uint32_t>(ctx.r8.u32);
  __imp__sub_82966990(ctx, base);
  LogManagerQuery("sub_82966990", base, n, manager, action, flags, r7, r8,
                  static_cast<uint32_t>(ctx.r3.u32));
}

extern "C" REX_FUNC(sub_82966438) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t root = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t slot = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t action = static_cast<uint32_t>(ctx.r5.u32);
  uint32_t flags = static_cast<uint32_t>(ctx.r6.u32);
  uint32_t min_value = static_cast<uint32_t>(ctx.r7.u32);
  uint32_t extra = static_cast<uint32_t>(ctx.r8.u32);
  __imp__sub_82966438(ctx, base);
  uint16_t raw_mask = CombinedPadMask();
  if (ShouldLogActionQuery(n, raw_mask, static_cast<uint32_t>(ctx.r3.u32))) {
    REXLOG_INFO("[HOOK] sub_82966438 #{} root={:08X} slot={} action={} flags={:08X} "
                "min={} extra={:08X} raw=0x{:04X} ret={:08X}",
                n, root, slot, action, flags, min_value, extra, raw_mask,
                static_cast<uint32_t>(ctx.r3.u32));
  }
}

extern "C" REX_FUNC(sub_82966700) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t player = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t binding = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t action = static_cast<uint32_t>(ctx.r5.u32);
  uint32_t flags = static_cast<uint32_t>(ctx.r6.u32);
  uint32_t min_value = static_cast<uint32_t>(ctx.r7.u32);
  uint32_t extra = static_cast<uint32_t>(ctx.r8.u32);
  __imp__sub_82966700(ctx, base);
  LogLowLevelQuery("sub_82966700", base, n, player, binding, action, flags, min_value, extra,
                   static_cast<uint32_t>(ctx.r3.u32));
}

// ----------------------------------------------------------------------------
// Video player tick (sub_82931578) - the black-screen-after-language-menu fix
// ----------------------------------------------------------------------------
//
// After the language menu, Skate 3 tries to play an intro video. The recomp
// tick function calls `bctrl mem[obj.vtable + 16]` where:
//   obj          = mem[0x83028448] (= mem[lis(-31997) - 31672])
//   obj.vtable   = mem[obj + 0]   = 0x823112A4 (after the language-menu
//                                                state transition)
//   obj.vtable[16] = mem[0x823112B4] = 0x00000000  ← NULL!
//
// The runtime catches the bctrl-to-NULL and returns, but the video player
// state machine never advances and the screen stays black forever. The
// vtable slot is NULL because the recompiler didn't decode that function
// from the original .rdata vtable for the post-language-menu video class.
//
// Strategy: when we detect the "broken" vtable case (obj alive but
// vtable[16] is NULL), permanently NULL the global obj pointer. This
// makes EVERY future call see "no video player" and skip the entire
// intro path. With luck the menu state machine times out the intro and
// transitions to the main menu screen.
extern "C" REX_FUNC(sub_82931578) {
  // Global at lis(-31997) - 31672 = 0x83030000 - 0x7BB8 = 0x83028448
  constexpr uint32_t kVideoPlayerObjGlobal = 0x83028448;

  uint32_t obj = static_cast<uint32_t>(REX_LOAD_U32(kVideoPlayerObjGlobal));
  uint32_t vtable = 0;
  uint32_t vtable_slot16 = 0;
  if (obj != 0) {
    vtable = static_cast<uint32_t>(REX_LOAD_U32(obj + 0));
    if (vtable != 0) {
      vtable_slot16 = static_cast<uint32_t>(REX_LOAD_U32(vtable + 16));
    }
  }

  // Patch broken vtable in place: when the post-language-menu video
  // player class shows up with vtable[16] == NULL, copy the equivalent
  // method address from the original (boot-time) vtable at 0x829412A4
  // (where slot 16 is sub_82933BB0). Both vtables share the same layout
  // in the original .rdata, just at different bases -- the recompiler
  // wrote a non-zero value at 0x829412B4 but apparently zeroed the
  // matching slot at 0x823112B4. Patching keeps the video player
  // dispatchable instead of nuking it (which corrupts later GPU state).
  if (obj != 0 && vtable != 0 && vtable_slot16 == 0) {
    constexpr uint32_t kKnownGoodMethod = 0x82933BB0;
    REX_STORE_U32(vtable + 16, kKnownGoodMethod);
    vtable_slot16 = kKnownGoodMethod;
    static std::atomic<bool> s_logged_patch{false};
    bool expected = false;
    if (s_logged_patch.compare_exchange_strong(expected, true,
                                               std::memory_order_relaxed)) {
      REXLOG_INFO("[HOOK] sub_82931578 patched vtable[16] in {:08X} -> {:08X}",
                  vtable, kKnownGoodMethod);
    }
  }

  static std::atomic<int> count{0};
  static std::atomic<uint32_t> last_logged_obj{0};
  static std::atomic<uint32_t> last_logged_vt{0};
  static std::atomic<uint32_t> last_logged_slot16{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t event_arg = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t prev_logged_obj = last_logged_obj.load(std::memory_order_relaxed);
  uint32_t prev_logged_vt = last_logged_vt.load(std::memory_order_relaxed);
  uint32_t prev_logged_s16 = last_logged_slot16.load(std::memory_order_relaxed);
  if (n <= 8 || obj != prev_logged_obj || vtable != prev_logged_vt ||
      vtable_slot16 != prev_logged_s16) {
    last_logged_obj.store(obj, std::memory_order_relaxed);
    last_logged_vt.store(vtable, std::memory_order_relaxed);
    last_logged_slot16.store(vtable_slot16, std::memory_order_relaxed);
    REXLOG_INFO("[HOOK] sub_82931578 #{} arg={} obj={:08X} vt={:08X} vt[16]={:08X}",
                n, event_arg, obj, vtable, vtable_slot16);
  }

  __imp__sub_82931578(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82933740 - the per-frame "tick all video player slots" loop. After the
// language menu, this fires every frame and dispatches to vtable entries
// that the recompiler couldn't fill in (slot 8, 40, etc), so each tick
// produces a flurry of NULL bctrl warnings and eventually destabilizes the
// state machine into a host-side segfault.
//
// Once we've seen the broken video player even once, just no-op every
// subsequent call to this function. The video won't play, but the state
// machine should be free to time-out and continue past the intro.
extern "C" REX_FUNC(sub_82933740) {
  static std::atomic<bool> s_disabled{false};
  static std::atomic<int> s_skip_count{0};

  // Same global as sub_82931578 hook -- when it's been zeroed by our kill
  // switch, treat the video tick as broken and skip it entirely.
  uint32_t obj = static_cast<uint32_t>(REX_LOAD_U32(0x83028448));
  if (obj == 0) {
    s_disabled.store(true, std::memory_order_relaxed);
  }

  if (s_disabled.load(std::memory_order_relaxed)) {
    int n = s_skip_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n == 1 || (n % 600) == 0) {
      REXLOG_INFO("[HOOK] sub_82933740 SKIPPED #{} (video tick disabled)", n);
    }
    ctx.r3.s64 = 0;
    return;
  }

  __imp__sub_82933740(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82933810 - the post-allocation initializer for the VideoEngine
// object. It sets up the object's vtable and fields, then returns the
// (initialized) object pointer in r3. The caller (sub_82933560) stores
// this pointer at mem[0x83028448] which the entire intro path uses.
//
// Pass-through: just call the original. Used as an anchor for further
// work if we need to instrument the post-init.
extern "C" REX_FUNC(sub_82933810) {
  __imp__sub_82933810(ctx, base);
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4) {
    uint32_t obj = static_cast<uint32_t>(ctx.r3.u32);
    uint32_t vtable = obj ? static_cast<uint32_t>(REX_LOAD_U32(obj + 0)) : 0;
    REXLOG_INFO("[HOOK] sub_82933810 #{} obj={:08X} vtable={:08X}", n, obj, vtable);
  }
}

// ----------------------------------------------------------------------------
// sub_8293F0E8 - main intro/video processor. The function has an inner
// "while (this->[36] == 0)" loop whose body does multiple vtable bctrls
// to advance state. After the language menu several of those vtable
// slots are NULL, so the body never advances `this->[36]` to non-zero
// and the loop spins forever, generating a tidal wave of NULL-bctrl
// warnings until the game gives up and exits.
//
// Strategy: detect when the loop is going to spin (inner sub-object's
// vtable[28] is NULL) and pre-set this->[36] = 1 to take the early exit.
// On the boot-time invocation the inner vtable is valid, so we leave
// this->[36] alone and let the function run normally.
// Saved this-pointer for the (single, long-lived) call to sub_8293F0E8.
// The function is one of the engine's per-frame loops with a sub_82EB7610
// timer-wait inside, so it normally runs for the entire game session.
// We use the saved pointer from cooperating hooks (sub_82F02368) to flip
// the exit byte when the post-language-menu transition leaves the loop's
// vtable methods NULL.
std::atomic<uint32_t> g_sub_8293F0E8_this{0};

extern "C" REX_FUNC(sub_8293F0E8) {
  uint32_t this_ptr = static_cast<uint32_t>(ctx.r3.u32);
  bool armed = false;
  bool primed = false;
  if (this_ptr) {
    g_sub_8293F0E8_this.store(this_ptr, std::memory_order_relaxed);
    // Cycle 16 fix per Opus 4.7 agent comparison vs Xenia 30s trace:
    // The title's pre-frame state-loop expects a singleton at
    // mem[0x8302845C] to be already primed. In Xenia's natural boot, the
    // asset-pipeline init wrapper sub_826C3508 fires at ~2 seconds and
    // populates this singleton, which triggers rwfilesys to open the
    // .big files. Our build never hits that natural call site -- the
    // singleton stays NULL forever, so r3 reads back 0x04020000 (a
    // sentinel constant from elsewhere in memory), the bctrl chain
    // dereferences garbage, and the entire loop spins until self-exit.
    //
    // Force the prereq to fire on first entry: when mem[0x8302845C]==0,
    // run sub_826C3508 with the boot-flow root from mem[0x83073534]
    // BEFORE entering the recompiled body.
    uint32_t inner = static_cast<uint32_t>(REX_LOAD_U32(0x8302845C));
    if (inner == 0) {
      static std::atomic<bool> s_primed_once{false};
      if (!s_primed_once.exchange(true, std::memory_order_relaxed)) {
        uint32_t boot_root = static_cast<uint32_t>(REX_LOAD_U32(0x83073534));
        if (IsLikelyGuestPointer(boot_root)) {
          REXLOG_INFO("[HOOK] sub_8293F0E8: priming via sub_826C3508 with "
                      "boot_root={:08X} (singleton at 0x8302845C was NULL)",
                      boot_root);
          PPCContext saved = ctx;
          ctx.r3.u64 = boot_root;
          __imp__sub_826C3508(ctx, base);
          ctx = saved;
          uint32_t after_inner =
              static_cast<uint32_t>(REX_LOAD_U32(0x8302845C));
          REXLOG_INFO("[HOOK] sub_8293F0E8: post-prime mem[0x8302845C]={:08X}",
                      after_inner);
          inner = after_inner;
          primed = true;
        } else {
          REXLOG_WARN("[HOOK] sub_8293F0E8: cannot prime; "
                      "mem[0x83073534]={:08X} not a guest pointer",
                      boot_root);
        }
      }
    }
    // Inner-inner dispatch chain (matches the recompiled body):
    //   inner = mem[0x8302845C]
    //   target = mem[inner + 16]
    //   bctrl mem[mem[target] + 28]   (lr=8293F30C)
    //   bctrl mem[mem[target] + 68]   (lr=8293F320)
    // If the inner sub-object's vtable is already NULL at entry the loop
    // would spin forever the moment we entered the body, so pre-arm the
    // exit byte before calling __imp__.
    if (inner != 0) {
      uint32_t target = static_cast<uint32_t>(REX_LOAD_U32(inner + 16));
      if (target != 0) {
        uint32_t vt = static_cast<uint32_t>(REX_LOAD_U32(target + 0));
        if (vt != 0) {
          uint32_t slot28 = static_cast<uint32_t>(REX_LOAD_U32(vt + 28));
          uint32_t slot68 = static_cast<uint32_t>(REX_LOAD_U32(vt + 68));
          if (slot28 == 0 || slot68 == 0) {
            REX_STORE_U8(this_ptr + 36, 1);
            armed = true;
          }
        }
      }
    }
  }
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || armed || primed) {
    REXLOG_INFO("[HOOK] sub_8293F0E8 enter #{} this={:08X} armed={} primed={}",
                n, this_ptr, armed ? 1 : 0, primed ? 1 : 0);
  }
  __imp__sub_8293F0E8(ctx, base);
  if (n <= 4 || armed || primed) {
    REXLOG_INFO("[HOOK] sub_8293F0E8 exit  #{} this={:08X} ret={:08X}", n,
                this_ptr, static_cast<uint32_t>(ctx.r3.u32));
  }
}

// ----------------------------------------------------------------------------
// sub_828E3F30 - input/event poll loop with NULL vtable[96]/[100]. Force
// the loop's exit byte (this->[9] = 1) on entry so the loop never spins.
extern "C" REX_FUNC(sub_828E3F30) {
  uint32_t this_ptr = static_cast<uint32_t>(ctx.r3.u32);
  if (this_ptr) {
    REX_STORE_U8(this_ptr + 9, 1);
  }
  __imp__sub_828E3F30(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_828F47D0 - linked-list method that dispatches a vtable call at
// lr=0x828F4A30 through:  this->[+20].[+4].vtable.[+16]
//
// Field layout of `this`: it's a doubly-linked list container with an
// external pool allocator (see also sub_828F4220 / sub_828F44C0 nearby).
// `this->[+20]` is the head iterator; `[+4]` of that is the first node,
// whose vtable slot at +16 is the "process element" callback.
//
// On the streaming thread (sk8_memcard_helper / RwAudioCore Dac) post the
// language-menu transition this vtable goes NULL and the bctrl traps. The
// call is an early-return predicate: the recompiled body checks
// `if (r3 < 0) return 2;` after the bctrl, so we can synthesize the same
// early return when the vtable is broken.
extern "C" REX_FUNC(sub_828F47D0) {
  uint32_t obj = static_cast<uint32_t>(ctx.r3.u32);
  if (IsLikelyGuestPointer(obj)) {
    uint32_t container = LoadU32IfPointer(base, obj, 20);
    uint32_t element = LoadU32IfPointer(base, container, 4);
    uint32_t vt = LoadU32IfPointer(base, element);
    uint32_t method4 =
        IsLikelyGuestPointer(vt) ? static_cast<uint32_t>(REX_LOAD_U32(vt + 16)) : 0;
    if (method4 == 0) {
      // Skip: take the same early-exit path the function would on r3<0.
      static std::atomic<int> skipped_count{0};
      int n = skipped_count.fetch_add(1, std::memory_order_relaxed) + 1;
      if (n <= 8 || (n % 256) == 0) {
        REXLOG_INFO("[HOOK] sub_828F47D0 #{} skip broken vtable obj={:08X} "
                    "container={:08X} element={:08X} vt={:08X}",
                    n, obj, container, element, vt);
      }
      ctx.r3.s64 = 2;
      return;
    }
  }
  __imp__sub_828F47D0(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_828F6B00 - companion list method, dispatches a callback function
// pointer (NOT a vtable) at lr=0x828F6B2C through: this->[+16].[+12].
//
// Same allocator pool as sub_828F47D0. After the streaming thread tears
// down its allocator, [+16] becomes NULL so the [+12] field-load reads
// the NULL guard page and we bctrl through 0. The recompiled body
// already checks `if (r3 == 0) skip post-processing`, so a no-op when
// the callback is missing matches the "callback returned 0" behavior
// the function expects.
extern "C" REX_FUNC(sub_828F6B00) {
  uint32_t obj = static_cast<uint32_t>(ctx.r3.u32);
  if (IsLikelyGuestPointer(obj)) {
    uint32_t inner = LoadU32IfPointer(base, obj, 16);
    if (!IsLikelyGuestPointer(inner)) {
      static std::atomic<int> skipped_count{0};
      int n = skipped_count.fetch_add(1, std::memory_order_relaxed) + 1;
      if (n <= 8 || (n % 256) == 0) {
        REXLOG_INFO("[HOOK] sub_828F6B00 #{} skip null inner obj={:08X} inner={:08X}",
                    n, obj, inner);
      }
      return;
    }
    uint32_t callback = static_cast<uint32_t>(REX_LOAD_U32(inner + 12));
    if (callback == 0) {
      static std::atomic<int> skipped_count{0};
      int n = skipped_count.fetch_add(1, std::memory_order_relaxed) + 1;
      if (n <= 8 || (n % 256) == 0) {
        REXLOG_INFO("[HOOK] sub_828F6B00 #{} skip null callback obj={:08X} inner={:08X}",
                    n, obj, inner);
      }
      return;
    }
  }
  __imp__sub_828F6B00(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82F02368 - per-frame post-language-menu dispatcher that calls vtable
// methods on multiple sub-objects. After A press, several of those vtables
// have NULL slots (slot 44, 40, 20). The recompiled body keeps spinning
// trying to call them, generating NULL-bctrl spam every frame and never
// makes forward progress, leaving the game wedged until it self-exits.
//
// No-op the entire function so the calling frame loop just sees "nothing
// happened this tick" and moves on. The visible side effect is that
// whatever post-menu animation this function drove won't update, but the
// game's overall state machine should be free to advance.
// Forward decl -- definition lives at end of file (cycle 16 asset broadcast).
namespace { void TryFireAssetBroadcast(uint8_t* base, PPCContext& ctx); }

extern "C" REX_FUNC(sub_82F02368) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  uint32_t this_ptr = static_cast<uint32_t>(ctx.r3.u32);

  // ========================================================================
  // Cycle 22 (vtable-repair): probe + force-correct the engine vtable slot.
  //
  // Investigation summary (sources cited at end):
  //   * recomp.104.cpp:63787-63799 -- the bctrl gate reads `vt = mem[r31+0]`
  //     then `bctrl mem[vt+8]`. If r3 of the call returns 0 the entire
  //     `loc_82F02468` build chain (sub_82F02178) is skipped and +184
  //     never gets a draw count. Run skate3_327.log line 2421+ confirms
  //     +184=0 forever despite all other gates open.
  //   * recomp.104.cpp:62675 (sub_82F01CC8) -- the engine constructor
  //     stores 0x82048CA8 into engine+0. Bytes at 0x82048CA8 (rdata) are
  //     data (0x00000001, 0x00020000, 0x00010002, ...) NOT function
  //     pointers, so `mem[0x82048CA8 + 8] = 0x00010002` is interpreted as
  //     a bogus code address. REX_CALL_INDIRECT_FUNC NULL-guards bad
  //     targets and returns 0 -> the gate stays closed.
  //   * The real vtable for the class that exposes sub_82F02368 lives at
  //     0x82058898 in rdata. Slot layout (verified by scanning rdata for
  //     0x82F02368 BE word -- it lives at 0x820588A8 which is +0x10):
  //         +0x00 0x82F0B730   (vtable[0]: ~dtor candidate)
  //         +0x04 0x8251F238   (vtable[1])
  //         +0x08 0x82F0B740   (vtable[2] -- THE GATE METHOD)
  //         +0x0C 0x82F009B8   (vtable[3])
  //         +0x10 0x82F02368   (vtable[4]: the dispatcher itself)
  //         +0x14 0x82F00CD8   ... etc.
  //
  // Conclusion: the engine instance at 0x4401C600 was constructed by a
  // base class (sub_82F01CC8) that left it with a metadata pointer at +0
  // instead of the derived vtable. The derived class' constructor (which
  // would normally overwrite +0 with 0x82058898) never ran in our boot
  // path -- this class layout matches the missing vtable[8]/[40]/[44]
  // NULL bctrl pattern reported in latest_run_status.md.
  //
  // Fix: when we detect engine+0 == 0x82048CA8 (the metadata sentinel),
  // overwrite it with 0x82058898 (the real vtable). This puts
  // sub_82F0B740 in the bctrl slot. sub_82F0B740 is a non-trivial method
  // (recomp.105.cpp:17084) that calls into the buffer manager; if it
  // returns 0 we still gain entry to sub_82F02178 because the OTHER
  // open gate (mem[r31+184] != 0 OR mem[r31+176] != 0, latter is 2 in
  // our run) routes through loc_82F02468 once vt8 returns non-zero.
  //
  // Logging requirements (per task #6): pre-call read of vt and vt8.
  if (this_ptr && (n == 1 || (n % 600) == 0)) {
    uint32_t vt   = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 0));
    uint32_t vt8  = 0;
    if (vt != 0) {
      vt8 = static_cast<uint32_t>(REX_LOAD_U32(vt + 8));
    }
    REXLOG_INFO("[HOOK] sub_82F02368 #{} VT_PROBE this={:08X} vt=mem[+0]={:08X} "
                "vt8=mem[vt+8]={:08X}",
                n, this_ptr, vt, vt8);
  }

  // CYCLE 22 ENGINE VTABLE REPAIR -- DISABLED 2026-04-29.
  // The repair (engine+0 0x82048CA8 -> 0x82048898) BROKE INTRO VP6 PLAYBACK!
  // Our metadata-sentinel hypothesis was wrong; 0x82048CA8 might NOT be data
  // for this specific instance — it could be a different VALID class layout.
  // Revert by leaving repair disabled. The downstream gates we're chasing
  // need a different approach (see cycle 19 memory note).
  static std::atomic<bool> s_vtable_repaired{false};
  (void)s_vtable_repaired;  // suppress unused warning
  // ========================================================================

  // VP6 decoder probe -- one-shot at first per-frame tick to confirm
  // libavcodec linkage and the VP6 codec is registered.
  if (n == 1) {
    Skate3VP6_ProbeAvailable();
  }

  // Cycle 16: 3-agent convergence found sub_8293F018 (asset-load completion
  // broadcast) NEVER fires. First attempt at probe-based dispatch fired on
  // wrong object (fe_root+48=822FFD38 was code-section data, not asset-mgr),
  // causing infinite vtable spin. DISABLED until we find the real asset-mgr
  // address. The investigation continues via TryFireAssetBroadcast logging
  // only -- it now just logs candidates without dispatching.
  TryFireAssetBroadcast(base, ctx);

  // VP6 playback driver: in the original game flow the order is
  //   1. Press-A / language menu (state=0 in our build, with logo overlay)
  //   2. After A pressed: Skate 3 logo (still state=0 for us, ~30s)
  //   3. Intro video (state=2 -- this is where we play VP6)
  //   4. Main menu (state advances past 2)
  //
  // Trigger VP6 playback only when the FE state global hits 2 (the
  // post-logo intro state). That way the logo finishes naturally before our
  // overlay kicks in. Once playback is going, advance one frame every other
  // tick (~30 fps at the assumed 60 Hz tick rate). On EOF, stop driving
  // the overlay; sub_82F02368 will see Skate3VP6_PlaybackActive() return
  // false and stop substituting in IssueSwap.
  constexpr uint32_t kFEStateGlobal2 = 0x8300B2B0;
  uint32_t fe_state_now = static_cast<uint32_t>(REX_LOAD_U32(kFEStateGlobal2));
  static std::atomic<bool> s_vp6_started{false};
  static std::atomic<int> s_vp6_advance_skip{0};
  if (fe_state_now == 2 &&
      !s_vp6_started.exchange(true, std::memory_order_relaxed)) {
    bool ok = Skate3VP6_StartPlayback(
        "C:\\Users\\admin\\Downloads\\xenia-analysis\\skate3-extracted\\data\\movies\\"
        "EA_Blackbox_english_ntsc.vp6");
    REXLOG_INFO("[HOOK] sub_82F02368 #{} state={} -> VP6 playback start = {}", n,
                fe_state_now, ok ? "OK" : "FAILED");
  }
  // Advance VP6 frames at the EA RWMovie native rate (29.97 fps NTSC) using
  // a wall-clock pacer. sub_82F02368 fires far faster than 60 Hz on Skate 3
  // (~250 Hz observed), so per-tick advance produces 8x playback speed.
  // Track elapsed time and only advance when >=33.367 ms have passed since
  // the previous frame (== 1 / 29.97 fps).
  if (s_vp6_started.load(std::memory_order_relaxed) && Skate3VP6_PlaybackActive()) {
    using clock = std::chrono::steady_clock;
    static auto s_vp6_last_frame_time = clock::now();
    auto now = clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now - s_vp6_last_frame_time)
        .count();
    if (elapsed_us >= 33367) {  // 1/29.97s -- NTSC native rate
      Skate3VP6_AdvanceFrame();
      s_vp6_last_frame_time = now;
    }
  }
  // Cycle 14 RE agent finding: pinning state=5 SHORT-CIRCUITS the FE state
  // machine. Title wants state 4 -> loading-screen substate -> state 5.
  // Pinning skips the loading-screen path so menu items never populate.
  //
  // Real fix: let state advance naturally, but force the "loading done"
  // sub-state at mem[mem[0x83073634]+4] = 7 so sub_825D3438 (FE-mgr
  // vtable[4]) returns ready. That predicate is what gates the menu UI
  // build, NOT the FE state global directly.
  if (s_vp6_started.load(std::memory_order_relaxed) && !Skate3VP6_PlaybackActive()) {
    constexpr uint32_t kLoadStateSingletonGlobal = 0x83073634u;
    uint32_t singleton =
        static_cast<uint32_t>(REX_LOAD_U32(kLoadStateSingletonGlobal));
    static std::atomic<int> s_log_count{0};
    int slc = s_log_count.fetch_add(1, std::memory_order_relaxed);
    if (slc < 3 || (slc % 600) == 0) {
      REXLOG_INFO("[HOOK] load-state probe: mem[0x83073634]={:08X}", singleton);
    }
    if (IsLikelyGuestPointer(singleton)) {
      uint32_t cur_substate =
          static_cast<uint32_t>(REX_LOAD_U32(singleton + 4));
      if (cur_substate != 7) {
        REX_STORE_U32(singleton + 4, 7);
        static std::atomic<bool> s_loadstate_logged{false};
        if (!s_loadstate_logged.exchange(true, std::memory_order_relaxed)) {
          REXLOG_INFO(
              "[HOOK] post-VP6: signal loading-complete via "
              "mem[*0x83073634 + 4] = 7 (was {}, singleton={:08X})",
              cur_substate, singleton);
        }
      }
    } else {
      // Try alternative interpretation: the address itself IS the state
      // struct (no indirection). Write 7 at 0x83073634+4 directly.
      static std::atomic<bool> s_direct_logged{false};
      if (!s_direct_logged.exchange(true, std::memory_order_relaxed)) {
        uint32_t before =
            static_cast<uint32_t>(REX_LOAD_U32(kLoadStateSingletonGlobal + 4));
        REX_STORE_U32(kLoadStateSingletonGlobal + 4, 7);
        REXLOG_INFO(
            "[HOOK] post-VP6: singleton ptr invalid; writing 7 directly to "
            "0x83073638 (was {})",
            before);
      }
    }
  }

  // VP6 finished -- drive the title's FE main-menu activator.
  // Per cycle 14 RE agent: the title's natural exit from state=2 (intro)
  // requires the MoviePlayer2 completion observer to post kind=3 message
  // into FE manager queue at mem[0x830734D8] -- but our recompile's
  // observer chain is broken (mem[player+520]=NULL etc.).
  //
  // The activator is `sub_825C1410` (skate3_recomp.20.cpp:12033). It is
  // the ONLY call site in the entire build that writes value=4 to the FE
  // state global. Once state=4, the per-tick driver advances to 5 (main
  // menu running), and FE manager vtable[10] starts populating the menu UI
  // and posting screen-build messages.
  //
  // Boot-flow root: mem[mem[0x83073494] + 13620] (singleton + boot-flow
  // offset). Pass that as r3 ("this") to sub_825C1410.
  static std::atomic<bool> s_vp6_eof_handled{false};
  if (s_vp6_started.load(std::memory_order_relaxed) &&
      !Skate3VP6_PlaybackActive() &&
      !s_vp6_eof_handled.exchange(true, std::memory_order_relaxed)) {
    uint32_t fe = static_cast<uint32_t>(REX_LOAD_U32(kFEStateGlobal2));
    REXLOG_INFO("[HOOK] VP6 playback EOF reached. FE state={} -- activating "
                "main menu via sub_825C1410.",
                fe);
    // Per direct decompile of skate3_recomp.21.cpp:5722-5728 (the natural
    // call site to sub_825C1410), the boot-flow root is loaded via:
    //   lis r31, -31993              ; r31 = 0x83070000
    //   lwz r3,  13620(r31)          ; r3 = mem[0x83073534]
    //   bl  0x825c1410
    // So `mem[0x83073534]` is the boot-flow root pointer (a single
    // dereference, not a singleton + offset).
    constexpr uint32_t kBootFlowRootGlobal = 0x83073534u;
    uint32_t boot_root =
        static_cast<uint32_t>(REX_LOAD_U32(kBootFlowRootGlobal));
    REXLOG_INFO("[HOOK] VP6 EOF: mem[0x83073534]=boot_root={:08X}", boot_root);
    if (IsLikelyGuestPointer(boot_root)) {
      // Step 1: Call wrapper sub_826C3508 first -- per agent it "primes
      // prerequisite subsystems" before calling sub_825C1410.
      PPCContext saved = ctx;
      ctx.r3.u64 = boot_root;
      __imp__sub_826C3508(ctx, base);
      uint32_t fe_after_wrapper =
          static_cast<uint32_t>(REX_LOAD_U32(kFEStateGlobal2));
      REXLOG_INFO("[HOOK] sub_826C3508 (wrapper) returned -- FE state -> {}",
                  fe_after_wrapper);
      // Step 2: Call sub_825C1410 directly to ensure menu activator runs.
      ctx = saved;
      ctx.r3.u64 = boot_root;
      __imp__sub_825C1410(ctx, base);
      ctx = saved;
      uint32_t fe_after =
          static_cast<uint32_t>(REX_LOAD_U32(kFEStateGlobal2));
      REXLOG_INFO("[HOOK] sub_825C1410 returned -- FE state {} -> {}", fe,
                  fe_after);
    } else {
      REXLOG_WARN("[HOOK] VP6 EOF: boot_root at 0x83073534 invalid, will "
                  "retry next frame");
      // Reset the EOF guard so the next frame can try again -- maybe the
      // singleton gets populated late.
      s_vp6_eof_handled.store(false, std::memory_order_relaxed);
    }
  }

  // Cycle 23: FIRE FE MENU UI BUILDER (sub_825D3700) per agent
  // ac3ac1a38f0818891 finding. The FE-mgr vtable[4] = sub_825D3700 is the
  // menu-item iterator. In our build, the vtable installer chain (9
  // constructors) is missing, so vtable dispatch never reaches it.
  // Fire it manually once state==5 has been stable for >= 60 frames
  // (post-VP6 EOF, post-state-machine settle).
  if (fe_state_now == 5 && Skate3VP6_PlaybackActive() == false) {
    static std::atomic<int> s_state5_frames{0};
    static std::atomic<bool> s_vt_log_done{false};
    int sf = s_state5_frames.fetch_add(1, std::memory_order_relaxed) + 1;
    if (sf >= 60) {
      // CYCLE 24 CORRECTION (agent a4b80fc165f5c48f8): the REAL FE-mgr
      // is at 0x8307353C with vtable 0x822FF2C8. Previously misidentified
      // as 0x83073544/0x822FF2C0 which is a 264-byte sibling singleton.
      // Verified by direct rdata dump: vtable[0]=0x823F7550..vtable[15]
      // are all valid 0x82xxxxxx code pointers; the 0x823071A0 from cycle
      // 23 was actually string data ("erPhotoSaveFlow"...) not a vtable.
      constexpr uint32_t kFEMgrSingleton = 0x8307353Cu;  // was 0x83073544u
      constexpr uint32_t kFEMgrVTable    = 0x822FF2C8u;  // was 0x822FF2C0u
      uint32_t fe_mgr = static_cast<uint32_t>(REX_LOAD_U32(kFEMgrSingleton));
      if (IsLikelyGuestPointer(fe_mgr)) {
        uint32_t cur_vt = static_cast<uint32_t>(REX_LOAD_U32(fe_mgr + 0));
        if (!s_vt_log_done.exchange(true, std::memory_order_relaxed)) {
          REXLOG_INFO("[HOOK] sub_82F02368 #{} FE-mgr probe: mem[0x83073544]={:08X} "
                      "current_vt={:08X} expected_vt={:08X}",
                      n, fe_mgr, cur_vt, kFEMgrVTable);
        }
        // Only fire if vtable looks reasonable (don't fire on garbage).
        if (cur_vt >= 0x82000000 && cur_vt < 0x83000000) {
          static std::atomic<int> s_menu_fire{0};
          int mf = s_menu_fire.fetch_add(1, std::memory_order_relaxed) + 1;
          if (mf <= 5 || (mf % 600) == 0) {
            PPCContext saved = ctx;
            ctx.r3.u32 = fe_mgr;
            ctx.r5.u32 = 0;
            ctx.r6.u32 = 0;
            __imp__sub_825D3700(ctx, base);
            uint32_t hr = static_cast<uint32_t>(ctx.r3.u32);
            ctx = saved;
            uint32_t list_lo = static_cast<uint32_t>(REX_LOAD_U32(fe_mgr + 0x20));
            uint32_t list_hi = static_cast<uint32_t>(REX_LOAD_U32(fe_mgr + 0x24));
            REXLOG_INFO("[HOOK] sub_82F02368 #{} fired sub_825D3700 (FE menu) "
                        "fe_mgr={:08X} vt={:08X} list[+0x20]={:08X} +0x24={:08X} "
                        "-> r3={:08X} (fire #{})",
                        n, fe_mgr, cur_vt, list_lo, list_hi, hr, mf);
          }
        }
      }
    }
  }

  // Cycle 24 (EXPERIMENTAL PROBE 2026-04-28): force-populate the FE-mgr
  // screen-list std::vector with 4 FAKE entries (20-byte stride) and observe
  // whether sub_825D3700's iteration loop ENTERS at all. Per recomp.20.cpp
  // line 65607-65623, the "exit immediately" gate is:
  //     count = (mem[r29+36] - mem[r29+32]) / 20;  if (count == 0) goto exit;
  // and a SECOND vector at +16/+20 (also stride 20) is iterated at line 65780+.
  // Currently fe_mgr +32/+36 are 0x10/0 -> count=0 -> immediate exit.
  //
  // Strategy: write 80 bytes of fake ScreenEntry payload into otherwise-unused
  // guest BSS at 0x83073700 (verified: no references anywhere in generated
  // code, lives next to the fe_mgr singleton at 0x83073544 in the same BSS
  // page). Then patch fe_mgr +32 = begin, +36 = begin+80, +40 = begin+80.
  //
  // EXPECTED OUTCOME: sub_825D3700 will pass the "count==0" gate and then
  // most likely CRASH on the first vtable dispatch (line 65687: load r10 from
  // mem[item_vtable+40] then bctrl). We're capturing WHERE it crashes, not
  // hoping for a render. If the crash dump shows "AV at offset N of vtable",
  // that tells us the next byte that needs to be valid.
  //
  // GUARDS: fires ONCE total (s_fake_list_armed), only after >=120 frames
  // at state==5, only after VT_PROBE has logged, with extensive pre-/post-
  // logging so we can correlate any crash to this site. To DISABLE on crash
  // hard, set kFakeListEnabled=false and rebuild.
  static constexpr bool kFakeListEnabled = true;
  if (kFakeListEnabled && fe_state_now == 5 && Skate3VP6_PlaybackActive() == false) {
    static std::atomic<int> s_fake_state5_frames{0};
    static std::atomic<bool> s_fake_list_armed{false};
    int ff = s_fake_state5_frames.fetch_add(1, std::memory_order_relaxed) + 1;
    if (ff >= 120 && !s_fake_list_armed.exchange(true, std::memory_order_relaxed)) {
      constexpr uint32_t kFEMgrSingleton = 0x83073544u;
      // Static scratch arena in guest BSS. 0x83073700 is 444 bytes past
      // fe_mgr singleton slot, no codegen reference, presumed safe BSS page.
      constexpr uint32_t kFakeListBase   = 0x83073700u;
      constexpr uint32_t kFakeItemCount  = 4u;
      constexpr uint32_t kFakeItemStride = 20u;
      constexpr uint32_t kFakeListBytes  = kFakeItemCount * kFakeItemStride;  // 80
      constexpr uint32_t kFakeVTablePad  = kFakeListBase + kFakeListBytes;    // 0x83073750
      // Reserve a tiny "fake vtable" zone just past the items. We fill 64
      // entries with self-pointers so the bctrl indirect call lands on a
      // 4-byte word that's also a guest pointer (which when treated as code
      // will trap, but at a controlled, logged address rather than NULL).
      constexpr uint32_t kFakeVTableEnd  = kFakeVTablePad + 256u;            // 0x83073850

      uint32_t fe_mgr = static_cast<uint32_t>(REX_LOAD_U32(kFEMgrSingleton));
      if (IsLikelyGuestPointer(fe_mgr)) {
        // Snapshot the original list pointers before we clobber them, so we
        // can log restore values if needed.
        uint32_t orig_begin = static_cast<uint32_t>(REX_LOAD_U32(fe_mgr + 32));
        uint32_t orig_end   = static_cast<uint32_t>(REX_LOAD_U32(fe_mgr + 36));
        uint32_t orig_cap   = static_cast<uint32_t>(REX_LOAD_U32(fe_mgr + 40));

        // Step A: zero out the scratch buffer (80 bytes for items, 256 for
        // fake vtable padding). Writing zeros is safe -- it's just BSS.
        for (uint32_t off = 0; off < kFakeListBytes + 256u; off += 4u) {
          REX_STORE_U32(kFakeListBase + off, 0u);
        }

        // Step B: populate the 4 fake ScreenEntry items. Per line 65687
        // analysis, sub_825D3700 reads:
        //    item.vtable = mem[item+0]      (the screen-object vtable ptr)
        //    method = mem[item.vtable + 40] (vtable[10] in 4-byte index)
        //    bctrl method(...)
        // We point each item.vtable at our own "fake vtable" pad, and fill
        // that pad's slot 40 with a self-pointer too -- so the eventual
        // bctrl jumps to kFakeVTablePad which (treated as code) will trap
        // immediately on illegal opcode. That trap address WILL appear in
        // the crash log and tells us the chain reached this point.
        for (uint32_t i = 0; i < kFakeItemCount; ++i) {
          uint32_t item_addr = kFakeListBase + i * kFakeItemStride;
          REX_STORE_U32(item_addr +  0, kFakeVTablePad);  // vtable ptr
          REX_STORE_U32(item_addr +  4, 0u);              // unknown field
          REX_STORE_U32(item_addr +  8, kFakeVTablePad);  // some inner ptr
          REX_STORE_U32(item_addr + 12, 0u);
          REX_STORE_U32(item_addr + 16, 0u);
        }
        // Fill the fake vtable: every slot points to kFakeVTablePad itself
        // so any indexed dispatch lands on a known address.
        for (uint32_t off = 0; off < 256u; off += 4u) {
          REX_STORE_U32(kFakeVTablePad + off, kFakeVTablePad);
        }

        // Step C: patch fe_mgr's std::vector<ScreenEntry> at +32/+36/+40.
        REX_STORE_U32(fe_mgr + 32, kFakeListBase);                  // begin
        REX_STORE_U32(fe_mgr + 36, kFakeListBase + kFakeListBytes); // end
        REX_STORE_U32(fe_mgr + 40, kFakeListBase + kFakeListBytes); // capacity

        // Step D: also patch the second vector at +16/+20/+24 (line 65780+).
        // Use the SAME backing storage -- harmless because both iterators
        // dispatch through item.vtable[40], same crash site.
        REX_STORE_U32(fe_mgr + 16, kFakeListBase);
        REX_STORE_U32(fe_mgr + 20, kFakeListBase + kFakeListBytes);
        REX_STORE_U32(fe_mgr + 24, kFakeListBase + kFakeListBytes);

        REXLOG_WARN("[HOOK] sub_82F02368 #{} CYCLE24 EXPERIMENTAL: forced "
                    "FE-mgr fake screen list. fe_mgr={:08X} buf={:08X}..{:08X} "
                    "fake_vt={:08X} (was begin={:08X} end={:08X} cap={:08X}). "
                    "Next sub_825D3700 call WILL likely crash at fake_vt -- "
                    "look for trap at code-fetch={:08X} or bctrl target.",
                    n, fe_mgr, kFakeListBase, kFakeListBase + kFakeListBytes,
                    kFakeVTablePad, orig_begin, orig_end, orig_cap,
                    kFakeVTablePad);
        REXLOG_WARN("[HOOK] sub_82F02368 #{} CYCLE24 also patched +16/+20/+24 "
                    "(second vec). To DISABLE on crash, set "
                    "kFakeListEnabled=false in src/hooks.cpp and rebuild.",
                    n);
      } else {
        REXLOG_WARN("[HOOK] sub_82F02368 #{} CYCLE24 EXPERIMENTAL: fe_mgr "
                    "singleton not yet valid ({:08X}); skipping.",
                    n, fe_mgr);
        // Not really armed if we couldn't fire -- allow retry next frame.
        s_fake_list_armed.store(false, std::memory_order_relaxed);
      }
      (void)kFakeVTableEnd;
    }
  }

  // Force-NULL the inner object slots that drive the failing bctrl loops.
  // Whatever they would dispatch to is broken (vtable[20] / vtable[40] /
  // vtable[44] all NULL after the language-menu transition), so making
  // the recompiled body see "no inner object" is strictly better than
  // letting it call into a NULL pointer. We only restore mem[this+8]
  // afterwards because some legitimate invocations expect that slot to
  // remain stable across calls -- mem[this+264] is what drives the
  // failing vtable[20] loop and we keep it permanently NULL once seen.
  uint32_t saved_8 = 0;
  uint32_t saved_264 = 0;
  static std::atomic<bool> s_kill_264{false};
  if (this_ptr) {
    saved_8 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 8));
    saved_264 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 264));

    // (Disabled 2026-04-28 cycle 4) Killing mem[this+264] was originally
    // added to short-circuit a NULL-bctrl loop on vtable[20] of that
    // inner object. Per Ghidra render-gate analysis, mem[this+264]
    // (along with mem[this+252]) is what populates the per-frame draw
    // list at +184/+176; killing it leaves both lists empty forever and
    // prevents the title from issuing any draw calls. So we now just
    // observe and log without actually nulling the pointer.
    if (saved_264 != 0 && !s_kill_264.exchange(true, std::memory_order_relaxed)) {
      REXLOG_INFO("[HOOK] sub_82F02368 observing (NOT killing) "
                  "mem[this+264]={:08X}", saved_264);
    }
  }

  // Once we're past the language-menu transition (s_kill_264 has fired),
  // also nuke the MoviePlayer2 singleton at mem[0x8302853C]. The decode
  // thread for that subsystem is permanently parked by the runtime
  // (xthread.cpp ShouldSkipThread short-circuit), so the title's intro
  // state machine will never get a "video done" signal and stays
  // wedged at the post-logo black screen. By clearing the singleton we
  // force every polling site that does `mem[0x8302853C]; if (obj) ...`
  // to take the "no video player" branch and let the menu state machine
  // advance to the main menu.
  // Signal "intro video done" by zeroing the state word that sub_82932570
  // polls. That predicate (skate3_recomp.50.cpp:65733-65750) does
  //   r11 = mem[r3 + 108]
  //   if (r11 == 0) return 0
  //   return  mem[r11 + 4] (u16)
  // and the surrounding loop (sub_82937BE8 in skate3_recomp.51.cpp:5943)
  // spins while the return is non-zero. So mem[(*0x8302844C + 108) + 4]
  // is the "intro running" word -- zero it and the loop exits.
  // Belt-and-braces: also write the other observed state-byte setters
  // (sub_82934010 / sub_82934068 / sub_82934078 / sub_82934080 /
  // sub_82934088) so any sister polling-site that picks a different
  // offset also sees "done".
  // PRIMARY signal: clear the Play List + Failed Requests List heads on the
  // MoviePlayer2 instance. Both lists are read by the title's per-frame
  // intro state machine via:
  //   sub_82E1AFD0 (vtable[5] of mp): return mem[r3+40]   <- Play List head
  //   sub_829ED120 (vtable[9] of mp): return mem[r3+44]   <- Failed List head
  // The intro waits in a loop while either list is non-NULL, then advances
  // to the main menu. Without a working VP6 decoder the lists never empty
  // by themselves, so we empty them ourselves every frame.
  // (.text strings "MoviePlayer2 Play List" / "MoviePlayer2 Failed Requests
  // List" at .rdata 0x82137480 / 0x82137498 named these for us.)
  // Intro-done state writes DISABLED (2026-04-28 fourth pass): empirically
  // writing to mp+40/+44 at all -- even gated by 600 frames -- prevents
  // the title's state machine from EVER queueing an intro video, which
  // means MoviePlayer2 never spawns, no XMPSetPlaybackController fires,
  // and we don't even reach the same wedge point as before. The title is
  // structurally weirder than "submit list, poll list, advance when empty"
  // -- something else uses mp+40/+44 as input rather than just output.
  // Leaving the slot writes off; the watchdog in sub_82EB7610 still keeps
  // sub_8293F0E8 from spinning forever.

  // FE state-machine override (2026-04-28 cycle 4, post-precise-RE):
  // sub_825D3438 (vtable[4] of TheFrontEndStateManager) compares against
  // an ABSOLUTE GLOBAL at 0x8300B2B0, NOT a member of `this`. Specifically:
  //   lis r11, -31999          ; r11 = 0x83010000
  //   addi r10, r11, -19812    ; r10 = 0x8300B29C
  //   lwz r9, 20(r10)          ; r9 = mem[0x8300B2B0]
  //   cmpwi cr6, r9, 5         ; equals 5 == main menu seen
  // So writing 5 to mem[0x8300B2B0] should satisfy the predicate
  // regardless of which `this` is on the stack.
  // Cycle 10 fix: previous hook wrote 5 (intro/movie state -- WHERE WE ARE
  // STUCK!) thinking it was "main menu seen". Per Opus RE agent analysis of
  // sub_825C0BB0 (FE per-tick driver in skate3_recomp.20.cpp:10764-11058),
  // state 5 is "logo/movie playback" with NO automatic exit because the
  // state-5 handler waits for MoviePlayer2 to finish playing. Without VP6
  // decoder, MP2 never finishes, state stays 5 forever, scene root never
  // populates, all draws go to non-existent grafo -> only the clear-color
  // (black) reaches the frontbuffer.
  //
  // Setting state=0 forces the post-intro path. State 0 is the value other
  // states transition TO (3->0, 6->0). Combined with sub_82A68EE0 hook
  // forcing MoviePlayer2 state=3, this should let the title advance to
  // main menu.
  if (s_kill_264.load(std::memory_order_relaxed)) {
    static std::atomic<int> s_fe_call{0};
    int fc = s_fe_call.fetch_add(1, std::memory_order_relaxed);
    constexpr int kFramesBeforeForce = 600;  // ~10s
    constexpr uint32_t kFEStateGlobal = 0x8300B2B0;
    uint32_t cur = static_cast<uint32_t>(REX_LOAD_U32(kFEStateGlobal));
    if (fc <= 4 || (fc % 600) == 0) {
      REXLOG_INFO("[HOOK] sub_82F02368 #{} FE-state mem[0x8300B2B0]={}",
                  fc, cur);
    }
    if (fc >= kFramesBeforeForce) {
      // Cycle 14: REMOVED state==5 -> 0 override. With our VP6 playback
      // (cycle 13) + main-menu activator (sub_825C1410, cycle 14), state=5
      // is the LEGITIMATE "main menu running" state, not the stuck intro
      // wait. Forcing 5 -> 0 was undoing the natural progression.
      if (false && cur == 5) {
        REX_STORE_U32(kFEStateGlobal, 0);
      } else if (cur == 2) {
        // State 2 has no exit in sub_825C0BB0's dispatcher -- title would
        // sit forever without external event. After ~30s at state 2,
        // force-cycle to 6 (which dispatcher transitions to 0 next tick),
        // then to 1 (which transitions to 2 again via vtable[12]). The
        // hope is that the cycle re-runs init paths that populate the
        // scene-graph root, eventually leaving state 2 with a real menu
        // built.
        static std::atomic<int> s_state2_count{0};
        int s2 = s_state2_count.fetch_add(1, std::memory_order_relaxed) + 1;
        // Wait ~60s at state 2 before forcing the cycle, so the natural
        // logo display gets time. 60s = 3600 frames at 60Hz.
        if (s2 == 3600) {
          REX_STORE_U32(kFEStateGlobal, 6);
          REXLOG_INFO("[HOOK] FE-state stuck at 2 for {} frames -> force 6 "
                      "(will cycle 6->0)", s2);
        } else if (s2 == 3900) {
          // After 6->0 transitioned, push to 1 to retrigger init.
          uint32_t now = static_cast<uint32_t>(REX_LOAD_U32(kFEStateGlobal));
          if (now == 0) {
            REX_STORE_U32(kFEStateGlobal, 1);
            REXLOG_INFO("[HOOK] FE-state cycled 6->0; now forcing 1 to "
                        "retrigger init");
          }
        }
      }
    }
  }

  // MoviePlayer2 play-list drain: the libavcodec/VP6 hook at sub_82A68E78
  // can't be installed (symbol doesn't exist in our codegen, and
  // libavcodec.lib isn't linked into our target). Instead, pop one
  // PlaybackRequest off mem[mp+40] per frame so the title's per-frame
  // polling sites (vtable[5] of mp -> read mem[mp+40]) eventually see the
  // queue empty. See DrainMoviePlayer2PlayList above for layout details.
  {
    uint32_t drained = DrainMoviePlayer2PlayList(base);
    if (drained != 0) {
      static std::atomic<int> s_drain_count{0};
      int dc = s_drain_count.fetch_add(1, std::memory_order_relaxed) + 1;
      if (dc <= 16 || (dc % 256) == 0) {
        REXLOG_INFO("[HOOK] sub_82F02368 drained {} MoviePlayer2 play-list head(s) "
                    "(total drains={})",
                    drained, dc);
      }
    }
  }

  // Cycle 18: COMBINED FIX (Agents 3+4 converged):
  //   Step 1: sub_82ECFFE0 to bootstrap +252 (initial slab; agent 4 noted
  //           it's a memory-pool slab not a scene-graph buffer, but ANY
  //           non-zero +252 lets us pass the rich-render gate)
  //   Step 2: clear stale +124 every frame so the natural build dispatcher
  //           can fire sub_82F02178 -> sub_82ED0900 -> sub_82ED07E0 ->
  //           sub_82ECFFE0(pool, size) with CORRECT (pool, size) args.
  //           That natural chain will OVERWRITE +252 with a proper
  //           scene-graph buffer that has valid vtable for vtable[9]/[8]
  //           dispatch in sub_82F01EE8.
  //
  // Step 1 is one-shot. Step 2 fires every frame.
  if (this_ptr) {
    uint32_t v8   = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 8));
    uint32_t v124 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 124));
    uint32_t v248 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 248));
    uint32_t v252 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 252));
    static std::atomic<int> s_setup_attempts{0};
    static std::atomic<int> s_setup_logs{0};
    // Step 1: bootstrap +252 once.
    if (v8 != 0 && v124 == 0 && v248 != 0 && v252 == 0 &&
        s_setup_attempts.fetch_add(1, std::memory_order_relaxed) < 64) {
      PPCRegister saved_r3 = ctx.r3;
      PPCRegister saved_r4 = ctx.r4;
      uint64_t    saved_lr = ctx.lr;
      ctx.r3.u32 = this_ptr;
      ctx.r4.u32 = v248;
      __imp__sub_82ECFFE0(ctx, base);
      uint32_t hr = static_cast<uint32_t>(ctx.r3.u32);
      ctx.r3 = saved_r3;
      ctx.r4 = saved_r4;
      ctx.lr = saved_lr;
      uint32_t v252_after =
          static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 252));
      if (s_setup_logs.fetch_add(1, std::memory_order_relaxed) < 8) {
        REXLOG_INFO("[HOOK] sub_82F02368 #{} drove sub_82ECFFE0 this={:08X} "
                    "v248={:08X} hr={:08X} +252_after={:08X}",
                    n, this_ptr, v248, hr, v252_after);
      }
    }
  }

  // (vtable repair already applied at line 985 with corrected address 0x82048898)

  // Cycle 20: BUILD-GATE STALE-LIST-HEAD CLEAR.
  // The natural draw-build dispatcher in sub_82F02368 (recomp.104.cpp:63802)
  // gates entry to sub_82F02178 (the draw-list builder, which transitively
  // calls sub_82ED0900 -> sub_82ED07E0 -> sub_82ECFFE0 with correct args)
  // on:    vtable[8] returns non-zero  AND  mem[this+124] == 0.
  //
  // Trace of our run (skate3_325.log): every frame logs
  //   +124=4402065C +184=00000000 +252=4401D290 rich_path=YES
  // i.e. the carve happened (+252 set) and the natural sub_82ECFFE0 path
  // pushed a tracking node onto +124 via sub_82ECF998(this+124) at
  // recomp.103.cpp:19307. That stale list head wedges the build gate
  // closed FOREVER -- so +184 never gets populated and the rich-render
  // path at recomp.104.cpp:63852 emits draws against an empty buffer.
  //
  // Clear +124 just before the natural per-frame dispatch so the gate
  // opens and the builder allocates a fresh draw count. Only when both
  // +8 and +252 are populated and +124 is non-zero (i.e. stale).
  if (this_ptr) {
    uint32_t v8_pre   = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 8));
    uint32_t v124_pre = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 124));
    uint32_t v252_pre = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 252));
    if (v8_pre != 0 && v124_pre != 0 && v252_pre != 0) {
      static std::atomic<int> s_124zap_count{0};
      int z = s_124zap_count.fetch_add(1, std::memory_order_relaxed) + 1;
      REX_STORE_U32(this_ptr + 124, 0);
      if (z <= 8 || (z % 600) == 0) {
        REXLOG_INFO("[HOOK] sub_82F02368 #{} cleared stale +124={:08X} "
                    "to open natural draw-build gate (v8={:08X} v252={:08X}, "
                    "total clears={})",
                    n, v124_pre, v8_pre, v252_pre, z);
      }
    }
  }

  if (this_ptr && (n == 1 || (n % 600) == 0)) {
    uint32_t v8 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 8));
    uint32_t v12 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 12));
    uint32_t v124 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 124));
    uint32_t v176 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 176));
    uint32_t v184 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 184));
    uint32_t v252 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 252));
    uint32_t v264 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 264));
    bool rich = (v8 != 0) && (v252 != 0);
    REXLOG_INFO(
        "[HOOK] sub_82F02368 #{} this={:08X} +8={:08X} +12={:08X} +124={:08X} "
        "+176={:08X} +184={:08X} +252={:08X} +264={:08X} rich_path={}",
        n, this_ptr, v8, v12, v124, v176, v184, v252, v264,
        rich ? "YES" : "NO_GATED");
  }

  // Cycle 19: vtable probe for the bctrl at sub_82F02368+0xD4
  // (lr=0x82F0243C, recomp.104.cpp:63793-63795). The recompile loads
  // vt = mem[r31+0], then dispatches mem[vt+32] (vtable[8]). Whatever
  // vt8 points to drives the post-call branch in r3 -- if it's NULL or
  // a stub returning 0, the build-gate stays closed forever and +184
  // never gets a draw count. Log vtable[0..11] + vtable[40] of BOTH
  // `this` and the +252 buffer at frame 1 / every 600 / first time we
  // see FE-state==5.
  static std::atomic<bool> s_vt_state5_logged{false};
  bool vt_state5_first = false;
  {
    uint32_t fe_now2 = static_cast<uint32_t>(REX_LOAD_U32(0x8300B2B0));
    if (fe_now2 == 5 &&
        !s_vt_state5_logged.exchange(true, std::memory_order_relaxed)) {
      vt_state5_first = true;
    }
  }
  uint32_t pre_vt8 = 0;
  uint32_t pre_v124 = 0;
  uint32_t pre_v184 = 0;
  bool log_vt_post = false;
  if (this_ptr && (n == 1 || (n % 600) == 0 || vt_state5_first)) {
    uint32_t vt = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 0));
    if (IsLikelyGuestPointer(vt)) {
      uint32_t vt0  = static_cast<uint32_t>(REX_LOAD_U32(vt + 0));
      uint32_t vt1  = static_cast<uint32_t>(REX_LOAD_U32(vt + 4));
      uint32_t vt2  = static_cast<uint32_t>(REX_LOAD_U32(vt + 8));
      uint32_t vt3  = static_cast<uint32_t>(REX_LOAD_U32(vt + 12));
      uint32_t vt4  = static_cast<uint32_t>(REX_LOAD_U32(vt + 16));
      uint32_t vt5  = static_cast<uint32_t>(REX_LOAD_U32(vt + 20));
      uint32_t vt6  = static_cast<uint32_t>(REX_LOAD_U32(vt + 24));
      uint32_t vt7  = static_cast<uint32_t>(REX_LOAD_U32(vt + 28));
      uint32_t vt8  = static_cast<uint32_t>(REX_LOAD_U32(vt + 32));
      uint32_t vt9  = static_cast<uint32_t>(REX_LOAD_U32(vt + 36));
      uint32_t vt10 = static_cast<uint32_t>(REX_LOAD_U32(vt + 40));
      uint32_t vt11 = static_cast<uint32_t>(REX_LOAD_U32(vt + 44));
      uint32_t vt40 = static_cast<uint32_t>(REX_LOAD_U32(vt + 160));
      pre_vt8 = vt8;
      REXLOG_INFO(
          "[HOOK] sub_82F02368 #{} engine vtable={:08X} vt0={:08X} vt1={:08X} "
          "vt2={:08X} vt3={:08X} vt4={:08X} vt5={:08X} vt6={:08X} vt7={:08X} "
          "vt8={:08X} vt9={:08X} vt10={:08X} vt11={:08X} vt40={:08X} "
          "(state5_first={})",
          n, vt, vt0, vt1, vt2, vt3, vt4, vt5, vt6, vt7, vt8, vt9, vt10, vt11,
          vt40, vt_state5_first ? "YES" : "no");
    } else {
      REXLOG_INFO(
          "[HOOK] sub_82F02368 #{} engine vtable={:08X} (not a guest pointer)",
          n, vt);
    }

    // Probe the +252 inner buffer's vtable too. sub_82F01EE8 (the
    // dispatcher reached recursively through vt8) also hits vtable[8]/[9]
    // on +252.
    uint32_t v252 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 252));
    if (v252 != 0) {
      uint32_t buf_vt = static_cast<uint32_t>(REX_LOAD_U32(v252 + 0));
      if (IsLikelyGuestPointer(buf_vt)) {
        uint32_t bvt0  = static_cast<uint32_t>(REX_LOAD_U32(buf_vt + 0));
        uint32_t bvt8  = static_cast<uint32_t>(REX_LOAD_U32(buf_vt + 32));
        uint32_t bvt9  = static_cast<uint32_t>(REX_LOAD_U32(buf_vt + 36));
        uint32_t bvt10 = static_cast<uint32_t>(REX_LOAD_U32(buf_vt + 40));
        uint32_t bvt11 = static_cast<uint32_t>(REX_LOAD_U32(buf_vt + 44));
        REXLOG_INFO(
            "[HOOK] sub_82F02368 #{} +252 buffer={:08X} vtable={:08X} "
            "vt0={:08X} vt8={:08X} vt9={:08X} vt10={:08X} vt11={:08X}",
            n, v252, buf_vt, bvt0, bvt8, bvt9, bvt10, bvt11);
      } else {
        REXLOG_INFO(
            "[HOOK] sub_82F02368 #{} +252 buffer={:08X} vtable={:08X} "
            "(not a guest pointer)",
            n, v252, buf_vt);
      }
    }

    pre_v124 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 124));
    pre_v184 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 184));
    log_vt_post = true;
  }

  __imp__sub_82F02368(ctx, base);

  // Cycle 19 post-call: log how the bctrl on vtable[8] effectively
  // resolved by checking what the natural body wrote into +124/+184.
  // We can't directly intercept the bctrl, but bctrl on vtable[8] is the
  // first major dispatch and its return value gates the rich-render branch.
  if (log_vt_post && this_ptr) {
    uint32_t post_v124 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 124));
    uint32_t post_v184 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 184));
    REXLOG_INFO(
        "[HOOK] sub_82F02368 #{} post-call vt8_callee={:08X} "
        "+124 {:08X}->{:08X} +184 {:08X}->{:08X}",
        n, pre_vt8, pre_v124, post_v124, pre_v184, post_v184);
  }

  // Cycle 21: Manual fire of sub_82F02178 (draw-list builder).
  //
  // The natural dispatch in sub_82F02368 (recomp.104.cpp:63793 -> 63828)
  // gates entry on the result of `bctrl mem[mem[this][0]+8]` (vtable[8] of
  // the engine itself). In our recompile that vtable slot returns 0 in the
  // post-language-menu state, so the `beq cr0, 0x82f0248c` at line 63799
  // routes to loc_82F0248C which writes mem[sp+80]=0 -- skipping the
  // builder entirely. mem[+184] therefore never gets populated and the
  // rich-render path (recomp.104.cpp:63852+) submits draws against an
  // empty buffer.
  //
  // Re-fire the builder manually here, with the same r3/r4 the natural
  // call site would have used (recomp.104.cpp:63818-63828):
  //   r3 = engine (this_ptr)
  //   r4 = mem[mem[engine+120]+12](u16) * count
  // where `count` mirrors what the natural call site's r30 would have
  // been: the result of vtable[44] dispatch on the sub-objects at +252
  // and +8. Since vtable[8] short-circuited above, those vtable[44]
  // calls never ran in the natural body. We approximate r30 by using
  // the pre-vtable count r29 from recomp.104.cpp:63704-63722:
  //   r11 = mem[mem[engine+80][0]+44]
  //   r10 = mem[mem[engine+80][0]+52]
  //   count = r10 / mem[r11+12](u16)
  // (the divwu before the first vtable[44] dispatch).
  //
  // sub_82F02178 explicitly handles r4==0 (recomp.104.cpp:63399-63408,
  // falls through to mem[mem[r3+4]+532] / sub_82F01038), so a 0-byte
  // request is a safe no-op rather than a crash.
  //
  // Gate:
  //   * +252 != 0 (rich path eligible -- scene-graph buffer carved)
  //   * +184 == 0 (no draws this frame yet)
  //   * frame counter >= 60 (let initial setup finish)
  //   * once per frame
  if (this_ptr && n >= 60) {
    uint32_t v252 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 252));
    uint32_t v184 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 184));
    if (v252 != 0 && v184 == 0) {
      // Compute r4 = mem[mem[engine+120]+12](u16) * count, with count
      // mirroring r29 (the divwu result at recomp.104.cpp:63704-63722).
      uint32_t v120 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 120));
      uint32_t v80  = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 80));
      uint32_t r4_val = 0;
      uint32_t count = 0;
      uint32_t stride = 0;
      if (IsLikelyGuestPointer(v120)) {
        stride = static_cast<uint32_t>(REX_LOAD_U16(v120 + 12));
      }
      if (IsLikelyGuestPointer(v80)) {
        uint32_t v80_0 = static_cast<uint32_t>(REX_LOAD_U32(v80 + 0));
        if (IsLikelyGuestPointer(v80_0)) {
          uint32_t r11_struct =
              static_cast<uint32_t>(REX_LOAD_U32(v80_0 + 44));
          uint32_t r10_total =
              static_cast<uint32_t>(REX_LOAD_U32(v80_0 + 52));
          if (IsLikelyGuestPointer(r11_struct)) {
            uint32_t divisor =
                static_cast<uint32_t>(REX_LOAD_U16(r11_struct + 12));
            if (divisor != 0) {
              count = r10_total / divisor;
            }
          }
        }
      }
      r4_val = stride * count;

      PPCContext saved = ctx;
      ctx.r3.u32 = this_ptr;
      ctx.r4.u32 = r4_val;
      __imp__sub_82F02178(ctx, base);
      uint32_t hr = static_cast<uint32_t>(ctx.r3.u32);
      ctx = saved;
      uint32_t v184_after = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 184));
      static std::atomic<int> s_force_count{0};
      int fc2 = s_force_count.fetch_add(1, std::memory_order_relaxed) + 1;
      if (fc2 <= 8 || (fc2 % 600) == 0) {
        REXLOG_INFO("[HOOK] sub_82F02368 #{} manual sub_82F02178 fire: "
                    "stride={} count={} r4={:08X} -> r3={:08X} "
                    "+184_before=0 +184_after={:08X} (fires={})",
                    n, stride, count, r4_val, hr, v184_after, fc2);
      }
    }
  }
}

// ----------------------------------------------------------------------------
// sub_82F01848 - the per-frame draw-buffer pop. Per Ghidra agent analysis,
// this calls sub_82F011E0(this+132) which pops the head of the buffer-
// descriptor free list at mem[this+132+36]. If the list is empty, the
// pop returns 0 -> mem[this+184] stays 0 -> sub_82F02178 (the draw-list
// builder) falls back to the stub-chain path that emits no real GPU
// commands. So GPU stays silent forever even though sub_82F02368 is
// firing every frame.
//
// The recompiled draw pipeline NEVER auto-refills this pool from the
// builder side -- the refill primitive is sub_82F01240 (61147 in
// skate3_recomp.104.cpp) which calls sub_82EC7870 to allocate 28-byte
// descriptors and chain them in. On real hardware presumably an init
// path runs that during boot, but in our recomp the boot path never
// invokes it. We force-refill on every empty-pool observation here.
extern "C" REX_FUNC(sub_82F01848) {
  uint32_t this_ptr = static_cast<uint32_t>(ctx.r3.u32);
  // Cycle 17 HIR-diff agent finding: Xenia NEVER compiled the
  // 82F00000-82F0B7FF block during a 50s working menu run. Lowest 82F-range
  // function compiled was 82F0B9F0. So Xenia's working menu pixels do NOT
  // go through sub_82F01848/sub_82F02368/sub_82F018F0 family at all. Our
  // hook was force-refilling pools on EVERY caller, even when the parent
  // object's "buffer-active" flag (+248) was 0 -- corrupting unrelated
  // memory and contributing to 8293F0E8 spin / 828F6B00 STOP cascades.
  //
  // Gate the force-refill by mem[this+248]==1 (the constructor at
  // sub_82F01CC8 / recomp.104.cpp:62734 sets +248=1 on a fully-built
  // GPU pipeline object). If +248 isn't 1, this is either a different
  // class entirely or a torn-down object; refilling its "pool" walks
  // arbitrary memory.
  if (IsLikelyGuestPointer(this_ptr)) {
    uint32_t buffer_active =
        static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 248));
    if (buffer_active == 1) {
      uint32_t pool_head =
          static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 132 + 36));
      if (pool_head == 0) {
        // Drive sub_82F01240 to refill before the natural pop.
        uint32_t saved_r3 = static_cast<uint32_t>(ctx.r3.u32);
        ctx.r3.u32 = this_ptr + 132;
        __imp__sub_82F01240(ctx, base);
        ctx.r3.u32 = saved_r3;
      }
    }
  }
  __imp__sub_82F01848(ctx, base);
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 8 || (n % 600) == 0) {
    uint32_t buf_after = this_ptr ?
        static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 184)) : 0;
    uint32_t pool_after = this_ptr ?
        static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 132 + 36)) : 0;
    REXLOG_INFO("[HOOK] sub_82F01848 #{} this={:08X} pool_after={:08X} +184_after={:08X}",
                n, this_ptr, pool_after, buf_after);
  }
}

// ----------------------------------------------------------------------------
// sub_82B5A648 - GPU dispatcher (calls __imp__VdGetSystemCommandBuffer at
// line 39943-onwards then __imp__VdSwap). If this fires per-frame we know
// the consumer thread is alive; if NEVER fires we know to look upstream
// for the missing thread spawn or sync wait.
// Forward decl: GPU consumer engine pointer captured by sub_82B5A648's hook
// and consumed by sub_82F01038's hook (KeSetEvent the right engine event).
extern std::atomic<uint32_t> g_gpu_engine;

extern "C" REX_FUNC(sub_82B5A648) {
  uint32_t engine = static_cast<uint32_t>(ctx.r3.u32);
  // Stash the GPU engine pointer so the producer (sub_82F01038) can kick its
  // event directly. The GPU consumer waits on engine+60 inside sub_82B61EE0;
  // signaling that event after each publish breaks the IRQ-vs-work
  // chicken-and-egg.
  if (IsLikelyGuestPointer(engine)) {
    g_gpu_engine.store(engine, std::memory_order_relaxed);
  }
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 60) == 0) {
    REXLOG_INFO("[HOOK] sub_82B5A648 GPU dispatcher entry #{} this={:08X}",
                n, engine);
  }
  __imp__sub_82B5A648(ctx, base);
}

// sub_82A52BF8 - render-frame coordinator (one of the two callers of
// sub_82B5A648). Hooking with logging so we can see if the per-frame
// path is reached at all.
extern "C" REX_FUNC(sub_82A52BF8) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 60) == 0) {
    REXLOG_INFO("[HOOK] sub_82A52BF8 render-frame entry #{} this={:08X}",
                n, static_cast<uint32_t>(ctx.r3.u32));
  }
  __imp__sub_82A52BF8(ctx, base);
}

// sub_82B62540 - GPU consumer thread spawner (recomp.70.cpp:61677). Iterates
// up to 6 GPU engines, calls KeInitializeDpc(0x82B62078, ...) and ExCreateThread
// with proc 0x82B61EE0 for each. Per Ghidra agent (cycle 6): if this returns 0
// the consumer threads were never spawned -> dispatcher sub_82B5A648 never gets
// woken up -> no GPU output. Logging here lets us disambiguate "threads
// spawned" vs "spawn failed".
extern "C" REX_FUNC(sub_82B62540) {
  uint32_t engine = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t engine_mask = IsLikelyGuestPointer(engine) ?
      static_cast<uint32_t>(REX_LOAD_U32(engine + 24192)) : 0;
  REXLOG_INFO("[HOOK] sub_82B62540 entry this={:08X} engineMask=+24192={:08X}",
              engine, engine_mask);
  __imp__sub_82B62540(ctx, base);
  REXLOG_INFO("[HOOK] sub_82B62540 exit -> r3={:08X}",
              static_cast<uint32_t>(ctx.r3.u32));
}

// sub_82F01038 - draw-block publish. Per cycle 6 attempt 1, kicking event
// at r3+60 (sub_82F01038's r3 is the BUILD engine 0x4401C600) didn't unblock
// the consumer because the consumer waits on a DIFFERENT engine struct
// 0x44002800 (sub_82B5A648's r3 -- the GPU dispatcher). The build engine
// presumably holds a pointer to the GPU engine; need to find offset.
// Meanwhile, also signal the GPU engine event captured from sub_82B5A648
// via a saved global so we can kick the right one.
std::atomic<uint32_t> g_gpu_engine{0};

extern "C" REX_FUNC(sub_82F01038) {
  uint32_t build_engine = static_cast<uint32_t>(ctx.r3.u32);
  __imp__sub_82F01038(ctx, base);
  uint32_t gpu_engine = g_gpu_engine.load(std::memory_order_relaxed);
  if (IsLikelyGuestPointer(gpu_engine)) {
    PPCContext saved = ctx;
    ctx.r3.u64 = gpu_engine + 60;
    ctx.r4.s64 = 1;
    ctx.r5.s64 = 0;
    __imp__KeSetEvent(ctx, base);
    ctx = saved;
    static std::atomic<int> kick_count{0};
    int kn = kick_count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (kn <= 4 || (kn % 600) == 0) {
      REXLOG_INFO("[HOOK] sub_82F01038 #{} kicked GPU engine event at {:08X}+60 "
                  "(build_engine={:08X})",
                  kn, gpu_engine, build_engine);
    }
  }
}

// ----------------------------------------------------------------------------
// sub_827DB100 -- The scene-render gate. Per Opus RE agent (cycle 9):
// "lines 48870-49000 contain sub_827DB6C8 / sub_827DBAC0 / sub_82B49C60 -- the
// ACTUAL scene-graph render". The function loads r17 = 0x83028FB4 (a global
// pointer slot) and dereferences it. Calls sub_827DB6C8 at line 48871; if
// that returns zero, the rich render path is skipped (jumps to loc_827DB460
// which only does minimal matrix setup, then later tests sub_827C3BF0 and
// conditionally calls sub_82A52BF8 -- the swap). The "all-black" frames we
// see in the swap mean the title is taking the skip-rich-render path:
// EDRAM gets only the clear-to-black, no scene draws. Logging r17 and what
// it dereferences to figure out whether the scene root is null.
extern "C" REX_FUNC(sub_827DB100) {
  uint32_t r3 = static_cast<uint32_t>(ctx.r3.u32);
  // Per Opus RE agent (cycle 10): the actual scene-root pointer lives at
  // global mem[0x83027DC4] (NOT r3+32196 as previously thought). Stores to
  // 0x83027DC4 happen at:
  //   - skate3_recomp.38.cpp:46070 in renderer constructor sub_827D9D90
  //   - skate3_recomp.38.cpp:46879 in renderer shutdown sub_827DA6D0 (NULLs it)
  // While the title is stuck in intro state 5, this is NULL, so sub_827DB100
  // takes its skip-rich-render path -> only clear-color reaches the
  // frontbuffer -> black screen.
  uint32_t scene_root = static_cast<uint32_t>(REX_LOAD_U32(0x83027DC4u));
  uint32_t scene_root_vt = IsLikelyGuestPointer(scene_root) ?
      static_cast<uint32_t>(REX_LOAD_U32(scene_root + 0)) : 0;
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 600) == 0) {
    REXLOG_INFO(
        "[HOOK] sub_827DB100 #{} this={:08X} scene_root[0x83027DC4]={:08X} vt={:08X}",
        n, r3, scene_root, scene_root_vt);
  }
  __imp__sub_827DB100(ctx, base);
}

// sub_827DB6C8 -- the gate test inside sub_827DB100. Returns non-zero when
// "scene has geometry to render". If returns 0, the rich render is skipped.
extern "C" REX_FUNC(sub_827DB6C8) {
  uint32_t r3 = static_cast<uint32_t>(ctx.r3.u32);
  __imp__sub_827DB6C8(ctx, base);
  uint32_t ret = static_cast<uint32_t>(ctx.r3.u32);
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 600) == 0) {
    REXLOG_INFO("[HOOK] sub_827DB6C8 #{} arg={:08X} ret={:08X} ({})",
                n, r3, ret, (ret & 0xFF) ? "RENDER" : "SKIP_RENDER");
  }
}

// ----------------------------------------------------------------------------
// sub_825D3438 -- FE manager vtable[4] "is main menu ready?" predicate.
// Per cycle 15 RE agent (af969964ce71f785d): this returns 1 only when:
//   1. state == 5 (we have this with VP6 EOF activator)
//   2. The FE event queue at mem[0x830734FC] is non-empty AND iteration
//      finds at least one record where mem[record+8] is a non-stub
//      handler index.
// In our recompile the queue is empty (singleton uninitialised); even with
// state=5 the function returns 0, so the menu UI build path doesn't run.
// Override: when state==5 has been observed for a while, force return 1.
extern "C" REX_FUNC(sub_825D3438) {
  __imp__sub_825D3438(ctx, base);
  uint32_t state =
      static_cast<uint32_t>(REX_LOAD_U32(0x8300B2B0u));
  if (state == 5) {
    static std::atomic<int> s_force_count{0};
    int n = s_force_count.fetch_add(1, std::memory_order_relaxed) + 1;
    uint32_t orig_ret = static_cast<uint32_t>(ctx.r3.u32);
    if (n <= 4 || (n % 600) == 0) {
      REXLOG_INFO("[HOOK] sub_825D3438 #{} state==5: forcing ret 1 (orig={})",
                  n, orig_ret);
    }
    ctx.r3.s64 = 1;
  }
}

// ----------------------------------------------------------------------------
// sub_82A674D8 -- the per-frame VP6 decode driver inside MoviePlayer2.
// Per Opus RE agent (cycle 11):
//   r3 = MoviePlayer instance
//   r4 = PlaybackRequest
//   PlaybackRequest layout:
//     +128 = frame counter (decremented by sub_82A68EE0)
//     +132 = state byte
//     +248 = bitstream packet ptr (no-alpha path)
//     +252 = output frame ptr / bitstream-with-alpha
//     +288 = packet length (best guess)
//     +324 = alpha-presence flag (non-zero = alpha-channel decode path)
//     +520 = playback frame index
//     +524 = frame duration (float)
// MoviePlayer:
//     +192 = worker
//     +216 = frame allocator vtable
//     +232 = atomic frame-event counter
//     +392 = playback-request queue header
//     +396 = (different queue?)
//     +408 = alpha-output-target
//     +480 = flag set to 0 at top of sub_82A674D8
//     +484 = saved worker count
//     +488,+492 = current/next presentation timestamp
//     +520 = ACTIVE PlaybackRequest (or VideoDecoder ptr)
//     +688 = state (3 = stopped/done)
//
// We hook in LOG-ONLY mode first to confirm the offsets at runtime. The
// actual decode-via-libavcodec wiring is the next step once we see real
// pointer/size values flowing.
extern "C" REX_FUNC(sub_82A674D8) {
  uint32_t player = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t playreq = static_cast<uint32_t>(ctx.r4.u32);
  uint32_t bs_no_alpha = IsLikelyGuestPointer(playreq) ?
      static_cast<uint32_t>(REX_LOAD_U32(playreq + 248)) : 0;
  uint32_t out_or_alpha_bs = IsLikelyGuestPointer(playreq) ?
      static_cast<uint32_t>(REX_LOAD_U32(playreq + 252)) : 0;
  uint32_t pkt_len = IsLikelyGuestPointer(playreq) ?
      static_cast<uint32_t>(REX_LOAD_U32(playreq + 288)) : 0;
  uint32_t alpha_flag = IsLikelyGuestPointer(playreq) ?
      static_cast<uint32_t>(REX_LOAD_U32(playreq + 324)) : 0;
  uint32_t frame_idx = IsLikelyGuestPointer(playreq) ?
      static_cast<uint32_t>(REX_LOAD_U32(playreq + 128)) : 0;
  uint32_t player_video_dec = IsLikelyGuestPointer(player) ?
      static_cast<uint32_t>(REX_LOAD_U32(player + 520)) : 0;
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 8 || (n % 60) == 0) {
    REXLOG_INFO(
        "[HOOK] sub_82A674D8 #{} player={:08X} playreq={:08X} "
        "+248bs={:08X} +252out/bs={:08X} +288len={} +324alpha={} "
        "+128frame={} player+520={:08X}",
        n, player, playreq, bs_no_alpha, out_or_alpha_bs, pkt_len,
        alpha_flag, frame_idx, player_video_dec);
  }
  // LOG-ONLY for now -- do NOT skip the real call yet.
  // (cycle 11: tried to skip the original and bypass the frame counter,
  // but that broke the LOGO display. Reverted to passthrough.)
  __imp__sub_82A674D8(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82A68EE0 -- the MoviePlayer2 wait-for-decode entry. Per Opus RE agent
// (cycle 9): the function reads `mem[player+688]` (state) at line 57007 of
// skate3_recomp.60.cpp; if state==3 ("stopped/done") it exits via
// loc_82A695A0. Otherwise it spins on a +232 atomic semaphore waiting for
// the VP6 decoder to post work. Since libavcodec isn't bridged to
// MoviePlayer2 in our build, state never advances from 1 (decoding) to 3
// (done) -- so the title is permanently stuck in intro. We force
// `mem[player+688] = 3` immediately on every entry so the wait loop
// early-exits. Combined with the FE state global write at 0x8300B2B0
// this should unblock the title's main render path past the intro.
extern "C" REX_FUNC(sub_82A68EE0) {
  uint32_t player = static_cast<uint32_t>(ctx.r3.u32);
  // Cycle 11 RE-ENABLED: disabling the force-stop broke the logo display.
  // The native MoviePlayer2 wait loop stalls something that the FE
  // depends on. Keep the force-stop.
  if (IsLikelyGuestPointer(player)) {
    uint32_t state_before = static_cast<uint32_t>(REX_LOAD_U32(player + 688));
    if (state_before != 3) {
      PPCContext saved = ctx;
      ctx.r3.u32 = player;
      __imp__sub_82A6A038(ctx, base);
      ctx = saved;
    }
    static std::atomic<int> count{0};
    int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
    if (n <= 4 || (n % 600) == 0) {
      uint32_t state_after = static_cast<uint32_t>(REX_LOAD_U32(player + 688));
      REXLOG_INFO("[HOOK] sub_82A68EE0 #{} player={:08X} state {} -> {} (forced sub_82A6A038)",
                  n, player, state_before, state_after);
    }
  }
  __imp__sub_82A68EE0(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82B4C5A8 - the function that writes CP_RB_WPTR (register 0x01C5 at GPU
// MMIO 0x7FC80714). Per skate3_recomp.70.cpp:6420:
//   `lis r10,32712  /  stw r11,1812(r10)` -> REX_MM_STORE_U32(0x7FC80714, r11)
// where r11 is the new ringbuffer write index. The write is reached only on
// the loc_82B4C628 path (when bit 30 of mem[r29+10941] is CLEAR). On the
// alternate path (bit set) the function bails into a vtable bctrl chain.
// We log:
//  - entry: r3 (engine), r4 (packet), bit-30 check
//  - taken-path identification: did the WPTR shadow at r29+10956 advance?
//  - the actual WPTR value being written
extern "C" REX_FUNC(sub_82B4C5A8) {
  uint32_t engine = static_cast<uint32_t>(ctx.r3.u32);
  uint32_t packet = static_cast<uint32_t>(ctx.r4.u32);
  uint8_t flags = IsLikelyGuestPointer(engine) ?
      static_cast<uint8_t>(REX_LOAD_U8(engine + 10941)) : 0;
  uint32_t wptr_before = IsLikelyGuestPointer(engine) ?
      static_cast<uint32_t>(REX_LOAD_U32(engine + 10956)) : 0;
  __imp__sub_82B4C5A8(ctx, base);
  uint32_t wptr_after = IsLikelyGuestPointer(engine) ?
      static_cast<uint32_t>(REX_LOAD_U32(engine + 10956)) : 0;
  static std::atomic<int> count{0};
  static std::atomic<int> wptr_writes{0};
  bool wptr_advanced = (wptr_after != wptr_before);
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (wptr_advanced) {
    wptr_writes.fetch_add(1, std::memory_order_relaxed);
  }
  if (n <= 8 || (n % 600) == 0) {
    REXLOG_INFO("[HOOK] sub_82B4C5A8 #{} engine={:08X} pkt={:08X} flags10941={:02X} "
                "(bit30={}) WPTR {:08X}->{:08X} wptr_writes_total={}",
                n, engine, packet, flags, ((flags & 0x2) ? 1 : 0),
                wptr_before, wptr_after,
                wptr_writes.load(std::memory_order_relaxed));
  }
}

// Parent callers - log when they enter so we can correlate with sub_82B4C5A8
// firing rate. If parents fire but sub_82B4C5A8 never reaches the WPTR-write
// path, we know the issue is in the conditional logic of sub_82B4C5A8 itself.
extern "C" REX_FUNC(sub_82B4CC28) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 600) == 0) {
    REXLOG_INFO("[HOOK] sub_82B4CC28 entry #{} this={:08X} r4={:08X}",
                n, static_cast<uint32_t>(ctx.r3.u32),
                static_cast<uint32_t>(ctx.r4.u32));
  }
  __imp__sub_82B4CC28(ctx, base);
}

extern "C" REX_FUNC(sub_82B60FA0) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 600) == 0) {
    REXLOG_INFO("[HOOK] sub_82B60FA0 entry #{} this={:08X} r4={:08X}",
                n, static_cast<uint32_t>(ctx.r3.u32),
                static_cast<uint32_t>(ctx.r4.u32));
  }
  __imp__sub_82B60FA0(ctx, base);
}

extern "C" REX_FUNC(sub_82B616D8) {
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 4 || (n % 600) == 0) {
    REXLOG_INFO("[HOOK] sub_82B616D8 entry #{} this={:08X} r4={:08X}",
                n, static_cast<uint32_t>(ctx.r3.u32),
                static_cast<uint32_t>(ctx.r4.u32));
  }
  __imp__sub_82B616D8(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82EB7610 - the NtSetTimerEx-based per-frame timer wait. Used as the
// blocking sleep at the end of each iteration of sub_8293F0E8's outer loop
// (and elsewhere). We hook it to act as a watchdog: when it's called from
// inside sub_8293F0E8 (lr just past `bl sub_82EB7610` at 0x8293F348),
// re-check the inner vtable that the loop's bctrls dispatch through. If
// it has gone NULL after the language-menu/MoviePlayer2 teardown we set
// the parent's exit byte so the next loop check breaks out cleanly --
// instead of the function spinning forever generating NULL-bctrl traffic
// and starving every other thread.
extern "C" REX_FUNC(sub_82EB7610) {
  // Watchdog: only break sub_8293F0E8's outer loop after we've seen the
  // broken-vtable condition for SUSTAINED iterations. The loop is the
  // engine's main per-frame render/update tick (60 Hz at the helper
  // boundary), and exiting it stops rendering -- so we can't bail on
  // the very first transient mismatch. Track consecutive broken
  // iterations and only flip the exit byte after ~5 seconds (300
  // iterations) of continuous brokenness, which empirically separates
  // the post-MoviePlayer2-suspend stuck state from any benign
  // single-frame vtable swap.
  uint32_t lr = static_cast<uint32_t>(ctx.lr);
  if (lr == 0x8293F348) {
    uint32_t this_ptr =
        g_sub_8293F0E8_this.load(std::memory_order_relaxed);
    uint32_t inner = static_cast<uint32_t>(REX_LOAD_U32(0x8302845C));
    bool broken = !IsLikelyGuestPointer(inner);
    if (!broken) {
      uint32_t target = LoadU32IfPointer(base, inner, 16);
      uint32_t vt = LoadU32IfPointer(base, target);
      uint32_t slot28 = LoadU32IfPointer(base, vt, 28);
      uint32_t slot68 = LoadU32IfPointer(base, vt, 68);
      // Original NULL-only check (run 213 known-good). We tried tightening
      // this to "must be in .text band" but that produced false positives
      // on legitimate boot-time vtables where one slot happened to point
      // outside 0x82xxxxxx. Stick with the proven NULL check.
      broken = (target == 0) || (vt == 0) || (slot28 == 0) || (slot68 == 0);
    }
    static std::atomic<int> s_consecutive_broken{0};
    int n = broken ? s_consecutive_broken.fetch_add(1, std::memory_order_relaxed) + 1
                   : (s_consecutive_broken.store(0, std::memory_order_relaxed), 0);
    constexpr int kBrokenThreshold = 300;  // ~5s at 60 Hz
    if (n >= kBrokenThreshold && IsLikelyGuestPointer(this_ptr)) {
      uint8_t exit_byte = static_cast<uint8_t>(REX_LOAD_U8(this_ptr + 36));
      if (exit_byte == 0) {
        REX_STORE_U8(this_ptr + 36, 1);
        static std::atomic<bool> s_logged{false};
        if (!s_logged.exchange(true, std::memory_order_relaxed)) {
          REXLOG_INFO("[HOOK] sub_82EB7610 watchdog: vtable broken {} iters "
                      "inside sub_8293F0E8, breaking outer loop (this={:08X} "
                      "inner={:08X})",
                      n, this_ptr, inner);
        }
      }
    }
  }
  __imp__sub_82EB7610(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82EB8CC8 - "process type validation" guard. The function does:
//   r11 = mem[r3+20]
//   if (r11 & 0x40000) {     <- bit 18 from LSB / "verify process type" flag
//     if (KeGetCurrentProcessType() != mem[r3+379]) {
//       KeBugCheckEx(244, ...)
//     }
//   }
//   ... rest of validation work ...
// On Skate 3 the bit is set on the streaming engine context AND
// KeGetCurrentProcessType() doesn't return the value the title expects, so
// every call traps. We mask the bit off before invoking __imp__ so the
// guard takes its safe-path and the function's normal validation work
// continues. Returning to KeBugCheckEx (via the SDK patch that no longer
// parks) would loop us back into this same guard immediately.
extern "C" REX_FUNC(sub_82EB8CC8) {
  uint32_t obj = static_cast<uint32_t>(ctx.r3.u32);
  if (IsLikelyGuestPointer(obj)) {
    uint32_t flags = static_cast<uint32_t>(REX_LOAD_U32(obj + 20));
    if (flags & 0x00040000u) {
      uint32_t cleared = flags & ~0x00040000u;
      REX_STORE_U32(obj + 20, cleared);
      static std::atomic<int> count{0};
      int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
      if (n <= 4 || (n % 256) == 0) {
        REXLOG_INFO("[HOOK] sub_82EB8CC8 #{} cleared verify-process-type bit "
                    "on obj={:08X} (flags {:08X} -> {:08X}) to skip BugCheckEx",
                    n, obj, flags, cleared);
      }
    }
  }
  __imp__sub_82EB8CC8(ctx, base);
}

// ----------------------------------------------------------------------------
// sub_82CDF8E8 - Sk8::FE::TheFrontEndStateManager::GetState (per Ghidra
// analysis on the dumped .text). Returns the current frontend state in r3.
// State value 5 corresponds to "main menu seen" (sub_825D3438 vtable[4]
// compares its `r9 = mem[r10+20]` field against 5). With the VP6 decoder
// permanently broken in our recomp the state machine never gets out of
// the boot/intro states by itself, so we clamp the getter to return 5
// once the system has had time to spin up.
extern "C" REX_FUNC(sub_82CDF8E8) {
  __imp__sub_82CDF8E8(ctx, base);
  static std::atomic<int> count{0};
  int n = count.fetch_add(1, std::memory_order_relaxed) + 1;
  // Skip the first ~10s of calls so the boot/init sequence runs through
  // its real states (asset preload, controller setup); only force-advance
  // after enough frames that the title would have been "ready" on real
  // hardware.
  constexpr int kFramesBeforeForce = 600;  // ~10s at 60 Hz
  uint32_t orig = static_cast<uint32_t>(ctx.r3.u32);
  if (n >= kFramesBeforeForce) {
    ctx.r3.u64 = 5;
  }
  if (n <= 4 || n == kFramesBeforeForce) {
    REXLOG_INFO("[HOOK] sub_82CDF8E8 #{} GetState returned orig={} {} forced={}",
                n, orig, (n >= kFramesBeforeForce ? "->" : "(passthrough)"),
                static_cast<uint32_t>(ctx.r3.u32));
  }
}

// ----------------------------------------------------------------------------
// sub_82F2A108 - rwfilesys thread-init dispatcher (per Cycle 16 Opus 4.7
// agent comparison vs Xenia 159MB trace at xenia.log:5704-6746).
//
// In Xenia, rwfilesys (handle F8000058, thread name 'rwfilesys') runs through
// sub_82F2A108 -> sub_82F2A088 which does:
//   r3 = sub_82F2D020()         ; allocates 196B TLS thread context
//   r11 = mem[r3+84]             ; entry function pointer
//   r3 = mem[r3+88]              ; "this" pointer for entry
//   bctrl                        ; reaches sub_82EB5890 -> NtCreateFile
//
// In our build the entry function pointer at mem[ctx+84] points into the
// animation observer band (0x82966xxx) instead of the file-server worker.
// The thread therefore spends its life dispatching FE event slot-73 actions
// (sub_82966970/990) and never reaches NtCreateFile -- so .big assets are
// never loaded. This explains why our build never opens d:\data\big\*.big
// (Xenia opens 10+ such files within 30s; we open 0).
//
// We log the entry on first calls so we can identify the correct file-server
// worker address, and once we've identified it we'll force the dispatch to
// route to it. This first pass is observation-only via the new
// log_noisy/log_level=trace combo.
extern "C" REX_FUNC(sub_82F2A108) {
  uint32_t arg = static_cast<uint32_t>(ctx.r3.u32);
  static std::atomic<int> s_call{0};
  int n = s_call.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n <= 16 && IsLikelyGuestPointer(arg)) {
    uint32_t mem_arg_84 = static_cast<uint32_t>(REX_LOAD_U32(arg + 84));
    uint32_t mem_arg_88 = static_cast<uint32_t>(REX_LOAD_U32(arg + 88));
    // mem[+88] is the "this" pointer passed to mem[+84](this).
    // sub_82A18550 (the typical entry) reads mem[this+16], mem[this+20],
    // mem[this+24], mem[this+28]. Slot +28 is the *real* worker function.
    uint32_t this_ptr = mem_arg_88;
    uint32_t mem_this_16 = 0, mem_this_20 = 0, mem_this_24 = 0,
             mem_this_28 = 0;
    if (IsLikelyGuestPointer(this_ptr)) {
      mem_this_16 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 16));
      mem_this_20 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 20));
      mem_this_24 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 24));
      mem_this_28 = static_cast<uint32_t>(REX_LOAD_U32(this_ptr + 28));
    }
    REXLOG_INFO("[HOOK] sub_82F2A108 #{} arg={:08X} entry={:08X} this={:08X} "
                "this[+16]={:08X} this[+20]={:08X} this[+24]={:08X} "
                "this[+28]={:08X}  <-- this[+28] is the actual worker fn",
                n, arg, mem_arg_84, this_ptr, mem_this_16, mem_this_20,
                mem_this_24, mem_this_28);
  }
  __imp__sub_82F2A108(ctx, base);
}

// ============================================================================
// CYCLE 16 BREAKTHROUGH: .big asset-load completion bridge
//
// Per 3-agent convergence (cycle 16, agents ad1817..., a70b51..., a38cd5...):
//
// In Xenia, after each .big file finishes loading, the guest function
// `sub_8293F018` runs on the rwfilesys worker thread:
//   1. Sets the asset object's "ready" flag at mem[this+52] = 1
//   2. Calls sub_82EB71B0(this->[+48]) which is NtSetEvent on the asset's
//      completion event handle (Xenia logs this as `NtSetEvent(F8000030)`
//      from thread F8000040, firing repeatedly)
//   3. Loops `bctrl this->vtable[+24]` -- the descriptor-pool refill broadcast.
//      This is what populates the secondary scene buffer at mem[+252] and
//      ultimately the GPU descriptor pool that mem[this+184] consumes.
//
// In OUR build, our NtReadFile is fully synchronous (xboxkrnl_io.cpp:262
// `if (true || file->is_synchronous())`) and NtClose is bare-bones
// (xboxkrnl_ob.cpp:222-227 just calls ReleaseHandle). The rwfilesys worker
// thread NEVER gets re-scheduled into sub_8293F018, so:
//   - ZERO NtSetEvent calls fire across our entire log (vs. dozens in Xenia)
//   - The vtable[24] broadcast never runs
//   - Scene-graph descriptor pool stays uninitialized
//   - Render thread sees only black-clear EDRAM -> black screen
//
// Bridge: hook NtClose, count guest .big handles closed. After the 3rd
// menu-critical .big (miscboot+db+fedynamic) is closed, walk the FE manager
// at mem[0x83073534] -> +48 (the asset_obj) and synthesize the broadcast
// by invoking sub_8293F018 directly. This fires the same callback chain
// Xenia eventually reaches, populating the descriptor pool and unblocking
// the render thread.
static std::atomic<int>  g_big_close_count{0};
static std::atomic<bool> g_assets_broadcast_done{false};

// Helper to fire the asset-completion broadcast once state=5 is stable.
// This will be called from the existing sub_82F02368 hook (per-frame).
// See cycle 16 memory note for rationale.
namespace {
std::atomic<bool> g_asset_broadcast_fired{false};
std::atomic<int>  g_state5_stable_frames{0};

void TryFireAssetBroadcast(uint8_t* base, PPCContext& ctx) {
  if (g_asset_broadcast_fired.load(std::memory_order_relaxed)) return;
  uint32_t state = static_cast<uint32_t>(REX_LOAD_U32(0x8300B2B0u));
  if (state != 5) {
    g_state5_stable_frames.store(0, std::memory_order_relaxed);
    return;
  }
  // Wait until state=5 has been stable for ~30 frames (~0.5s) so all
  // post-VP6 setup is done before we fire the broadcast.
  int n = g_state5_stable_frames.fetch_add(1, std::memory_order_relaxed) + 1;
  if (n < 30) return;

  // Try to fire only once.
  if (g_asset_broadcast_fired.exchange(true, std::memory_order_relaxed))
    return;

  uint32_t fe_root = static_cast<uint32_t>(REX_LOAD_U32(0x83073534u));
  if (!IsLikelyGuestPointer(fe_root)) {
    REXLOG_WARN("[HOOK] AssetBroadcast: fe_root at 0x83073534 = {:08X} "
                "(not a guest ptr); cannot fire sub_8293F018",
                fe_root);
    g_asset_broadcast_fired.store(false, std::memory_order_relaxed);
    return;
  }

  // Try a few candidate offsets where the asset-mgr could live in fe_root.
  // Per agent 3: sub_8293F018 reads mem[r3+48] as NtSetEvent target. The
  // asset-mgr ptr should live in some slot of fe_root.
  for (uint32_t off : {48u, 52u, 56u, 60u, 64u, 68u, 72u, 76u, 80u, 84u,
                       88u, 92u, 96u, 100u, 104u}) {
    uint32_t cand = static_cast<uint32_t>(REX_LOAD_U32(fe_root + off));
    if (IsLikelyGuestPointer(cand)) {
      uint32_t cand_vt =
          static_cast<uint32_t>(REX_LOAD_U32(cand + 0));
      // Asset-mgr objects have their vtable in the .text band 0x82xxxxxx.
      // mem[cand+8] is the asset handle (we'll check non-zero too).
      uint32_t cand_mem8 =
          static_cast<uint32_t>(REX_LOAD_U32(cand + 8));
      uint32_t cand_mem48 =
          static_cast<uint32_t>(REX_LOAD_U32(cand + 48));
      REXLOG_INFO("[HOOK] AssetBroadcast: probe fe_root+{} cand={:08X} "
                  "vt={:08X} cand[+8]={:08X} cand[+48]={:08X}",
                  off, cand, cand_vt, cand_mem8, cand_mem48);
      // Stricter signature for the asset-mgr object:
      //   vt in code band 0x82xxxxxx ✓
      //   cand IS a heap pointer (0x40000000+) -- code-section data is NOT
      //   cand[+8] is a heap pointer (the handle/event obj)
      //   cand[+48] is a heap pointer (NtSetEvent target)
      bool looks_like_real_asset_mgr =
          (cand >= 0x40000000u && cand < 0x70000000u) &&
          (cand_vt >= 0x82000000u && cand_vt < 0x83000000u) &&
          (cand_mem8 == 0 || (cand_mem8 >= 0x40000000u && cand_mem8 < 0x70000000u)) &&
          (cand_mem48 >= 0xF0000000u || cand_mem48 == 0);  // handle range
      if (looks_like_real_asset_mgr) {
        REXLOG_INFO("[HOOK] AssetBroadcast: STRONG match at fe_root+{}={:08X} "
                    "-- would fire sub_8293F018 (gated until verified)",
                    off, cand);
        // Don't fire yet -- need manual verification of structure.
        return;
      }
    }
  }
  REXLOG_INFO("[HOOK] AssetBroadcast: no strong asset-mgr match at "
              "fe_root={:08X}; need different lookup strategy",
              fe_root);
  // Don't reset — try once and stop logging.
}
}  // namespace
