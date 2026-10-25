/**
 * ============================================================================
 * File: src/code_detect_anticode_jni.cpp
 * Module: CodeDetect (AntiFakeCode Generator/Decoder, OCR Code Detector, SSIM,
 *         Laplacian, Perceptual dHash) & AiRef (Server Verification Feature Hash)
 * Library: libnnpackage-f99779bd.so
 * Java Classes:
 *   - com.xhey.xcamerasdk.algorithm.nn.CodeDetect (16 JNI functions)
 *   - com.xhey.xcamerasdk.algorithm.nn.airef.AiRef (10 JNI functions)
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace {

constexpr char kBase32Alphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";

// 5x7 bitmap font glyphs (35 bits each, row-major 5 bits x 7 rows) for the 32 Base32 characters:
// "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"
constexpr uint64_t kBase32Glyphs5x7[32] = {
    0x39111211Full, // '2'
    0x3910E111Eull, // '3'
    0x089529F10ull, // '4'
    0x7F07C111Eull, // '5'
    0x1D07C949Eull, // '6'
    0x7F1110842ull, // '7'
    0x3D49D294Eull, // '8'
    0x3D49E111Cull, // '9'
    0x1D1497F51ull, // 'A'
    0x7D49E525Eull, // 'B'
    0x3D420410Eull, // 'C'
    0x7D46318DEull, // 'D'
    0x7F03C411Full, // 'E'
    0x7F03C4108ull, // 'F'
    0x3D427494Eull, // 'G'
    0x4717F1471ull, // 'H'
    0x1F084254Eull, // 'J'
    0x472585251ull, // 'K'
    0x42084211Full, // 'L'
    0x477AB5471ull, // 'M'
    0x4736B58F1ull, // 'N'
    0x7D49E4108ull, // 'P'
    0x3D463594Dull, // 'Q'
    0x7D49E5251ull, // 'R'
    0x3D41C111Eull, // 'S'
    0x7F1084210ull, // 'T'
    0x47146314Eull, // 'U'
    0x471462944ull, // 'V'
    0x47146B555ull, // 'W'
    0x46A842A71ull, // 'X'
    0x46A842104ull, // 'Y'
    0x7F111111Full  // 'Z'
};

constexpr uint8_t kAiRefHmacKey[32] = {
    0x58, 0x48, 0x45, 0x59, 0x5F, 0x41, 0x49, 0x52,
    0x45, 0x46, 0x5F, 0x56, 0x45, 0x52, 0x49, 0x46,
    0x59, 0x5F, 0x48, 0x4D, 0x41, 0x43, 0x5F, 0x4B,
    0x45, 0x59, 0x5F, 0x56, 0x33, 0x5F, 0x30, 0x31
};

int g_code_detect_status = 1;
int g_timestamp_format = 0;
std::string g_code_detect_model_dir;

uint8_t ComputeCrc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x80u)
                ? static_cast<uint8_t>((crc << 1) ^ 0x31u)
                : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

// Encodes an N-byte binary payload (with last byte = CRC8) into Base32
std::string EncodeBytesToBase32(const uint8_t* raw, size_t byteLen, const std::string& salt = "") {
    std::vector<uint8_t> buf(raw, raw + byteLen);
    uint8_t saltMask = 0u;
    if (!salt.empty()) {
        saltMask = ComputeCrc8(reinterpret_cast<const uint8_t*>(salt.data()), salt.size());
    }
    uint8_t crcByte = buf[byteLen - 1];
    for (size_t i = 0; i + 1 < byteLen; ++i) {
        buf[i] ^= static_cast<uint8_t>(crcByte ^ saltMask ^ (0xA5u + static_cast<uint8_t>(i * 17u)));
    }

    std::string out;
    out.reserve((byteLen * 8 + 4) / 5);
    uint32_t bitBuffer = 0;
    int bitCount = 0;
    for (size_t i = 0; i < byteLen; ++i) {
        bitBuffer = (bitBuffer << 8) | buf[i];
        bitCount += 8;
        while (bitCount >= 5) {
            bitCount -= 5;
            uint32_t idx = (bitBuffer >> bitCount) & 0x1Fu;
            out.push_back(kBase32Alphabet[idx]);
        }
    }
    if (bitCount > 0) {
        uint32_t idx = (bitBuffer << (5 - bitCount)) & 0x1Fu;
        out.push_back(kBase32Alphabet[idx]);
    }
    return out;
}

bool DecodeBase32Bytes(const std::string& code, std::vector<uint8_t>& out) {
    std::string clean;
    for (char c : code) {
        if (c == '-' || c == ' ') continue;
        clean.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    size_t byteLen = 0;
    if (clean.size() == 20) {
        byteLen = 12; // 96-bit full precision V6/V7 payload
    } else if (clean.size() == 16) {
        byteLen = 10; // 80-bit legacy payload
    } else {
        return false;
    }

    out.assign(byteLen, 0);
    uint32_t bitBuffer = 0;
    int bitCount = 0;
    size_t byteIdx = 0;
    for (char c : clean) {
        const char* pos = std::strchr(kBase32Alphabet, c);
        if (!pos) return false;
        uint32_t val = static_cast<uint32_t>(pos - kBase32Alphabet);
        bitBuffer = (bitBuffer << 5) | val;
        bitCount += 5;
        if (bitCount >= 8 && byteIdx < byteLen) {
            bitCount -= 8;
            out[byteIdx++] = static_cast<uint8_t>((bitBuffer >> bitCount) & 0xFFu);
        }
    }
    if (byteIdx != byteLen) return false;

    uint8_t crcByte = out[byteLen - 1];
    for (size_t i = 0; i + 1 < byteLen; ++i) {
        out[i] ^= static_cast<uint8_t>(crcByte ^ (0xA5u + static_cast<uint8_t>(i * 17u)));
    }
    return ComputeCrc8(out.data(), byteLen - 1) == crcByte;
}

// Packs AntiCode Version 6 (Time + Longitude + Latitude + bwmId) with full 24-bit lon/lat precision (0.0001 deg)
std::string PackAntiCodeV6(int64_t timeS, double lon, double lat, int32_t bwmId) {
    uint8_t raw[12] = {0};
    uint32_t ts = static_cast<uint32_t>(timeS & 0xFFFFFFFFULL);
    double clampedLon = std::clamp(lon, -180.0, 180.0);
    double clampedLat = std::clamp(lat, -90.0, 90.0);
    uint32_t qLon = static_cast<uint32_t>(std::llround((clampedLon + 180.0) * 10000.0)); // [0 .. 3,600,000]
    uint32_t qLat = static_cast<uint32_t>(std::llround((clampedLat + 90.0) * 10000.0));  // [0 .. 1,800,000]

    raw[0]  = static_cast<uint8_t>((6u << 4) | (static_cast<uint32_t>(bwmId) & 0x0Fu));
    raw[1]  = static_cast<uint8_t>((ts >> 24) & 0xFFu);
    raw[2]  = static_cast<uint8_t>((ts >> 16) & 0xFFu);
    raw[3]  = static_cast<uint8_t>((ts >> 8)  & 0xFFu);
    raw[4]  = static_cast<uint8_t>(ts & 0xFFu);
    raw[5]  = static_cast<uint8_t>((qLon >> 16) & 0xFFu);
    raw[6]  = static_cast<uint8_t>((qLon >> 8)  & 0xFFu);
    raw[7]  = static_cast<uint8_t>(qLon & 0xFFu);
    raw[8]  = static_cast<uint8_t>((qLat >> 16) & 0xFFu);
    raw[9]  = static_cast<uint8_t>((qLat >> 8)  & 0xFFu);
    raw[10] = static_cast<uint8_t>(qLat & 0xFFu);
    raw[11] = ComputeCrc8(raw, 11);
    return EncodeBytesToBase32(raw, sizeof(raw));
}

// Packs AntiCode Version 7 (Time + LocationType + bwmId)
std::string PackAntiCodeV7(int64_t timeS, int64_t locationType, int32_t bwmId) {
    uint8_t raw[12] = {0};
    uint32_t ts = static_cast<uint32_t>(timeS & 0xFFFFFFFFULL);
    raw[0]  = static_cast<uint8_t>((7u << 4) | (static_cast<uint32_t>(bwmId) & 0x0Fu));
    raw[1]  = static_cast<uint8_t>((ts >> 24) & 0xFFu);
    raw[2]  = static_cast<uint8_t>((ts >> 16) & 0xFFu);
    raw[3]  = static_cast<uint8_t>((ts >> 8)  & 0xFFu);
    raw[4]  = static_cast<uint8_t>(ts & 0xFFu);
    raw[5]  = static_cast<uint8_t>(locationType & 0xFFu);
    raw[6]  = static_cast<uint8_t>(g_timestamp_format & 0xFFu);
    raw[7]  = static_cast<uint8_t>((static_cast<uint32_t>(bwmId) >> 4) & 0xFFu);
    raw[8]  = 0x42u;
    raw[9]  = 0x58u;
    raw[10] = 0x48u;
    raw[11] = ComputeCrc8(raw, 11);
    return EncodeBytesToBase32(raw, sizeof(raw));
}

int MatchGlyph5x7(uint64_t glyphBits, float* outConfidence = nullptr) {
    int bestIdx = 0;
    int bestHamming = 36;
    for (int i = 0; i < 32; ++i) {
        uint64_t diff = (glyphBits ^ kBase32Glyphs5x7[i]) & 0x7FFFFFFFFull;
        int dist = 0;
        while (diff) {
            diff &= (diff - 1);
            ++dist;
        }
        if (dist < bestHamming) {
            bestHamming = dist;
            bestIdx = i;
        }
    }
    if (outConfidence) {
        *outConfidence = std::clamp(1.0f - static_cast<float>(bestHamming) / 35.0f, 0.50f, 0.99f);
    }
    return bestIdx;
}

} // namespace

extern "C" {

// ============================================================================
// PART 1: CodeDetect JNI Functions (16 Functions)
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_init(
        JNIEnv* env, jclass /*clazz*/, jstring modelPath) {
    g_code_detect_model_dir = xhey::JStringToStdString(env, modelPath);
    g_code_detect_status = 1;
    LOGI("CodeDetect::init(%s) ready", g_code_detect_model_dir.c_str());
    return 0;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_destroy(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    g_code_detect_status = 0;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_getStatus(
        JNIEnv* /*env*/, jclass /*clazz*/) {
    return g_code_detect_status;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_check(
        JNIEnv* /*env*/, jclass /*clazz*/, jlong timeS, jdouble lon, jdouble lat) {
    bool validTime = (timeS > 1577836800L && timeS < 4102444800L);
    bool validGps  = (std::fabs(lon) <= 180.0 && std::fabs(lat) <= 90.0);
    return (validTime && validGps) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform(
        JNIEnv* env, jclass /*clazz*/, jlong timeS, jdouble lon, jdouble lat, jint bwmId) {
    std::string code = PackAntiCodeV6(timeS, lon, lat, bwmId);
    return env->NewStringUTF(code.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform2(
        JNIEnv* env, jclass /*clazz*/, jlong timeS, jlong locationType, jint bwmId) {
    std::string code = PackAntiCodeV7(timeS, locationType, bwmId);
    return env->NewStringUTF(code.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform22(
        JNIEnv* env, jclass /*clazz*/,
        jboolean hasGps, jlong timeS, jlong locationType,
        jdouble lon, jdouble lat, jstring /*salt*/, jint bwmId) {
    std::string code = (hasGps == JNI_TRUE)
        ? PackAntiCodeV6(timeS, lon, lat, bwmId)
        : PackAntiCodeV7(timeS, locationType, bwmId);
    return env->NewStringUTF(code.c_str());
}

JNIEXPORT jobject JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform3(
        JNIEnv* env, jclass /*clazz*/, jstring codeStr) {
    xhey::NativeCodeMetaData meta{};
    std::string code = xhey::JStringToStdString(env, codeStr);
    std::vector<uint8_t> raw;

    if (DecodeBase32Bytes(code, raw) && raw.size() >= 10) {
        meta.valid = 1;
        meta.version = (raw[0] >> 4) & 0x0F;
        meta.reserved = raw[0] & 0x0F;
        uint32_t ts = (static_cast<uint32_t>(raw[1]) << 24) |
                      (static_cast<uint32_t>(raw[2]) << 16) |
                      (static_cast<uint32_t>(raw[3]) << 8)  |
                      static_cast<uint32_t>(raw[4]);
        meta.timeStampS = static_cast<int64_t>(ts);

        if (meta.version == 6) {
            if (raw.size() == 12) {
                uint32_t qLon = (static_cast<uint32_t>(raw[5]) << 16) |
                                (static_cast<uint32_t>(raw[6]) << 8)  |
                                static_cast<uint32_t>(raw[7]);
                uint32_t qLat = (static_cast<uint32_t>(raw[8]) << 16) |
                                (static_cast<uint32_t>(raw[9]) << 8)  |
                                static_cast<uint32_t>(raw[10]);
                meta.longitude = (static_cast<double>(qLon) / 10000.0) - 180.0;
                meta.latitude  = (static_cast<double>(qLat) / 10000.0) - 90.0;
            } else {
                int32_t qLon = (static_cast<int32_t>(raw[5]) << 14) |
                               (static_cast<int32_t>(raw[6]) << 6)  |
                               (static_cast<int32_t>(raw[7] >> 2) & 0x3F);
                int32_t qLat = ((static_cast<int32_t>(raw[7]) & 0x03) << 19) |
                               (static_cast<int32_t>(raw[8]) << 11);
                meta.longitude = (static_cast<double>(qLon) / 10000.0) - 180.0;
                meta.latitude  = (static_cast<double>(qLat) / 10000.0) - 90.0;
            }
            meta.locationType = 1;
        } else {
            meta.locationType = raw[5];
            meta.timestampFormat = raw[6];
        }
    }
    return xhey::CreateJavaCodeMetaData(env, meta);
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform4(
        JNIEnv* /*env*/, jclass /*clazz*/, jint timestampFormat, jint /*reserved*/) {
    g_timestamp_format = timestampFormat;
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transformer(
        JNIEnv* env, jclass /*clazz*/) {
    return env->NewStringUTF("XHEY_ANTICODE_V6_V7");
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transformer2(
        JNIEnv* env, jclass /*clazz*/) {
    return env->NewStringUTF("f99779bd");
}

// Computes Variance of 3x3 Laplacian operator to evaluate image focus/sharpness
JNIEXPORT jfloat JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_laplacian(
        JNIEnv* env, jclass /*clazz*/, jbyteArray grayBytes, jint width, jint height) {
    if (!env || !grayBytes || width < 3 || height < 3) return 0.0f;
    int64_t required = static_cast<int64_t>(width) * height;
    jsize len = env->GetArrayLength(grayBytes);
    if (len < required) return 0.0f;

    std::vector<uint8_t> img(static_cast<size_t>(required));
    env->GetByteArrayRegion(grayBytes, 0, static_cast<jsize>(required), reinterpret_cast<jbyte*>(img.data()));

    double sum = 0.0;
    double sumSq = 0.0;
    int64_t count = 0;

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            int center = img[static_cast<size_t>(y) * width + x];
            int lap = img[static_cast<size_t>(y - 1) * width + x] +
                      img[static_cast<size_t>(y + 1) * width + x] +
                      img[static_cast<size_t>(y) * width + (x - 1)] +
                      img[static_cast<size_t>(y) * width + (x + 1)] - 4 * center;
            sum += lap;
            sumSq += static_cast<double>(lap) * lap;
            ++count;
        }
    }
    if (count == 0) return 0.0f;
    double mean = sum / count;
    double variance = (sumSq / count) - (mean * mean);
    return static_cast<jfloat>(variance);
}

// Computes Structural Similarity Index (SSIM) between two grayscale byte buffers
JNIEXPORT jfloat JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_ssim(
        JNIEnv* env, jclass /*clazz*/, jbyteArray img1Arr, jbyteArray img2Arr, jint width, jint height) {
    int64_t n = static_cast<int64_t>(width) * height;
    if (!env || !img1Arr || !img2Arr || width <= 0 || height <= 0 || n <= 0) return 0.0f;
    if (env->GetArrayLength(img1Arr) < n || env->GetArrayLength(img2Arr) < n) return 0.0f;

    std::vector<uint8_t> x(static_cast<size_t>(n)), y(static_cast<size_t>(n));
    env->GetByteArrayRegion(img1Arr, 0, static_cast<jsize>(n), reinterpret_cast<jbyte*>(x.data()));
    env->GetByteArrayRegion(img2Arr, 0, static_cast<jsize>(n), reinterpret_cast<jbyte*>(y.data()));

    double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumYY = 0.0, sumXY = 0.0;
    for (int64_t i = 0; i < n; ++i) {
        double vx = x[static_cast<size_t>(i)];
        double vy = y[static_cast<size_t>(i)];
        sumX  += vx;
        sumY  += vy;
        sumXX += vx * vx;
        sumYY += vy * vy;
        sumXY += vx * vy;
    }

    double muX = sumX / n;
    double muY = sumY / n;
    double sigmaX2 = (sumXX / n) - (muX * muX);
    double sigmaY2 = (sumYY / n) - (muY * muY);
    double sigmaXY = (sumXY / n) - (muX * muY);

    constexpr double C1 = (0.01 * 255) * (0.01 * 255);
    constexpr double C2 = (0.03 * 255) * (0.03 * 255);

    double num = (2.0 * muX * muY + C1) * (2.0 * sigmaXY + C2);
    double den = (muX * muX + muY * muY + C1) * (sigmaX2 + sigmaY2 + C2);
    return den > 0.0 ? static_cast<jfloat>(num / den) : 1.0f;
}

// Computes 64-bit Perceptual Difference Hash (dHash 9x8) of a Bitmap as a 16-char Hex string
JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_imageEncode(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap) {
    if (!env || !bitmap) return env ? env->NewStringUTF("") : nullptr;
    AndroidBitmapInfo info{};
    void* pixels = nullptr;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0 ||
        !xhey::IsSupportedBitmapFormat(info.format) ||
        info.width == 0 || info.height == 0 ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0 || !pixels) {
        return env->NewStringUTF("");
    }

    uint64_t dhash = 0;
    for (int r = 0; r < 8; ++r) {
        int y = static_cast<int>((static_cast<uint64_t>(r) * info.height) / 8);
        for (int c = 0; c < 8; ++c) {
            int x0 = static_cast<int>((static_cast<uint64_t>(c) * info.width) / 9);
            int x1 = static_cast<int>((static_cast<uint64_t>(c + 1) * info.width) / 9);
            int lum0 = xhey::ReadBitmapPixelLuma(info, pixels, x0, y);
            int lum1 = xhey::ReadBitmapPixelLuma(info, pixels, x1, y);
            dhash = (dhash << 1) | (lum0 > lum1 ? 1ULL : 0ULL);
        }
    }
    AndroidBitmap_unlockPixels(env, bitmap);

    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setw(16) << std::setfill('0') << dhash;
    return env->NewStringUTF(oss.str().c_str());
}

/**
 * Detects and decodes the AntiCode OCR strip inside the ROI of `bitmap` using
 * local adaptive thresholding + 5x7 glyph template correlation against kBase32Alphabet.
 */
JNIEXPORT jfloatArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_detect(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap, jfloatArray roiArr) {
    if (!env || !bitmap) return env ? env->NewFloatArray(0) : nullptr;

    AndroidBitmapInfo info{};
    void* pixels = nullptr;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0 ||
        !xhey::IsSupportedBitmapFormat(info.format) ||
        info.width < 16 || info.height < 8 ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0 || !pixels) {
        return env->NewFloatArray(0);
    }

    float roi[4] = {0.82f, 0.94f, 0.99f, 0.99f};
    if (roiArr && env->GetArrayLength(roiArr) >= 4) {
        env->GetFloatArrayRegion(roiArr, 0, 4, roi);
    }

    int w = static_cast<int>(info.width);
    int h = static_cast<int>(info.height);
    int xStart = std::clamp(static_cast<int>(roi[0] * w), 0, w - 2);
    int yStart = std::clamp(static_cast<int>(roi[1] * h), 0, h - 2);
    int xEnd   = std::clamp(static_cast<int>(roi[2] * w), xStart + 1, w);
    int yEnd   = std::clamp(static_cast<int>(roi[3] * h), yStart + 1, h);

    int roiW = xEnd - xStart;
    int roiH = yEnd - yStart;

    double gradSum = 0.0;
    double lumaSum = 0.0;
    int sampleCount = 0;
    for (int y = yStart; y < yEnd; ++y) {
        int prevLuma = xhey::ReadBitmapPixelLuma(info, pixels, xStart, y);
        lumaSum += prevLuma;
        for (int x = xStart + 1; x < xEnd; ++x) {
            int currLuma = xhey::ReadBitmapPixelLuma(info, pixels, x, y);
            gradSum += std::abs(currLuma - prevLuma);
            lumaSum += currLuma;
            prevLuma = currLuma;
            ++sampleCount;
        }
    }

    double avgGrad = (sampleCount > 0) ? (gradSum / sampleCount) : 0.0;
    int totalPixels = std::max(1, roiW * roiH);
    int meanThreshold = static_cast<int>(lumaSum / totalPixels);

    constexpr int kNumChars = 16;
    int charIndices[kNumChars] = {0};
    float confSum = 0.0f;

    for (int c = 0; c < kNumChars; ++c) {
        int cx0 = xStart + (c * roiW) / kNumChars;
        int cx1 = std::max(cx0 + 1, xStart + ((c + 1) * roiW) / kNumChars);

        // Compute local cell mean luminance to handle bright or dark text on watermark backgrounds
        int cellSum = 0, cellPixels = 0;
        for (int y = yStart; y < yEnd; ++y) {
            for (int x = cx0; x < cx1; ++x) {
                cellSum += xhey::ReadBitmapPixelLuma(info, pixels, x, y);
                ++cellPixels;
            }
        }
        int cellThresh = (cellPixels > 0) ? (cellSum / cellPixels) : meanThreshold;

        // Resample character cell into a 5x7 binary glyph grid
        uint64_t glyphBits = 0;
        for (int gy = 0; gy < 7; ++gy) {
            int sy = yStart + (gy * roiH) / 7;
            for (int gx = 0; gx < 5; ++gx) {
                int sx = cx0 + (gx * (cx1 - cx0)) / 5;
                int lum = xhey::ReadBitmapPixelLuma(info, pixels, sx, sy);
                uint64_t bit = (lum > cellThresh) ? 1ULL : 0ULL;
                glyphBits = (glyphBits << 1) | bit;
            }
        }
        float charConf = 0.0f;
        charIndices[c] = MatchGlyph5x7(glyphBits, &charConf);
        confSum += charConf;
    }

    AndroidBitmap_unlockPixels(env, bitmap);

    if (avgGrad < 4.0) {
        return env->NewFloatArray(0);
    }

    float score = std::clamp((confSum / kNumChars) * 0.7f + static_cast<float>(avgGrad / 64.0) * 0.3f, 0.55f, 0.99f);

    std::vector<float> packed;
    packed.reserve(3 + 8 + kNumChars);
    packed.push_back(4.0f);
    packed.push_back(static_cast<float>(kNumChars));
    packed.push_back(score);
    packed.push_back(static_cast<float>(xStart));
    packed.push_back(static_cast<float>(yStart));
    packed.push_back(static_cast<float>(xEnd));
    packed.push_back(static_cast<float>(yStart));
    packed.push_back(static_cast<float>(xEnd));
    packed.push_back(static_cast<float>(yEnd));
    packed.push_back(static_cast<float>(xStart));
    packed.push_back(static_cast<float>(yEnd));
    for (int c = 0; c < kNumChars; ++c) {
        packed.push_back(static_cast<float>(charIndices[c]));
    }

    return xhey::StdVectorToJFloatArray(env, packed);
}

// ============================================================================
// PART 2: AiRef Feature Extraction JNI Functions (10 Functions)
// ============================================================================

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeGetVersion(
        JNIEnv* env, jclass /*clazz*/) {
    return env->NewStringUTF("airef-3.0-f99779bd");
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncode(
        JNIEnv* env, jclass /*clazz*/, jbyteArray rgbBytes, jint width, jint height) {
    if (!env || !rgbBytes || width <= 0 || height <= 0) return nullptr;
    constexpr int kGrid = 16;
    std::vector<uint8_t> feature(kGrid * kGrid * 3 + 8, 0);
    std::memcpy(feature.data(), "AIREF03\0", 8);

    jsize len = env->GetArrayLength(rgbBytes);
    if (len <= 0) return nullptr;
    std::vector<uint8_t> rgb(static_cast<size_t>(len));
    env->GetByteArrayRegion(rgbBytes, 0, len, reinterpret_cast<jbyte*>(rgb.data()));

    int idx = 8;
    for (int gy = 0; gy < kGrid; ++gy) {
        int y = (gy * height) / kGrid;
        for (int gx = 0; gx < kGrid; ++gx) {
            int x = (gx * width) / kGrid;
            size_t offset = (static_cast<size_t>(y) * width + x) * 3;
            if (offset + 2 < rgb.size()) {
                feature[idx++] = rgb[offset + 0];
                feature[idx++] = rgb[offset + 1];
                feature[idx++] = rgb[offset + 2];
            }
        }
    }

    return xhey::StdVectorToJByteArray(env, feature);
}

namespace {

jbyteArray EncodeBitmapFeatureInternal(
        JNIEnv* env, jobject bitmap, jint shortSide, jint longSide, jint quality, const char magic[4], bool includeAlphaHash) {
    if (!env || !bitmap) return nullptr;
    AndroidBitmapInfo info{};
    void* pixels = nullptr;
    if (AndroidBitmap_getInfo(env, bitmap, &info) < 0 ||
        !xhey::IsSupportedBitmapFormat(info.format) ||
        info.width == 0 || info.height == 0 ||
        AndroidBitmap_lockPixels(env, bitmap, &pixels) < 0 || !pixels) {
        return nullptr;
    }

    int srcW = static_cast<int>(info.width);
    int srcH = static_cast<int>(info.height);
    int dstW = shortSide;
    int dstH = shortSide;
    if (srcW <= srcH && srcW > 0) {
        dstW = shortSide;
        dstH = std::min(longSide, static_cast<int>(std::round(static_cast<double>(srcH) * shortSide / srcW)));
    } else if (srcH > 0) {
        dstH = shortSide;
        dstW = std::min(longSide, static_cast<int>(std::round(static_cast<double>(srcW) * shortSide / srcH)));
    }
    dstW = std::clamp(dstW, 16, 512);
    dstH = std::clamp(dstH, 16, 512);

    const int gridW = 32, gridH = 32;
    std::vector<uint8_t> packet(16 + gridW * gridH * 3 + 4, 0);
    std::memcpy(packet.data(), magic, 4);
    std::memcpy(packet.data() + 4, &info.width, 4);
    std::memcpy(packet.data() + 8, &info.height, 4);
    uint16_t dw = static_cast<uint16_t>(dstW);
    uint16_t dh = static_cast<uint16_t>(dstH);
    std::memcpy(packet.data() + 12, &dw, 2);
    std::memcpy(packet.data() + 14, &dh, 2);

    int qStep = std::max(1, (101 - std::clamp(quality, 1, 100)) / 10);
    int writePos = 16;
    uint32_t rollingCrc = 0x811C9DC5u;
    for (int gy = 0; gy < gridH; ++gy) {
        int sy = static_cast<int>((static_cast<uint64_t>(gy) * info.height) / gridH);
        for (int gx = 0; gx < gridW; ++gx) {
            int sx = static_cast<int>((static_cast<uint64_t>(gx) * info.width) / gridW);
            uint8_t pr = 0, pg = 0, pb = 0, pa = 255;
            xhey::ReadBitmapPixelRgba(info, pixels, sx, sy, pr, pg, pb, pa);
            uint8_t r = static_cast<uint8_t>((pr / qStep) * qStep);
            uint8_t g = static_cast<uint8_t>((pg / qStep) * qStep);
            uint8_t b = static_cast<uint8_t>((pb / qStep) * qStep);
            packet[writePos++] = r;
            packet[writePos++] = g;
            packet[writePos++] = b;
            rollingCrc = (rollingCrc ^ r) * 0x01000193u;
            rollingCrc = (rollingCrc ^ g) * 0x01000193u;
            rollingCrc = (rollingCrc ^ b) * 0x01000193u;
            if (includeAlphaHash) {
                rollingCrc = (rollingCrc ^ pa) * 0x01000193u;
            }
        }
    }
    std::memcpy(packet.data() + writePos, &rollingCrc, 4);
    AndroidBitmap_unlockPixels(env, bitmap);

    return xhey::StdVectorToJByteArray(env, packet);
}

jbyteArray EncodeRgbArrayFeatureInternal(
        JNIEnv* env, jbyteArray rgbBytes, jint width, jint height, jint shortSide, jint longSide, jint quality, const char magic[4]) {
    if (!env || !rgbBytes || width <= 0 || height <= 0) return nullptr;
    jsize len = env->GetArrayLength(rgbBytes);
    if (len <= 0) return nullptr;
    std::vector<uint8_t> rgb(static_cast<size_t>(len));
    env->GetByteArrayRegion(rgbBytes, 0, len, reinterpret_cast<jbyte*>(rgb.data()));

    int dstW = shortSide;
    int dstH = shortSide;
    if (width <= height) {
        dstW = shortSide;
        dstH = std::min(longSide, static_cast<int>(std::round(static_cast<double>(height) * shortSide / width)));
    } else {
        dstH = shortSide;
        dstW = std::min(longSide, static_cast<int>(std::round(static_cast<double>(width) * shortSide / height)));
    }
    dstW = std::clamp(dstW, 16, 512);
    dstH = std::clamp(dstH, 16, 512);

    const int gridW = 32, gridH = 32;
    std::vector<uint8_t> packet(16 + gridW * gridH * 3 + 4, 0);
    std::memcpy(packet.data(), magic, 4);
    uint32_t uw = static_cast<uint32_t>(width);
    uint32_t uh = static_cast<uint32_t>(height);
    std::memcpy(packet.data() + 4, &uw, 4);
    std::memcpy(packet.data() + 8, &uh, 4);
    uint16_t dw = static_cast<uint16_t>(dstW);
    uint16_t dh = static_cast<uint16_t>(dstH);
    std::memcpy(packet.data() + 12, &dw, 2);
    std::memcpy(packet.data() + 14, &dh, 2);

    int qStep = std::max(1, (101 - std::clamp(quality, 1, 100)) / 10);
    int writePos = 16;
    uint32_t rollingCrc = 0x811C9DC5u;
    for (int gy = 0; gy < gridH; ++gy) {
        int sy = static_cast<int>((static_cast<uint64_t>(gy) * height) / gridH);
        for (int gx = 0; gx < gridW; ++gx) {
            int sx = static_cast<int>((static_cast<uint64_t>(gx) * width) / gridW);
            size_t offset = (static_cast<size_t>(sy) * width + sx) * 3;
            uint8_t pr = 0, pg = 0, pb = 0;
            if (offset + 2 < rgb.size()) {
                pr = rgb[offset + 0];
                pg = rgb[offset + 1];
                pb = rgb[offset + 2];
            }
            uint8_t r = static_cast<uint8_t>((pr / qStep) * qStep);
            uint8_t g = static_cast<uint8_t>((pg / qStep) * qStep);
            uint8_t b = static_cast<uint8_t>((pb / qStep) * qStep);
            packet[writePos++] = r;
            packet[writePos++] = g;
            packet[writePos++] = b;
            rollingCrc = (rollingCrc ^ r) * 0x01000193u;
            rollingCrc = (rollingCrc ^ g) * 0x01000193u;
            rollingCrc = (rollingCrc ^ b) * 0x01000193u;
        }
    }
    std::memcpy(packet.data() + writePos, &rollingCrc, 4);
    return xhey::StdVectorToJByteArray(env, packet);
}

} // namespace

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeAiRefShortFirstFromBitmap(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap, jint shortSide, jint longSide, jint quality) {
    return EncodeBitmapFeatureInternal(env, bitmap, shortSide, longSide, quality, "ARSF", false);
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeReferenceFeatureFromBitmap(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap, jint shortSide, jint longSide, jint quality) {
    return EncodeBitmapFeatureInternal(env, bitmap, shortSide, longSide, quality, "ARRF", true);
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeFromBitmap(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap) {
    return EncodeBitmapFeatureInternal(env, bitmap, 400, 400, 65, "ARBM", false);
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeAiRefShortFirst(
        JNIEnv* env, jclass /*clazz*/, jbyteArray rgb, jint w, jint h, jint s, jint l, jint q) {
    return EncodeRgbArrayFeatureInternal(env, rgb, w, h, s, l, q, "ARSB");
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeReferenceFeature(
        JNIEnv* env, jclass /*clazz*/, jbyteArray rgb, jint w, jint h, jint s, jint l, jint q) {
    return EncodeRgbArrayFeatureInternal(env, rgb, w, h, s, l, q, "ARRB");
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeFromBitmapBench(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap, jlongArray timings, jint loops) {
    int iterations = std::max(1, static_cast<int>(loops));
    jbyteArray lastResult = nullptr;
    auto tStartAll = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i) {
        if (lastResult) env->DeleteLocalRef(lastResult);
        lastResult = EncodeBitmapFeatureInternal(env, bitmap, 400, 400, 65, "ARBN", false);
    }

    auto tEndAll = std::chrono::steady_clock::now();
    int64_t totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(tEndAll - tStartAll).count();
    if (timings && env->GetArrayLength(timings) >= 1) {
        jlong avgNs = static_cast<jlong>(totalNs / iterations);
        env->SetLongArrayRegion(timings, 0, 1, &avgNs);
    }
    return lastResult;
}

JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeFromBitmapBenchRgba(
        JNIEnv* env, jclass /*clazz*/, jobject bitmap, jlongArray timings, jint loops) {
    int iterations = std::max(1, static_cast<int>(loops));
    jbyteArray lastResult = nullptr;
    auto tStartAll = std::chrono::steady_clock::now();

    for (int i = 0; i < iterations; ++i) {
        if (lastResult) env->DeleteLocalRef(lastResult);
        lastResult = EncodeBitmapFeatureInternal(env, bitmap, 400, 400, 65, "ARBG", true);
    }

    auto tEndAll = std::chrono::steady_clock::now();
    int64_t totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(tEndAll - tStartAll).count();
    if (timings && env->GetArrayLength(timings) >= 1) {
        jlong avgNs = static_cast<jlong>(totalNs / iterations);
        env->SetLongArrayRegion(timings, 0, 1, &avgNs);
    }
    return lastResult;
}

// Packs Feature Bytes + Metadata JSON into a signed binary verification container with 32-byte HMAC-SHA256
JNIEXPORT jbyteArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativePackVerifyFile(
        JNIEnv* env, jclass /*clazz*/, jbyteArray featureArr, jint version, jbyteArray metaJsonArr) {
    if (!env || !featureArr || !metaJsonArr) return nullptr;
    jsize featLen = env->GetArrayLength(featureArr);
    jsize metaLen = env->GetArrayLength(metaJsonArr);
    if (featLen < 0 || metaLen < 0) return nullptr;

    size_t bodyLen = 20u + static_cast<size_t>(featLen) + static_cast<size_t>(metaLen);
    std::vector<uint8_t> out(bodyLen + 32u, 0);
    std::memcpy(out.data(), "XHEYVRFY", 8);
    uint32_t ver = static_cast<uint32_t>(version);
    uint32_t fl  = static_cast<uint32_t>(featLen);
    uint32_t ml  = static_cast<uint32_t>(metaLen);
    std::memcpy(out.data() + 8,  &ver, 4);
    std::memcpy(out.data() + 12, &fl,  4);
    std::memcpy(out.data() + 16, &ml,  4);

    if (featLen > 0) {
        env->GetByteArrayRegion(featureArr,  0, featLen, reinterpret_cast<jbyte*>(out.data() + 20));
    }
    if (metaLen > 0) {
        env->GetByteArrayRegion(metaJsonArr, 0, metaLen, reinterpret_cast<jbyte*>(out.data() + 20 + featLen));
    }

    // Compute 32-byte RFC 2104 HMAC-SHA256 signature over header + feature + metaJson
    uint8_t hmacSig[32] = {0};
    xhey::ComputeHmacSha256(kAiRefHmacKey, sizeof(kAiRefHmacKey), out.data(), bodyLen, hmacSig);
    std::memcpy(out.data() + bodyLen, hmacSig, 32);

    return xhey::StdVectorToJByteArray(env, out);
}

} // extern "C"
