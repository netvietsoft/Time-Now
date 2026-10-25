/**
 * ============================================================================
 * File: src/mcap_robotics_jni.cpp
 * Library: libnnpackage-f99779bd.so
 *
 * Description:
 *   Spec-compliant Foxglove MCAP v0 Binary Container Muxer & Demuxer
 *   (https://mcap.dev/spec) for Multi-Modal Robotics / Ego-Centric AI Dataset
 *   Collection:
 *     - com.xhey.xcamerasdk.mcap.McapRecorder
 *     - com.xhey.xcamerasdk.mcap.McapDemuxer
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"

#include <android/log.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

// Official Foxglove MCAP v0 Magic Bytes: \x89 M C A P 0 \r \n
constexpr uint8_t kMcapMagic[8] = {0x89, 'M', 'C', 'A', 'P', '0', '\r', '\n'};

// Official MCAP v0 Record Opcodes (https://mcap.dev/spec)
constexpr uint8_t kOpHeader     = 0x01;
constexpr uint8_t kOpFooter     = 0x02;
constexpr uint8_t kOpSchema     = 0x03;
constexpr uint8_t kOpChannel    = 0x04;
constexpr uint8_t kOpMessage    = 0x05;
constexpr uint8_t kOpAttachment = 0x09;
constexpr uint8_t kOpMetadata   = 0x0C;
constexpr uint8_t kOpDataEnd    = 0x0F;

// Schema IDs
constexpr uint16_t kSchemaVideoCompressed = 1;
constexpr uint16_t kSchemaRawAudio        = 2;
constexpr uint16_t kSchemaSensorBinary    = 3;
constexpr uint16_t kSchemaJsonTelemetry   = 4;

// Predefined Channel IDs matching McapChannel.java
constexpr uint16_t kChannelVideoCompressed   = 1;  // /camera/color/compressed
constexpr uint16_t kChannelAudioBlock        = 2;  // /audio/raw
constexpr uint16_t kChannelCameraMeta        = 3;  // /camera/metadata
constexpr uint16_t kChannelImu               = 4;  // /imu/data
constexpr uint16_t kChannelMag               = 5;  // /imu/mag
constexpr uint16_t kChannelBaro              = 6;  // /sensor/baro
constexpr uint16_t kChannelLight             = 7;  // /sensor/light
constexpr uint16_t kChannelGnssLocation      = 8;  // /gnss/fix
constexpr uint16_t kChannelGnssMeasurement   = 9;  // /gnss/raw
constexpr uint16_t kChannelOis               = 10; // /camera/ois
constexpr uint16_t kChannelWifiScan          = 11; // /wireless/wifi
constexpr uint16_t kChannelBleScan           = 12; // /wireless/ble
constexpr uint16_t kChannelDeviceStatus      = 13; // /system/status
constexpr uint16_t kChannelHandPose          = 14; // /ai/hand_pose
constexpr uint16_t kChannelCameraCalibration = 15; // /camera/calibration
constexpr uint16_t kChannelDepthImage        = 16; // /camera/depth/image_raw
constexpr uint16_t kChannelConfidenceImage   = 17; // /camera/depth/confidence
constexpr uint16_t kChannelEnvInfo           = 18; // /env/info
constexpr uint16_t kChannelHandsPresence     = 19; // /ai/hands_presence
constexpr uint16_t kChannelTransform         = 20; // /tf

struct InternalRecorderState {
    xhey::jni::McapRecorderContext base;
    std::mutex mtx;
    int32_t video_width = 1920;
    int32_t video_height = 1080;
    int32_t video_fps = 30;
    bool video_is_hevc = true;
    int32_t audio_sample_rate = 16000;
    int32_t audio_channels = 1;
    uint32_t data_section_crc = 0;
    std::vector<uint8_t> codec_config;
};

struct DemuxedPacket {
    uint64_t timestamp_ns = 0;
    uint32_t flags = 0;
    bool is_keyframe = false;
    std::vector<uint8_t> payload;
};

struct InternalDemuxerState {
    xhey::jni::McapDemuxerContext base;
    std::vector<DemuxedPacket> video_frames;
    std::vector<DemuxedPacket> audio_blocks;
    std::vector<uint8_t> video_codec_config;
    size_t video_cursor = 0;
    size_t audio_cursor = 0;
};

std::string JStr(JNIEnv* env, jstring s) {
    return xhey::jni::JStringToStdString(env, s);
}

inline void AppendU16LE(std::vector<uint8_t>& buf, uint16_t v) {
    buf.push_back(static_cast<uint8_t>(v & 0xffu));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xffu));
}

inline void AppendU32LE(std::vector<uint8_t>& buf, uint32_t v) {
    for (int i = 0; i < 4; ++i) buf.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xffu));
}

inline void AppendU64LE(std::vector<uint8_t>& buf, uint64_t v) {
    for (int i = 0; i < 8; ++i) buf.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xffu));
}

inline void AppendF32LE(std::vector<uint8_t>& buf, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(float));
    AppendU32LE(buf, bits);
}

inline void AppendF64LE(std::vector<uint8_t>& buf, double v) {
    uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(double));
    AppendU64LE(buf, bits);
}

inline void AppendMcapString(std::vector<uint8_t>& buf, const std::string& s) {
    AppendU32LE(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

inline void AppendMcapBytes(std::vector<uint8_t>& buf, const void* data, uint32_t len) {
    AppendU32LE(buf, len);
    if (data && len > 0) {
        const auto* p = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), p, p + len);
    }
}

// Official MCAP Map<string, string> serialization: prefixed with uint32_t total byte length of entries!
inline void AppendMcapMap(
        std::vector<uint8_t>& buf,
        const std::vector<std::pair<std::string, std::string>>& kvs) {
    std::vector<uint8_t> mapBytes;
    for (const auto& kv : kvs) {
        AppendMcapString(mapBytes, kv.first);
        AppendMcapString(mapBytes, kv.second);
    }
    AppendU32LE(buf, static_cast<uint32_t>(mapBytes.size()));
    buf.insert(buf.end(), mapBytes.begin(), mapBytes.end());
}

bool ReadMcapString(const uint8_t* data, size_t len, size_t& pos, std::string& out) {
    if (pos + 4 > len) return false;
    uint32_t sLen = 0;
    std::memcpy(&sLen, data + pos, 4);
    pos += 4;
    if (pos + sLen > len) return false;
    out.assign(reinterpret_cast<const char*>(data + pos), sLen);
    pos += sLen;
    return true;
}

bool ParseMcapMetadataRecord(
        const std::vector<uint8_t>& body,
        std::string& outName,
        std::unordered_map<std::string, std::string>& outMap) {
    size_t pos = 0;
    if (!ReadMcapString(body.data(), body.size(), pos, outName)) return false;
    if (pos + 4 > body.size()) return false;
    uint32_t mapByteLen = 0;
    std::memcpy(&mapByteLen, body.data() + pos, 4);
    pos += 4;
    if (pos + mapByteLen > body.size()) return false;
    size_t mapEnd = pos + mapByteLen;
    while (pos < mapEnd) {
        std::string k, v;
        if (!ReadMcapString(body.data(), mapEnd, pos, k)) return false;
        if (!ReadMcapString(body.data(), mapEnd, pos, v)) return false;
        outMap[k] = v;
    }
    return true;
}

bool WriteRawRecord(InternalRecorderState* st, uint8_t opcode, const void* body, uint64_t bodyLen) {
    if (!st || !st->base.fp || st->base.write_failed) return false;
    uint8_t recHdr[9];
    recHdr[0] = opcode;
    std::memcpy(recHdr + 1, &bodyLen, 8);
    if (std::fwrite(recHdr, 1, sizeof(recHdr), st->base.fp) != sizeof(recHdr) ||
        (bodyLen > 0 && std::fwrite(body, 1, static_cast<size_t>(bodyLen), st->base.fp) != bodyLen)) {
        st->base.write_failed = true;
        return false;
    }
    if (opcode != kOpFooter && opcode != kOpDataEnd) {
        st->data_section_crc = xhey::jni::ComputeCrc32(recHdr, sizeof(recHdr), st->data_section_crc);
        if (bodyLen > 0 && body) {
            st->data_section_crc = xhey::jni::ComputeCrc32(body, static_cast<size_t>(bodyLen), st->data_section_crc);
        }
    }
    st->base.bytes_written += 9 + bodyLen;
    return true;
}

// Official MCAP Schema Record (0x03): id(u16) + name(str) + encoding(str) + data(u32 + bytes)
bool WriteMcapSchemaRecord(
        InternalRecorderState* st,
        uint16_t schemaId,
        const std::string& name,
        const std::string& encoding,
        const void* schemaData,
        uint32_t schemaDataLen) {
    std::vector<uint8_t> body;
    AppendU16LE(body, schemaId);
    AppendMcapString(body, name);
    AppendMcapString(body, encoding);
    AppendMcapBytes(body, schemaData, schemaDataLen);
    return WriteRawRecord(st, kOpSchema, body.data(), body.size());
}

// Official MCAP Channel Record (0x04): id(u16) + schema_id(u16) + topic(str) + message_encoding(str) + metadata(Map)
bool WriteMcapChannelRecord(
        InternalRecorderState* st,
        uint16_t channelId,
        uint16_t schemaId,
        const std::string& topic,
        const std::string& messageEncoding,
        const std::vector<std::pair<std::string, std::string>>& metadata = {}) {
    std::vector<uint8_t> body;
    AppendU16LE(body, channelId);
    AppendU16LE(body, schemaId);
    AppendMcapString(body, topic);
    AppendMcapString(body, messageEncoding);
    AppendMcapMap(body, metadata);
    return WriteRawRecord(st, kOpChannel, body.data(), body.size());
}

// Official MCAP Metadata Record (0x0C): name(str) + metadata(Map<string,string>)
bool WriteMcapMetadataRecord(
        InternalRecorderState* st,
        const std::string& name,
        const std::vector<std::pair<std::string, std::string>>& metadata) {
    std::vector<uint8_t> body;
    AppendMcapString(body, name);
    AppendMcapMap(body, metadata);
    return WriteRawRecord(st, kOpMetadata, body.data(), body.size());
}

// Official MCAP Attachment Record (0x09): log_time(u64) + create_time(u64) + name(str) + media_type(str) + data(u64+bytes) + crc(u32)
bool WriteMcapAttachmentRecord(
        InternalRecorderState* st,
        uint64_t logTimeNs,
        const std::string& name,
        const std::string& mediaType,
        const void* data,
        size_t dataLen) {
    std::vector<uint8_t> body;
    AppendU64LE(body, logTimeNs);
    AppendU64LE(body, logTimeNs);
    AppendMcapString(body, name);
    AppendMcapString(body, mediaType);
    AppendU64LE(body, static_cast<uint64_t>(dataLen));
    if (data && dataLen > 0) {
        const auto* p = static_cast<const uint8_t*>(data);
        body.insert(body.end(), p, p + dataLen);
    }
    uint32_t crc = xhey::jni::ComputeCrc32(body.data(), body.size());
    AppendU32LE(body, crc);
    return WriteRawRecord(st, kOpAttachment, body.data(), body.size());
}

bool WriteMcapMessage(
        InternalRecorderState* st,
        uint16_t channelId,
        uint64_t timestampNs,
        const void* payload,
        size_t payloadLen) {
    if (!st || !st->base.is_recording || !st->base.fp || st->base.write_failed) return false;

    uint32_t seq = ++st->base.sequence_counter;
    uint64_t bodyLen = 2 + 4 + 8 + 8 + static_cast<uint64_t>(payloadLen);
    std::vector<uint8_t> body;
    body.reserve(static_cast<size_t>(bodyLen));
    AppendU16LE(body, channelId);
    AppendU32LE(body, seq);
    AppendU64LE(body, timestampNs);
    AppendU64LE(body, timestampNs);
    if (payload && payloadLen > 0) {
        const auto* p = static_cast<const uint8_t*>(payload);
        body.insert(body.end(), p, p + payloadLen);
    }
    if (!WriteRawRecord(st, kOpMessage, body.data(), body.size())) {
        return false;
    }
    st->base.message_count += 1;
    return true;
}

bool InitializeMcapStreamRecords(InternalRecorderState* st) {
    if (!st || !st->base.fp) return false;

    // 1. Write 8-byte MCAP v0 file header magic
    if (std::fwrite(kMcapMagic, 1, sizeof(kMcapMagic), st->base.fp) != sizeof(kMcapMagic)) {
        st->base.write_failed = true;
        st->base.is_recording = false;
        return false;
    }
    st->base.bytes_written += sizeof(kMcapMagic);
    st->data_section_crc = 0;

    // 2. Write MCAP Header record (Opcode 0x01)
    std::vector<uint8_t> hdrBody;
    AppendMcapString(hdrBody, "xhey_ego_v1");
    AppendMcapString(hdrBody, "libnnpackage-f99779bd.so");
    WriteRawRecord(st, kOpHeader, hdrBody.data(), hdrBody.size());

    // 3. Write spec-compliant MCAP Schema records (Opcode 0x03)
    const char kVideoSchemaDesc[] = "foxglove.CompressedVideo";
    const char kAudioSchemaDesc[] = "xhey.RawAudio";
    const char kSensorSchemaDesc[] = "xhey.SensorBinary";
    const char kJsonSchemaDesc[] = "{\"type\":\"object\"}";
    WriteMcapSchemaRecord(st, kSchemaVideoCompressed, "foxglove.CompressedVideo", "protobuf", kVideoSchemaDesc, sizeof(kVideoSchemaDesc) - 1);
    WriteMcapSchemaRecord(st, kSchemaRawAudio,        "xhey.RawAudio",            "raw",      kAudioSchemaDesc, sizeof(kAudioSchemaDesc) - 1);
    WriteMcapSchemaRecord(st, kSchemaSensorBinary,    "xhey.SensorBinary",        "cdr",      kSensorSchemaDesc, sizeof(kSensorSchemaDesc) - 1);
    WriteMcapSchemaRecord(st, kSchemaJsonTelemetry,   "xhey.JsonTelemetry",       "jsonschema", kJsonSchemaDesc, sizeof(kJsonSchemaDesc) - 1);

    // 4. Write spec-compliant MCAP Channel records (Opcode 0x04)
    WriteMcapChannelRecord(st, kChannelVideoCompressed,   kSchemaVideoCompressed, "/camera/color/compressed", st->video_is_hevc ? "h265" : "h264");
    WriteMcapChannelRecord(st, kChannelAudioBlock,        kSchemaRawAudio,        "/audio/raw",               "pcm");
    WriteMcapChannelRecord(st, kChannelCameraMeta,        kSchemaSensorBinary,    "/camera/metadata",         "cdr");
    WriteMcapChannelRecord(st, kChannelImu,               kSchemaSensorBinary,    "/imu/data",                "cdr");
    WriteMcapChannelRecord(st, kChannelMag,               kSchemaSensorBinary,    "/imu/mag",                 "cdr");
    WriteMcapChannelRecord(st, kChannelBaro,              kSchemaSensorBinary,    "/sensor/baro",             "cdr");
    WriteMcapChannelRecord(st, kChannelLight,             kSchemaSensorBinary,    "/sensor/light",            "cdr");
    WriteMcapChannelRecord(st, kChannelGnssLocation,      kSchemaSensorBinary,    "/gnss/fix",                "cdr");
    WriteMcapChannelRecord(st, kChannelGnssMeasurement,   kSchemaSensorBinary,    "/gnss/raw",                "cdr");
    WriteMcapChannelRecord(st, kChannelOis,               kSchemaSensorBinary,    "/camera/ois",              "cdr");
    WriteMcapChannelRecord(st, kChannelWifiScan,          kSchemaJsonTelemetry,   "/wireless/wifi",           "json");
    WriteMcapChannelRecord(st, kChannelBleScan,           kSchemaJsonTelemetry,   "/wireless/ble",            "json");
    WriteMcapChannelRecord(st, kChannelDeviceStatus,      kSchemaSensorBinary,    "/system/status",           "cdr");
    WriteMcapChannelRecord(st, kChannelHandPose,          kSchemaSensorBinary,    "/ai/hand_pose",            "cdr");
    WriteMcapChannelRecord(st, kChannelCameraCalibration, kSchemaSensorBinary,    "/camera/calibration",      "cdr");
    WriteMcapChannelRecord(st, kChannelDepthImage,        kSchemaSensorBinary,    "/camera/depth/image_raw",  "cdr");
    WriteMcapChannelRecord(st, kChannelConfidenceImage,   kSchemaSensorBinary,    "/camera/depth/confidence", "cdr");
    WriteMcapChannelRecord(st, kChannelEnvInfo,           kSchemaSensorBinary,    "/env/info",                "cdr");
    WriteMcapChannelRecord(st, kChannelHandsPresence,     kSchemaSensorBinary,    "/ai/hands_presence",       "cdr");
    WriteMcapChannelRecord(st, kChannelTransform,         kSchemaSensorBinary,    "/tf",                      "cdr");

    // 5. Write spec-compliant MCAP Metadata record (Opcode 0x0C) with stream config
    WriteMcapMetadataRecord(st, "xhey.stream_config", {
        {"session_id",        st->base.session_id},
        {"video_width",       std::to_string(st->video_width)},
        {"video_height",      std::to_string(st->video_height)},
        {"depth_width",       std::to_string(st->base.depth_width)},
        {"depth_height",      std::to_string(st->base.depth_height)},
        {"video_fps",         std::to_string(st->video_fps)},
        {"is_hevc",           st->video_is_hevc ? "1" : "0"},
        {"audio_sample_rate", std::to_string(st->audio_sample_rate)},
        {"audio_channels",    std::to_string(st->audio_channels)}
    });

    return !st->base.write_failed;
}

bool FinalizeMcapFile(InternalRecorderState* st) {
    if (!st || !st->base.fp) return false;

    // 1. Write official MCAP DataEnd record (Opcode 0x0F: uint32_t data_section_crc)
    std::vector<uint8_t> dataEndBody;
    AppendU32LE(dataEndBody, st->data_section_crc);
    WriteRawRecord(st, kOpDataEnd, dataEndBody.data(), dataEndBody.size());

    // 2. Write official MCAP Footer record (Opcode 0x02: summary_start=0, summary_offset_start=0, summary_crc=0)
    std::vector<uint8_t> footerBody;
    AppendU64LE(footerBody, 0ULL);
    AppendU64LE(footerBody, 0ULL);
    AppendU32LE(footerBody, 0U);
    WriteRawRecord(st, kOpFooter, footerBody.data(), footerBody.size());

    // 3. Write trailing 8-byte MCAP v0 magic
    if (std::fwrite(kMcapMagic, 1, sizeof(kMcapMagic), st->base.fp) == sizeof(kMcapMagic)) {
        st->base.bytes_written += sizeof(kMcapMagic);
    } else {
        st->base.write_failed = true;
    }
    std::fflush(st->base.fp);
    std::fclose(st->base.fp);
    st->base.fp = nullptr;
    st->base.is_recording = false;
    return !st->base.write_failed;
}

bool ContainsAnnexBKeyFrameOrCsd(const uint8_t* data, size_t len, bool isHevc, std::vector<uint8_t>* outCsd = nullptr) {
    if (!data || len < 5) return false;
    bool isKey = false;
    for (size_t i = 0; i + 4 < len; ++i) {
        if (data[i] == 0x00 && data[i + 1] == 0x00 &&
            ((data[i + 2] == 0x01) || (data[i + 2] == 0x00 && data[i + 3] == 0x01))) {
            size_t nalOffset = (data[i + 2] == 0x01) ? (i + 3) : (i + 4);
            if (nalOffset >= len) break;
            if (isHevc) {
                uint8_t nalType = (data[nalOffset] >> 1) & 0x3Fu;
                if (nalType >= 16 && nalType <= 21) isKey = true;
                if ((nalType == 32 || nalType == 33 || nalType == 34) && outCsd && outCsd->empty()) {
                    outCsd->assign(data, data + std::min<size_t>(len, 256));
                }
            } else {
                uint8_t nalType = data[nalOffset] & 0x1Fu;
                if (nalType == 5) isKey = true;
                if ((nalType == 7 || nalType == 8) && outCsd && outCsd->empty()) {
                    outCsd->assign(data, data + std::min<size_t>(len, 256));
                }
            }
        }
    }
    return isKey;
}

} // namespace

extern "C" {

// ============================================================================
// 1. McapRecorder JNI Implementation (Timemark v2 + SDK v1)
// ============================================================================

/**
 * Timemark McapRecorder.java signature:
 * private static native long nativeCreate(String path, String sessionId,
 *     int colorWidth, int colorHeight, int depthWidth, int depthHeight,
 *     int audioSampleRate, int audioChannelCount, int audioSourceKind,
 *     boolean audioAecEnabled, boolean audioNsEnabled, boolean audioAgcEnabled);
 */
JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeCreate(
        JNIEnv* env, jclass /*clazz*/,
        jstring outputPath, jstring sessionId,
        jint colorWidth, jint colorHeight, jint depthWidth, jint depthHeight,
        jint audioSampleRate, jint audioChannelCount, jint audioSourceKind,
        jboolean audioAecEnabled, jboolean audioNsEnabled, jboolean audioAgcEnabled) {
    auto* st = new InternalRecorderState();
    st->base.output_path = JStr(env, outputPath);
    st->base.session_id = JStr(env, sessionId);
    st->base.color_width = colorWidth > 0 ? colorWidth : 1920;
    st->base.color_height = colorHeight > 0 ? colorHeight : 1080;
    st->base.depth_width = depthWidth > 0 ? depthWidth : 256;
    st->base.depth_height = depthHeight > 0 ? depthHeight : 192;
    st->base.audio_sample_rate = audioSampleRate > 0 ? audioSampleRate : 16000;
    st->base.audio_channels = audioChannelCount > 0 ? audioChannelCount : 1;
    st->base.audio_source_kind = audioSourceKind;
    st->base.audio_aec = (audioAecEnabled == JNI_TRUE);
    st->base.audio_ns = (audioNsEnabled == JNI_TRUE);
    st->base.audio_agc = (audioAgcEnabled == JNI_TRUE);

    st->video_width = st->base.color_width;
    st->video_height = st->base.color_height;
    st->audio_sample_rate = st->base.audio_sample_rate;
    st->audio_channels = st->base.audio_channels;

    if (!st->base.output_path.empty()) {
        st->base.fp = std::fopen(st->base.output_path.c_str(), "wb");
        if (!st->base.fp) {
            delete st;
            return 0L;
        }
        st->base.is_recording = true;
        if (!InitializeMcapStreamRecords(st)) {
            std::fclose(st->base.fp);
            st->base.fp = nullptr;
            delete st;
            return 0L;
        }
    }
    return reinterpret_cast<jlong>(st);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeDestroy(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;
    {
        std::lock_guard<std::mutex> lock(st->mtx);
        if (st->base.fp) {
            FinalizeMcapFile(st);
        }
    }
    delete st;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteHandPose(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jint imgWidth, jint imgHeight, jfloatArray keypoints, jint handCount) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || handCount < 0) return;

    std::vector<float> kp;
    if (keypoints) {
        jsize len = env->GetArrayLength(keypoints);
        if (len > 0) {
            kp.resize(static_cast<size_t>(len));
            env->GetFloatArrayRegion(keypoints, 0, len, kp.data());
        }
    }

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    AppendU32LE(payload, static_cast<uint32_t>(imgWidth));
    AppendU32LE(payload, static_cast<uint32_t>(imgHeight));
    AppendU32LE(payload, static_cast<uint32_t>(handCount));
    for (float v : kp) {
        AppendF32LE(payload, v);
    }
    WriteMcapMessage(st, kChannelHandPose, static_cast<uint64_t>(timestampNs), payload.data(), payload.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteImu(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs,
        jfloatArray accelArr, jfloatArray gyroArr, jfloatArray magArr, jfloatArray quatArr,
        jboolean hasQuat) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    float accel[3] = {0.f, 0.f, 0.f};
    float gyro[3]  = {0.f, 0.f, 0.f};
    float mag[3]   = {0.f, 0.f, 0.f};
    float quat[4]  = {0.f, 0.f, 0.f, 1.f};

    if (accelArr && env->GetArrayLength(accelArr) >= 3) env->GetFloatArrayRegion(accelArr, 0, 3, accel);
    if (gyroArr  && env->GetArrayLength(gyroArr)  >= 3) env->GetFloatArrayRegion(gyroArr,  0, 3, gyro);
    if (magArr   && env->GetArrayLength(magArr)   >= 3) env->GetFloatArrayRegion(magArr,   0, 3, mag);
    if (hasQuat == JNI_TRUE && quatArr && env->GetArrayLength(quatArr) >= 4) {
        env->GetFloatArrayRegion(quatArr, 0, 4, quat);
    }

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    payload.reserve(56);
    for (int i = 0; i < 3; ++i) AppendF32LE(payload, accel[i]);
    for (int i = 0; i < 3; ++i) AppendF32LE(payload, gyro[i]);
    for (int i = 0; i < 3; ++i) AppendF32LE(payload, mag[i]);
    for (int i = 0; i < 4; ++i) AppendF32LE(payload, quat[i]);
    AppendU32LE(payload, (hasQuat == JNI_TRUE) ? 1u : 0u);

    if (WriteMcapMessage(st, kChannelImu, static_cast<uint64_t>(timestampNs), payload.data(), payload.size())) {
        st->base.imu_sample_count += 1;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeOpen(
        JNIEnv* env, jclass /*clazz*/, jlong handle, jstring outputPath,
        jint videoWidth, jint videoHeight, jint fps, jboolean isHevc,
        jint audioSampleRate, jint audioChannels) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !outputPath) return JNI_FALSE;

    std::lock_guard<std::mutex> lock(st->mtx);
    if (st->base.fp) {
        std::fclose(st->base.fp);
        st->base.fp = nullptr;
    }

    st->base.output_path = JStr(env, outputPath);
    st->base.fp = std::fopen(st->base.output_path.c_str(), "wb");
    if (!st->base.fp) {
        st->base.write_failed = true;
        return JNI_FALSE;
    }

    st->video_width = videoWidth;
    st->video_height = videoHeight;
    st->video_fps = fps;
    st->video_is_hevc = (isHevc == JNI_TRUE);
    st->audio_sample_rate = audioSampleRate;
    st->audio_channels = audioChannels;
    st->base.bytes_written = 0;
    st->base.message_count = 0;
    st->base.sequence_counter = 0;
    st->base.write_failed = false;
    st->base.is_recording = true;

    return InitializeMcapStreamRecords(st) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeClose(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;
    std::lock_guard<std::mutex> lock(st->mtx);
    FinalizeMcapFile(st);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeSetMetadata(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jstring deviceModel, jstring osVersion, jstring appVersion, jstring sessionId, jstring extraJson) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    WriteMcapMetadataRecord(st, "xhey.session_metadata", {
        {"device_model", JStr(env, deviceModel)},
        {"os_version",   JStr(env, osVersion)},
        {"app_version",  JStr(env, appVersion)},
        {"session_id",   JStr(env, sessionId)},
        {"extra_json",   JStr(env, extraJson)}
    });
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteCameraCalibration(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jint width, jint height,
        jfloat fx, jfloat fy, jfloat cx, jfloat cy, jfloatArray /*distortionCoeffs*/) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    AppendU32LE(payload, static_cast<uint32_t>(width));
    AppendU32LE(payload, static_cast<uint32_t>(height));
    AppendF32LE(payload, fx);
    AppendF32LE(payload, fy);
    AppendF32LE(payload, cx);
    AppendF32LE(payload, cy);
    WriteMcapMessage(st, kChannelCameraCalibration, static_cast<uint64_t>(timestampNs), payload.data(), payload.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeSetCodecConfig(
        JNIEnv* env, jclass /*clazz*/, jlong handle, jbyteArray configBytes, jint size) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !configBytes || size <= 0) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    jsize len = env->GetArrayLength(configBytes);
    int copyLen = std::min<int>(size, len);
    st->codec_config.resize(static_cast<size_t>(copyLen));
    env->GetByteArrayRegion(configBytes, 0, copyLen, reinterpret_cast<jbyte*>(st->codec_config.data()));

    WriteMcapAttachmentRecord(st, 0ULL, "video_csd", "application/octet-stream",
                              st->codec_config.data(), st->codec_config.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteVideoFrame(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jobject directBuffer, jint offset, jint size, jint flags) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !directBuffer || size <= 0 || offset < 0) return;

    const auto* basePtr = static_cast<const uint8_t*>(env->GetDirectBufferAddress(directBuffer));
    jlong cap = env->GetDirectBufferCapacity(directBuffer);
    if (!basePtr || static_cast<jlong>(offset) + size > cap) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> packet(4 + static_cast<size_t>(size));
    uint32_t uFlags = static_cast<uint32_t>(flags);
    std::memcpy(packet.data(), &uFlags, 4);
    std::memcpy(packet.data() + 4, basePtr + offset, static_cast<size_t>(size));
    if (WriteMcapMessage(st, kChannelVideoCompressed, static_cast<uint64_t>(timestampNs), packet.data(), packet.size())) {
        st->base.video_frame_count += 1;
    }
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteVideoFrameBytes(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jbyteArray data, jint offset, jint size, jint flags) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !data || size <= 0) return;

    jsize arrLen = env->GetArrayLength(data);
    if (offset < 0 || offset + size > arrLen) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> packet(4 + static_cast<size_t>(size));
    uint32_t uFlags = static_cast<uint32_t>(flags);
    std::memcpy(packet.data(), &uFlags, 4);
    env->GetByteArrayRegion(data, offset, size, reinterpret_cast<jbyte*>(packet.data() + 4));
    if (WriteMcapMessage(st, kChannelVideoCompressed, static_cast<uint64_t>(timestampNs), packet.data(), packet.size())) {
        st->base.video_frame_count += 1;
    }
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteCameraMeta(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jlong frameNumber, jlong exposureTimeNs,
        jint iso, jfloat focalLength, jfloat focusDistance, jfloat lensAperture) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    AppendU64LE(payload, static_cast<uint64_t>(frameNumber));
    AppendU64LE(payload, static_cast<uint64_t>(exposureTimeNs));
    AppendU32LE(payload, static_cast<uint32_t>(iso));
    AppendF32LE(payload, focalLength);
    AppendF32LE(payload, focusDistance);
    AppendF32LE(payload, lensAperture);
    WriteMcapMessage(st, kChannelCameraMeta, static_cast<uint64_t>(timestampNs), payload.data(), payload.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteAudioBlock(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jbyteArray pcmData, jint offset, jint size) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !pcmData || size <= 0) return;

    jsize arrLen = env->GetArrayLength(pcmData);
    if (offset < 0 || offset + size > arrLen) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload(static_cast<size_t>(size));
    env->GetByteArrayRegion(pcmData, offset, size, reinterpret_cast<jbyte*>(payload.data()));
    if (WriteMcapMessage(st, kChannelAudioBlock, static_cast<uint64_t>(timestampNs), payload.data(), payload.size())) {
        st->base.audio_block_count += 1;
    }
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteImuBatch(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlongArray timestampsNs, jfloatArray imuValues, jint count) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !timestampsNs || !imuValues || count <= 0) return;

    if (env->GetArrayLength(timestampsNs) < count || env->GetArrayLength(imuValues) < count * 6) return;

    std::vector<jlong> ts(static_cast<size_t>(count));
    std::vector<jfloat> vals(static_cast<size_t>(count) * 6);
    env->GetLongArrayRegion(timestampsNs, 0, count, ts.data());
    env->GetFloatArrayRegion(imuValues, 0, count * 6, vals.data());

    std::lock_guard<std::mutex> lock(st->mtx);
    for (int i = 0; i < count; ++i) {
        WriteMcapMessage(st, kChannelImu, static_cast<uint64_t>(ts[static_cast<size_t>(i)]),
                         &vals[static_cast<size_t>(i) * 6], 6 * sizeof(float));
    }
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteMag(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jfloat mx, jfloat my, jfloat mz) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    float mag[3] = {mx, my, mz};
    WriteMcapMessage(st, kChannelMag, static_cast<uint64_t>(timestampNs), mag, sizeof(mag));
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteBaro(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jfloat pressureHpa, jfloat temperatureC) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    float baro[2] = {pressureHpa, temperatureC};
    WriteMcapMessage(st, kChannelBaro, static_cast<uint64_t>(timestampNs), baro, sizeof(baro));
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteLight(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jfloat lux) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    WriteMcapMessage(st, kChannelLight, static_cast<uint64_t>(timestampNs), &lux, sizeof(lux));
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteGnssLocation(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jdouble latitude, jdouble longitude, jdouble altitude,
        jfloat speed, jfloat bearing, jfloat horizontalAccuracy, jfloat verticalAccuracy) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    AppendF64LE(payload, latitude);
    AppendF64LE(payload, longitude);
    AppendF64LE(payload, altitude);
    AppendF32LE(payload, speed);
    AppendF32LE(payload, bearing);
    AppendF32LE(payload, horizontalAccuracy);
    AppendF32LE(payload, verticalAccuracy);
    WriteMcapMessage(st, kChannelGnssLocation, static_cast<uint64_t>(timestampNs), payload.data(), payload.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteGnssMeasurement(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jint svid, jint constellationType,
        jdouble cn0DbHz, jdouble pseudorangeRateMetersPerSec, jstring rawJson) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    AppendU32LE(payload, static_cast<uint32_t>(svid));
    AppendU32LE(payload, static_cast<uint32_t>(constellationType));
    AppendF64LE(payload, cn0DbHz);
    AppendF64LE(payload, pseudorangeRateMetersPerSec);
    AppendMcapString(payload, JStr(env, rawJson));
    WriteMcapMessage(st, kChannelGnssMeasurement, static_cast<uint64_t>(timestampNs), payload.data(), payload.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteOis(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jfloat shiftX, jfloat shiftY) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    float ois[2] = {shiftX, shiftY};
    WriteMcapMessage(st, kChannelOis, static_cast<uint64_t>(timestampNs), ois, sizeof(ois));
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteWifiScan(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jstring wifiScanJson) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::string s = JStr(env, wifiScanJson);
    std::lock_guard<std::mutex> lock(st->mtx);
    WriteMcapMessage(st, kChannelWifiScan, static_cast<uint64_t>(timestampNs), s.data(), s.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteBleScan(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jstring bleScanJson) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::string s = JStr(env, bleScanJson);
    std::lock_guard<std::mutex> lock(st->mtx);
    WriteMcapMessage(st, kChannelBleScan, static_cast<uint64_t>(timestampNs), s.data(), s.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteDeviceStatus(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jint batteryLevel, jfloat batteryTempC, jint thermalStatus, jlong availableRamBytes) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    std::vector<uint8_t> payload;
    AppendU32LE(payload, static_cast<uint32_t>(batteryLevel));
    AppendF32LE(payload, batteryTempC);
    AppendU32LE(payload, static_cast<uint32_t>(thermalStatus));
    AppendU64LE(payload, static_cast<uint64_t>(availableRamBytes));
    WriteMcapMessage(st, kChannelDeviceStatus, static_cast<uint64_t>(timestampNs), payload.data(), payload.size());
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteDepthImage(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jobject depthBuffer, jint offsetBytes, jint byteOrPixelCount) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !depthBuffer || byteOrPixelCount <= 0 || offsetBytes < 0) return;

    const auto* ptr = static_cast<const uint8_t*>(env->GetDirectBufferAddress(depthBuffer));
    jlong cap = env->GetDirectBufferCapacity(depthBuffer);
    if (!ptr || offsetBytes >= cap) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    size_t avail = static_cast<size_t>(cap - offsetBytes);
    size_t writeBytes = std::min(avail, static_cast<size_t>(byteOrPixelCount));
    WriteMcapMessage(st, kChannelDepthImage, static_cast<uint64_t>(timestampNs), ptr + offsetBytes, writeBytes);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeWriteConfidenceImage(
        JNIEnv* env, jclass /*clazz*/, jlong handle,
        jlong timestampNs, jobject confBuffer, jint offsetBytes, jint byteCount) {
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    if (!st || !confBuffer || byteCount <= 0 || offsetBytes < 0) return;

    const auto* ptr = static_cast<const uint8_t*>(env->GetDirectBufferAddress(confBuffer));
    jlong cap = env->GetDirectBufferCapacity(confBuffer);
    if (!ptr || offsetBytes >= cap) return;

    std::lock_guard<std::mutex> lock(st->mtx);
    size_t avail = static_cast<size_t>(cap - offsetBytes);
    size_t writeBytes = std::min(avail, static_cast<size_t>(byteCount));
    WriteMcapMessage(st, kChannelConfidenceImage, static_cast<uint64_t>(timestampNs), ptr + offsetBytes, writeBytes);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapRecorder_nativeGetStats(
        JNIEnv* env, jclass /*clazz*/, jlong handle, jlongArray outStats) {
    if (!env || !outStats || env->GetArrayLength(outStats) < 4) return;
    auto* st = reinterpret_cast<InternalRecorderState*>(handle);
    jlong stats[4] = {0, 0, 0, 1};
    if (st) {
        std::lock_guard<std::mutex> lock(st->mtx);
        stats[0] = static_cast<jlong>(st->base.message_count);
        stats[1] = static_cast<jlong>(st->base.bytes_written);
        stats[2] = st->base.is_recording ? 1L : 0L;
        stats[3] = st->base.write_failed ? 1L : 0L;
    }
    env->SetLongArrayRegion(outStats, 0, 4, stats);
}

// ============================================================================
// 2. McapDemuxer JNI Implementation (11 Functions in McapDemuxer.java + v1)
// ============================================================================

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_mcap_McapDemuxer_nativeOpen(
        JNIEnv* env, jclass /*clazz*/, jstring mcapFilePath) {
    if (!env || !mcapFilePath) return 0L;
    std::string path = JStr(env, mcapFilePath);
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return 0L;

    uint8_t magic[8]{};
    if (std::fread(magic, 1, 8, fp) != 8 || std::memcmp(magic, kMcapMagic, 8) != 0) {
        std::fclose(fp);
        return 0L;
    }

    auto* demux = new InternalDemuxerState();
    demux->base.fp = fp;
    demux->base.file_path = path;
    demux->base.video_width = 1920;
    demux->base.video_height = 1080;
    demux->base.video_fps = 30;
    demux->base.is_hevc = true;
    demux->base.audio_sample_rate = 16000;
    demux->base.audio_channels = 1;

    uint64_t minTimeNs = UINT64_MAX;
    uint64_t maxTimeNs = 0;

    while (true) {
        uint8_t opcode = 0;
        uint64_t bodyLen = 0;
        if (std::fread(&opcode, 1, 1, fp) != 1) break;
        if (opcode == kMcapMagic[0]) break;
        if (std::fread(&bodyLen, sizeof(uint64_t), 1, fp) != 1) break;
        if (bodyLen > (64ULL << 20)) break;

        std::vector<uint8_t> body(static_cast<size_t>(bodyLen));
        if (bodyLen > 0 && std::fread(body.data(), 1, static_cast<size_t>(bodyLen), fp) != bodyLen) {
            break;
        }

        if (opcode == kOpMetadata) {
            std::string metaName;
            std::unordered_map<std::string, std::string> kv;
            if (ParseMcapMetadataRecord(body, metaName, kv) && metaName == "xhey.stream_config") {
                if (kv.count("video_width"))       demux->base.video_width       = std::atoi(kv["video_width"].c_str());
                if (kv.count("video_height"))      demux->base.video_height      = std::atoi(kv["video_height"].c_str());
                if (kv.count("video_fps"))         demux->base.video_fps         = std::atoi(kv["video_fps"].c_str());
                if (kv.count("is_hevc"))           demux->base.is_hevc           = (std::atoi(kv["is_hevc"].c_str()) != 0);
                if (kv.count("audio_sample_rate")) demux->base.audio_sample_rate = std::atoi(kv["audio_sample_rate"].c_str());
                if (kv.count("audio_channels"))    demux->base.audio_channels    = std::atoi(kv["audio_channels"].c_str());
            }
        } else if (opcode == kOpAttachment && body.size() > 28) {
            size_t pos = 16;
            std::string attName, attMime;
            if (ReadMcapString(body.data(), body.size(), pos, attName) &&
                ReadMcapString(body.data(), body.size(), pos, attMime) &&
                pos + 8 <= body.size()) {
                uint64_t dataLen = 0;
                std::memcpy(&dataLen, body.data() + pos, 8);
                pos += 8;
                if (attName == "video_csd" && pos + dataLen <= body.size()) {
                    demux->video_codec_config.assign(body.data() + pos, body.data() + pos + static_cast<size_t>(dataLen));
                }
            }
        } else if (opcode == kOpMessage && body.size() >= 22) {
            uint16_t channelId = 0;
            uint64_t logTimeNs = 0;
            std::memcpy(&channelId, body.data(), 2);
            std::memcpy(&logTimeNs, body.data() + 6, 8);
            const uint8_t* msgData = body.data() + 22;
            size_t msgLen = body.size() - 22;

            minTimeNs = std::min(minTimeNs, logTimeNs);
            maxTimeNs = std::max(maxTimeNs, logTimeNs);

            if (channelId == kChannelVideoCompressed && msgLen >= 4) {
                DemuxedPacket pkt;
                pkt.timestamp_ns = logTimeNs;
                std::memcpy(&pkt.flags, msgData, 4);
                pkt.payload.assign(msgData + 4, msgData + msgLen);
                pkt.is_keyframe = (pkt.flags & 1u) != 0 ||
                    ContainsAnnexBKeyFrameOrCsd(pkt.payload.data(), pkt.payload.size(),
                                                demux->base.is_hevc, &demux->video_codec_config);
                if (demux->video_frames.empty()) pkt.is_keyframe = true;
                if (pkt.is_keyframe) {
                    demux->base.keyframe_indices.push_back(static_cast<int32_t>(demux->video_frames.size()));
                }
                demux->base.frame_times_ns.push_back(static_cast<int64_t>(logTimeNs));
                demux->video_frames.push_back(std::move(pkt));
            } else if (channelId == kChannelAudioBlock && msgLen > 0) {
                DemuxedPacket pkt;
                pkt.timestamp_ns = logTimeNs;
                pkt.payload.assign(msgData, msgData + msgLen);
                demux->base.audio_times_ns.push_back(static_cast<int64_t>(logTimeNs));
                demux->audio_blocks.push_back(std::move(pkt));
            }
        } else if (opcode == kOpFooter) {
            break;
        }
    }

    demux->base.total_frames = demux->video_frames.size();
    demux->base.duration_ns = (maxTimeNs >= minTimeNs && minTimeNs != UINT64_MAX)
        ? static_cast<int64_t>(maxTimeNs - minTimeNs)
        : 0LL;
    return reinterpret_cast<jlong>(demux);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapDemuxer_nativeClose(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong handle) {
    auto* demux = reinterpret_cast<InternalDemuxerState*>(handle);
    if (!demux) return;
    if (demux->base.fp) {
        std::fclose(demux->base.fp);
        demux->base.fp = nullptr;
    }
    delete demux;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_mcap_McapDemuxer_nativeGetInfo(
        JNIEnv* env, jclass /*clazz*/, jlong handle, jintArray outInfo) {
    if (!env || !outInfo || env->GetArrayLength(outInfo) < 7) return;
    auto* demux = reinterpret_cast<InternalDemuxerState*>(handle);
    jint info[7] = {0, 0, 0, 0, 0, 0, 0};
    if (demux) {
        info[0] = static_cast<jint>(demux->video_frames.size());
        info[1] = static_cast<jint>(demux->base.video_width);
        info[2] = static_cast<jint>(demux->base.video_height);
        info[3] = demux->base.is_hevc ? 1 : 0;
        info[4] = static_cast<jint>(demux->audio_blocks.size());
        info[5] = static_cast<jint>(demux->base.audio_sample_rate);
        info[6] = static_cast<jint>(demux->base.audio_channels);
    }
    env->SetIntArrayRegion(outInfo, 0, 7, info);
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_mcap_McapDemuxer_nativeGetCodecConfig(
        JNIEnv* env, jclass /*clazz*/, jlong handle) {
    auto* demux = reinterpret_cast<InternalDemuxerState*>(handle);
    if (!demux || demux->video_codec_config.empty()) return env->NewByteArray(0);
    return xhey::StdVectorToJByteArray(env, demux->video_codec_config);
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_mcap_McapDemuxer_nativeReadNextVideoFrame(
        JNIEnv* env, jclass /*clazz*/, jlong handle, jobject outBuffer, jlongArray outMeta) {
    auto* demux = reinterpret_cast<InternalDemuxerState*>(handle);
    if (!demux || !outBuffer || demux->video_cursor >= demux->video_frames.size()) return -1;

    const auto& pkt = demux->video_frames[demux->video_cursor++];
    auto* dst = static_cast<uint8_t*>(env->GetDirectBufferAddress(outBuffer));
    jlong cap = env->GetDirectBufferCapacity(outBuffer);
    if (!dst || cap < static_cast<jlong>(pkt.payload.size())) return -1;

    std::memcpy(dst, pkt.payload.data(), pkt.payload.size());
    if (outMeta && env->GetArrayLength(outMeta) >= 2) {
        jlong meta[2] = {
            static_cast<jlong>(pkt.timestamp_ns),
            static_cast<jlong>(pkt.flags)
        };
        env->SetLongArrayRegion(outMeta, 0, 2, meta);
    }
    return static_cast<jint>(pkt.payload.size());
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_mcap_McapDemuxer_nativeReadNextAudioBlock(
        JNIEnv* env, jclass /*clazz*/, jlong handle, jobject outBuffer, jlongArray outTimestampNs) {
    auto* demux = reinterpret_cast<InternalDemuxerState*>(handle);
    if (!demux || !outBuffer || demux->audio_cursor >= demux->audio_blocks.size()) return -1;

    const auto& pkt = demux->audio_blocks[demux->audio_cursor++];
    auto* dst = static_cast<uint8_t*>(env->GetDirectBufferAddress(outBuffer));
    jlong cap = env->GetDirectBufferCapacity(outBuffer);
    if (!dst || cap < static_cast<jlong>(pkt.payload.size())) return -1;

    std::memcpy(dst, pkt.payload.data(), pkt.payload.size());
    if (outTimestampNs && env->GetArrayLength(outTimestampNs) >= 1) {
        jlong ts = static_cast<jlong>(pkt.timestamp_ns);
        env->SetLongArrayRegion(outTimestampNs, 0, 1, &ts);
    }
    return static_cast<jint>(pkt.payload.size());
}

} // extern "C"
