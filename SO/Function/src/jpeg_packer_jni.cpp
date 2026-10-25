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

// ============================================================================
// Standard FIPS-197 AES-128-CBC Decryptor for Release APK Exif UserComment
// Key & IV from TodayApplication.java:205: "2qR2nuPdYrq3IMSr"
// ============================================================================
constexpr uint8_t kAesInvSBox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

constexpr uint8_t kAesFwdSBox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

inline uint8_t GfMul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; ++i) {
        if (b & 1) p ^= a;
        bool hi = (a & 0x80) != 0;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return p;
}

void Aes128DecryptBlock(const uint8_t in[16], const uint8_t key[16], uint8_t out[16]) {
    uint8_t rk[176];
    std::memcpy(rk, key, 16);
    static constexpr uint8_t rcon[10] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
    for (int i = 4; i < 44; ++i) {
        uint8_t t[4] = {rk[(i - 1) * 4 + 0], rk[(i - 1) * 4 + 1], rk[(i - 1) * 4 + 2], rk[(i - 1) * 4 + 3]};
        if (i % 4 == 0) {
            uint8_t tmp = t[0];
            t[0] = kAesFwdSBox[t[1]] ^ rcon[i / 4 - 1];
            t[1] = kAesFwdSBox[t[2]];
            t[2] = kAesFwdSBox[t[3]];
            t[3] = kAesFwdSBox[tmp];
        }
        for (int j = 0; j < 4; ++j) rk[i * 4 + j] = rk[(i - 4) * 4 + j] ^ t[j];
    }

    uint8_t s[16];
    for (int i = 0; i < 16; ++i) s[i] = in[i] ^ rk[160 + i];

    for (int round = 9; round >= 1; --round) {
        // InvShiftRows
        uint8_t t[16];
        t[0]=s[0];  t[1]=s[13]; t[2]=s[10]; t[3]=s[7];
        t[4]=s[4];  t[5]=s[1];  t[6]=s[14]; t[7]=s[11];
        t[8]=s[8];  t[9]=s[5];  t[10]=s[2]; t[11]=s[15];
        t[12]=s[12];t[13]=s[9]; t[14]=s[6]; t[15]=s[3];
        // InvSubBytes & AddRoundKey
        for (int i = 0; i < 16; ++i) s[i] = kAesInvSBox[t[i]] ^ rk[round * 16 + i];
        // InvMixColumns
        for (int c = 0; c < 4; ++c) {
            uint8_t a0 = s[c*4+0], a1 = s[c*4+1], a2 = s[c*4+2], a3 = s[c*4+3];
            s[c*4+0] = GfMul(a0,0x0e) ^ GfMul(a1,0x0b) ^ GfMul(a2,0x0d) ^ GfMul(a3,0x09);
            s[c*4+1] = GfMul(a0,0x09) ^ GfMul(a1,0x0e) ^ GfMul(a2,0x0b) ^ GfMul(a3,0x0d);
            s[c*4+2] = GfMul(a0,0x0d) ^ GfMul(a1,0x09) ^ GfMul(a2,0x0e) ^ GfMul(a3,0x0b);
            s[c*4+3] = GfMul(a0,0x0b) ^ GfMul(a1,0x0d) ^ GfMul(a2,0x09) ^ GfMul(a3,0x0e);
        }
    }
    uint8_t t[16];
    t[0]=s[0];  t[1]=s[13]; t[2]=s[10]; t[3]=s[7];
    t[4]=s[4];  t[5]=s[1];  t[6]=s[14]; t[7]=s[11];
    t[8]=s[8];  t[9]=s[5];  t[10]=s[2]; t[11]=s[15];
    t[12]=s[12];t[13]=s[9]; t[14]=s[6]; t[15]=s[3];
    for (int i = 0; i < 16; ++i) out[i] = kAesInvSBox[t[i]] ^ rk[i];
}

std::vector<uint8_t> Base64DecodeSimple(const std::string& in) {
    std::vector<uint8_t> out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        int d = -1;
        if (c >= 'A' && c <= 'Z') d = c - 'A';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 26;
        else if (c >= '0' && c <= '9') d = c - '0' + 52;
        else if (c == '+') d = 62;
        else if (c == '/') d = 63;
        else if (c == '=') break;
        else continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string UrlDecodeUtf8(const std::string& str) {
    std::string out;
    out.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            out.push_back(' ');
        } else if (str[i] == '%' && i + 2 < str.size()) {
            auto hexVal = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hexVal(str[i + 1]);
            int lo = hexVal(str[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
            } else {
                out.push_back(str[i]);
            }
        } else {
            out.push_back(str[i]);
        }
    }
    return out;
}

bool ExtractReleaseExifUserCommentSlice(
        const std::string& filePath,
        int packPosition,
        xhey::NativeSliceDataInfo& outSlice) {
    FILE* fp = std::fopen(filePath.c_str(), "rb");
    if (!fp) return false;
    std::vector<uint8_t> head(65536);
    size_t n = std::fread(head.data(), 1, head.size(), fp);
    std::fclose(fp);
    if (n < 64 || head[0] != 0xFF || head[1] != 0xD8) return false;

    std::string view(reinterpret_cast<const char*>(head.data()), n);
    size_t ucPos = view.find("<exif:UserComment>");
    if (ucPos == std::string::npos) return false;
    size_t ucEnd = view.find("</exif:UserComment>", ucPos);
    if (ucEnd == std::string::npos) return false;

    std::string block = view.substr(ucPos, ucEnd - ucPos);
    size_t liStart = block.find("x-default");
    if (liStart == std::string::npos) return false;
    size_t gtPos = block.find('>', liStart);
    if (gtPos == std::string::npos) return false;
    liStart = gtPos + 1;
    size_t liEnd = block.find("</rdf:li>", liStart);
    if (liEnd == std::string::npos || liEnd <= liStart) return false;

    std::string b64 = block.substr(liStart, liEnd - liStart);
    std::vector<uint8_t> cipher = Base64DecodeSimple(b64);
    if (cipher.empty() || (cipher.size() % 16) != 0) return false;

    static const uint8_t kReleaseAesKey[16] = {
        '2','q','R','2','n','u','P','d','Y','r','q','3','I','M','S','r'
    };
    std::vector<uint8_t> plain(cipher.size(), 0);
    const uint8_t* prevBlock = kReleaseAesKey;
    for (size_t off = 0; off < cipher.size(); off += 16) {
        uint8_t dec[16];
        Aes128DecryptBlock(cipher.data() + off, kReleaseAesKey, dec);
        for (int j = 0; j < 16; ++j) {
            plain[off + j] = dec[j] ^ prevBlock[j];
        }
        prevBlock = cipher.data() + off;
    }

    uint8_t pad = plain.back();
    if (pad >= 1 && pad <= 16 && plain.size() >= pad) {
        plain.resize(plain.size() - pad);
    }
    std::string rawUrlStr(plain.begin(), plain.end());
    std::string jsonStr = UrlDecodeUtf8(rawUrlStr);
    if (jsonStr.empty() || jsonStr.find('{') == std::string::npos) return false;

    outSlice = xhey::NativeSliceDataInfo{};
    outSlice.errorCode = 0;
    outSlice.position = packPosition;
    outSlice.version = 1;
    outSlice.id = 1;
    outSlice.type = 255;
    outSlice.data.assign(jsonStr.begin(), jsonStr.end());
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

        xhey::NativeSliceDataInfo exifSlice{};
        if (!path.empty() &&
            (CheckFileHasPackedTrailer(path, posVec[static_cast<size_t>(i)]) ||
             ExtractReleaseExifUserCommentSlice(path, posVec[static_cast<size_t>(i)], exifSlice))) {
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
        xhey::NativeSliceDataInfo exifSlice{};
        if (ExtractReleaseExifUserCommentSlice(path, packPosition, exifSlice)) {
            jclass sliceCls = env->FindClass("com/xhey/xcamerasdk/algorithm/nn/SliceDataInfo");
            if (!sliceCls) return nullptr;
            jobjectArray resultArr = env->NewObjectArray(1, sliceCls, nullptr);
            jobject obj = xhey::CreateJavaSliceDataInfo(env, exifSlice);
            env->SetObjectArrayElement(resultArr, 0, obj);
            if (obj) env->DeleteLocalRef(obj);
            env->DeleteLocalRef(sliceCls);
            return resultArr;
        }
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
