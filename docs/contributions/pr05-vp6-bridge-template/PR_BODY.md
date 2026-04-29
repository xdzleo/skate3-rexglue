# VP6 decoder bridge as optional `rex::audio::vp6` module

## Summary

**Status:** 📝 PROPOSAL — not yet integrated as a packaged module.

This proposal packages the libavcodec VP6 decoder bridge developed for Skate 3's intro video (`EA_Blackbox_english_ntsc.vp6`, 268 frames @ 29.97 fps) into a reusable rexglue-sdk optional module.

## Problem statement

Many Xbox 360 games — particularly EA, Activision, and Take-Two titles — ship pre-rendered cutscenes in the `.vp6` (Truemotion VP6) format wrapped in EA's RWMovie container (`MV0K`/`MV0F` chunks).

The original Xbox 360 hardware decodes these via the `MoviePlayer2` system component. Static recompilation of `MoviePlayer2` is unreliable — the recompiled decoder hits an uninitialized pointer in some titles (verified in Skate 3).

Currently, each rexglue-based port has to:
1. Write its own VP6 demuxer
2. Integrate libavcodec
3. Wire the decoded frames into the GPU swap chain

We solved this for Skate 3 in `skate3-rexglue/src/vp6_bridge.cpp` (521 lines). This PR proposes packaging that work as an optional module.

## Proposed module API

```cpp
// rex/audio/vp6.h
namespace rex::video::vp6 {

struct DecoderState;

// Open a .vp6 (RWMovie wrapper) file. Returns nullptr on failure.
DecoderState* OpenFile(const char* host_path);

// Decode the next frame into an internal RGBA8888 buffer.
// Returns true on success, false on EOF or error.
bool AdvanceFrame(DecoderState* state);

// Get current frame dimensions and metadata.
struct FrameInfo {
    uint32_t width;
    uint32_t height;
    uint32_t frame_index;
    uint32_t total_frames;
    float fps;
};
FrameInfo GetFrameInfo(const DecoderState* state);

// Copy current decoded frame to a destination buffer (RGBA8888).
// Returns bytes copied, or 0 if no frame available.
int CopyCurrentRgba(const DecoderState* state, uint8_t* dst, int max_bytes);

// Release decoder resources.
void Close(DecoderState* state);

}  // namespace rex::video::vp6
```

## Optional D3D12 IssueSwap overlay

For titles that want their VP6 frames to appear in the swap chain without needing a custom GPU pipeline, provide a `rex::video::vp6::D3D12Overlay` helper:

```cpp
// rex/video/vp6_d3d12_overlay.h
namespace rex::video::vp6 {

class D3D12Overlay {
 public:
    // Construct given a CommandProcessor. Lazily creates D3D12 resources
    // (DEFAULT heap texture + UPLOAD heap intermediate, BGRA8 1280x720).
    D3D12Overlay(rex::graphics::d3d12::D3D12CommandProcessor* cp);

    // Substitute the current frontbuffer source with the VP6 frame for
    // this swap. Called from D3D12CommandProcessor::IssueSwap().
    bool ApplyToSwap(uint32_t guest_frontbuffer_ptr,
                     uint32_t* out_substituted_address,
                     ID3D12Resource** out_substituted_resource);

    void Close();
};

}
```

The CommandProcessor's `IssueSwap()` would then call:

```cpp
if (rex::video::vp6::IsActive()) {
    overlay.ApplyToSwap(...);
    // Use substituted address/resource
}
```

## Implementation reference

Working implementation in `skate3-rexglue/src/vp6_bridge.cpp`:

| Function | Description |
|----------|-------------|
| `Skate3VP6_OpenFile` | Open + parse RWMovie header |
| `Skate3VP6_NextPacket` | Yield next VP6 chunk (key/delta) |
| `Skate3VP6_DecodeFrame` | Pass to libavcodec, get YUV420P |
| `Skate3VP6_StartPlayback` | Begin advancing frames |
| `Skate3VP6_AdvanceFrame` | Decode + convert YUV→RGBA + copy to internal buffer |
| `Skate3VP6_CopyCurrentRgba` | Copy current frame to caller's buffer |
| `Skate3VP6_GetMovieInfo` | Return width/height/frame count |

D3D12 overlay implementation in `rexglue-sdk/src/graphics/d3d12/command_processor.cpp::IssueSwap`:
- Lazy-creates DEFAULT heap texture (BGRA8, 1280×720)
- Lazy-creates UPLOAD heap intermediate
- Map + memcpy + CopyTextureRegion + barrier transitions
- Weak-linkage trampolines so SDK targets without VP6 silently skip

## Test evidence

In Skate 3 PC port:

```
[VP6] OpenFile: 'EA_Blackbox_english_ntsc.vp6' OK size=14001720 1280x720 frames=268 fps=29.970
[VP6] StartPlayback OK
[VP6] AdvanceFrame: now at frame 268/268 (final)
```

Visible Black Box logo + Skate 3 intro plays cleanly at native 29.97 fps. No frame drops, no decode errors.

## Dependencies

- libavcodec (already a transitive dep of `rex::audio` via XMA decoder)
- libavutil (same)
- D3D12 (existing rexglue-sdk dep)

Header impact: minimal — just `rex/video/vp6.h` and `rex/video/vp6_d3d12_overlay.h`.

## Build flag

Gate behind `REX_BUILD_VP6 = ON` (default OFF) to avoid forcing libavcodec linkage on titles that don't need it:

```cmake
option(REX_BUILD_VP6 "Build VP6 video decoder module (libavcodec)" OFF)
if(REX_BUILD_VP6)
    add_subdirectory(src/video/vp6)
endif()
```

## Test plan

1. Skate 3 (verified — works)
2. Burnout Paradise (untested — has `.vp6` cutscenes)
3. NBA Live (untested — has `.vp6` cutscenes)
4. Any other EA RenderWare 3 title

## Future work

- Add audio track support (VP6 typically pairs with XMA audio in RWMovie containers)
- Add subtitle/caption track support
- Optimize YUV→RGBA via D3D12 compute shader instead of CPU memcpy

## References

- libavcodec VP6 decoder: https://ffmpeg.org/
- RWMovie format reverse-engineering: archived EA Skate 3 modding forums
- Existing implementation: `skate3-rexglue/src/vp6_bridge.cpp`
