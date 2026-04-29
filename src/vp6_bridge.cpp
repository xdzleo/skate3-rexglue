/**
 * @file        src/vp6_bridge.cpp
 * @brief       Skate 3 -- libavcodec VP6 decoder bridge to MoviePlayer2.
 *
 * Skate 3's intro video and other movies use VP6 (TrueMotion VP6 / On2 VP6).
 * The title's MoviePlayer2 thread is supposed to feed VP6 bitstream chunks
 * to a decoder and write decoded RGB frames into the FE renderer's source
 * texture. In our recompile, that decoder path was never bridged to the
 * libavcodec sources we compile, so the title sat forever in the intro
 * state waiting for frames that never arrived.
 *
 * This file provides:
 *   1. A persistent libavcodec VP6 decoder context (initialized lazily).
 *   2. A `Skate3VP6_DecodeFrame` C entry point that takes a VP6 bitstream
 *      packet and returns an RGBA frame in a caller-supplied buffer.
 *   3. A `Skate3VP6_Reset` function for switching files / restarting.
 *
 * The `hooks.cpp` MoviePlayer2 hook will:
 *   - Open the .vp6 file (already done by the title; we just intercept
 *     the bitstream the title would have passed to its native decoder)
 *   - Call `Skate3VP6_DecodeFrame` per frame
 *   - Write the decoded pixels into the title's expected frame buffer
 *   - Update MoviePlayer2 state so the title's per-frame loop sees
 *     "a new frame is ready"
 *
 * For now this file is a STUB that just verifies libavcodec is reachable
 * from skate3.exe (linker-level). The actual decode call site will be
 * wired in incrementally once the integration point is confirmed.
 */

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
}

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <rex/logging.h>

namespace {

struct VP6Decoder {
  const AVCodec* codec = nullptr;
  AVCodecContext* ctx = nullptr;
  AVPacket* pkt = nullptr;
  AVFrame* frame = nullptr;
  std::mutex lock;
  bool initialized = false;
  bool failed = false;
  std::atomic<int> frame_count{0};
};

VP6Decoder& Decoder() {
  static VP6Decoder dec;
  return dec;
}

bool EnsureInitialized() {
  auto& d = Decoder();
  std::lock_guard<std::mutex> g(d.lock);
  if (d.initialized) {
    return true;
  }
  if (d.failed) {
    return false;
  }
  // VP6 codec ID (TrueMotion VP6, used by EA's RWMovie). On2 also has
  // AV_CODEC_ID_VP6F (flash) and AV_CODEC_ID_VP6A (alpha). Skate 3 ships
  // VP6 (non-alpha non-flash); we try VP6 first and fall back if init
  // fails.
  d.codec = avcodec_find_decoder(AV_CODEC_ID_VP6);
  if (!d.codec) {
    REXLOG_ERROR("[VP6] avcodec_find_decoder(VP6) returned NULL -- VP6 not "
                 "registered in this libavcodec build");
    d.failed = true;
    return false;
  }
  d.ctx = avcodec_alloc_context3(d.codec);
  if (!d.ctx) {
    REXLOG_ERROR("[VP6] avcodec_alloc_context3 failed");
    d.failed = true;
    return false;
  }
  if (int rc = avcodec_open2(d.ctx, d.codec, nullptr); rc < 0) {
    REXLOG_ERROR("[VP6] avcodec_open2 failed rc={}", rc);
    avcodec_free_context(&d.ctx);
    d.failed = true;
    return false;
  }
  d.pkt = av_packet_alloc();
  d.frame = av_frame_alloc();
  if (!d.pkt || !d.frame) {
    REXLOG_ERROR("[VP6] av_packet_alloc/av_frame_alloc failed");
    if (d.pkt) av_packet_free(&d.pkt);
    if (d.frame) av_frame_free(&d.frame);
    avcodec_free_context(&d.ctx);
    d.failed = true;
    return false;
  }
  d.initialized = true;
  REXLOG_INFO("[VP6] Decoder initialized (codec={}, name={})",
              fmt::ptr(d.codec), d.codec->name);
  return true;
}

}  // namespace

extern "C" {

// Returns true if libavcodec is reachable AND a VP6 decoder context can be
// built. Hooks.cpp can call this once at boot to log the result.
bool Skate3VP6_ProbeAvailable() {
  bool ok = EnsureInitialized();
  if (ok) {
    auto& d = Decoder();
    REXLOG_INFO("[VP6] Probe OK: codec={} ID={}", d.codec->name,
                int(d.codec->id));
  } else {
    REXLOG_WARN("[VP6] Probe FAILED -- decoder unavailable");
  }
  return ok;
}

// Decode a single VP6 frame. The caller provides the bitstream packet
// `data[size]` and a destination RGBA buffer `out_rgba[width*height*4]`.
// Returns 1 on success (frame in out_rgba), 0 on error.
//
// Thread-safe via internal mutex.
int Skate3VP6_DecodeFrame(const uint8_t* data, int size, uint8_t* out_rgba,
                          int out_width, int out_height) {
  if (!data || size <= 0 || !out_rgba || out_width <= 0 || out_height <= 0) {
    return 0;
  }
  if (!EnsureInitialized()) {
    return 0;
  }
  auto& d = Decoder();
  std::lock_guard<std::mutex> g(d.lock);
  d.pkt->data = const_cast<uint8_t*>(data);
  d.pkt->size = size;
  if (int rc = avcodec_send_packet(d.ctx, d.pkt); rc < 0) {
    REXLOG_DEBUG("[VP6] send_packet rc={}", rc);
    return 0;
  }
  if (int rc = avcodec_receive_frame(d.ctx, d.frame); rc < 0) {
    REXLOG_DEBUG("[VP6] receive_frame rc={}", rc);
    return 0;
  }
  // Frame is YUV; convert to RGBA. For now use a simple loop (sws_scale
  // would be cleaner but adds a dependency).
  // VP6 outputs YUV420P. Limit to the smallest of (out_width, frame->width).
  int copy_w = (d.frame->width < out_width) ? d.frame->width : out_width;
  int copy_h = (d.frame->height < out_height) ? d.frame->height : out_height;
  for (int y = 0; y < copy_h; y++) {
    for (int x = 0; x < copy_w; x++) {
      int Y = d.frame->data[0][y * d.frame->linesize[0] + x];
      int U = d.frame->data[1][(y / 2) * d.frame->linesize[1] + (x / 2)];
      int V = d.frame->data[2][(y / 2) * d.frame->linesize[2] + (x / 2)];
      // YUV420P -> RGBA (BT.601)
      int C = Y - 16;
      int D = U - 128;
      int E = V - 128;
      int R = (298 * C + 409 * E + 128) >> 8;
      int G = (298 * C - 100 * D - 208 * E + 128) >> 8;
      int B = (298 * C + 516 * D + 128) >> 8;
      if (R < 0) R = 0; else if (R > 255) R = 255;
      if (G < 0) G = 0; else if (G > 255) G = 255;
      if (B < 0) B = 0; else if (B > 255) B = 255;
      uint8_t* p = out_rgba + (y * out_width + x) * 4;
      p[0] = static_cast<uint8_t>(R);
      p[1] = static_cast<uint8_t>(G);
      p[2] = static_cast<uint8_t>(B);
      p[3] = 0xFF;
    }
  }
  d.frame_count.fetch_add(1, std::memory_order_relaxed);
  return 1;
}

void Skate3VP6_Reset() {
  auto& d = Decoder();
  std::lock_guard<std::mutex> g(d.lock);
  if (d.ctx) {
    avcodec_flush_buffers(d.ctx);
  }
  d.frame_count.store(0, std::memory_order_relaxed);
}

int Skate3VP6_FrameCount() { return Decoder().frame_count.load(std::memory_order_relaxed); }

}  // extern "C"

// ----------------------------------------------------------------------------
// EA RWMovie container parser. Per Opus RE agent (cycle 12) confirming spec
// at https://wiki.multimedia.cx/index.php/Electronic_Arts_VP6 and verified
// against rexglue-sdk/thirdparty/FFmpeg/libavformat/electronicarts.c.
//
// Container layout:
//   - All chunks: 4-byte FourCC + 4-byte LE size (size INCLUDES the 8-byte
//     preamble and payload).
//   - File starts with `MVhd` (24-byte payload after preamble):
//       offset 0x08 [4]: codec FourCC = 'VP60'
//       offset 0x0C [2]: width  (LE u16)
//       offset 0x0E [2]: height (LE u16)
//       offset 0x10 [4]: frame_count (LE u32)
//       offset 0x14 [4]: largest_frame_size (LE u32)
//       offset 0x18 [4]: timescale  (LE u32)  -- ticks per second
//       offset 0x1C [4]: timebase   (LE u32)  -- ticks per frame
//   - `SHEN`/`SHGE`/`SHFR`: per-language audio stream headers (skip).
//   - `SCEN`/`SCGE`/`SCFR`: per-language stream count markers (skip).
//   - `MV0K`: VP6 keyframe -- payload is raw On2 VP60 bitstream.
//   - `MV0F`: VP6 delta frame -- raw On2 VP60 bitstream.
//   - `SDEN`/`SDGE`/`SDFR`: per-language audio data (skip; we don't bridge).
//   - Per-frame interleave is MV0[K|F] then SDEN, SDGE, SDFR.

namespace {

constexpr uint32_t kFourCC_MVhd = 0x4D566864;  // 'MVhd' big-endian -> uint32
constexpr uint32_t kFourCC_MV0K = 0x4D56304B;  // 'M'=4D 'V'=56 '0'=30 'K'=4B
constexpr uint32_t kFourCC_MV0F = 0x4D563046;  // 'M'=4D 'V'=56 '0'=30 'F'=46

// Helper: read 4-byte FourCC at the given offset, returning as a big-endian
// composite (so 'MVhd' becomes 0x4D566864, matching the constants above).
inline uint32_t Read4CC(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

inline uint32_t ReadLE32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline uint16_t ReadLE16(const uint8_t* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}

struct EaMovie {
  std::vector<uint8_t> raw_bytes;  // Whole file in memory.
  size_t cursor = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t frame_count = 0;
  uint32_t timescale = 0;
  uint32_t timebase = 0;
  bool open = false;
  std::mutex lock;
};

EaMovie& Movie() {
  static EaMovie m;
  return m;
}

}  // namespace

extern "C" {

// Open an EA RWMovie file (.vp6) from the host filesystem. Reads the entire
// file into memory and parses the MVhd header. Returns true on success.
bool Skate3VP6_OpenFile(const char* path) {
  auto& m = Movie();
  std::lock_guard<std::mutex> g(m.lock);
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in) {
    REXLOG_ERROR("[VP6] OpenFile: cannot open '{}'", path);
    return false;
  }
  std::streamsize size = in.tellg();
  in.seekg(0, std::ios::beg);
  m.raw_bytes.resize(static_cast<size_t>(size));
  if (!in.read(reinterpret_cast<char*>(m.raw_bytes.data()), size)) {
    REXLOG_ERROR("[VP6] OpenFile: read failed for '{}' (size={})", path, size);
    return false;
  }
  if (m.raw_bytes.size() < 0x20) {
    REXLOG_ERROR("[VP6] OpenFile: file too small ({} bytes)", m.raw_bytes.size());
    return false;
  }
  // Verify MVhd magic.
  if (Read4CC(m.raw_bytes.data()) != kFourCC_MVhd) {
    REXLOG_ERROR("[VP6] OpenFile: '{}' does not start with MVhd", path);
    return false;
  }
  uint32_t mvhd_size = ReadLE32(m.raw_bytes.data() + 4);
  if (mvhd_size < 0x20 || mvhd_size > m.raw_bytes.size()) {
    REXLOG_ERROR("[VP6] OpenFile: bad MVhd size {}", mvhd_size);
    return false;
  }
  // Parse MVhd payload.
  m.width = ReadLE16(m.raw_bytes.data() + 0x0C);
  m.height = ReadLE16(m.raw_bytes.data() + 0x0E);
  m.frame_count = ReadLE32(m.raw_bytes.data() + 0x10);
  m.timescale = ReadLE32(m.raw_bytes.data() + 0x18);
  m.timebase = ReadLE32(m.raw_bytes.data() + 0x1C);
  m.cursor = mvhd_size;  // start scanning after MVhd
  m.open = true;
  REXLOG_INFO("[VP6] OpenFile: '{}' OK  size={}  {}x{}  frames={}  fps={:.3f}", path,
              m.raw_bytes.size(), m.width, m.height, m.frame_count,
              m.timebase ? double(m.timescale) / double(m.timebase) : 0.0);
  return true;
}

// Pull next MV0K/MV0F packet from the parsed movie. On success, sets
// out_data/out_size to a pointer INTO the in-memory file (valid until next
// OpenFile/CloseFile) and out_is_key. Returns 0 on EOF.
int Skate3VP6_NextPacket(const uint8_t** out_data, int* out_size, int* out_is_key) {
  auto& m = Movie();
  std::lock_guard<std::mutex> g(m.lock);
  if (!m.open) {
    return 0;
  }
  while (m.cursor + 8 <= m.raw_bytes.size()) {
    const uint8_t* p = m.raw_bytes.data() + m.cursor;
    uint32_t fcc = Read4CC(p);
    uint32_t chunk_size = ReadLE32(p + 4);
    if (chunk_size < 8 || m.cursor + chunk_size > m.raw_bytes.size()) {
      REXLOG_WARN("[VP6] NextPacket: bad chunk at offset {} fcc={:08X} size={}",
                  m.cursor, fcc, chunk_size);
      return 0;
    }
    if (fcc == kFourCC_MV0K || fcc == kFourCC_MV0F) {
      *out_data = p + 8;
      *out_size = static_cast<int>(chunk_size - 8);
      *out_is_key = (fcc == kFourCC_MV0K) ? 1 : 0;
      m.cursor += chunk_size;
      return 1;
    }
    m.cursor += chunk_size;
  }
  return 0;  // EOF
}

void Skate3VP6_CloseFile() {
  auto& m = Movie();
  std::lock_guard<std::mutex> g(m.lock);
  m.raw_bytes.clear();
  m.cursor = 0;
  m.width = m.height = m.frame_count = m.timescale = m.timebase = 0;
  m.open = false;
}

void Skate3VP6_GetMovieInfo(uint32_t* w, uint32_t* h, uint32_t* frame_count) {
  auto& m = Movie();
  std::lock_guard<std::mutex> g(m.lock);
  if (w) *w = m.width;
  if (h) *h = m.height;
  if (frame_count) *frame_count = m.frame_count;
}

}  // extern "C" (close the OpenFile/NextPacket/CloseFile/GetMovieInfo block)

// ----------------------------------------------------------------------------
// Persistent intro playback API. This is what hooks.cpp / the d3d12 IssueSwap
// hook talks to.
//
// Usage:
//   Skate3VP6_StartPlayback("path/to/intro.vp6")
//   ...each frame...
//     Skate3VP6_AdvanceFrame()    // -> 1 if a new frame is in the buffer,
//                                 //    0 on EOF (and frees resources)
//   const uint8_t* rgba = Skate3VP6_CurrentRgba();   // valid until next
//                                                     // AdvanceFrame
//   Skate3VP6_GetCurrentSize(&w, &h);
//
// Threading: the buffer is owned by a single playback thread; the renderer
// reads it from the swap thread. We snapshot on AdvanceFrame and the
// renderer copies-out under a mutex.

namespace {

struct Playback {
  std::mutex lock;
  std::vector<uint8_t> rgba_current;   // size = width*height*4 (BGRA, alpha=FF)
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t frame_idx = 0;
  uint32_t total_frames = 0;
  bool active = false;
  bool eof = false;
};

Playback& Pb() {
  static Playback p;
  return p;
}

}  // namespace

extern "C" {

bool Skate3VP6_StartPlayback(const char* path) {
  auto& p = Pb();
  std::lock_guard<std::mutex> g(p.lock);
  if (p.active) {
    Skate3VP6_CloseFile();
    p.active = false;
    p.eof = false;
    p.rgba_current.clear();
    p.width = p.height = p.frame_idx = p.total_frames = 0;
  }
  if (!Skate3VP6_OpenFile(path)) {
    return false;
  }
  uint32_t w, h, fc;
  Skate3VP6_GetMovieInfo(&w, &h, &fc);
  if (w == 0 || h == 0) {
    Skate3VP6_CloseFile();
    return false;
  }
  p.width = w;
  p.height = h;
  p.total_frames = fc;
  p.frame_idx = 0;
  p.eof = false;
  p.active = true;
  p.rgba_current.assign(static_cast<size_t>(w) * h * 4, 0);
  Skate3VP6_Reset();
  REXLOG_INFO("[VP6] StartPlayback '{}' OK -- {}x{} frames={}", path, w, h, fc);
  return true;
}

// Decodes the next packet. Returns:
//   1: new frame is now in the rgba_current buffer
//   0: EOF reached, no new frame (subsequent calls also return 0)
int Skate3VP6_AdvanceFrame() {
  auto& p = Pb();
  std::lock_guard<std::mutex> g(p.lock);
  if (!p.active || p.eof) {
    return 0;
  }
  const uint8_t* pkt_data = nullptr;
  int pkt_size = 0;
  int pkt_key = 0;
  if (!Skate3VP6_NextPacket(&pkt_data, &pkt_size, &pkt_key)) {
    p.eof = true;
    return 0;
  }
  if (Skate3VP6_DecodeFrame(pkt_data, pkt_size, p.rgba_current.data(),
                             static_cast<int>(p.width),
                             static_cast<int>(p.height))) {
    p.frame_idx++;
    return 1;
  }
  // decode failed -- treat as EOF
  REXLOG_WARN("[VP6] AdvanceFrame: decode failed at frame {} -- EOF", p.frame_idx);
  p.eof = true;
  return 0;
}

bool Skate3VP6_PlaybackActive() {
  auto& p = Pb();
  return p.active && !p.eof;
}

bool Skate3VP6_PlaybackEof() {
  auto& p = Pb();
  return p.eof;
}

void Skate3VP6_GetCurrentSize(uint32_t* w, uint32_t* h) {
  auto& p = Pb();
  if (w) *w = p.width;
  if (h) *h = p.height;
}

uint32_t Skate3VP6_GetFrameIndex() { return Pb().frame_idx; }

// Copies the current frame's RGBA into a caller buffer. Returns bytes copied.
// max_bytes prevents overrun. Thread-safe.
int Skate3VP6_CopyCurrentRgba(uint8_t* dst, int max_bytes) {
  auto& p = Pb();
  std::lock_guard<std::mutex> g(p.lock);
  if (!p.active || p.rgba_current.empty()) {
    return 0;
  }
  int n = static_cast<int>(p.rgba_current.size());
  if (n > max_bytes) n = max_bytes;
  std::memcpy(dst, p.rgba_current.data(), static_cast<size_t>(n));
  return n;
}

// One-shot test: open EA_Blackbox file, walk all packets, decode each via
// libavcodec, log how many succeed. Returns count of decoded frames.
int Skate3VP6_TestDecode(const char* path) {
  if (!Skate3VP6_OpenFile(path)) {
    return 0;
  }
  uint32_t w, h, fc;
  Skate3VP6_GetMovieInfo(&w, &h, &fc);
  if (w == 0 || h == 0) {
    REXLOG_ERROR("[VP6] TestDecode: width/height zero");
    Skate3VP6_CloseFile();
    return 0;
  }
  // Allocate one RGBA frame buffer.
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4);
  int decoded = 0;
  int failed = 0;
  const uint8_t* pkt_data = nullptr;
  int pkt_size = 0;
  int pkt_key = 0;
  while (Skate3VP6_NextPacket(&pkt_data, &pkt_size, &pkt_key)) {
    if (Skate3VP6_DecodeFrame(pkt_data, pkt_size, rgba.data(), int(w), int(h))) {
      ++decoded;
    } else {
      ++failed;
    }
  }
  REXLOG_INFO("[VP6] TestDecode: '{}' decoded={} failed={} expected={}", path, decoded, failed,
              fc);
  Skate3VP6_CloseFile();
  return decoded;
}

}  // extern "C"
