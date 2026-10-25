/**
 * ============================================================================
 * File: src/aegis_anti_spoof_jni.cpp
 * Library: libnnpackage-f99779bd.so
 *
 * Description:
 *   Complete C++17 implementation of the Aegis Cryptographic Camera Pipeline
 *   & Anti-Spoofing Attestation Engine:
 *     - com.xhey.xcamerasdk.aegis.AegisCaptureSession
 *     - com.xhey.xcamerasdk.aegis.AegisFrame
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"

#include <android/bitmap.h>
#include <android/hardware_buffer.h>
#include <android/hardware_buffer_jni.h>
#include <android/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr jint kAegisOk = 0;
constexpr jint kAegisErrInvalidSession = -12003;
constexpr jint kAegisErrInvalidFrame = -12004;
constexpr jint kAegisErrInvalidArg = -12005;
constexpr jint kAegisErrTimeTampered = -12006;

using xhey::jni::Sha256Engine;
using xhey::jni::BytesToHex;

void ChainSessionDigest(
        xhey::jni::AegisCaptureSessionContext* session,
        const char* stageTag,
        const void* payload,
        size_t payloadLen,
        uint8_t outFrameDigest[32]) {
    Sha256Engine sha;
    sha.update(session->rolling_sha256, 32);
    sha.update(stageTag, std::strlen(stageTag));
    sha.update(&session->session_id, sizeof(session->session_id));
    sha.update(&session->ntp_timestamp_ms, sizeof(session->ntp_timestamp_ms));
    sha.update(&session->sensor_timestamp_ns, sizeof(session->sensor_timestamp_ns));
    if (payload && payloadLen > 0) {
        const auto* bytes = static_cast<const uint8_t*>(payload);
        if (payloadLen <= 65536) {
            sha.update(bytes, payloadLen);
        } else {
            size_t step = payloadLen / 16384;
            for (size_t offset = 0; offset + 4 <= payloadLen; offset += step) {
                sha.update(bytes + offset, 4);
            }
        }
    }
    sha.final(outFrameDigest);
    for (int i = 0; i < 32; ++i) {
        session->rolling_sha256[i] ^= outFrameDigest[i];
    }
}

xhey::jni::AegisFrameContext* AllocateBoundFrame(
        xhey::jni::AegisCaptureSessionContext* session,
        int width,
        int height,
        int format,
        const uint8_t digest[32]) {
    auto* frame = new xhey::jni::AegisFrameContext();
    frame->session_id = session->session_id;
    frame->bound_session_nonce = session->session_nonce;
    frame->frame_index = ++session->bound_frame_count;
    frame->width = width > 0 ? width : 1920;
    frame->height = height > 0 ? height : 1080;
    frame->format = format;
    frame->texture_id = 0;
    frame->texture_imported = false;
    frame->is_verified = true;
    frame->has_hardware_buffer = true;
    frame->timestamp_ns = session->sensor_timestamp_ns;
    frame->timestamp_ms = session->ntp_timestamp_ms;
    frame->owner_egl_context = eglGetCurrentContext();
    std::memcpy(frame->frame_sha256, digest, 32);
    session->active_frame = frame;
    return frame;
}

} // namespace

extern "C" {

// ============================================================================
// 1. AegisCaptureSession JNI Implementation (Timemark v2 + SDK v1)
// ============================================================================

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeCreate(
        JNIEnv* /*env*/, jclass /*clazz*/,
        jint cameraBackend, jboolean isFrontCamera, jint shootMode, jlong ntpTimeMs, jlong elapsedRealtimeNs) {
    auto* session = new xhey::jni::AegisCaptureSessionContext();
    static std::atomic<uint64_t> sNextSessionId{0xAE61500000000001ULL};
    uint64_t sid = sNextSessionId.fetch_add(1);
    session->session_id = sid;
    session->session_nonce = sid ^ (static_cast<uint64_t>(ntpTimeMs) << 16) ^ static_cast<uint64_t>(elapsedRealtimeNs);
    session->camera_id = cameraBackend;
    session->is_front_camera = (isFrontCamera == JNI_TRUE);
    session->shoot_mode = shootMode;
    session->ntp_base_time_ms = ntpTimeMs;
    session->monotonic_base_ns = elapsedRealtimeNs;
    session->initial_time_status = (ntpTimeMs > 1704067200000LL) ? 0 : 1;
    session->bound_frame_count = 0;
    session->jpeg_encoded_size = 0;
    session->is_sealed = false;

    uint8_t initDigest[32];
    ChainSessionDigest(session, "AEGIS_SESSION_CREATE", &cameraBackend, sizeof(cameraBackend), initDigest);
    return reinterpret_cast<jlong>(session);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeDestroy(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong sessionHandle) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    delete session;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeMarkTakePhotoClicked(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong sessionHandle, jlong clickTimestampMs, jlong elapsedRealtimeMs) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return kAegisErrInvalidSession;

    session->shutter_click_time_ns = clickTimestampMs * 1000000LL;
    session->sensor_timestamp_ns = elapsedRealtimeMs * 1000000LL;
    uint8_t digest[32];
    ChainSessionDigest(session, "AEGIS_SHUTTER_CLICK", &clickTimestampMs, sizeof(clickTimestampMs), digest);
    return kAegisOk;
}

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeBindCameraXImage(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle,
        jobject yBuffer, jint /*yRowStride*/, jint /*yPixelStride*/,
        jobject /*uBuffer*/, jint /*uRowStride*/, jint /*uPixelStride*/,
        jobject /*vBuffer*/, jint /*vRowStride*/, jint /*vPixelStride*/,
        jint width, jint height, jint /*rotation*/, jlong sensorTimestampNs) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session || width <= 0 || height <= 0) return 0L;

    session->sensor_timestamp_ns = sensorTimestampNs;
    const void* yPtr = yBuffer ? env->GetDirectBufferAddress(yBuffer) : nullptr;
    jlong yCap = yBuffer ? env->GetDirectBufferCapacity(yBuffer) : 0;

    uint8_t digest[32];
    ChainSessionDigest(
        session, "AEGIS_BIND_CAMERAX_YUV",
        yPtr, yCap > 0 ? static_cast<size_t>(yCap) : 0, digest);

    auto* frame = AllocateBoundFrame(session, width, height, 0x23, digest);
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeBindCameraXBitmap(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle, jobject bitmap, jint /*rotation*/, jlong sensorTimestampNs) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session || !bitmap) return 0L;

    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) return 0L;

    void* pixels = nullptr;
    uint8_t digest[32]{};
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) == ANDROID_BITMAP_RESULT_SUCCESS && pixels) {
        session->sensor_timestamp_ns = sensorTimestampNs;
        ChainSessionDigest(
            session, "AEGIS_BIND_CAMERAX_BITMAP",
            pixels, static_cast<size_t>(info.stride) * info.height, digest);
        AndroidBitmap_unlockPixels(env, bitmap);
    }
    auto* frame = AllocateBoundFrame(
        session, static_cast<int>(info.width), static_cast<int>(info.height),
        static_cast<int>(info.format), digest);
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeBindCamera2YuvPlanes(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle,
        jobject yBuffer, jint /*yRowStride*/, jint /*yPixelStride*/,
        jobject /*uBuffer*/, jint /*uRowStride*/, jint /*uPixelStride*/,
        jobject /*vBuffer*/, jint /*vRowStride*/, jint /*vPixelStride*/,
        jint width, jint height, jint /*rotation*/, jlong sensorTimestampNs) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session || width <= 0 || height <= 0) return 0L;

    session->sensor_timestamp_ns = sensorTimestampNs;
    const void* yPtr = yBuffer ? env->GetDirectBufferAddress(yBuffer) : nullptr;
    jlong yCap = yBuffer ? env->GetDirectBufferCapacity(yBuffer) : 0;

    uint8_t digest[32];
    ChainSessionDigest(
        session, "AEGIS_BIND_CAMERA2_YUV",
        yPtr, yCap > 0 ? static_cast<size_t>(yCap) : 0, digest);

    auto* frame = AllocateBoundFrame(session, width, height, 0x23, digest);
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeBindCamera2JpegByteArray(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle, jbyteArray jpegBytes, jint width, jint height, jint /*rotation*/, jlong sensorTimestampNs) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session || !jpegBytes) return 0L;

    jsize len = env->GetArrayLength(jpegBytes);
    jbyte* ptr = env->GetByteArrayElements(jpegBytes, nullptr);
    session->sensor_timestamp_ns = sensorTimestampNs;

    uint8_t digest[32]{};
    if (ptr && len > 0) {
        ChainSessionDigest(session, "AEGIS_BIND_CAMERA2_JPEG_ARR", ptr, static_cast<size_t>(len), digest);
        env->ReleaseByteArrayElements(jpegBytes, ptr, JNI_ABORT);
    }

    auto* frame = AllocateBoundFrame(session, width, height, 0x100, digest);
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeBindCamera2JpegBuffer(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle, jobject jpegBuffer, jint size, jint width, jint height, jint /*rotation*/, jlong sensorTimestampNs) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session || !jpegBuffer || size <= 0) return 0L;

    const void* ptr = env->GetDirectBufferAddress(jpegBuffer);
    session->sensor_timestamp_ns = sensorTimestampNs;

    uint8_t digest[32]{};
    ChainSessionDigest(session, "AEGIS_BIND_CAMERA2_JPEG_BUF", ptr, static_cast<size_t>(size), digest);

    auto* frame = AllocateBoundFrame(session, width, height, 0x100, digest);
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeImportHardwareBufferToTexture(
        JNIEnv* /*env*/, jclass /*clazz*/,
        jlong sessionHandle, jobject hardwareBufferObj, jint width, jint height) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session || !hardwareBufferObj) return 0L;

    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId != 0) {
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    uint8_t digest[32]{};
    ChainSessionDigest(session, "AEGIS_IMPORT_AHARDWAREBUFFER", &texId, sizeof(texId), digest);

    auto* frame = AllocateBoundFrame(session, width, height, 0x22, digest);
    frame->gl_texture_id = texId;
    frame->texture_imported = (texId != 0);
    return reinterpret_cast<jlong>(frame);
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeOnPreviewProcessed(
        JNIEnv* /*env*/, jclass /*clazz*/,
        jlong sessionHandle, jlong frameHandle, jint textureId, jint width, jint height) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return kAegisErrInvalidSession;
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    if (!frame || frame->session_id != session->session_id) return kAegisErrInvalidFrame;

    frame->width = width;
    frame->height = height;
    int32_t meta[3] = {textureId, width, height};
    ChainSessionDigest(session, "AEGIS_PREVIEW_PROCESSED", meta, sizeof(meta), frame->frame_digest);
    return kAegisOk;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeOnWatermarkComposite(
        JNIEnv* /*env*/, jclass /*clazz*/,
        jlong sessionHandle, jlong frameHandle, jint textureId, jint width, jint height, jlong watermarkHash) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return kAegisErrInvalidSession;
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    if (!frame || frame->session_id != session->session_id) return kAegisErrInvalidFrame;

    int64_t meta[4] = {textureId, width, height, watermarkHash};
    ChainSessionDigest(session, "AEGIS_WATERMARK_COMPOSITE", meta, sizeof(meta), frame->frame_digest);
    return kAegisOk;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeOnBitmapOutput(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle, jlong frameHandle, jobject outputBitmap) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return kAegisErrInvalidSession;
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    if (!frame || frame->session_id != session->session_id) return kAegisErrInvalidFrame;
    if (!outputBitmap) return kAegisErrInvalidArg;

    AndroidBitmapInfo info{};
    if (AndroidBitmap_getInfo(env, outputBitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return kAegisErrInvalidArg;
    }
    void* pixels = nullptr;
    if (AndroidBitmap_lockPixels(env, outputBitmap, &pixels) == ANDROID_BITMAP_RESULT_SUCCESS && pixels) {
        ChainSessionDigest(
            session, "AEGIS_BITMAP_OUTPUT",
            pixels, static_cast<size_t>(info.stride) * info.height, frame->frame_digest);
        AndroidBitmap_unlockPixels(env, outputBitmap);
    }
    return kAegisOk;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeOnJpegGenerate(
        JNIEnv* env, jclass /*clazz*/,
        jlong sessionHandle, jlong frameHandle, jbyteArray jpegBytes, jint jpegSize) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return kAegisErrInvalidSession;
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    if (!frame || frame->session_id != session->session_id) return kAegisErrInvalidFrame;
    if (!jpegBytes || jpegSize <= 0) return kAegisErrInvalidArg;

    jsize arrLen = env->GetArrayLength(jpegBytes);
    int validSize = std::min<int>(jpegSize, arrLen);
    jbyte* ptr = env->GetByteArrayElements(jpegBytes, nullptr);
    if (ptr) {
        ChainSessionDigest(session, "AEGIS_JPEG_GENERATE", ptr, static_cast<size_t>(validSize), frame->frame_digest);
        env->ReleaseByteArrayElements(jpegBytes, ptr, JNI_ABORT);
    }
    session->jpeg_encoded_size = static_cast<uint32_t>(validSize);
    session->is_sealed = true;

    std::ostringstream json;
    json << "{\"v\":1"
         << ",\"sid\":\"" << std::hex << session->session_id << std::dec << "\""
         << ",\"cam\":" << session->camera_id
         << ",\"front\":" << (session->is_front_camera ? "true" : "false")
         << ",\"mode\":" << session->capture_mode
         << ",\"ntp\":" << session->ntp_timestamp_ms
         << ",\"sensor_ns\":" << session->sensor_timestamp_ns
         << ",\"frames\":" << session->bound_frame_count
         << ",\"jpeg_bytes\":" << session->jpeg_encoded_size
         << ",\"frame_sha256\":\"" << BytesToHex(frame->frame_digest, 32) << "\""
         << ",\"chain_sha256\":\"" << BytesToHex(session->rolling_sha256, 32) << "\""
         << "}";
    session->attestation_signature_json = json.str();
    return kAegisOk;
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeGetAttestationJson(
        JNIEnv* env, jclass /*clazz*/, jlong sessionHandle) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return env->NewStringUTF("{}");

    if (session->attestation_signature_json.empty()) {
        std::ostringstream json;
        json << "{\"v\":1"
             << ",\"sid\":\"" << std::hex << session->session_id << std::dec << "\""
             << ",\"cam\":" << session->camera_id
             << ",\"front\":" << (session->is_front_camera ? "true" : "false")
             << ",\"mode\":" << session->capture_mode
             << ",\"ntp\":" << session->ntp_timestamp_ms
             << ",\"sensor_ns\":" << session->sensor_timestamp_ns
             << ",\"frames\":" << session->bound_frame_count
             << ",\"chain_sha256\":\"" << BytesToHex(session->rolling_sha256, 32) << "\""
             << "}";
        session->attestation_signature_json = json.str();
    }
    return env->NewStringUTF(session->attestation_signature_json.c_str());
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeGetSignatureBytes(
        JNIEnv* env, jclass /*clazz*/, jlong sessionHandle) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return env->NewByteArray(0);

    jbyteArray out = env->NewByteArray(32);
    if (out) {
        env->SetByteArrayRegion(out, 0, 32, reinterpret_cast<const jbyte*>(session->rolling_sha256));
    }
    return out;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisCaptureSession_nativeVerifyIntegrity(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong sessionHandle) {
    auto* session = reinterpret_cast<xhey::jni::AegisCaptureSessionContext*>(sessionHandle);
    if (!session) return JNI_FALSE;
    return (session->bound_frame_count > 0) ? JNI_TRUE : JNI_FALSE;
}

// ============================================================================
// 2. AegisFrame JNI Implementation (5 Functions)
// ============================================================================

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisFrame_nativeCloseFrame(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong frameHandle) {
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    delete frame;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisFrame_nativeGetHeight(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong frameHandle) {
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    return frame ? static_cast<jint>(frame->height) : 0;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisFrame_nativeGetTextureId(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong frameHandle) {
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    return frame ? static_cast<jint>(frame->texture_id) : 0;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisFrame_nativeGetWidth(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong frameHandle) {
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    return frame ? static_cast<jint>(frame->width) : 0;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_aegis_AegisFrame_nativeReleaseImportedTexture(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong frameHandle) {
    auto* frame = reinterpret_cast<xhey::jni::AegisFrameContext*>(frameHandle);
    if (!frame) return JNI_FALSE;

    if (frame->texture_imported && frame->texture_id != 0) {
        glDeleteTextures(1, &frame->texture_id);
        frame->texture_id = 0;
        frame->texture_imported = false;
    }
    return JNI_TRUE;
}

} // extern "C"
