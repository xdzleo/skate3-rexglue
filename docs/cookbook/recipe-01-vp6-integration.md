# Recipe 1 — VP6 Video Integration

**Audience:** Xbox 360 PC ports of titles with `.vp6` (Truemotion VP6) cutscenes wrapped in EA RWMovie containers.

## When to use

Symptoms:
- Title's `data/movies/*.vp6` files are present
- Recompiled `MoviePlayer2` decoder crashes or hangs at intro
- You want to play the intro video to verify state machine progression

Common in:
- All Skate series (1, 2, 3)
- Most EA RenderWare 3 titles
- Burnout series
- Need For Speed Carbon and later

## Why it works

The Xbox 360's `MoviePlayer2` system component handles VP6 decode in hardware. Static recompilation of this is unreliable. Instead, we:

1. Bypass the recompiled decoder
2. Open the `.vp6` file ourselves with libavcodec
3. Decode each frame to RGBA8888
4. Substitute the decoded frame into the swap chain via `D3D12CommandProcessor::IssueSwap`

## Implementation reference

See `skate3-rexglue/src/vp6_bridge.cpp` (521 lines) for a complete working implementation.

### File format (EA RWMovie)

```
[8 bytes]  "MVhd" + little-endian u32 size = 8
[12 bytes] Header: u32 frames, u32 fps_x256, u32 unknown
... repeating ...
[8 bytes]  "MV0K" + LE u32 size  → keyframe data follows
[8 bytes]  "MV0F" + LE u32 size  → delta-frame data follows
... ...
[8 bytes]  "MVfn"  → end-of-file
```

FourCC constants:
```cpp
constexpr uint32_t kFourCC_MV0K = 0x4D56304B;  // 'MV0K' big-endian
constexpr uint32_t kFourCC_MV0F = 0x4D563046;  // 'MV0F' big-endian (note: F = 0x46, not 0x4F!)
```

### Decoder API

```cpp
extern "C" bool   Skate3VP6_OpenFile(const char* host_path);
extern "C" bool   Skate3VP6_StartPlayback(const char* host_path);
extern "C" int    Skate3VP6_AdvanceFrame();    // returns 0 on success, < 0 on EOF
extern "C" bool   Skate3VP6_PlaybackActive();
extern "C" int    Skate3VP6_FrameCount();
extern "C" void   Skate3VP6_GetMovieInfo(uint32_t* w, uint32_t* h, uint32_t* fc);
extern "C" int    Skate3VP6_CopyCurrentRgba(const uint8_t* dst, int max_bytes);
extern "C" void   Skate3VP6_CloseFile();
extern "C" void   Skate3VP6_Reset();
```

### libavcodec wiring

```cpp
#include <libavcodec/avcodec.h>

const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_VP6);
AVCodecContext* ctx = avcodec_alloc_context3(codec);
avcodec_open2(ctx, codec, nullptr);

// Per packet:
AVPacket pkt;
av_init_packet(&pkt);
pkt.data = vp6_bytes;
pkt.size = vp6_len;
avcodec_send_packet(ctx, &pkt);

AVFrame* frame = av_frame_alloc();
avcodec_receive_frame(ctx, frame);
// frame is YUV420P, planar
```

### YUV420P → BGRA8888 conversion (BT.601)

```cpp
void YUV420toBGRA8(const AVFrame* frame, uint8_t* dst, int dst_pitch) {
    int w = frame->width, h = frame->height;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int Y = frame->data[0][y * frame->linesize[0] + x];
            int U = frame->data[1][(y/2) * frame->linesize[1] + (x/2)];
            int V = frame->data[2][(y/2) * frame->linesize[2] + (x/2)];
            int C = Y - 16, D = U - 128, E = V - 128;
            int R = (298*C + 409*E + 128) >> 8;
            int G = (298*C - 100*D - 208*E + 128) >> 8;
            int B = (298*C + 516*D + 128) >> 8;
            R = std::clamp(R, 0, 255);
            G = std::clamp(G, 0, 255);
            B = std::clamp(B, 0, 255);
            uint8_t* px = &dst[y * dst_pitch + x * 4];
            px[0] = B; px[1] = G; px[2] = R; px[3] = 0xFF;  // BGRA
        }
    }
}
```

## Trigger pattern

In your sub_82F02368 (or equivalent per-frame poller) hook:

```cpp
constexpr uint32_t kFEStateGlobal = 0x8300B2B0;  // game-specific
uint32_t fe_state = REX_LOAD_U32(kFEStateGlobal);
static std::atomic<bool> s_vp6_started{false};

if (fe_state == 2 &&  // intro state
    !s_vp6_started.exchange(true, std::memory_order_relaxed)) {
    Skate3VP6_StartPlayback("path/to/intro.vp6");
}

// Wall-clock pacer: only advance every 33,367μs (29.97 fps NTSC)
if (Skate3VP6_PlaybackActive()) {
    using clock = std::chrono::steady_clock;
    static auto last_frame = clock::now();
    auto now = clock::now();
    if (std::chrono::duration_cast<std::chrono::microseconds>(now - last_frame).count() >= 33367) {
        Skate3VP6_AdvanceFrame();
        last_frame = now;
    }
}
```

## Caveats

1. **FourCC byte order:** `MV0F` is `4D 56 30 46` — the 'F' is `0x46`, not `0x4F` (which would be 'O'). Common bug.
2. **Audio:** RWMovie containers usually have separate XMA audio track. This recipe handles video only. For audio, hook the XMA decoder via `rex::audio` separately.
3. **NTSC vs PAL:** Skate 3's `.vp6` files come in `_ntsc.vp6` (29.97 fps) and `_pal.vp6` (25 fps) variants. Use the appropriate frame interval.
4. **Decoder thread:** The original `MoviePlayer2 Decode Thread` should be left suspended. In `xthread.cpp:SetThreadName`, detect it and don't resume.

## Test evidence

Verified working in Skate 3 PC port:

```
[VP6] OpenFile: 'EA_Blackbox_english_ntsc.vp6' OK size=14001720 1280x720 frames=268 fps=29.970
[VP6] StartPlayback OK
... (268 frames decoded over ~9 seconds) ...
[HOOK] VP6 playback EOF reached. FE state=2 -- activating main menu via sub_825C1410.
```

Visible Black Box logo + Skate 3 intro plays at native rate.

## See also

- Recipe 6 — VP6 D3D12 IssueSwap overlay (gets pixels onto screen)
- PR 05 (proposed) — package as `rex::video::vp6` module for upstream
