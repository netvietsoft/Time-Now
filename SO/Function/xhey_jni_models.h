#pragma once

/**
 * ============================================================================
 * File: include/xhey_jni_models.h
 * Project: Timemark (com.oceangalaxy.camera.new v10.0.281) Native C++17 SDK
 * ============================================================================
 * Master header defining all C++17 data structures, context handles, JNI
 * reflection marshaling helpers, FIPS 180-4 SHA-256 / RFC 2104 HMAC-SHA256,
 * IEEE 802.3 CRC-32, safe multi-format AndroidBitmap accessors, and vision
 * post-processing utilities shared across:
 *   1. src/jpeg_packer_jni.cpp            (JpegPacker)
 *   2. src/code_detect_anticode_jni.cpp   (CodeDetect, AiRef)
 *   3. src/edge_ai_ncnn_jni.cpp           (NCNN & MediaPipe Edge AI classes)
 *   4. src/aegis_anti_spoof_jni.cpp       (AegisCaptureSession, AegisFrame)
 *   5. src/mcap_robotics_jni.cpp          (McapRecorder, McapDemuxer)
 *   6. src/xhey_media_and_security_jni.cpp(NativeMediaSDK, VideoEdit, JNIUtils)
 * ============================================================================
 */

#include <jni.h>
#include <android/log.h>
#include <android/bitmap.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <numeric>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#define XHEY_LOG_TAG "XHNNpackage-Native"
#ifndef LOGI
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  XHEY_LOG_TAG, __VA_ARGS__)
#endif
#ifndef LOGW
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  XHEY_LOG_TAG, __VA_ARGS__)
#endif
#ifndef LOGE
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, XHEY_LOG_TAG, __VA_ARGS__)
#endif

namespace xhey {
namespace jni {

// ============================================================================
// 1. FIPS 180-4 SHA-256, RFC 2104 HMAC-SHA256 & IEEE 802.3 CRC-32
// ============================================================================

struct Sha256Engine {
    uint32_t state[8];
    uint64_t bitlen = 0;
    uint8_t buffer[64];
    size_t buflen = 0;

    static constexpr uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
        0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
        0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
        0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
        0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
        0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
        0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
        0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
        0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
        0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
        0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
    };

    static inline uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32u - n));
    }

    Sha256Engine() { reset(); }

    void reset() {
        state[0] = 0x6a09e667u;
        state[1] = 0xbb67ae85u;
        state[2] = 0x3c6ef372u;
        state[3] = 0xa54ff53au;
        state[4] = 0x510e527fu;
        state[5] = 0x9b05688cu;
        state[6] = 0x1f83d9abu;
        state[7] = 0x5be0cd19u;
        bitlen = 0;
        buflen = 0;
        std::memset(buffer, 0, sizeof(buffer));
    }

    void transform(const uint8_t data[64]) {
        uint32_t m[64];
        for (int i = 0; i < 16; ++i) {
            m[i] = (static_cast<uint32_t>(data[i * 4 + 0]) << 24) |
                   (static_cast<uint32_t>(data[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(data[i * 4 + 2]) << 8)  |
                   (static_cast<uint32_t>(data[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(m[i - 15], 7) ^ rotr(m[i - 15], 18) ^ (m[i - 15] >> 3);
            uint32_t s1 = rotr(m[i - 2], 17) ^ rotr(m[i - 2], 19) ^ (m[i - 2] >> 10);
            m[i] = m[i - 16] + s0 + m[i - 7] + s1;
        }
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
        uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + S1 + ch + K[i] + m[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d;
        state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }

    void update(const void* ptr, size_t len) {
        if (!ptr || len == 0) return;
        const auto* data = static_cast<const uint8_t*>(ptr);
        for (size_t i = 0; i < len; ++i) {
            buffer[buflen++] = data[i];
            if (buflen == 64) {
                transform(buffer);
                bitlen += 512;
                buflen = 0;
            }
        }
    }

    void final(uint8_t hash[32]) {
        size_t i = buflen;
        if (buflen < 56) {
            buffer[i++] = 0x80u;
            while (i < 56) buffer[i++] = 0x00u;
        } else {
            buffer[i++] = 0x80u;
            while (i < 64) buffer[i++] = 0x00u;
            transform(buffer);
            std::memset(buffer, 0, 56);
        }
        bitlen += static_cast<uint64_t>(buflen) * 8ULL;
        for (int j = 0; j < 8; ++j) {
            buffer[63 - j] = static_cast<uint8_t>((bitlen >> (j * 8)) & 0xffu);
        }
        transform(buffer);
        for (int j = 0; j < 8; ++j) {
            hash[j * 4 + 0] = static_cast<uint8_t>((state[j] >> 24) & 0xffu);
            hash[j * 4 + 1] = static_cast<uint8_t>((state[j] >> 16) & 0xffu);
            hash[j * 4 + 2] = static_cast<uint8_t>((state[j] >> 8)  & 0xffu);
            hash[j * 4 + 3] = static_cast<uint8_t>(state[j] & 0xffu);
        }
    }
};

inline void ComputeSha256(const void* data, size_t len, uint8_t out[32]) {
    Sha256Engine eng;
    eng.update(data, len);
    eng.final(out);
}

inline void ComputeHmacSha256(
        const void* keyPtr, size_t keyLen,
        const void* msgPtr, size_t msgLen,
        uint8_t outMac[32]) {
    uint8_t kPad[64];
    std::memset(kPad, 0, sizeof(kPad));
    if (keyLen > 64) {
        ComputeSha256(keyPtr, keyLen, kPad);
    } else if (keyPtr && keyLen > 0) {
        std::memcpy(kPad, keyPtr, keyLen);
    }

    uint8_t iKeyPad[64];
    uint8_t oKeyPad[64];
    for (int i = 0; i < 64; ++i) {
        iKeyPad[i] = static_cast<uint8_t>(kPad[i] ^ 0x36u);
        oKeyPad[i] = static_cast<uint8_t>(kPad[i] ^ 0x5cu);
    }

    uint8_t innerHash[32];
    Sha256Engine inner;
    inner.update(iKeyPad, 64);
    inner.update(msgPtr, msgLen);
    inner.final(innerHash);

    Sha256Engine outer;
    outer.update(oKeyPad, 64);
    outer.update(innerHash, 32);
    outer.final(outMac);
}

inline uint32_t ComputeCrc32(const void* data, size_t len, uint32_t initCrc = 0u) {
    uint32_t crc = ~initCrc;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        crc ^= bytes[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(crc & 1u)));
        }
    }
    return ~crc;
}

inline std::string BytesToHex(const uint8_t* data, size_t len) {
    static constexpr char kHex[17] = "0123456789abcdef";
    std::string out(len * 2, '0');
    for (size_t i = 0; i < len; ++i) {
        out[i * 2]     = kHex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

// ============================================================================
// 2. Safe Multi-Format AndroidBitmap Pixel Reader / Writer
//    Handles RGBA_8888, RGB_565, RGBA_4444, and A_8 safely with bounds checks
// ============================================================================

inline bool IsSupportedBitmapFormat(int32_t format) {
    return format == ANDROID_BITMAP_FORMAT_RGBA_8888 ||
           format == ANDROID_BITMAP_FORMAT_RGB_565 ||
           format == ANDROID_BITMAP_FORMAT_RGBA_4444 ||
           format == ANDROID_BITMAP_FORMAT_A_8;
}

inline bool ReadBitmapPixelRgba(
        const AndroidBitmapInfo& info,
        const void* pixels,
        int x, int y,
        uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    if (!pixels || info.width == 0 || info.height == 0) {
        r = g = b = 0; a = 255;
        return false;
    }
    int cx = std::clamp(x, 0, static_cast<int>(info.width) - 1);
    int cy = std::clamp(y, 0, static_cast<int>(info.height) - 1);
    const auto* row = static_cast<const uint8_t*>(pixels) + static_cast<size_t>(cy) * info.stride;

    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        const uint8_t* p = row + static_cast<size_t>(cx) * 4;
        r = p[0];
        g = p[1];
        b = p[2];
        a = p[3];
        return true;
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        uint16_t v = 0;
        std::memcpy(&v, row + static_cast<size_t>(cx) * 2, sizeof(uint16_t));
        uint8_t r5 = static_cast<uint8_t>((v >> 11) & 0x1Fu);
        uint8_t g6 = static_cast<uint8_t>((v >> 5)  & 0x3Fu);
        uint8_t b5 = static_cast<uint8_t>(v & 0x1Fu);
        r = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
        g = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
        b = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
        a = 255;
        return true;
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGBA_4444) {
        uint16_t v = 0;
        std::memcpy(&v, row + static_cast<size_t>(cx) * 2, sizeof(uint16_t));
        uint8_t r4 = static_cast<uint8_t>((v >> 12) & 0x0Fu);
        uint8_t g4 = static_cast<uint8_t>((v >> 8)  & 0x0Fu);
        uint8_t b4 = static_cast<uint8_t>((v >> 4)  & 0x0Fu);
        uint8_t a4 = static_cast<uint8_t>(v & 0x0Fu);
        r = static_cast<uint8_t>((r4 << 4) | r4);
        g = static_cast<uint8_t>((g4 << 4) | g4);
        b = static_cast<uint8_t>((b4 << 4) | b4);
        a = static_cast<uint8_t>((a4 << 4) | a4);
        return true;
    } else if (info.format == ANDROID_BITMAP_FORMAT_A_8) {
        uint8_t gray = row[cx];
        r = g = b = gray;
        a = 255;
        return true;
    }
    r = g = b = 0;
    a = 255;
    return false;
}

inline int ReadBitmapPixelLuma(
        const AndroidBitmapInfo& info,
        const void* pixels,
        int x, int y) {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    ReadBitmapPixelRgba(info, pixels, x, y, r, g, b, a);
    return (static_cast<int>(r) * 77 + static_cast<int>(g) * 150 + static_cast<int>(b) * 29) >> 8;
}

inline void WriteBitmapPixelRgba(
        const AndroidBitmapInfo& info,
        void* pixels,
        int x, int y,
        uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!pixels || x < 0 || y < 0 ||
        x >= static_cast<int>(info.width) || y >= static_cast<int>(info.height)) {
        return;
    }
    auto* row = static_cast<uint8_t*>(pixels) + static_cast<size_t>(y) * info.stride;
    if (info.format == ANDROID_BITMAP_FORMAT_RGBA_8888) {
        uint8_t* p = row + static_cast<size_t>(x) * 4;
        p[0] = r; p[1] = g; p[2] = b; p[3] = a;
    } else if (info.format == ANDROID_BITMAP_FORMAT_RGB_565) {
        uint16_t v = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        std::memcpy(row + static_cast<size_t>(x) * 2, &v, sizeof(uint16_t));
    } else if (info.format == ANDROID_BITMAP_FORMAT_A_8) {
        row[x] = static_cast<uint8_t>((static_cast<int>(r) * 77 + static_cast<int>(g) * 150 + static_cast<int>(b) * 29) >> 8);
    }
}

// ============================================================================
// 3. Native C++ Mirrors of Java Data Classes in com.xhey.xcamerasdk.algorithm.nn
// ============================================================================

struct NativeDetectionInfo {
    int32_t label = 0;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    float area = 0.0f;
    float score = 0.0f;
    std::string labelName;
};

using DetectionInfoNative = NativeDetectionInfo;

struct NativeSliceDataInfo {
    int64_t errorCode = 0;
    int64_t position = 1; // 1 = JPEG_APPEND_END, 0 = JPEG_APP_N, 2 = AUTO
    int64_t version = 1;  // 1 = Plain, 2 = Encrypted
    int64_t id = 0;
    int64_t type = 255;
    float rect[4] = {0.f, 0.f, 1.f, 1.f};
    std::vector<uint8_t> data;
};

struct NativeCodeMetaData {
    int32_t valid = 0;
    int32_t version = 6;
    int32_t timestampFormat = 0;
    int64_t timeStampS = 0;
    double longitude = 0.0;
    double latitude = 0.0;
    int32_t locationType = 0;
    int32_t reserved = 0;
};

struct NativeHandLandmarkInfo {
    float score = 0.0f;
    float handedness = 1.0f;
    float box[4] = {0.f, 0.f, 0.f, 0.f};
    float keypoints[42] = {0.f}; // 21 landmarks * (x, y)
    float presence = 1.0f;
};

struct NativePlateResult {
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    std::string label;
    float score = 0.f;
    std::string color;
};

struct NativeOCRInfoData {
    float score = 0.f;
    float rect[8] = {0.f}; // 4-corner polygon (x0,y0, x1,y1, x2,y2, x3,y3)
    std::string text;
    std::vector<int32_t> wordIndex;
};

struct NativePaddleOCRParam {
    bool justRec = false;
    int32_t boxLimit = 1000;
    bool justMaxBox = false;
    bool isNormOne = true;
    float rectOfInterest[4] = {0.0f, 0.0f, 1.0f, 1.0f};
};

struct NativeImageDetectionParam {
    int32_t netW = 640;
    int32_t netH = 640;
    float probThreshold = 0.5f;
    float nmsThreshold = 0.45f;
    int32_t encrypt = 1;
    std::vector<std::string> labels;
    std::vector<float> labelProbThresholds;
};

using ImageDetectionParamNative = NativeImageDetectionParam;

struct NativePhotoStandardsParam {
    int32_t cameraDirect = 0;
    std::vector<int32_t> photoStandards;
};

struct NativePhotoStandardsResult {
    std::vector<int32_t> id;
    std::vector<int32_t>& ids = id; // Alias so both res.id and res.ids compile cleanly
    bool isFakeDetect = false;

    NativePhotoStandardsResult() = default;
    NativePhotoStandardsResult(const NativePhotoStandardsResult& other)
        : id(other.id), ids(id), isFakeDetect(other.isFakeDetect) {}
    NativePhotoStandardsResult& operator=(const NativePhotoStandardsResult& other) {
        if (this != &other) {
            id = other.id;
            isFakeDetect = other.isFakeDetect;
        }
        return *this;
    }
};

// ============================================================================
// 4. Aegis Anti-Spoofing Session & Frame Contexts
// ============================================================================

struct AegisFrameContext {
    uint64_t session_id = 0;
    uint32_t frame_index = 0;
    int32_t width = 1920;
    int32_t height = 1080;
    int32_t format = 0x22;
    uint32_t texture_id = 0;
    uint32_t& gl_texture_id = texture_id;
    bool texture_imported = false;
    bool is_verified = true;
    EGLContext owner_egl_context = EGL_NO_CONTEXT;
    int64_t timestamp_ms = 0;
    int64_t timestamp_ns = 0;
    bool has_hardware_buffer = true;
    bool is_frozen_jpeg = false;
    bool has_decoded_frozen_jpeg = false;
    uint64_t bound_session_nonce = 0;
    uint8_t frame_sha256[32] = {0};
    uint8_t (&frame_digest)[32] = frame_sha256;
    std::vector<uint8_t> rgba_pixels;
    std::vector<uint8_t> frozen_jpeg_bytes;

    AegisFrameContext() = default;
    AegisFrameContext(const AegisFrameContext& o)
        : session_id(o.session_id), frame_index(o.frame_index),
          width(o.width), height(o.height), format(o.format),
          texture_id(o.texture_id), texture_imported(o.texture_imported),
          is_verified(o.is_verified), owner_egl_context(o.owner_egl_context),
          timestamp_ms(o.timestamp_ms), timestamp_ns(o.timestamp_ns),
          has_hardware_buffer(o.has_hardware_buffer), is_frozen_jpeg(o.is_frozen_jpeg),
          has_decoded_frozen_jpeg(o.has_decoded_frozen_jpeg),
          bound_session_nonce(o.bound_session_nonce),
          rgba_pixels(o.rgba_pixels), frozen_jpeg_bytes(o.frozen_jpeg_bytes) {
        std::memcpy(frame_sha256, o.frame_sha256, 32);
    }
};

struct AegisCaptureSessionContext {
    uint64_t session_id = 0;
    int32_t camera_id = 0;
    bool is_front_camera = false;
    int32_t shoot_mode = 0;
    int32_t& capture_mode = shoot_mode;
    int32_t image_width = 1920;
    int32_t image_height = 1080;
    int64_t ntp_base_time_ms = 0;
    int64_t& ntp_timestamp_ms = ntp_base_time_ms;
    int64_t monotonic_base_ns = 0;
    int64_t& sensor_timestamp_ns = monotonic_base_ns;
    int64_t shutter_click_time_ns = 0;
    uint64_t session_nonce = 0;
    int32_t initial_time_status = 0;
    uint32_t bound_frame_count = 0;
    uint32_t jpeg_encoded_size = 0;
    bool is_sealed = false;
    uint8_t rolling_sha256[32] = {0};
    std::string attestation_signature_json;
    ANativeWindow* input_window = nullptr;
    AegisFrameContext* active_frame = nullptr;
};

// ============================================================================
// 5. Foxglove MCAP Robotics Container Contexts
// ============================================================================

struct McapIndexedPacket {
    uint64_t file_offset = 0;
    uint32_t byte_length = 0;
    int64_t timestamp_ns = 0;
    uint32_t flags = 0;
    bool is_keyframe = false;
    std::vector<uint8_t> payload;
};

struct McapAttachmentRecord {
    std::string name;
    std::string media_type;
    std::vector<uint8_t> data;
};

struct McapRecorderContext {
    std::string output_path;
    std::string session_id;
    int32_t color_width = 1920;
    int32_t color_height = 1080;
    int32_t depth_width = 256;
    int32_t depth_height = 192;
    int32_t audio_sample_rate = 16000;
    int32_t audio_channels = 1;
    int32_t audio_source_kind = 0;
    bool audio_aec = false;
    bool audio_ns = false;
    bool audio_agc = false;
    bool is_hevc = true;

    FILE* fp = nullptr;
    bool is_recording = false;
    bool write_failed = false;

    uint32_t sequence_counter = 0;
    uint64_t bytes_written = 0;
    uint64_t message_count = 0;
    uint64_t video_frame_count = 0;
    uint64_t audio_block_count = 0;
    uint64_t imu_sample_count = 0;

    std::vector<uint8_t> csd_bytes;
    std::vector<McapIndexedPacket> video_index;
    std::vector<McapIndexedPacket> audio_index;
    std::vector<McapAttachmentRecord> attachments;
};

struct McapDemuxerContext {
    std::string file_path;
    FILE* fp = nullptr;
    int32_t video_width = 1920;
    int32_t video_height = 1080;
    int32_t video_fps = 30;
    bool is_hevc = true;
    int32_t audio_sample_rate = 16000;
    int32_t audio_channels = 1;
    int64_t duration_ns = 0;
    size_t total_frames = 0;

    std::vector<int64_t> frame_times_ns;
    std::vector<int64_t> audio_times_ns;
    std::vector<int32_t> keyframe_indices;
    std::vector<uint8_t> csd_bytes;
    std::vector<McapIndexedPacket> video_packets;
    std::vector<McapIndexedPacket> audio_packets;
};

// ============================================================================
// 6. JNI Array & Object Conversion Helpers
// ============================================================================

inline std::string JStringToStdString(JNIEnv* env, jstring jstr) {
    if (!env || !jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    if (!chars) return "";
    std::string result(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return result;
}

inline std::vector<uint8_t> JByteArrayToStdVector(JNIEnv* env, jbyteArray jarr) {
    if (!env || !jarr) return {};
    jsize len = env->GetArrayLength(jarr);
    if (len <= 0) return {};
    std::vector<uint8_t> out(static_cast<size_t>(len));
    env->GetByteArrayRegion(jarr, 0, len, reinterpret_cast<jbyte*>(out.data()));
    return out;
}

inline jbyteArray StdVectorToJByteArray(JNIEnv* env, const std::vector<uint8_t>& vec) {
    if (!env) return nullptr;
    jbyteArray arr = env->NewByteArray(static_cast<jsize>(vec.size()));
    if (!arr) return nullptr;
    if (!vec.empty()) {
        env->SetByteArrayRegion(arr, 0, static_cast<jsize>(vec.size()),
                                reinterpret_cast<const jbyte*>(vec.data()));
    }
    return arr;
}

inline jfloatArray StdVectorToJFloatArray(JNIEnv* env, const std::vector<float>& vec) {
    if (!env) return nullptr;
    jfloatArray arr = env->NewFloatArray(static_cast<jsize>(vec.size()));
    if (!arr) return nullptr;
    if (!vec.empty()) {
        env->SetFloatArrayRegion(arr, 0, static_cast<jsize>(vec.size()), vec.data());
    }
    return arr;
}

inline jintArray StdVectorToJIntArray(JNIEnv* env, const std::vector<int32_t>& vec) {
    if (!env) return nullptr;
    jintArray arr = env->NewIntArray(static_cast<jsize>(vec.size()));
    if (!arr) return nullptr;
    if (!vec.empty()) {
        env->SetIntArrayRegion(arr, 0, static_cast<jsize>(vec.size()),
                              reinterpret_cast<const jint*>(vec.data()));
    }
    return arr;
}

inline NativeImageDetectionParam ParseImageDetectionParam(JNIEnv* env, jobject paramObj) {
    NativeImageDetectionParam out{};
    if (!env || !paramObj) return out;
    jclass cls = env->GetObjectClass(paramObj);
    if (!cls) return out;

    if (jfieldID fid = env->GetFieldID(cls, "netW", "I")) out.netW = env->GetIntField(paramObj, fid);
    if (jfieldID fid = env->GetFieldID(cls, "netH", "I")) out.netH = env->GetIntField(paramObj, fid);
    if (jfieldID fid = env->GetFieldID(cls, "probThreshold", "F")) out.probThreshold = env->GetFloatField(paramObj, fid);
    if (jfieldID fid = env->GetFieldID(cls, "nmsThreshold", "F")) out.nmsThreshold = env->GetFloatField(paramObj, fid);
    if (jfieldID fid = env->GetFieldID(cls, "encrypt", "I")) out.encrypt = env->GetIntField(paramObj, fid);

    if (jfieldID fidLabels = env->GetFieldID(cls, "labels", "[Ljava/lang/String;")) {
        auto arr = static_cast<jobjectArray>(env->GetObjectField(paramObj, fidLabels));
        if (arr) {
            jsize n = env->GetArrayLength(arr);
            out.labels.reserve(static_cast<size_t>(n));
            for (jsize i = 0; i < n; ++i) {
                auto js = static_cast<jstring>(env->GetObjectArrayElement(arr, i));
                out.labels.push_back(JStringToStdString(env, js));
                if (js) env->DeleteLocalRef(js);
            }
            env->DeleteLocalRef(arr);
        }
    }

    if (jfieldID fidProbs = env->GetFieldID(cls, "labelProbThresholds", "[F")) {
        auto arr = static_cast<jfloatArray>(env->GetObjectField(paramObj, fidProbs));
        if (arr) {
            jsize n = env->GetArrayLength(arr);
            if (n > 0) {
                out.labelProbThresholds.resize(static_cast<size_t>(n));
                env->GetFloatArrayRegion(arr, 0, n, out.labelProbThresholds.data());
            }
            env->DeleteLocalRef(arr);
        }
    }
    env->DeleteLocalRef(cls);
    return out;
}

inline NativePaddleOCRParam ParsePaddleOCRParam(JNIEnv* env, jobject paramObj) {
    NativePaddleOCRParam out{};
    if (!env || !paramObj) return out;
    jclass cls = env->GetObjectClass(paramObj);
    if (!cls) return out;

    if (jfieldID fid = env->GetFieldID(cls, "justRec", "Z")) out.justRec = (env->GetBooleanField(paramObj, fid) == JNI_TRUE);
    if (jfieldID fid = env->GetFieldID(cls, "boxLimit", "I")) out.boxLimit = env->GetIntField(paramObj, fid);
    if (jfieldID fid = env->GetFieldID(cls, "justMaxBox", "Z")) out.justMaxBox = (env->GetBooleanField(paramObj, fid) == JNI_TRUE);
    if (jfieldID fid = env->GetFieldID(cls, "isNormOne", "Z")) out.isNormOne = (env->GetBooleanField(paramObj, fid) == JNI_TRUE);

    if (jfieldID fidRoi = env->GetFieldID(cls, "rectOfInterest", "[F")) {
        auto arr = static_cast<jfloatArray>(env->GetObjectField(paramObj, fidRoi));
        if (arr) {
            if (env->GetArrayLength(arr) >= 4) {
                env->GetFloatArrayRegion(arr, 0, 4, out.rectOfInterest);
            }
            env->DeleteLocalRef(arr);
        }
    }
    env->DeleteLocalRef(cls);
    return out;
}

inline NativePhotoStandardsParam ParsePhotoStandardsParam(JNIEnv* env, jobject paramObj) {
    NativePhotoStandardsParam out{};
    if (!env || !paramObj) return out;
    jclass cls = env->GetObjectClass(paramObj);
    if (!cls) return out;

    if (jfieldID fid = env->GetFieldID(cls, "cameraDirect", "I")) {
        out.cameraDirect = env->GetIntField(paramObj, fid);
    }
    if (jfieldID fidArr = env->GetFieldID(cls, "photoStandards", "[I")) {
        auto arr = static_cast<jintArray>(env->GetObjectField(paramObj, fidArr));
        if (arr) {
            jsize n = env->GetArrayLength(arr);
            if (n > 0) {
                out.photoStandards.resize(static_cast<size_t>(n));
                env->GetIntArrayRegion(arr, 0, n, reinterpret_cast<jint*>(out.photoStandards.data()));
            }
            env->DeleteLocalRef(arr);
        }
    }
    env->DeleteLocalRef(cls);
    return out;
}

inline jobject CreateJavaDetectionInfo(JNIEnv* env, const NativeDetectionInfo& d) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/DetectionInfo");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(IFFFFFF)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    jobject obj = env->NewObject(cls, ctor, d.label, d.x0, d.y0, d.x1, d.y1, d.area, d.score);
    if (!d.labelName.empty() && obj) {
        jfieldID fidName = env->GetFieldID(cls, "labelName", "Ljava/lang/String;");
        if (fidName) {
            jstring jname = env->NewStringUTF(d.labelName.c_str());
            env->SetObjectField(obj, fidName, jname);
            env->DeleteLocalRef(jname);
        }
    }
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobjectArray CreateJavaDetectionInfoArray(JNIEnv* env, const std::vector<NativeDetectionInfo>& items) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/DetectionInfo");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(items.size()), cls, nullptr);
    for (size_t i = 0; i < items.size(); ++i) {
        jobject obj = CreateJavaDetectionInfo(env, items[i]);
        env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
        if (obj) env->DeleteLocalRef(obj);
    }
    env->DeleteLocalRef(cls);
    return arr;
}

inline jobjectArray CreateDetectionInfoArray(JNIEnv* env, const std::vector<NativeDetectionInfo>& items) {
    return CreateJavaDetectionInfoArray(env, items);
}

inline jobject CreateJavaHandLandmarkInfo(JNIEnv* env, const NativeHandLandmarkInfo& h) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/HandLandmarkInfo");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(FF[F[FF)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    jfloatArray boxArr = env->NewFloatArray(4);
    env->SetFloatArrayRegion(boxArr, 0, 4, h.box);

    jfloatArray kpArr = env->NewFloatArray(42);
    env->SetFloatArrayRegion(kpArr, 0, 42, h.keypoints);

    jobject obj = env->NewObject(cls, ctor, h.score, h.handedness, boxArr, kpArr, h.presence);
    env->DeleteLocalRef(boxArr);
    env->DeleteLocalRef(kpArr);
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobjectArray CreateJavaHandLandmarkInfoArray(JNIEnv* env, const std::vector<NativeHandLandmarkInfo>& items) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/HandLandmarkInfo");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(items.size()), cls, nullptr);
    for (size_t i = 0; i < items.size(); ++i) {
        jobject obj = CreateJavaHandLandmarkInfo(env, items[i]);
        env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
        if (obj) env->DeleteLocalRef(obj);
    }
    env->DeleteLocalRef(cls);
    return arr;
}

inline jobjectArray CreateHandLandmarkInfoArray(JNIEnv* env, const std::vector<NativeHandLandmarkInfo>& items) {
    return CreateJavaHandLandmarkInfoArray(env, items);
}

inline jobject CreateJavaPlateResult(JNIEnv* env, const NativePlateResult& p) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/PlateResult");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(FFFFLjava/lang/String;FLjava/lang/String;)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    jstring jlabel = env->NewStringUTF(p.label.c_str());
    jstring jcolor = env->NewStringUTF(p.color.c_str());
    jobject obj = env->NewObject(cls, ctor, p.x0, p.y0, p.x1, p.y1, jlabel, p.score, jcolor);
    env->DeleteLocalRef(jlabel);
    env->DeleteLocalRef(jcolor);
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobjectArray CreateJavaPlateResultArray(JNIEnv* env, const std::vector<NativePlateResult>& items) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/PlateResult");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(items.size()), cls, nullptr);
    for (size_t i = 0; i < items.size(); ++i) {
        jobject obj = CreateJavaPlateResult(env, items[i]);
        env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
        if (obj) env->DeleteLocalRef(obj);
    }
    env->DeleteLocalRef(cls);
    return arr;
}

inline jobjectArray CreatePlateResultArray(JNIEnv* env, const std::vector<NativePlateResult>& items) {
    return CreateJavaPlateResultArray(env, items);
}

inline jobject CreateJavaOCRInfoData(JNIEnv* env, const NativeOCRInfoData& o) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/OCRInfoData");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(F[FLjava/lang/String;[I)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    jfloatArray rectArr = env->NewFloatArray(8);
    env->SetFloatArrayRegion(rectArr, 0, 8, o.rect);

    jstring jtext = env->NewStringUTF(o.text.c_str());
    jintArray wordArr = StdVectorToJIntArray(env, o.wordIndex);

    jobject obj = env->NewObject(cls, ctor, o.score, rectArr, jtext, wordArr);
    env->DeleteLocalRef(rectArr);
    env->DeleteLocalRef(jtext);
    if (wordArr) env->DeleteLocalRef(wordArr);
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobjectArray CreateJavaOCRInfoDataArray(JNIEnv* env, const std::vector<NativeOCRInfoData>& items) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/OCRInfoData");
    if (!cls) return nullptr;
    jobjectArray arr = env->NewObjectArray(static_cast<jsize>(items.size()), cls, nullptr);
    for (size_t i = 0; i < items.size(); ++i) {
        jobject obj = CreateJavaOCRInfoData(env, items[i]);
        env->SetObjectArrayElement(arr, static_cast<jsize>(i), obj);
        if (obj) env->DeleteLocalRef(obj);
    }
    env->DeleteLocalRef(cls);
    return arr;
}

inline jobjectArray CreateOCRInfoDataArray(JNIEnv* env, const std::vector<NativeOCRInfoData>& items) {
    return CreateJavaOCRInfoDataArray(env, items);
}

inline jobject CreateJavaCodeMetaData(JNIEnv* env, const NativeCodeMetaData& m) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/CodeMetaData");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(IIIJDDII)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    jobject obj = env->NewObject(cls, ctor,
        m.valid, m.version, m.timestampFormat, static_cast<jlong>(m.timeStampS),
        m.longitude, m.latitude, m.locationType, m.reserved);
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobject CreateJavaSliceDataInfo(JNIEnv* env, const NativeSliceDataInfo& s) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/SliceDataInfo");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(JJJJJ[F[B)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }

    jfloatArray rectArr = env->NewFloatArray(4);
    env->SetFloatArrayRegion(rectArr, 0, 4, s.rect);

    jbyteArray dataArr = StdVectorToJByteArray(env, s.data);

    jobject obj = env->NewObject(cls, ctor,
        static_cast<jlong>(s.errorCode),
        static_cast<jlong>(s.position),
        static_cast<jlong>(s.version),
        static_cast<jlong>(s.id),
        static_cast<jlong>(s.type),
        rectArr, dataArr);

    env->DeleteLocalRef(rectArr);
    if (dataArr) env->DeleteLocalRef(dataArr);
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobject CreateJavaPhotoStandardsResult(JNIEnv* env, const NativePhotoStandardsResult& res) {
    jclass cls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/PhotoStandardsResult");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "([IZ)V");
    if (!ctor) {
        env->DeleteLocalRef(cls);
        return nullptr;
    }
    jintArray idArr = StdVectorToJIntArray(env, res.id);
    jobject obj = env->NewObject(cls, ctor, idArr, res.isFakeDetect ? JNI_TRUE : JNI_FALSE);
    if (idArr) env->DeleteLocalRef(idArr);
    env->DeleteLocalRef(cls);
    return obj;
}

inline jobject CreatePhotoStandardsResultObject(JNIEnv* env, const NativePhotoStandardsResult& res) {
    return CreateJavaPhotoStandardsResult(env, res);
}

// ============================================================================
// 7. Common Vision Post-Processing (IoU & Non-Maximum Suppression)
// ============================================================================

inline float ComputeIoU(const NativeDetectionInfo& a, const NativeDetectionInfo& b) {
    float interX0 = std::max(a.x0, b.x0);
    float interY0 = std::max(a.y0, b.y0);
    float interX1 = std::min(a.x1, b.x1);
    float interY1 = std::min(a.y1, b.y1);
    float interW = std::max(0.0f, interX1 - interX0);
    float interH = std::max(0.0f, interY1 - interY0);
    float interArea = interW * interH;
    float unionArea = a.area + b.area - interArea;
    return (unionArea > 1e-6f) ? (interArea / unionArea) : 0.0f;
}

inline std::vector<NativeDetectionInfo> ApplyNMS(
        std::vector<NativeDetectionInfo> proposals,
        float nmsThreshold,
        bool classAware = true) {
    std::sort(proposals.begin(), proposals.end(),
              [](const NativeDetectionInfo& a, const NativeDetectionInfo& b) {
                  return a.score > b.score;
              });
    std::vector<NativeDetectionInfo> picked;
    std::vector<bool> suppressed(proposals.size(), false);
    for (size_t i = 0; i < proposals.size(); ++i) {
        if (suppressed[i]) continue;
        picked.push_back(proposals[i]);
        for (size_t j = i + 1; j < proposals.size(); ++j) {
            if (suppressed[j]) continue;
            if (classAware && proposals[i].label != proposals[j].label) continue;
            if (ComputeIoU(proposals[i], proposals[j]) > nmsThreshold) {
                suppressed[j] = true;
            }
        }
    }
    return picked;
}

} // namespace jni

using namespace jni;

} // namespace xhey
