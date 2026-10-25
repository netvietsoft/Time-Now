/**
 * ============================================================================
 * Module: JpegPacker (Reversible Watermark Steganography Engine)
 * Library: libnnpackage-f99779bd.so
 * Java Class: com.xhey.xcamerasdk.algorithm.nn.JpegPacker
 * ============================================================================
 * Implements all 5 JNI methods of JpegPacker:
 *   1. init(String config) -> int
 *   2. destroy() -> void
 *   3. getCanjpegDecode(String[] filePaths, int[] packPositions) -> int[]
 *   4. startEncode(String jpegPath, SliceDataInfo[] slices, SliceDataExt ext) -> int
 *   5. startDecode(int packPosition, String jpegPath, String optOutDir) -> SliceDataInfo[]
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"
#include <cstdio>
#include <mutex>

namespace {

constexpr char kPackerMagic[8] = {'X', 'H', 'E', 'Y', 'P', 'K', '0', '1'};
constexpr uint8_t kXorKey = 0x5Au;
constexpr uint32_t kMaxSliceCount = 256u;
constexpr uint32_t kMaxSlicePayloadBytes = 50u * 1024u * 1024u; // 50 MiB

std::mutex g_packer_mutex;
bool g_packer_initialized = false;
std::string g_packer_config;

#pragma pack(push, 1)
struct PackFooter {
    uint64_t payloadOffset; // Byte offset of packed slice stream in file
    uint32_t sliceCount;    // Number of SliceDataInfo records (1 .. kMaxSliceCount)
    char magic[8];          // "XHEYPK01"
};

struct SliceHeaderDisk {
    int64_t position;
    int64_t version;
    int64_t id;
    int64_t type;
    float rect[4];
    uint32_t dataLen;
};
#pragma pack(pop)

bool WriteAllBytes(FILE* fp, const void* buf, size_t len) {
    if (!fp) return false;
    if (len == 0) return true;
    return std::fwrite(buf, 1, len, fp) == len;
}

bool CheckFileHasApp9Marker(FILE* fp, long fileSize, uint64_t* outPayloadOffset = nullptr) {
    if (!fp || fileSize < 16) return false;
    if (std::fseek(fp, 0, SEEK_SET) != 0) return false;
    uint8_t soi[2] = {0};
    if (std::fread(soi, 1, 2, fp) != 2 || soi[0] != 0xFF || soi[1] != 0xD8) {
        return false;
    }
    while (std::ftell(fp) + 4 < fileSize) {
        uint8_t markerHdr[4] = {0};
        if (std::fread(markerHdr, 1, 4, fp) != 4) break;
        if (markerHdr[0] != 0xFF) break;
        uint8_t marker = markerHdr[1];
        if (marker == 0xDA || marker == 0xD9) break; // SOS or EOI
        uint16_t segLen = static_cast<uint16_t>((markerHdr[2] << 8) | markerHdr[3]);
        if (segLen < 2) break;
        long segDataStart = std::ftell(fp);
        if (marker == 0xE9 && segLen >= 10) { // APP9
            char magic[8] = {0};
            if (std::fread(magic, 1, 8, fp) == 8 &&
                std::memcmp(magic, kPackerMagic, 8) == 0) {
                if (outPayloadOffset) {
                    *outPayloadOffset = static_cast<uint64_t>(segDataStart + 8);
                }
                return true;
            }
        }
        if (std::fseek(fp, segDataStart + static_cast<long>(segLen) - 2, SEEK_SET) != 0) {
            break;
        }
    }
    return false;
}

bool CheckFileHasPackedTrailer(
        const std::string& filePath,
        int packPosition,
        PackFooter* outFooter = nullptr,
        long* outFileSize = nullptr) {
    FILE* fp = std::fopen(filePath.c_str(), "rb");
    if (!fp) return false;

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    long fileSize = std::ftell(fp);
    if (outFileSize) *outFileSize = fileSize;

    if (fileSize < static_cast<long>(sizeof(PackFooter) + sizeof(SliceHeaderDisk))) {
        std::fclose(fp);
        return false;
    }

    if (packPosition == 0) {
        bool hasApp9 = CheckFileHasApp9Marker(fp, fileSize);
        if (hasApp9 && !outFooter) {
            std::fclose(fp);
            return true;
        }
    }

    if (std::fseek(fp, fileSize - static_cast<long>(sizeof(PackFooter)), SEEK_SET) != 0) {
        std::fclose(fp);
        return false;
    }
    PackFooter footer{};
    size_t readBytes = std::fread(&footer, 1, sizeof(PackFooter), fp);
    std::fclose(fp);

    if (readBytes != sizeof(PackFooter)) return false;
    if (std::memcmp(footer.magic, kPackerMagic, 8) != 0) return false;

    uint64_t trailerLimit = static_cast<uint64_t>(fileSize) - sizeof(PackFooter);
    if (footer.payloadOffset >= trailerLimit) return false;
    if (footer.sliceCount == 0 || footer.sliceCount > kMaxSliceCount) return false;

    uint64_t availableBytes = trailerLimit - footer.payloadOffset;
    if (static_cast<uint64_t>(footer.sliceCount) * sizeof(SliceHeaderDisk) > availableBytes) {
        return false;
    }

    if (outFooter) *outFooter = footer;
    return true;
}

} // namespace

extern "C" {

// ----------------------------------------------------------------------------
// 1. JpegPacker.init(String config)
// ----------------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_init(
        JNIEnv* env, jclass /*clazz*/, jstring config) {
    std::lock_guard<std::mutex> lock(g_packer_mutex);
    g_packer_config = xhey::JStringToStdString(env, config);
    g_packer_initialized = true;
    return 0;
}

// ----------------------------------------------------------------------------
// 2. JpegPacker.destroy()
// ----------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_destroy(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    std::lock_guard<std::mutex> lock(g_packer_mutex);
    g_packer_config.clear();
    g_packer_initialized = false;
}

// ----------------------------------------------------------------------------
// 3. JpegPacker.getCanjpegDecode(String[] filePaths, int[] packPositions)
// ----------------------------------------------------------------------------
JNIEXPORT jintArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_getCanjpegDecode(
        JNIEnv* env, jclass /*clazz*/, jobjectArray filePaths, jintArray packPositions) {
    if (!env || !filePaths) return nullptr;
    jsize count = env->GetArrayLength(filePaths);
    jintArray resultArr = env->NewIntArray(count);
    if (!resultArr) return nullptr;
    if (count <= 0) return resultArr;

    std::vector<jint> posVec(static_cast<size_t>(count), 1);
    if (packPositions) {
        jsize posLen = env->GetArrayLength(packPositions);
        jsize copyLen = std::min(count, posLen);
        if (copyLen > 0) {
            env->GetIntArrayRegion(packPositions, 0, copyLen, posVec.data());
        }
    }

    std::vector<jint> flags(static_cast<size_t>(count), 0);
    for (jsize i = 0; i < count; ++i) {
        auto jpath = static_cast<jstring>(env->GetObjectArrayElement(filePaths, i));
        std::string path = xhey::JStringToStdString(env, jpath);
        if (jpath) env->DeleteLocalRef(jpath);

        if (!path.empty() && CheckFileHasPackedTrailer(path, posVec[static_cast<size_t>(i)])) {
            flags[static_cast<size_t>(i)] = 1;
        }
    }

    env->SetIntArrayRegion(resultArr, 0, count, flags.data());
    return resultArr;
}

// ----------------------------------------------------------------------------
// 4. JpegPacker.startEncode(String jpegPath, SliceDataInfo[] slices, SliceDataExt ext)
// ----------------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startEncode(
        JNIEnv* env, jclass /*clazz*/,
        jstring jpegPath, jobjectArray sliceArray, jobject sliceDataExt) {
    std::lock_guard<std::mutex> lock(g_packer_mutex);
    if (!env || !jpegPath || !sliceArray) return -1;

    std::string path = xhey::JStringToStdString(env, jpegPath);
    if (path.empty()) return -1;

    jsize sliceCount = env->GetArrayLength(sliceArray);
    if (sliceCount <= 0 || static_cast<uint32_t>(sliceCount) > kMaxSliceCount) {
        return -2;
    }

    int extEnable = 1;
    jclass extCls = nullptr;
    jfieldID fExtPacked = nullptr;
    if (sliceDataExt) {
        extCls = env->GetObjectClass(sliceDataExt);
        if (extCls) {
            jfieldID fEnable = env->GetFieldID(extCls, "enable", "I");
            fExtPacked = env->GetFieldID(extCls, "packedRawData", "[B");
            if (fEnable) {
                extEnable = env->GetIntField(sliceDataExt, fEnable);
            }
        }
    }
    if (extEnable == 0) {
        if (extCls) env->DeleteLocalRef(extCls);
        return 0;
    }

    if (CheckFileHasPackedTrailer(path, 1)) {
        if (extCls) env->DeleteLocalRef(extCls);
        return -32;
    }

    jclass sliceCls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/SliceDataInfo");
    if (!sliceCls) {
        if (extCls) env->DeleteLocalRef(extCls);
        return -1;
    }
    jfieldID fPos  = env->GetFieldID(sliceCls, "position", "J");
    jfieldID fVer  = env->GetFieldID(sliceCls, "version", "J");
    jfieldID fId   = env->GetFieldID(sliceCls, "id", "J");
    jfieldID fType = env->GetFieldID(sliceCls, "type", "J");
    jfieldID fRect = env->GetFieldID(sliceCls, "rect", "[F");
    jfieldID fData = env->GetFieldID(sliceCls, "data", "[B");

    std::vector<uint8_t> packedStream;
    uint32_t validSlicesWritten = 0;

    for (jsize i = 0; i < sliceCount; ++i) {
        jobject sliceObj = env->GetObjectArrayElement(sliceArray, i);
        if (!sliceObj) continue;

        SliceHeaderDisk hdr{};
        hdr.position = fPos  ? env->GetLongField(sliceObj, fPos)  : 1;
        hdr.version  = fVer  ? env->GetLongField(sliceObj, fVer)  : 1;
        hdr.id       = fId   ? env->GetLongField(sliceObj, fId)   : 0;
        hdr.type     = fType ? env->GetLongField(sliceObj, fType) : 255;

        auto rectArr = fRect ? static_cast<jfloatArray>(env->GetObjectField(sliceObj, fRect)) : nullptr;
        if (rectArr && env->GetArrayLength(rectArr) >= 4) {
            env->GetFloatArrayRegion(rectArr, 0, 4, hdr.rect);
        }
        if (rectArr) env->DeleteLocalRef(rectArr);

        auto dataArr = fData ? static_cast<jbyteArray>(env->GetObjectField(sliceObj, fData)) : nullptr;
        std::vector<uint8_t> payload;
        if (dataArr) {
            jsize len = env->GetArrayLength(dataArr);
            if (len < 0 || static_cast<uint32_t>(len) > kMaxSlicePayloadBytes) {
                env->DeleteLocalRef(dataArr);
                env->DeleteLocalRef(sliceObj);
                env->DeleteLocalRef(sliceCls);
                if (extCls) env->DeleteLocalRef(extCls);
                return -6;
            }
            payload.resize(static_cast<size_t>(len));
            if (len > 0) {
                env->GetByteArrayRegion(dataArr, 0, len, reinterpret_cast<jbyte*>(payload.data()));
                if (hdr.version == 2) {
                    for (auto& b : payload) b ^= kXorKey;
                }
            }
            env->DeleteLocalRef(dataArr);
        }
        hdr.dataLen = static_cast<uint32_t>(payload.size());

        const auto* hdrBytes = reinterpret_cast<const uint8_t*>(&hdr);
        packedStream.insert(packedStream.end(), hdrBytes, hdrBytes + sizeof(SliceHeaderDisk));
        if (!payload.empty()) {
            packedStream.insert(packedStream.end(), payload.begin(), payload.end());
        }
        ++validSlicesWritten;
        env->DeleteLocalRef(sliceObj);
    }
    env->DeleteLocalRef(sliceCls);

    if (validSlicesWritten == 0) {
        if (extCls) env->DeleteLocalRef(extCls);
        return -2;
    }

    FILE* fp = std::fopen(path.c_str(), "ab+");
    if (!fp) {
        if (extCls) env->DeleteLocalRef(extCls);
        LOGE("JpegPacker::startEncode cannot open file: %s", path.c_str());
        return -3;
    }

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        if (extCls) env->DeleteLocalRef(extCls);
        return -5;
    }
    long endPos = std::ftell(fp);
    if (endPos < 0) {
        std::fclose(fp);
        if (extCls) env->DeleteLocalRef(extCls);
        return -5;
    }
    uint64_t payloadOffset = static_cast<uint64_t>(endPos);

    PackFooter footer{};
    footer.payloadOffset = payloadOffset;
    footer.sliceCount = validSlicesWritten;
    std::memcpy(footer.magic, kPackerMagic, 8);

    bool ioOk = WriteAllBytes(fp, packedStream.data(), packedStream.size()) &&
                WriteAllBytes(fp, &footer, sizeof(PackFooter)) &&
                (std::fflush(fp) == 0);
    int closeRes = std::fclose(fp);
    if (!ioOk || closeRes != 0) {
        if (extCls) env->DeleteLocalRef(extCls);
        return -5;
    }

    if (sliceDataExt && fExtPacked) {
        const auto* footerBytes = reinterpret_cast<const uint8_t*>(&footer);
        packedStream.insert(packedStream.end(), footerBytes, footerBytes + sizeof(PackFooter));
        jbyteArray jraw = xhey::StdVectorToJByteArray(env, packedStream);
        if (jraw) {
            env->SetObjectField(sliceDataExt, fExtPacked, jraw);
            env->DeleteLocalRef(jraw);
        }
    }
    if (extCls) env->DeleteLocalRef(extCls);

    LOGI("JpegPacker::startEncode packed %u slices into %s", validSlicesWritten, path.c_str());
    return 0;
}

// ----------------------------------------------------------------------------
// 5. JpegPacker.startDecode(int packPosition, String jpegPath, String optOutDir)
// ----------------------------------------------------------------------------
JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startDecode(
        JNIEnv* env, jclass /*clazz*/,
        jint packPosition, jstring jpegPath, jstring optOutDir) {
    std::lock_guard<std::mutex> lock(g_packer_mutex);
    if (!env || !jpegPath) return nullptr;

    std::string path = xhey::JStringToStdString(env, jpegPath);
    std::string outDir = xhey::JStringToStdString(env, optOutDir);

    PackFooter footer{};
    long fileSize = 0;
    if (!CheckFileHasPackedTrailer(path, packPosition, &footer, &fileSize)) {
        return nullptr;
    }

    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return nullptr;

    uint64_t trailerOffset = static_cast<uint64_t>(fileSize) - sizeof(PackFooter);
    if (std::fseek(fp, static_cast<long>(footer.payloadOffset), SEEK_SET) != 0) {
        std::fclose(fp);
        return nullptr;
    }

    std::vector<xhey::NativeSliceDataInfo> slices;
    slices.reserve(footer.sliceCount);

    for (uint32_t i = 0; i < footer.sliceCount; ++i) {
        long curPos = std::ftell(fp);
        if (curPos < 0 || static_cast<uint64_t>(curPos) + sizeof(SliceHeaderDisk) > trailerOffset) {
            break;
        }

        SliceHeaderDisk hdr{};
        if (std::fread(&hdr, sizeof(SliceHeaderDisk), 1, fp) != 1) {
            break;
        }

        xhey::NativeSliceDataInfo item{};
        item.errorCode = 0;
        item.position  = (packPosition == 0 || packPosition == 1) ? packPosition : hdr.position;
        item.version   = hdr.version;
        item.id        = hdr.id;
        item.type      = hdr.type;
        std::memcpy(item.rect, hdr.rect, sizeof(hdr.rect));

        uint64_t dataPos = static_cast<uint64_t>(std::ftell(fp));
        uint64_t remainingInTrailer = (dataPos <= trailerOffset) ? (trailerOffset - dataPos) : 0;

        if (hdr.dataLen > remainingInTrailer) {
            item.errorCode = -4;
            slices.push_back(std::move(item));
            break;
        }

        if (hdr.dataLen > kMaxSlicePayloadBytes) {
            item.errorCode = -6;
            if (std::fseek(fp, static_cast<long>(hdr.dataLen), SEEK_CUR) != 0) {
                slices.push_back(std::move(item));
                break;
            }
            slices.push_back(std::move(item));
            continue;
        }

        if (hdr.dataLen > 0) {
            item.data.resize(hdr.dataLen);
            if (std::fread(item.data.data(), 1, hdr.dataLen, fp) == hdr.dataLen) {
                if (item.version == 2) {
                    for (auto& b : item.data) b ^= kXorKey;
                }
                if (!outDir.empty() && (item.type == 253 || item.type == 254 || item.type == 255)) {
                    std::string dumpPath = outDir + "/slice_" + std::to_string(item.id) + "_" + std::to_string(i) + ".bin";
                    FILE* outFp = std::fopen(dumpPath.c_str(), "wb");
                    if (outFp) {
                        WriteAllBytes(outFp, item.data.data(), item.data.size());
                        std::fclose(outFp);
                    }
                }
            } else {
                item.data.clear();
                item.errorCode = -4;
                slices.push_back(std::move(item));
                break;
            }
        }
        slices.push_back(std::move(item));
    }
    std::fclose(fp);

    jclass sliceCls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/SliceDataInfo");
    if (!sliceCls) return nullptr;
    jobjectArray resultArr = env->NewObjectArray(static_cast<jsize>(slices.size()), sliceCls, nullptr);
    for (size_t i = 0; i < slices.size(); ++i) {
        jobject obj = xhey::CreateJavaSliceDataInfo(env, slices[i]);
        env->SetObjectArrayElement(resultArr, static_cast<jsize>(i), obj);
        if (obj) env->DeleteLocalRef(obj);
    }
    env->DeleteLocalRef(sliceCls);
    return resultArr;
}

} // extern "C"
