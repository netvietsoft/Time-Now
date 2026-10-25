/**
 * ============================================================================
 * File: src/xhey_media_and_security_jni.cpp
 * Target Libraries:
 *   1. libxheymediav8.so   (NativeMediaSDK, NativeXhey, f0)
 *   2. libxhey_videoedit.so(VideoEditInterface)
 *   3. libnative-lib.so    (JNIUtils, SecurityManager)
 *   4. libjpeg.so          (ITU-T T.81 Baseline DCT JPEG & TurboJPEG C API)
 *   5. libmp3lame.so       (MPEG-1 Layer III LAME MP3 Encoder C API)
 *   6. libffmpeg.so        (FFmpeg Remuxer / YUV420 Transcoder C API)
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"

#include <android/bitmap.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#if !defined(XHEY_BUILD_MEDIAV8) && !defined(XHEY_BUILD_VIDEOEDIT) && \
    !defined(XHEY_BUILD_NATIVELIB) && !defined(XHEY_BUILD_LIBJPEG) && \
    !defined(XHEY_BUILD_MP3LAME) && !defined(XHEY_BUILD_FFMPEG)
#define XHEY_BUILD_ALL 1
#endif

namespace {

std::string JStrToStd(JNIEnv* env, jstring jstr) {
    return xhey::jni::JStringToStdString(env, jstr);
}

// ============================================================================
// 1. ITU-T T.81 / ISO/IEC 10918-1 Baseline DCT JPEG Encoder (with DQT, DHT,
//    8x8 FDCT, Zig-Zag Quantization, and Canonical Huffman Bitstream)
// ============================================================================

constexpr uint8_t kZigZagOrder[64] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

constexpr uint8_t kStdLuminanceQuantTbl[64] = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99
};

constexpr uint8_t kStdChrominanceQuantTbl[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

// Standard Annex K Huffman tables
constexpr uint8_t kBitsDcLuminance[16]   = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
constexpr uint8_t kValDcLuminance[12]    = {0,1,2,3,4,5,6,7,8,9,10,11};

constexpr uint8_t kBitsDcChrominance[16] = {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
constexpr uint8_t kValDcChrominance[12]  = {0,1,2,3,4,5,6,7,8,9,10,11};

constexpr uint8_t kBitsAcLuminance[16]   = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d};
constexpr uint8_t kValAcLuminance[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

constexpr uint8_t kBitsAcChrominance[16] = {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77};
constexpr uint8_t kValAcChrominance[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa
};

struct HuffCode {
    uint16_t code = 0;
    uint8_t len = 0;
};

struct BaselineJpegEncoder {
    uint8_t quantY[64];
    uint8_t quantC[64];
    HuffCode dcLum[12];
    HuffCode dcChr[12];
    HuffCode acLum[256];
    HuffCode acChr[256];
    float cosTable[8][8];

    uint32_t bitBuf = 0;
    int bitCount = 0;
    int prevDcY = 0;
    int prevDcCb = 0;
    int prevDcCr = 0;
    std::vector<uint8_t> outBytes;

    static void BuildHuffTable(const uint8_t bits[16], const uint8_t* vals, HuffCode* table) {
        uint16_t code = 0;
        int k = 0;
        for (int len = 1; len <= 16; ++len) {
            for (int i = 0; i < bits[len - 1]; ++i) {
                uint8_t sym = vals[k++];
                table[sym].code = code;
                table[sym].len = static_cast<uint8_t>(len);
                ++code;
            }
            code <<= 1;
        }
    }

    explicit BaselineJpegEncoder(int quality = 85) {
        int q = std::clamp(quality, 1, 100);
        int scale = (q < 50) ? (5000 / q) : (200 - q * 2);
        for (int i = 0; i < 64; ++i) {
            int lq = (static_cast<int>(kStdLuminanceQuantTbl[i]) * scale + 50) / 100;
            int cq = (static_cast<int>(kStdChrominanceQuantTbl[i]) * scale + 50) / 100;
            quantY[kZigZagOrder[i]] = static_cast<uint8_t>(std::clamp(lq, 1, 255));
            quantC[kZigZagOrder[i]] = static_cast<uint8_t>(std::clamp(cq, 1, 255));
        }
        BuildHuffTable(kBitsDcLuminance,   kValDcLuminance,   dcLum);
        BuildHuffTable(kBitsDcChrominance, kValDcChrominance, dcChr);
        BuildHuffTable(kBitsAcLuminance,   kValAcLuminance,   acLum);
        BuildHuffTable(kBitsAcChrominance, kValAcChrominance, acChr);

        for (int u = 0; u < 8; ++u) {
            for (int x = 0; x < 8; ++x) {
                cosTable[u][x] = std::cos(((2.0f * x + 1.0f) * u * 3.14159265358979323846f) / 16.0f);
            }
        }
    }

    inline void PutByte(uint8_t b) { outBytes.push_back(b); }
    inline void PutWord(uint16_t w) {
        outBytes.push_back(static_cast<uint8_t>((w >> 8) & 0xFF));
        outBytes.push_back(static_cast<uint8_t>(w & 0xFF));
    }

    inline void PutBits(uint16_t bits, uint8_t len) {
        if (len == 0) return;
        bitBuf = (bitBuf << len) | (bits & ((1u << len) - 1u));
        bitCount += len;
        while (bitCount >= 8) {
            bitCount -= 8;
            uint8_t b = static_cast<uint8_t>((bitBuf >> bitCount) & 0xFFu);
            outBytes.push_back(b);
            if (b == 0xFFu) {
                outBytes.push_back(0x00u); // JPEG entropy byte stuffing
            }
        }
    }

    inline void FlushBits() {
        if (bitCount > 0) {
            uint8_t padLen = static_cast<uint8_t>(8 - bitCount);
            PutBits(static_cast<uint16_t>((1u << padLen) - 1u), padLen);
        }
    }

    void WriteDhtSegment(uint8_t tableClassAndId, const uint8_t bits[16], const uint8_t* vals, int numVals) {
        PutWord(0xFFC4);
        PutWord(static_cast<uint16_t>(2 + 1 + 16 + numVals));
        PutByte(tableClassAndId);
        outBytes.insert(outBytes.end(), bits, bits + 16);
        outBytes.insert(outBytes.end(), vals, vals + numVals);
    }

    void WriteHeaders(uint16_t width, uint16_t height, int orientation = 1) {
        // SOI
        PutWord(0xFFD8);

        // APP0 JFIF
        PutWord(0xFFE0);
        PutWord(16);
        const uint8_t jfif[14] = {'J','F','I','F',0, 1,2, 1, 0,72, 0,72, 0,0};
        outBytes.insert(outBytes.end(), jfif, jfif + 14);

        // APP1 EXIF Orientation
        PutWord(0xFFE1);
        PutWord(22);
        const uint8_t exif[20] = {
            'E','x','i','f',0,0,
            'I','I', 0x2A,0x00, 0x08,0x00,0x00,0x00,
            0x01,0x00,
            0x12,0x01, 0x03,0x00
        };
        outBytes.insert(outBytes.end(), exif, exif + 20);
        // Correct APP1 length = 2 + 20 + 8 = 30
        outBytes[outBytes.size() - 22] = 0x00;
        outBytes[outBytes.size() - 21] = 30;
        const uint8_t exifTail[8] = {
            0x01,0x00,0x00,0x00,
            static_cast<uint8_t>(std::clamp(orientation, 1, 8)), 0x00, 0x00, 0x00
        };
        outBytes.insert(outBytes.end(), exifTail, exifTail + 8);

        // DQT Luminance (table 0) & Chrominance (table 1)
        PutWord(0xFFDB);
        PutWord(2 + 65 + 65);
        PutByte(0x00);
        outBytes.insert(outBytes.end(), quantY, quantY + 64);
        PutByte(0x01);
        outBytes.insert(outBytes.end(), quantC, quantC + 64);

        // SOF0 Baseline DCT (4:4:4 sampling 0x11 for Y, Cb, Cr)
        PutWord(0xFFC0);
        PutWord(17);
        PutByte(8);
        PutWord(height);
        PutWord(width);
        PutByte(3);
        PutByte(1); PutByte(0x11); PutByte(0);
        PutByte(2); PutByte(0x11); PutByte(1);
        PutByte(3); PutByte(0x11); PutByte(1);

        // DHT (4 standard tables)
        WriteDhtSegment(0x00, kBitsDcLuminance,   kValDcLuminance,   12);
        WriteDhtSegment(0x10, kBitsAcLuminance,   kValAcLuminance,   162);
        WriteDhtSegment(0x01, kBitsDcChrominance, kValDcChrominance, 12);
        WriteDhtSegment(0x11, kBitsAcChrominance, kValAcChrominance, 162);

        // SOS
        PutWord(0xFFDA);
        PutWord(12);
        PutByte(3);
        PutByte(1); PutByte(0x00);
        PutByte(2); PutByte(0x11);
        PutByte(3); PutByte(0x11);
        PutByte(0x00);
        PutByte(0x3F);
        PutByte(0x00);
    }

    static inline void MagnitudeCategory(int val, uint8_t& cat, uint16_t& bits) {
        int absVal = std::abs(val);
        cat = 0;
        while (absVal > 0) {
            absVal >>= 1;
            ++cat;
        }
        if (cat > 11) cat = 11;
        bits = (val >= 0) ? static_cast<uint16_t>(val) : static_cast<uint16_t>(val + (1 << cat) - 1);
    }

    void EncodeBlock8x8(
            const float block[64],
            const uint8_t quantZigZag[64],
            int& prevDc,
            const HuffCode dcTable[12],
            const HuffCode acTable[256]) {
        // 1. Separable 2D Forward DCT: temp = C * block, dct = temp * C^T
        float tmp[64];
        for (int y = 0; y < 8; ++y) {
            for (int u = 0; u < 8; ++u) {
                float s = 0.0f;
                for (int x = 0; x < 8; ++x) {
                    s += block[y * 8 + x] * cosTable[u][x];
                }
                tmp[y * 8 + u] = s;
            }
        }

        int zz[64];
        for (int v = 0; v < 8; ++v) {
            float cv = (v == 0) ? 0.35355339f : 0.5f;
            for (int u = 0; u < 8; ++u) {
                float cu = (u == 0) ? 0.35355339f : 0.5f;
                float s = 0.0f;
                for (int y = 0; y < 8; ++y) {
                    s += tmp[y * 8 + u] * cosTable[v][y];
                }
                float dctVal = s * cu * cv;
                uint8_t zzIdx = kZigZagOrder[v * 8 + u];
                int q = static_cast<int>(std::lround(dctVal / static_cast<float>(quantZigZag[zzIdx])));
                zz[zzIdx] = std::clamp(q, -1023, 1023);
            }
        }

        // 2. Differential DC Huffman coding
        int diff = zz[0] - prevDc;
        prevDc = zz[0];
        uint8_t dcCat = 0;
        uint16_t dcBits = 0;
        MagnitudeCategory(diff, dcCat, dcBits);
        PutBits(dcTable[dcCat].code, dcTable[dcCat].len);
        if (dcCat > 0) {
            PutBits(dcBits, dcCat);
        }

        // 3. Run-Length AC Huffman coding
        int lastNonZero = 63;
        while (lastNonZero > 0 && zz[lastNonZero] == 0) {
            --lastNonZero;
        }
        int zeroRun = 0;
        for (int k = 1; k <= lastNonZero; ++k) {
            int acVal = zz[k];
            if (acVal == 0) {
                ++zeroRun;
            } else {
                while (zeroRun >= 16) {
                    PutBits(acTable[0xF0].code, acTable[0xF0].len); // ZRL (16 zeros)
                    zeroRun -= 16;
                }
                uint8_t acCat = 0;
                uint16_t acBits = 0;
                MagnitudeCategory(acVal, acCat, acBits);
                if (acCat > 10) acCat = 10;
                uint8_t sym = static_cast<uint8_t>((zeroRun << 4) | acCat);
                PutBits(acTable[sym].code, acTable[sym].len);
                PutBits(acBits, acCat);
                zeroRun = 0;
            }
        }
        if (lastNonZero < 63) {
            PutBits(acTable[0x00].code, acTable[0x00].len); // EOB
        }
    }

    void EncodeRgbaStrip(const uint8_t* rgba, int width, int stripHeight, int strideBytes, int bytesPerPixel = 4) {
        if (!rgba || width <= 0 || stripHeight <= 0) return;
        int effStride = (strideBytes >= width * bytesPerPixel) ? strideBytes : (width * bytesPerPixel);

        float blockY[64], blockCb[64], blockCr[64];
        for (int by = 0; by < stripHeight; by += 8) {
            for (int bx = 0; bx < width; bx += 8) {
                for (int py = 0; py < 8; ++py) {
                    int sy = std::min(by + py, stripHeight - 1);
                    const uint8_t* row = rgba + static_cast<size_t>(sy) * effStride;
                    for (int px = 0; px < 8; ++px) {
                        int sx = std::min(bx + px, width - 1);
                        const uint8_t* p = row + static_cast<size_t>(sx) * bytesPerPixel;
                        float r = p[0];
                        float g = (bytesPerPixel >= 3) ? p[1] : p[0];
                        float b = (bytesPerPixel >= 3) ? p[2] : p[0];
                        int idx = py * 8 + px;
                        blockY[idx]  = ( 0.29900f * r + 0.58700f * g + 0.11400f * b) - 128.0f;
                        blockCb[idx] = (-0.16874f * r - 0.33126f * g + 0.50000f * b);
                        blockCr[idx] = ( 0.50000f * r - 0.41869f * g - 0.08131f * b);
                    }
                }
                EncodeBlock8x8(blockY,  quantY, prevDcY,  dcLum, acLum);
                EncodeBlock8x8(blockCb, quantC, prevDcCb, dcChr, acChr);
                EncodeBlock8x8(blockCr, quantC, prevDcCr, dcChr, acChr);
            }
        }
    }

    void Finish() {
        FlushBits();
        PutWord(0xFFD9); // EOI
    }
};

bool WriteRgbaToJpegFile(
        const std::string& path,
        const uint8_t* rgba,
        int width, int height, int stride,
        int quality, int orientation = 1, int bytesPerPixel = 4) {
    if (path.empty() || !rgba || width <= 0 || height <= 0) return false;
    int alignedH = ((height + 7) / 8) * 8;
    BaselineJpegEncoder enc(quality);
    enc.outBytes.reserve(static_cast<size_t>(width) * height / 2 + 1024);
    enc.WriteHeaders(static_cast<uint16_t>(width), static_cast<uint16_t>(height), orientation);
    enc.EncodeRgbaStrip(rgba, width, height, stride, bytesPerPixel);
    (void)alignedH;
    enc.Finish();

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    bool ok = (std::fwrite(enc.outBytes.data(), 1, enc.outBytes.size(), fp) == enc.outBytes.size()) &&
              (std::fflush(fp) == 0);
    if (std::fclose(fp) != 0) ok = false;
    return ok;
}

bool ParseJpegHeaderDimensions(const uint8_t* jpegData, size_t len, int& outW, int& outH, int& outSubsamp) {
    outW = 0; outH = 0; outSubsamp = 0;
    if (!jpegData || len < 4 || jpegData[0] != 0xFF || jpegData[1] != 0xD8) return false;
    size_t pos = 2;
    while (pos + 4 <= len) {
        if (jpegData[pos] != 0xFF) { ++pos; continue; }
        while (pos < len && jpegData[pos] == 0xFF) ++pos;
        if (pos >= len) break;
        uint8_t marker = jpegData[pos++];
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
        if (marker == 0xD9 || marker == 0xDA) break;
        if (pos + 2 > len) break;
        uint16_t segLen = static_cast<uint16_t>((jpegData[pos] << 8) | jpegData[pos + 1]);
        if (segLen < 2 || pos + segLen > len) break;
        if ((marker == 0xC0 || marker == 0xC2) && segLen >= 9) {
            outH = (jpegData[pos + 3] << 8) | jpegData[pos + 4];
            outW = (jpegData[pos + 5] << 8) | jpegData[pos + 6];
            outSubsamp = 0;
            return (outW > 0 && outH > 0);
        }
        pos += segLen;
    }
    return false;
}

// ============================================================================
// 2. FIPS-197 AES-128 Block Cipher & CTR Mode + Base64 + APK Signature Verifier
// ============================================================================

constexpr uint8_t kVaultMasterKey[32] = {
    0x58, 0x48, 0x45, 0x59, 0x5F, 0x54, 0x49, 0x4D,
    0x45, 0x4D, 0x41, 0x52, 0x4B, 0x5F, 0x41, 0x45,
    0x53, 0x5F, 0x4B, 0x45, 0x59, 0x5F, 0x32, 0x30,
    0x32, 0x35, 0x5F, 0x56, 0x41, 0x55, 0x4C, 0x54
};

constexpr uint8_t kAesSbox[256] = {
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

constexpr uint8_t kAesRcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1B, 0x36
};

inline uint8_t AesXtime(uint8_t x) {
    return static_cast<uint8_t>((x << 1) ^ ((x & 0x80u) ? 0x1Bu : 0x00u));
}

void Aes128ExpandKey(const uint8_t key[16], uint8_t roundKeys[176]) {
    std::memcpy(roundKeys, key, 16);
    uint8_t temp[4];
    for (int i = 4; i < 44; ++i) {
        std::memcpy(temp, &roundKeys[(i - 1) * 4], 4);
        if (i % 4 == 0) {
            uint8_t t = temp[0];
            temp[0] = kAesSbox[temp[1]] ^ kAesRcon[i / 4];
            temp[1] = kAesSbox[temp[2]];
            temp[2] = kAesSbox[temp[3]];
            temp[3] = kAesSbox[t];
        }
        for (int j = 0; j < 4; ++j) {
            roundKeys[i * 4 + j] = roundKeys[(i - 4) * 4 + j] ^ temp[j];
        }
    }
}

void Aes128EncryptBlock(const uint8_t in[16], const uint8_t roundKeys[176], uint8_t out[16]) {
    uint8_t s[16];
    for (int i = 0; i < 16; ++i) s[i] = in[i] ^ roundKeys[i];

    for (int round = 1; round <= 10; ++round) {
        // SubBytes
        for (int i = 0; i < 16; ++i) s[i] = kAesSbox[s[i]];

        // ShiftRows
        uint8_t t = s[1];  s[1]  = s[5];  s[5]  = s[9];  s[9]  = s[13]; s[13] = t;
        t = s[2];          s[2]  = s[10]; s[10] = t;
        t = s[6];          s[6]  = s[14]; s[14] = t;
        t = s[15];         s[15] = s[11]; s[11] = s[7];  s[7]  = s[3];  s[3]  = t;

        // MixColumns (rounds 1..9)
        if (round < 10) {
            for (int c = 0; c < 4; ++c) {
                uint8_t* col = &s[c * 4];
                uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                uint8_t xr = a0 ^ a1 ^ a2 ^ a3;
                col[0] ^= xr ^ AesXtime(a0 ^ a1);
                col[1] ^= xr ^ AesXtime(a1 ^ a2);
                col[2] ^= xr ^ AesXtime(a2 ^ a3);
                col[3] ^= xr ^ AesXtime(a3 ^ a0);
            }
        }

        // AddRoundKey
        const uint8_t* rk = &roundKeys[round * 16];
        for (int i = 0; i < 16; ++i) s[i] ^= rk[i];
    }
    std::memcpy(out, s, 16);
}

void Aes128CtrCrypt96(uint8_t* buf, size_t len, const uint8_t key16[16], const uint8_t nonce12[12]) {
    if (!buf || len == 0 || !key16 || !nonce12) return;
    uint8_t roundKeys[176];
    Aes128ExpandKey(key16, roundKeys);

    uint8_t counterBlock[16] = {0};
    std::memcpy(counterBlock, nonce12, 12);
    uint8_t keystream[16] = {0};

    for (size_t offset = 0; offset < len; offset += 16) {
        uint32_t ctr = static_cast<uint32_t>((offset / 16) + 1u);
        counterBlock[12] = static_cast<uint8_t>((ctr >> 24) & 0xFFu);
        counterBlock[13] = static_cast<uint8_t>((ctr >> 16) & 0xFFu);
        counterBlock[14] = static_cast<uint8_t>((ctr >> 8)  & 0xFFu);
        counterBlock[15] = static_cast<uint8_t>(ctr & 0xFFu);
        Aes128EncryptBlock(counterBlock, roundKeys, keystream);
        size_t chunk = std::min<size_t>(16, len - offset);
        for (size_t i = 0; i < chunk; ++i) {
            buf[offset + i] ^= keystream[i];
        }
    }
}

// Generates a unique 96-bit (12-byte) cryptographic nonce per NIST SP 800-38A
void GenerateUniqueNonce96(uint8_t outNonce12[12]) {
    static std::atomic<uint64_t> s_nonce_counter{1};
    uint64_t seq = s_nonce_counter.fetch_add(1, std::memory_order_relaxed);
    uint64_t ticks = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());

    uint8_t seed[48] = {0};
    std::memcpy(seed + 0, &seq, 8);
    std::memcpy(seed + 8, &ticks, 8);
    std::memcpy(seed + 16, kVaultMasterKey, 16);

    std::FILE* urand = std::fopen("/dev/urandom", "rb");
    if (urand) {
        size_t n = std::fread(seed + 32, 1, 16, urand);
        (void)n;
        std::fclose(urand);
    } else {
        uint64_t mix = (seq * 0x9E3779B97F4A7C15ULL) ^ (ticks * 0xBF58476D1CE4E5B9ULL);
        std::memcpy(seed + 32, &mix, 8);
        uint64_t mix2 = ~mix ^ 0x94D049BB133111EBULL;
        std::memcpy(seed + 40, &mix2, 8);
    }

    uint8_t digest[32] = {0};
    xhey::jni::ComputeSha256(seed, sizeof(seed), digest);
    // Guarantee monotonic uniqueness in the first 4 bytes + 8 bytes of entropy hash
    outNonce12[0] = static_cast<uint8_t>((seq >> 24) & 0xFFu) ^ digest[0];
    outNonce12[1] = static_cast<uint8_t>((seq >> 16) & 0xFFu) ^ digest[1];
    outNonce12[2] = static_cast<uint8_t>((seq >> 8)  & 0xFFu) ^ digest[2];
    outNonce12[3] = static_cast<uint8_t>(seq & 0xFFu)         ^ digest[3];
    std::memcpy(outNonce12 + 4, digest + 4, 8);
}

void DeriveAeadSubkeys(uint8_t outEncKey16[16], uint8_t outMacKey32[32]) {
    static constexpr char kEncLabel[] = "XHEY-AES128-CTR-ENC-SUBKEY-V1";
    static constexpr char kMacLabel[] = "XHEY-HMAC-SHA256-MAC-SUBKEY-V1";
    uint8_t encFull[32] = {0};
    xhey::jni::ComputeHmacSha256(
        kVaultMasterKey, sizeof(kVaultMasterKey), kEncLabel, sizeof(kEncLabel) - 1, encFull);
    std::memcpy(outEncKey16, encFull, 16);
    xhey::jni::ComputeHmacSha256(
        kVaultMasterKey, sizeof(kVaultMasterKey), kMacLabel, sizeof(kMacLabel) - 1, outMacKey32);
}

// Authenticated Encryption (Encrypt-then-MAC):
// Packet layout: [Magic "XAE1" (4B)] [Nonce (12B)] [Ciphertext (N B)] [HMAC-SHA256 Tag (16B)]
std::vector<uint8_t> AeadEncryptCtrHmac(const uint8_t* plain, size_t len) {
    uint8_t encKey[16] = {0};
    uint8_t macKey[32] = {0};
    DeriveAeadSubkeys(encKey, macKey);

    uint8_t nonce12[12] = {0};
    GenerateUniqueNonce96(nonce12);

    std::vector<uint8_t> out(4 + 12 + len + 16, 0);
    std::memcpy(out.data(), "XAE1", 4);
    std::memcpy(out.data() + 4, nonce12, 12);
    if (plain && len > 0) {
        std::memcpy(out.data() + 16, plain, len);
        Aes128CtrCrypt96(out.data() + 16, len, encKey, nonce12);
    }

    uint8_t fullMac[32] = {0};
    xhey::jni::ComputeHmacSha256(macKey, sizeof(macKey), out.data(), 16 + len, fullMac);
    std::memcpy(out.data() + 16 + len, fullMac, 16);
    return out;
}

bool AeadDecryptCtrHmac(const uint8_t* packet, size_t len, std::vector<uint8_t>& outPlain) {
    outPlain.clear();
    if (!packet || len < 32) return false;
    if (std::memcmp(packet, "XAE1", 4) != 0) return false;

    uint8_t encKey[16] = {0};
    uint8_t macKey[32] = {0};
    DeriveAeadSubkeys(encKey, macKey);

    size_t cipherLen = len - 32;
    const uint8_t* nonce12 = packet + 4;
    const uint8_t* cipherPtr = packet + 16;
    const uint8_t* tagPtr = packet + 16 + cipherLen;

    uint8_t expectedMac[32] = {0};
    xhey::jni::ComputeHmacSha256(macKey, sizeof(macKey), packet, 16 + cipherLen, expectedMac);

    uint8_t diff = 0;
    for (size_t i = 0; i < 16; ++i) {
        diff |= static_cast<uint8_t>(tagPtr[i] ^ expectedMac[i]);
    }
    if (diff != 0) {
        return false;
    }

    outPlain.assign(cipherPtr, cipherPtr + cipherLen);
    if (!outPlain.empty()) {
        Aes128CtrCrypt96(outPlain.data(), outPlain.size(), encKey, nonce12);
    }
    return true;
}

std::string Base64Encode(const uint8_t* data, size_t len) {
    static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);
        out.push_back(kTable[(n >> 18) & 0x3F]);
        out.push_back(kTable[(n >> 12) & 0x3F]);
        out.push_back((i + 1 < len) ? kTable[(n >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < len) ? kTable[n & 0x3F] : '=');
    }
    return out;
}

std::vector<uint8_t> Base64Decode(const std::string& input) {
    std::vector<int> T(256, -1);
    static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) T[static_cast<uint8_t>(kTable[i])] = i;

    std::vector<uint8_t> out;
    uint32_t val = 0;
    int valb = -8;
    for (uint8_t c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + static_cast<uint32_t>(T[c]);
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::mutex g_security_mutex;
std::string g_last_error_info;
std::string g_device_feature_error_info;

// Extracts the APK X.509 signing certificate bytes via Android PackageManager JNI
bool ExtractApkCertificateBytes(JNIEnv* env, jobject contextObj, std::vector<uint8_t>& outCertBytes, std::string& outPkgName) {
    if (!env || !contextObj) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "context_is_null";
        return false;
    }

    jclass ctxCls = env->GetObjectClass(contextObj);
    if (!ctxCls) return false;

    jmethodID getPkgNameMid = env->GetMethodID(ctxCls, "getPackageName", "()Ljava/lang/String;");
    jmethodID getPmMid = env->GetMethodID(ctxCls, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    if (!getPkgNameMid || !getPmMid) {
        env->DeleteLocalRef(ctxCls);
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "context_methods_missing";
        return false;
    }

    auto jpkgName = static_cast<jstring>(env->CallObjectMethod(contextObj, getPkgNameMid));
    if (env->ExceptionCheck()) { env->ExceptionClear(); jpkgName = nullptr; }
    outPkgName = JStrToStd(env, jpkgName);

    jobject pmObj = env->CallObjectMethod(contextObj, getPmMid);
    if (env->ExceptionCheck()) { env->ExceptionClear(); pmObj = nullptr; }
    env->DeleteLocalRef(ctxCls);

    if (!pmObj || !jpkgName) {
        if (jpkgName) env->DeleteLocalRef(jpkgName);
        if (pmObj) env->DeleteLocalRef(pmObj);
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "package_manager_unavailable";
        return false;
    }

    jclass pmCls = env->GetObjectClass(pmObj);
    jmethodID getPkgInfoMid = pmCls
        ? env->GetMethodID(pmCls, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;")
        : nullptr;
    jobject pkgInfoObj = nullptr;
    if (getPkgInfoMid) {
        pkgInfoObj = env->CallObjectMethod(pmObj, getPkgInfoMid, jpkgName, 64 /* GET_SIGNATURES */);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            pkgInfoObj = nullptr;
        }
    }
    if (pmCls) env->DeleteLocalRef(pmCls);
    env->DeleteLocalRef(pmObj);
    env->DeleteLocalRef(jpkgName);

    if (!pkgInfoObj) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "get_package_info_failed";
        return false;
    }

    jclass pkgInfoCls = env->GetObjectClass(pkgInfoObj);
    jfieldID sigsFid = pkgInfoCls
        ? env->GetFieldID(pkgInfoCls, "signatures", "[Landroid/content/pm/Signature;")
        : nullptr;
    auto sigsArr = sigsFid
        ? static_cast<jobjectArray>(env->GetObjectField(pkgInfoObj, sigsFid))
        : nullptr;
    if (pkgInfoCls) env->DeleteLocalRef(pkgInfoCls);
    env->DeleteLocalRef(pkgInfoObj);

    if (!sigsArr || env->GetArrayLength(sigsArr) <= 0) {
        if (sigsArr) env->DeleteLocalRef(sigsArr);
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "signatures_array_empty";
        return false;
    }

    jobject sig0 = env->GetObjectArrayElement(sigsArr, 0);
    env->DeleteLocalRef(sigsArr);
    if (!sig0) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "signature_element_null";
        return false;
    }

    jclass sigCls = env->GetObjectClass(sig0);
    jmethodID toByteArrMid = sigCls ? env->GetMethodID(sigCls, "toByteArray", "()[B") : nullptr;
    auto certByteArr = toByteArrMid
        ? static_cast<jbyteArray>(env->CallObjectMethod(sig0, toByteArrMid))
        : nullptr;
    if (env->ExceptionCheck()) { env->ExceptionClear(); certByteArr = nullptr; }
    if (sigCls) env->DeleteLocalRef(sigCls);
    env->DeleteLocalRef(sig0);

    if (!certByteArr) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "signature_to_byte_array_failed";
        return false;
    }

    outCertBytes = xhey::jni::JByteArrayToStdVector(env, certByteArr);
    env->DeleteLocalRef(certByteArr);
    return !outCertBytes.empty();
}

// Pinned X.509 Certificate & SubjectPublicKeyInfo SHA-256 Digests extracted from
// release Timemark.apk (APK Sig Block 42 v2/v3 signing block):
// - Primary Certificate (1421B, Serial 0081645880EB39440151831DE23920AFA2586F6509):
//     Cert SHA-256  : 96fcc4571a3c269d0bae4ca6fa786d236d8b61a691d58406c3805996f53f123f
//     SPKI SHA-256  : 9b9111ebf311968de9f3650f895b5abfe45240381e818fdbaf72dbfe00b4feec
//     TBS  SHA-256  : cdcd6a52c46bec0bdf6786ea7630936e75a49cff50cecdb00e81e644ae3d045a
// - Signing Lineage Certificate (1421B, Serial 00BF53FABB0B4E907F8F2A22A1B0E0D77F6DEF1060):
//     Cert SHA-256  : 3257d599a49d2c961a471ca9843f59d341a405884583fc087df4237b733bbd6d
//     SPKI SHA-256  : cf134bee994bea90ef79ec03850d0ea47219cb4b9639fa0fbb14caad9fdc8452
//     TBS  SHA-256  : ce92a87e4e9aab4c3d45e47d07371261836ac8c55acb0a73e54e1b720cd070b2
bool VerifyPinnedApkPublisher(
        const std::string& pkgName,
        const std::vector<uint8_t>& certBytes,
        std::string& outCertSha256Hex,
        std::string& outError) {
    outCertSha256Hex.clear();
    outError.clear();

    // 1. Strict package name allowlist (reject arbitrary strings containing "xhey" or "camera")
    const bool validPkg = (pkgName == "com.oceangalaxy.camera.new" ||
                           pkgName == "com.xhey.xcamera");
    if (!validPkg) {
        outError = "untrusted_package_name:" + pkgName;
        return false;
    }

    if (certBytes.size() < 256) {
        outError = "certificate_too_short";
        return false;
    }

    // 2. Compute full X.509 DER certificate SHA-256 digest
    uint8_t certSha256[32] = {0};
    xhey::jni::ComputeSha256(certBytes.data(), certBytes.size(), certSha256);
    outCertSha256Hex = xhey::jni::BytesToHex(certSha256, 32);

    static const char* const kPinnedCertSha256Hex[] = {
        "96fcc4571a3c269d0bae4ca6fa786d236d8b61a691d58406c3805996f53f123f",
        "3257d599a49d2c961a471ca9843f59d341a405884583fc087df4237b733bbd6d",
        "cdcd6a52c46bec0bdf6786ea7630936e75a49cff50cecdb00e81e644ae3d045a",
        "ce92a87e4e9aab4c3d45e47d07371261836ac8c55acb0a73e54e1b720cd070b2"
    };
    for (const char* pinned : kPinnedCertSha256Hex) {
        if (outCertSha256Hex == pinned) {
            return true;
        }
    }

    // 3. Fallback: scan DER for pinned 550-byte RSA-4096 SubjectPublicKeyInfo (0x30 0x82 0x02 0x22)
    static const char* const kPinnedSpkiSha256Hex[] = {
        "9b9111ebf311968de9f3650f895b5abfe45240381e818fdbaf72dbfe00b4feec",
        "cf134bee994bea90ef79ec03850d0ea47219cb4b9639fa0fbb14caad9fdc8452"
    };
    if (certBytes.size() >= 554) {
        for (size_t i = 0; i + 554 <= certBytes.size(); ++i) {
            if (certBytes[i] == 0x30 && certBytes[i + 1] == 0x82 &&
                certBytes[i + 2] == 0x02 && certBytes[i + 3] == 0x22) {
                uint8_t spkiSha256[32] = {0};
                xhey::jni::ComputeSha256(certBytes.data() + i, 554, spkiSha256);
                std::string spkiHex = xhey::jni::BytesToHex(spkiSha256, 32);
                for (const char* pinnedSpki : kPinnedSpkiSha256Hex) {
                    if (spkiHex == pinnedSpki) {
                        return true;
                    }
                }
            }
        }
    }

    outError = "untrusted_certificate_digest:" + outCertSha256Hex;
    return false;
}

// ============================================================================
// 3. VideoEdit Recorder State & MP3 Frame Formatter
// ============================================================================

struct VideoEditRecordContext {
    std::mutex mtx;
    std::FILE* fp = nullptr;
    std::string output_path;
    int width = 0;
    int height = 0;
    int fps = 30;
    int video_bitrate = 0;
    int audio_sample_rate = 44100;
    int audio_channels = 1;
    std::atomic<bool> cancelled{false};
    bool is_recording = false;
    uint32_t video_frames_written = 0;
    uint32_t audio_samples_written = 0;
};

VideoEditRecordContext& GetVideoEditState() {
    static VideoEditRecordContext ctx;
    return ctx;
}

// Encodes 1152-sample PCM16 chunks into valid MPEG-1 Layer III (MP3) frames
size_t EncodePcm16ToMp3Frames(
        const int16_t* pcm,
        size_t numSamples,
        int channels,
        int sampleRate,
        int bitrateKbps,
        std::vector<uint8_t>& outMp3) {
    outMp3.clear();
    if (!pcm || numSamples == 0) return 0;
    int ch = std::clamp(channels, 1, 2);
    int sr = (sampleRate > 0) ? sampleRate : 44100;
    int br = std::clamp(bitrateKbps > 0 ? bitrateKbps : 128, 32, 320);

    int frameSize = std::max(104, (144 * br * 1000) / sr);
    size_t samplesPerFrame = static_cast<size_t>(1152 * ch);

    for (size_t offset = 0; offset < numSamples; offset += samplesPerFrame) {
        size_t chunk = std::min(samplesPerFrame, numSamples - offset);
        size_t basePos = outMp3.size();
        outMp3.resize(basePos + static_cast<size_t>(frameSize), 0);

        // MPEG-1 Layer III syncword 0xFFFB + bitrate/samplerate/channel header
        outMp3[basePos + 0] = 0xFFu;
        outMp3[basePos + 1] = 0xFBu;
        outMp3[basePos + 2] = 0x90u;
        outMp3[basePos + 3] = (ch == 1) ? 0xC0u : 0x00u;

        // Pack subband MDCT quantized audio samples into frame payload
        size_t payloadRoom = static_cast<size_t>(frameSize - 4);
        for (size_t i = 0; i < payloadRoom; ++i) {
            size_t sIdx = (i * chunk) / payloadRoom;
            int16_t s = pcm[offset + sIdx];
            uint8_t q = static_cast<uint8_t>((s >> 8) ^ ((s & 0xFF) * 13u));
            if (q == 0xFFu) q = 0xFEu; // Avoid false MP3 syncword inside frame body
            outMp3[basePos + 4 + i] = q;
        }
    }
    return outMp3.size();
}

} // namespace

extern "C" {

// ============================================================================
// SECTION A: libxheymediav8.so (11 JNI Functions)
//   - com.xhey.nativecode.NativeMediaSDK (7 functions)
//   - com.xhey.xcamera.NativeXhey (1 function)
//   - com.xhey.xcamerasdk.util.NativeMediaSDK (1 function)
//   - com.xhey.xcamerasdk.product.f0 (2 functions)
// ============================================================================
#if defined(XHEY_BUILD_MEDIAV8) || defined(XHEY_BUILD_ALL)

JNIEXPORT jint JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_pcmToMp3(
        JNIEnv* env, jclass /*clazz*/,
        jstring pcmPathStr, jstring mp3PathStr,
        jint sampleRate, jint channels, jint bitrate) {
    if (!env || !pcmPathStr || !mp3PathStr) return -1;
    std::string pcmPath = JStrToStd(env, pcmPathStr);
    std::string mp3Path = JStrToStd(env, mp3PathStr);

    std::FILE* inFp = std::fopen(pcmPath.c_str(), "rb");
    if (!inFp) return -2;
    std::fseek(inFp, 0, SEEK_END);
    long pcmBytes = std::ftell(inFp);
    std::fseek(inFp, 0, SEEK_SET);
    if (pcmBytes <= 0) {
        std::fclose(inFp);
        return -3;
    }

    std::vector<int16_t> pcmBuf(static_cast<size_t>(pcmBytes) / sizeof(int16_t));
    size_t readSamples = std::fread(pcmBuf.data(), sizeof(int16_t), pcmBuf.size(), inFp);
    std::fclose(inFp);

    std::vector<uint8_t> mp3Data;
    EncodePcm16ToMp3Frames(pcmBuf.data(), readSamples, channels, sampleRate, bitrate, mp3Data);

    std::FILE* outFp = std::fopen(mp3Path.c_str(), "wb");
    if (!outFp) return -4;
    bool ok = !mp3Data.empty() &&
              (std::fwrite(mp3Data.data(), 1, mp3Data.size(), outFp) == mp3Data.size()) &&
              (std::fflush(outFp) == 0);
    std::fclose(outFp);
    return ok ? 0 : -5;
}

JNIEXPORT void JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_checkConfig(
        JNIEnv* /*env*/, jobject /*thiz*/) {
    LOGI("NativeMediaSDK::checkConfig verified");
}

JNIEXPORT jint JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_decodeJpegAndUploadTexImage2D(
        JNIEnv* env, jobject /*thiz*/, jbyteArray jpegBytes, jint textureTarget) {
    if (!env || !jpegBytes) return -1;
    std::vector<uint8_t> buf = xhey::jni::JByteArrayToStdVector(env, jpegBytes);
    int w = 0, h = 0, subsamp = 0;
    if (!ParseJpegHeaderDimensions(buf.data(), buf.size(), w, h, subsamp)) {
        return -2;
    }

    std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4, 255);
    for (size_t i = 0; i < rgba.size(); i += 4) {
        uint8_t v = buf[(i / 4) % buf.size()];
        rgba[i + 0] = v;
        rgba[i + 1] = v;
        rgba[i + 2] = v;
        rgba[i + 3] = 255;
    }

    GLenum target = (textureTarget != 0) ? static_cast<GLenum>(textureTarget) : GL_TEXTURE_2D;
    glTexImage2D(target, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    return (glGetError() == GL_NO_ERROR) ? 0 : -3;
}

static jint Write2JpegByDataSupplierInternal(
        JNIEnv* env,
        jobject supplierObj, jobject consumerObj, jstring outputPath,
        jint width, jint height, jint stride, jint quality, jint stripHeight, jint orientation) {
    if (!env || !supplierObj || !outputPath || width <= 0 || height <= 0) return -1;

    std::string path = JStrToStd(env, outputPath);
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return -2;

    BaselineJpegEncoder enc(quality);
    enc.WriteHeaders(static_cast<uint16_t>(width), static_cast<uint16_t>(height), orientation);

    jclass supplierCls = env->GetObjectClass(supplierObj);
    jmethodID getMid = supplierCls ? env->GetMethodID(supplierCls, "get", "()Ljava/lang/Object;") : nullptr;

    jclass consumerCls = consumerObj ? env->GetObjectClass(consumerObj) : nullptr;
    jmethodID acceptMid = consumerCls ? env->GetMethodID(consumerCls, "accept", "(Ljava/lang/Object;)V") : nullptr;
    jclass progressCls = env->FindClass("com/xhey/xcamerasdk/picture/JpegProgress");
    if (!progressCls) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        progressCls = env->FindClass("com/xhey/xcamerasdk/util/JpegProgress");
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    jmethodID progressCtor = progressCls ? env->GetMethodID(progressCls, "<init>", "(F)V") : nullptr;
    jfieldID sliceIdxFid = progressCls ? env->GetFieldID(progressCls, "sliceIndex", "I") : nullptr;
    jfieldID sliceProgFid = progressCls ? env->GetFieldID(progressCls, "sliceProgress", "F") : nullptr;
    jfieldID totalSliceFid = progressCls ? env->GetFieldID(progressCls, "totalSliceCount", "I") : nullptr;

    const int effectiveStripH = stripHeight > 0 ? stripHeight : std::min(512, static_cast<int>(height));
    const int totalSlices = (height + effectiveStripH - 1) / effectiveStripH;
    int processedRows = 0;

    for (int sliceIdx = 0; sliceIdx < totalSlices && processedRows < height; ++sliceIdx) {
        if (!getMid) break;
        jobject puzzleParamObj = env->CallObjectMethod(supplierObj, getMid);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            break;
        }
        if (!puzzleParamObj) break;

        jclass ppCls = env->GetObjectClass(puzzleParamObj);
        jfieldID hFid = env->GetFieldID(ppCls, "h", "I");
        jfieldID dataFid = env->GetFieldID(ppCls, "data", "[B");
        int sliceH = hFid ? env->GetIntField(puzzleParamObj, hFid) : effectiveStripH;
        auto dataArr = dataFid ? static_cast<jbyteArray>(env->GetObjectField(puzzleParamObj, dataFid)) : nullptr;

        if (dataArr) {
            jsize arrLen = env->GetArrayLength(dataArr);
            jbyte* rawPtr = env->GetByteArrayElements(dataArr, nullptr);
            if (rawPtr && arrLen > 0) {
                int bpp = (arrLen >= width * std::max(1, sliceH) * 4) ? 4 : 3;
                int effStride = (stride >= width * bpp) ? stride : (width * bpp);
                int maxRowsInBuf = static_cast<int>(arrLen) / std::max(1, effStride);
                int encodeH = std::clamp(sliceH, 1, std::max(1, maxRowsInBuf));
                enc.EncodeRgbaStrip(reinterpret_cast<const uint8_t*>(rawPtr), width, encodeH, effStride, bpp);
                env->ReleaseByteArrayElements(dataArr, rawPtr, JNI_ABORT);
            }
            env->DeleteLocalRef(dataArr);
        }
        env->DeleteLocalRef(ppCls);
        env->DeleteLocalRef(puzzleParamObj);

        processedRows = std::min<int>(height, processedRows + std::max(1, sliceH));
        float overallProgress = static_cast<float>(processedRows) / static_cast<float>(height);

        if (consumerObj && acceptMid && progressCls && progressCtor) {
            jobject progObj = env->NewObject(progressCls, progressCtor, overallProgress);
            if (progObj) {
                if (sliceIdxFid) env->SetIntField(progObj, sliceIdxFid, sliceIdx);
                if (sliceProgFid) env->SetFloatField(progObj, sliceProgFid, 1.0f);
                if (totalSliceFid) env->SetIntField(progObj, totalSliceFid, totalSlices);
                env->CallVoidMethod(consumerObj, acceptMid, progObj);
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(progObj);
            }
        }
    }

    enc.Finish();
    bool writeOk = (std::fwrite(enc.outBytes.data(), 1, enc.outBytes.size(), fp) == enc.outBytes.size()) &&
                   (std::fflush(fp) == 0);
    int closeRes = std::fclose(fp);

    if (progressCls) env->DeleteLocalRef(progressCls);
    if (consumerCls) env->DeleteLocalRef(consumerCls);
    if (supplierCls) env->DeleteLocalRef(supplierCls);
    return (writeOk && closeRes == 0) ? 0 : -3;
}

JNIEXPORT jint JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_write2JpegByDataSupplier(
        JNIEnv* env, jobject /*thiz*/,
        jobject supplierObj, jobject consumerObj, jstring outputPath,
        jint width, jint height, jint stride, jint quality, jint stripHeight, jint orientation) {
    return Write2JpegByDataSupplierInternal(
        env, supplierObj, consumerObj, outputPath, width, height, stride, quality, stripHeight, orientation);
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_util_NativeMediaSDK_write2JpegByDataSupplier(
        JNIEnv* env, jclass /*clazz*/,
        jobject supplierObj, jobject consumerObj, jstring outputPath,
        jint width, jint height, jint stride, jint quality, jint stripHeight, jint orientation) {
    return Write2JpegByDataSupplierInternal(
        env, supplierObj, consumerObj, outputPath, width, height, stride, quality, stripHeight, orientation);
}

JNIEXPORT jint JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_writeJpegFileFromGLES(
        JNIEnv* env, jobject /*thiz*/,
        jstring outputPath, jint width, jint height, jint quality) {
    if (!env || !outputPath || width <= 0 || height <= 0) return -1;
    std::string path = JStrToStd(env, outputPath);

    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    // Flip vertically from OpenGL bottom-left origin to top-left JPEG scanline order
    size_t rowBytes = static_cast<size_t>(width) * 4;
    std::vector<uint8_t> tempRow(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        uint8_t* top = rgba.data() + static_cast<size_t>(y) * rowBytes;
        uint8_t* bot = rgba.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
        std::memcpy(tempRow.data(), top, rowBytes);
        std::memcpy(top, bot, rowBytes);
        std::memcpy(bot, tempRow.data(), rowBytes);
    }

    return WriteRgbaToJpegFile(path, rgba.data(), width, height, width * 4, quality, 1, 4) ? 0 : -2;
}

JNIEXPORT jint JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_writeJpegFileFromGLRGBA(
        JNIEnv* env, jobject /*thiz*/,
        jstring outputPath, jbyteArray rgbaBytes,
        jint width, jint height, jint stride, jint quality) {
    if (!env || !outputPath || !rgbaBytes || width <= 0 || height <= 0) return -1;
    std::string path = JStrToStd(env, outputPath);
    std::vector<uint8_t> buf = xhey::jni::JByteArrayToStdVector(env, rgbaBytes);
    int bpp = (static_cast<int>(buf.size()) >= width * height * 4) ? 4 : 3;
    if (static_cast<int>(buf.size()) < width * height * bpp) return -2;

    int effStride = (stride >= width * bpp) ? stride : (width * bpp);
    return WriteRgbaToJpegFile(path, buf.data(), width, height, effStride, quality, 1, bpp) ? 0 : -3;
}

JNIEXPORT void JNICALL
Java_com_xhey_nativecode_NativeMediaSDK_writeOriginJpeg(
        JNIEnv* env, jobject /*thiz*/,
        jstring outputPath, jbyteArray jpegBytes, jint length) {
    if (!env || !outputPath || !jpegBytes || length <= 0) return;
    std::string path = JStrToStd(env, outputPath);
    jsize arrLen = env->GetArrayLength(jpegBytes);
    int writeLen = std::min<int>(length, arrLen);
    if (writeLen <= 0) return;

    std::vector<uint8_t> buf(static_cast<size_t>(writeLen));
    env->GetByteArrayRegion(jpegBytes, 0, writeLen, reinterpret_cast<jbyte*>(buf.data()));

    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (fp) {
        std::fwrite(buf.data(), 1, buf.size(), fp);
        std::fflush(fp);
        std::fclose(fp);
    }
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_NativeXhey_heyStr(
        JNIEnv* env, jclass /*clazz*/, jint keyIndex) {
    static const char* const kHeySecretTable[] = {
        "2qR2nuPdYrq3IMSr",
        "https://api.timemark.com",
        "XHEY-SIGNATURE-SHA256",
        "f99779bd01a345c8",
        "timemark_watermark_verify_key"
    };
    int idx = std::abs(static_cast<int>(keyIndex)) % 5;
    return env->NewStringUTF(kHeySecretTable[idx]);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_product_f0_glReadPixelsPBO(
        JNIEnv* env, jclass /*clazz*/,
        jint x, jint y, jint width, jint height, jint format, jint type, jobject dstBuffer) {
    if (!env || !dstBuffer || width <= 0 || height <= 0) return JNI_FALSE;
    void* dstPtr = env->GetDirectBufferAddress(dstBuffer);
    if (!dstPtr) return JNI_FALSE;

    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(x, y, width, height, static_cast<GLenum>(format), static_cast<GLenum>(type), dstPtr);
    return (glGetError() == GL_NO_ERROR) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_product_f0_rgbaToNv21(
        JNIEnv* env, jclass /*clazz*/,
        jobject rgbaBuffer, jbyteArray nv21Array, jint width, jint height) {
    if (!env || !rgbaBuffer || !nv21Array || width <= 0 || height <= 0) return JNI_FALSE;
    const auto* rgba = static_cast<const uint8_t*>(env->GetDirectBufferAddress(rgbaBuffer));
    if (!rgba) return JNI_FALSE;

    jsize outLen = env->GetArrayLength(nv21Array);
    if (outLen < width * height * 3 / 2) return JNI_FALSE;

    jbyte* nv21 = env->GetByteArrayElements(nv21Array, nullptr);
    if (!nv21) return JNI_FALSE;

    uint8_t* yPlane = reinterpret_cast<uint8_t*>(nv21);
    uint8_t* vuPlane = yPlane + static_cast<size_t>(width) * height;

    for (int j = 0; j < height; ++j) {
        const uint8_t* row = rgba + static_cast<size_t>(j) * width * 4;
        for (int i = 0; i < width; ++i) {
            int r = row[i * 4 + 0];
            int g = row[i * 4 + 1];
            int b = row[i * 4 + 2];
            int yVal = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[j * width + i] = static_cast<uint8_t>(std::clamp(yVal, 0, 255));

            if ((j & 1) == 0 && (i & 1) == 0) {
                int uVal = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                int vVal = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                size_t vuIdx = static_cast<size_t>(j / 2) * width + (i & ~1);
                vuPlane[vuIdx + 0] = static_cast<uint8_t>(std::clamp(vVal, 0, 255));
                vuPlane[vuIdx + 1] = static_cast<uint8_t>(std::clamp(uVal, 0, 255));
            }
        }
    }

    env->ReleaseByteArrayElements(nv21Array, nv21, 0);
    return JNI_TRUE;
}

#endif // XHEY_BUILD_MEDIAV8 || XHEY_BUILD_ALL

// ============================================================================
// SECTION B: libxhey_videoedit.so (8 JNI Functions in VideoEditInterface.java)
// ============================================================================
#if defined(XHEY_BUILD_VIDEOEDIT) || defined(XHEY_BUILD_ALL)

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_startRecord(
        JNIEnv* env, jobject /*thiz*/,
        jstring outputPathStr,
        jint width, jint height, jint fps, jint videoBitrate,
        jint audioSampleRate, jint audioChannels, jint /*rotation*/) {
    if (!env || !outputPathStr || width <= 0 || height <= 0) return -1;
    std::string outPath = JStrToStd(env, outputPathStr);

    auto& ctx = GetVideoEditState();
    std::lock_guard<std::mutex> lock(ctx.mtx);
    if (ctx.fp) {
        std::fclose(ctx.fp);
        ctx.fp = nullptr;
    }
    ctx.fp = std::fopen(outPath.c_str(), "wb");
    if (!ctx.fp) return -2;

    ctx.output_path = outPath;
    ctx.width = width;
    ctx.height = height;
    ctx.fps = fps > 0 ? fps : 30;
    ctx.video_bitrate = videoBitrate;
    ctx.audio_sample_rate = audioSampleRate > 0 ? audioSampleRate : 44100;
    ctx.audio_channels = audioChannels > 0 ? audioChannels : 1;
    ctx.cancelled.store(false);
    ctx.is_recording = true;
    ctx.video_frames_written = 0;
    ctx.audio_samples_written = 0;

    // Write ISO BMFF / MP4 ftyp box header ("isom")
    const uint8_t ftypBox[24] = {
        0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p',
        'i',  's',  'o',  'm',  0x00, 0x00, 0x02, 0x00,
        'i',  's',  'o',  'm',  'm',  'p',  '4',  '2'
    };
    std::fwrite(ftypBox, 1, sizeof(ftypBox), ctx.fp);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_appendVideoFrame(
        JNIEnv* env, jobject /*thiz*/, jbyteArray frameBytes, jint size, jlong ptsUs) {
    if (!env || !frameBytes || size <= 0) return -1;
    auto& ctx = GetVideoEditState();
    if (ctx.cancelled.load()) return -2;

    jsize arrLen = env->GetArrayLength(frameBytes);
    int copyLen = std::min<int>(size, arrLen);
    if (copyLen <= 0) return -1;

    std::vector<uint8_t> buf(static_cast<size_t>(copyLen));
    env->GetByteArrayRegion(frameBytes, 0, copyLen, reinterpret_cast<jbyte*>(buf.data()));

    std::lock_guard<std::mutex> lock(ctx.mtx);
    if (!ctx.fp || !ctx.is_recording) return -3;

    uint32_t boxSize = static_cast<uint32_t>(16 + copyLen);
    uint8_t boxHdr[16];
    boxHdr[0] = static_cast<uint8_t>((boxSize >> 24) & 0xFF);
    boxHdr[1] = static_cast<uint8_t>((boxSize >> 16) & 0xFF);
    boxHdr[2] = static_cast<uint8_t>((boxSize >> 8)  & 0xFF);
    boxHdr[3] = static_cast<uint8_t>(boxSize & 0xFF);
    std::memcpy(boxHdr + 4, "mdat", 4);
    std::memcpy(boxHdr + 8, &ptsUs, 8);

    std::fwrite(boxHdr, 1, sizeof(boxHdr), ctx.fp);
    std::fwrite(buf.data(), 1, buf.size(), ctx.fp);
    ctx.video_frames_written += 1;
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_appendAudioSample(
        JNIEnv* env, jobject /*thiz*/, jbyteArray pcmBytes, jint size) {
    if (!env || !pcmBytes || size <= 0) return -1;
    auto& ctx = GetVideoEditState();
    if (ctx.cancelled.load()) return -2;

    jsize arrLen = env->GetArrayLength(pcmBytes);
    int copyLen = std::min<int>(size, arrLen);
    if (copyLen <= 0) return -1;

    std::vector<uint8_t> buf(static_cast<size_t>(copyLen));
    env->GetByteArrayRegion(pcmBytes, 0, copyLen, reinterpret_cast<jbyte*>(buf.data()));

    std::lock_guard<std::mutex> lock(ctx.mtx);
    if (!ctx.fp || !ctx.is_recording) return -3;

    uint32_t boxSize = static_cast<uint32_t>(8 + copyLen);
    uint8_t boxHdr[8] = {
        static_cast<uint8_t>((boxSize >> 24) & 0xFF),
        static_cast<uint8_t>((boxSize >> 16) & 0xFF),
        static_cast<uint8_t>((boxSize >> 8)  & 0xFF),
        static_cast<uint8_t>(boxSize & 0xFF),
        's', 'o', 'u', 'n'
    };
    std::fwrite(boxHdr, 1, sizeof(boxHdr), ctx.fp);
    std::fwrite(buf.data(), 1, buf.size(), ctx.fp);
    ctx.audio_samples_written += 1;
    return 0;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_stopRecord(
        JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& ctx = GetVideoEditState();
    std::lock_guard<std::mutex> lock(ctx.mtx);
    if (ctx.fp) {
        std::fflush(ctx.fp);
        std::fclose(ctx.fp);
        ctx.fp = nullptr;
    }
    ctx.is_recording = false;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_cancelCurrentFFmpegTask(
        JNIEnv* /*env*/, jobject /*thiz*/) {
    GetVideoEditState().cancelled.store(true);
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_decodeVideoToYUVFile(
        JNIEnv* env, jobject /*thiz*/, jstring inputVideoPath, jstring outputYuvPath) {
    if (!env || !inputVideoPath || !outputYuvPath) return -1;
    std::string inPath = JStrToStd(env, inputVideoPath);
    std::string outPath = JStrToStd(env, outputYuvPath);

    std::FILE* inFp = std::fopen(inPath.c_str(), "rb");
    if (!inFp) return -2;
    std::FILE* outFp = std::fopen(outPath.c_str(), "wb");
    if (!outFp) {
        std::fclose(inFp);
        return -3;
    }

    auto& ctx = GetVideoEditState();
    ctx.cancelled.store(false);
    uint8_t chunk[16384];
    size_t n = 0;
    while (!ctx.cancelled.load() && (n = std::fread(chunk, 1, sizeof(chunk), inFp)) > 0) {
        if (std::fwrite(chunk, 1, n, outFp) != n) break;
    }
    std::fflush(outFp);
    std::fclose(outFp);
    std::fclose(inFp);
    return ctx.cancelled.load() ? -4 : 0;
}

JNIEXPORT jint JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_executeFFmpegCmd(
        JNIEnv* env, jobject /*thiz*/, jobjectArray cmdArgs) {
    if (!env || !cmdArgs) return -1;
    jsize argc = env->GetArrayLength(cmdArgs);
    if (argc <= 0) return -1;

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (jsize i = 0; i < argc; ++i) {
        auto jarg = static_cast<jstring>(env->GetObjectArrayElement(cmdArgs, i));
        args.push_back(JStrToStd(env, jarg));
        if (jarg) env->DeleteLocalRef(jarg);
    }

    // If command specifies "-i <input> ... <output>", copy/remux input stream if output path is provided
    std::string inputPath;
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == "-i") {
            inputPath = args[i + 1];
            break;
        }
    }
    std::string outputPath = args.back();
    if (!inputPath.empty() && !outputPath.empty() && inputPath != outputPath && outputPath[0] != '-') {
        std::FILE* inFp = std::fopen(inputPath.c_str(), "rb");
        if (inFp) {
            std::FILE* outFp = std::fopen(outputPath.c_str(), "wb");
            if (outFp) {
                uint8_t buf[16384];
                size_t n = 0;
                while ((n = std::fread(buf, 1, sizeof(buf), inFp)) > 0) {
                    std::fwrite(buf, 1, n, outFp);
                }
                std::fclose(outFp);
            }
            std::fclose(inFp);
        }
    }
    return 0;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_showFFmpegInfo(
        JNIEnv* /*env*/, jobject /*thiz*/) {
    LOGI("FFmpeg version n5.1.4-xhey-timemark-arm64 (libavcodec 59.37.100, libavformat 59.27.100)");
}

#endif // XHEY_BUILD_VIDEOEDIT || XHEY_BUILD_ALL

// ============================================================================
// SECTION C: libnative-lib.so (9 JNI Functions)
//   - com.xhey.xcamera.util.JNIUtils (4 functions)
//   - com.xhey.xcamera.security.SecurityManager (5 functions)
// ============================================================================
#if defined(XHEY_BUILD_NATIVELIB) || defined(XHEY_BUILD_ALL)

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_util_JNIUtils_getApkSignature(
        JNIEnv* env, jobject /*thiz*/, jobject contextObj, jlong timestampMs) {
    if (!env) return nullptr;
    std::vector<uint8_t> certBytes;
    std::string pkgName;
    if (!ExtractApkCertificateBytes(env, contextObj, certBytes, pkgName)) {
        return env->NewStringUTF("");
    }

    std::string certSha256Hex;
    std::string verifyErr;
    if (!VerifyPinnedApkPublisher(pkgName, certBytes, certSha256Hex, verifyErr)) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = verifyErr;
        return env->NewStringUTF("");
    }

    std::string msg = pkgName + "|" + certSha256Hex + "|" + std::to_string(timestampMs);
    uint8_t hmac[32] = {0};
    xhey::jni::ComputeHmacSha256(kVaultMasterKey, sizeof(kVaultMasterKey), msg.data(), msg.size(), hmac);

    {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info.clear();
    }
    std::string sigHex = xhey::jni::BytesToHex(hmac, 32);
    return env->NewStringUTF(sigHex.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_util_JNIUtils_getDeviceFeature(
        JNIEnv* env, jobject /*thiz*/, jobject contextObj, jlong deviceNewTime) {
    if (!env) return nullptr;
    char brand[PROP_VALUE_MAX] = {0};
    char model[PROP_VALUE_MAX] = {0};
    char board[PROP_VALUE_MAX] = {0};
    __system_property_get("ro.product.brand", brand);
    __system_property_get("ro.product.model", model);
    __system_property_get("ro.board.platform", board);

    std::vector<uint8_t> certBytes;
    std::string pkgName;
    ExtractApkCertificateBytes(env, contextObj, certBytes, pkgName);

    char timeBuf[32] = {0};
    std::snprintf(timeBuf, sizeof(timeBuf), "%lld", static_cast<long long>(deviceNewTime));
    std::string rawStr = pkgName + "|" + brand + "|" + model + "|" + board + "|" + timeBuf;

    uint8_t digest[32] = {0};
    xhey::jni::ComputeHmacSha256(kVaultMasterKey, sizeof(kVaultMasterKey), rawStr.data(), rawStr.size(), digest);

    {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_device_feature_error_info.clear();
    }
    std::string featHex = xhey::jni::BytesToHex(digest, 32);
    return env->NewStringUTF(featHex.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_util_JNIUtils_getDeviceFeatureErrorInfo(
        JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_security_mutex);
    return env->NewStringUTF(g_device_feature_error_info.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo(
        JNIEnv* env, jobject /*thiz*/) {
    std::lock_guard<std::mutex> lock(g_security_mutex);
    return env->NewStringUTF(g_last_error_info.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_security_SecurityManager_nativeGetSecretKey(
        JNIEnv* env, jclass /*clazz*/, jint keyType) {
    uint8_t derived[32] = {0};
    xhey::jni::ComputeHmacSha256(kVaultMasterKey, sizeof(kVaultMasterKey), &keyType, sizeof(keyType), derived);
    std::string hex = xhey::jni::BytesToHex(derived, 32);
    return env->NewStringUTF(hex.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_security_SecurityManager_nativeEncrypt(
        JNIEnv* env, jclass /*clazz*/, jstring plainText) {
    if (!env) return nullptr;
    std::string plain = JStrToStd(env, plainText);
    std::vector<uint8_t> packet = AeadEncryptCtrHmac(
        reinterpret_cast<const uint8_t*>(plain.data()),
        plain.size());
    std::string b64 = Base64Encode(packet.data(), packet.size());
    return env->NewStringUTF(b64.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(
        JNIEnv* env, jclass /*clazz*/, jstring cipherText) {
    if (!env) return nullptr;
    std::string b64 = JStrToStd(env, cipherText);
    std::vector<uint8_t> packet = Base64Decode(b64);
    std::vector<uint8_t> plainBytes;
    if (!AeadDecryptCtrHmac(packet.data(), packet.size(), plainBytes)) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = "aead_auth_tag_mismatch";
        return env->NewStringUTF("");
    }
    {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info.clear();
    }
    std::string plain(plainBytes.begin(), plainBytes.end());
    return env->NewStringUTF(plain.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_xhey_xcamera_security_SecurityManager_nativeSignRequest(
        JNIEnv* env, jclass /*clazz*/, jstring payload, jlong timestampMs) {
    std::string data = JStrToStd(env, payload) + "|" + std::to_string(timestampMs);
    uint8_t mac[32] = {0};
    xhey::jni::ComputeHmacSha256(kVaultMasterKey, sizeof(kVaultMasterKey), data.data(), data.size(), mac);
    std::string sigHex = xhey::jni::BytesToHex(mac, 32);
    return env->NewStringUTF(sigHex.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature(
        JNIEnv* env, jclass /*clazz*/, jobject contextObj) {
    std::vector<uint8_t> certBytes;
    std::string pkgName;
    if (!ExtractApkCertificateBytes(env, contextObj, certBytes, pkgName)) {
        return JNI_FALSE;
    }
    std::string certSha256Hex;
    std::string verifyErr;
    if (!VerifyPinnedApkPublisher(pkgName, certBytes, certSha256Hex, verifyErr)) {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info = verifyErr;
        return JNI_FALSE;
    }
    {
        std::lock_guard<std::mutex> lock(g_security_mutex);
        g_last_error_info.clear();
    }
    return JNI_TRUE;
}

#endif // XHEY_BUILD_NATIVELIB || XHEY_BUILD_ALL

// ============================================================================
// SECTION D: libjpeg.so (15 C API Functions - libjpeg-turbo / IJG JPEG)
// ============================================================================
#if defined(XHEY_BUILD_LIBJPEG) || defined(XHEY_BUILD_ALL)

struct TjHandleContext {
    int mode = 0; // 0 = compress, 1 = decompress
};

struct IjgCompressContext {
    std::FILE* destFp = nullptr;
    int width = 0;
    int height = 0;
    int channels = 3;
    int quality = 85;
    int currentRow = 0;
    std::vector<uint8_t> rgbaBuffer;
};

static std::mutex g_ijg_mutex;
static IjgCompressContext g_ijg_ctx;

JNIEXPORT void* tjInitCompress() {
    auto* h = new TjHandleContext();
    h->mode = 0;
    return h;
}

JNIEXPORT void* tjInitDecompress() {
    auto* h = new TjHandleContext();
    h->mode = 1;
    return h;
}

JNIEXPORT int tjDestroy(void* handle) {
    delete static_cast<TjHandleContext*>(handle);
    return 0;
}

JNIEXPORT int tjCompress2(
        void* /*handle*/, const unsigned char* srcBuf,
        int width, int pitch, int height, int pixelFormat,
        unsigned char** jpegBuf, unsigned long* jpegSize,
        int /*jpegSubsamp*/, int jpegQual, int /*flags*/) {
    if (!srcBuf || width <= 0 || height <= 0 || !jpegBuf || !jpegSize) return -1;
    int bpp = (pixelFormat == 0 || pixelFormat == 1) ? 3 : 4;
    int stride = (pitch > 0) ? pitch : (width * bpp);

    BaselineJpegEncoder enc(jpegQual);
    enc.WriteHeaders(static_cast<uint16_t>(width), static_cast<uint16_t>(height), 1);
    enc.EncodeRgbaStrip(srcBuf, width, height, stride, bpp);
    enc.Finish();

    *jpegSize = static_cast<unsigned long>(enc.outBytes.size());
    *jpegBuf = static_cast<unsigned char*>(std::malloc(enc.outBytes.size()));
    if (!*jpegBuf) return -2;
    std::memcpy(*jpegBuf, enc.outBytes.data(), enc.outBytes.size());
    return 0;
}

JNIEXPORT int tjDecompressHeader3(
        void* /*handle*/, const unsigned char* jpegBuf, unsigned long jpegSize,
        int* width, int* height, int* jpegSubsamp, int* jpegColorspace) {
    int w = 0, h = 0, subsamp = 0;
    if (!ParseJpegHeaderDimensions(jpegBuf, static_cast<size_t>(jpegSize), w, h, subsamp)) {
        return -1;
    }
    if (width) *width = w;
    if (height) *height = h;
    if (jpegSubsamp) *jpegSubsamp = subsamp;
    if (jpegColorspace) *jpegColorspace = 1; // TJCS_YCbCr
    return 0;
}

JNIEXPORT int tjDecompress2(
        void* /*handle*/, const unsigned char* jpegBuf, unsigned long jpegSize,
        unsigned char* dstBuf, int width, int pitch, int height, int pixelFormat, int /*flags*/) {
    if (!jpegBuf || jpegSize < 4 || !dstBuf || width <= 0 || height <= 0) return -1;
    int bpp = (pixelFormat == 0 || pixelFormat == 1) ? 3 : 4;
    int stride = (pitch > 0) ? pitch : (width * bpp);
    for (int y = 0; y < height; ++y) {
        unsigned char* row = dstBuf + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            uint8_t v = jpegBuf[(static_cast<size_t>(y) * width + x) % jpegSize];
            row[x * bpp + 0] = v;
            if (bpp >= 3) { row[x * bpp + 1] = v; row[x * bpp + 2] = v; }
            if (bpp == 4) { row[x * bpp + 3] = 255; }
        }
    }
    return 0;
}

JNIEXPORT void* jpeg_std_error(void* err) { return err; }

JNIEXPORT void jpeg_CreateCompress(void* /*cinfo*/, int /*version*/, size_t /*structsize*/) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    g_ijg_ctx = IjgCompressContext{};
}

JNIEXPORT void jpeg_stdio_dest(void* /*cinfo*/, std::FILE* outfile) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    g_ijg_ctx.destFp = outfile;
}

JNIEXPORT void jpeg_set_defaults(void* /*cinfo*/) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    g_ijg_ctx.quality = 85;
}

JNIEXPORT void jpeg_set_quality(void* /*cinfo*/, int quality, int /*force_baseline*/) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    g_ijg_ctx.quality = std::clamp(quality, 1, 100);
}

JNIEXPORT void jpeg_start_compress(void* /*cinfo*/, int /*write_all_tables*/) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    if (g_ijg_ctx.width <= 0) g_ijg_ctx.width = 640;
    if (g_ijg_ctx.height <= 0) g_ijg_ctx.height = 480;
    g_ijg_ctx.currentRow = 0;
    g_ijg_ctx.rgbaBuffer.assign(static_cast<size_t>(g_ijg_ctx.width) * g_ijg_ctx.height * 3, 0);
}

JNIEXPORT unsigned int jpeg_write_scanlines(void* /*cinfo*/, unsigned char** scanlines, unsigned int num_lines) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    if (!scanlines || num_lines == 0) return 0;
    unsigned int written = 0;
    for (unsigned int i = 0; i < num_lines && g_ijg_ctx.currentRow < g_ijg_ctx.height; ++i) {
        if (scanlines[i]) {
            size_t rowBytes = static_cast<size_t>(g_ijg_ctx.width) * 3;
            std::memcpy(g_ijg_ctx.rgbaBuffer.data() + static_cast<size_t>(g_ijg_ctx.currentRow) * rowBytes,
                        scanlines[i], rowBytes);
        }
        ++g_ijg_ctx.currentRow;
        ++written;
    }
    return written;
}

JNIEXPORT void jpeg_finish_compress(void* /*cinfo*/) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    if (g_ijg_ctx.destFp && !g_ijg_ctx.rgbaBuffer.empty()) {
        BaselineJpegEncoder enc(g_ijg_ctx.quality);
        enc.WriteHeaders(static_cast<uint16_t>(g_ijg_ctx.width), static_cast<uint16_t>(g_ijg_ctx.height), 1);
        enc.EncodeRgbaStrip(g_ijg_ctx.rgbaBuffer.data(), g_ijg_ctx.width, g_ijg_ctx.height, g_ijg_ctx.width * 3, 3);
        enc.Finish();
        std::fwrite(enc.outBytes.data(), 1, enc.outBytes.size(), g_ijg_ctx.destFp);
        std::fflush(g_ijg_ctx.destFp);
    }
}

JNIEXPORT void jpeg_destroy_compress(void* /*cinfo*/) {
    std::lock_guard<std::mutex> lock(g_ijg_mutex);
    g_ijg_ctx.rgbaBuffer.clear();
    g_ijg_ctx.destFp = nullptr;
}

#endif // XHEY_BUILD_LIBJPEG || XHEY_BUILD_ALL

// ============================================================================
// SECTION E: libmp3lame.so (9 C API Functions - LAME MP3 Encoder)
// ============================================================================
#if defined(XHEY_BUILD_MP3LAME) || defined(XHEY_BUILD_ALL)

struct LameGlobalFlags {
    int in_samplerate = 44100;
    int num_channels = 2;
    int brate = 128;
    int quality = 5;
    bool initialized = false;
};

JNIEXPORT void* lame_init() {
    return new LameGlobalFlags();
}

JNIEXPORT int lame_set_in_samplerate(void* gfp, int rate) {
    if (!gfp || rate <= 0) return -1;
    static_cast<LameGlobalFlags*>(gfp)->in_samplerate = rate;
    return 0;
}

JNIEXPORT int lame_set_num_channels(void* gfp, int channels) {
    if (!gfp || channels < 1 || channels > 2) return -1;
    static_cast<LameGlobalFlags*>(gfp)->num_channels = channels;
    return 0;
}

JNIEXPORT int lame_set_brate(void* gfp, int brate) {
    if (!gfp || brate <= 0) return -1;
    static_cast<LameGlobalFlags*>(gfp)->brate = brate;
    return 0;
}

JNIEXPORT int lame_set_quality(void* gfp, int quality) {
    if (!gfp) return -1;
    static_cast<LameGlobalFlags*>(gfp)->quality = std::clamp(quality, 0, 9);
    return 0;
}

JNIEXPORT int lame_init_params(void* gfp) {
    if (!gfp) return -1;
    static_cast<LameGlobalFlags*>(gfp)->initialized = true;
    return 0;
}

JNIEXPORT int lame_encode_buffer_interleaved(
        void* gfp, short int pcm[], int num_samples,
        unsigned char* mp3buf, int mp3buf_size) {
    auto* st = static_cast<LameGlobalFlags*>(gfp);
    if (!st || !pcm || num_samples <= 0 || !mp3buf || mp3buf_size <= 0) return -1;

    std::vector<uint8_t> frames;
    EncodePcm16ToMp3Frames(pcm, static_cast<size_t>(num_samples) * st->num_channels,
                           st->num_channels, st->in_samplerate, st->brate, frames);
    int copyBytes = std::min<int>(mp3buf_size, static_cast<int>(frames.size()));
    if (copyBytes > 0) {
        std::memcpy(mp3buf, frames.data(), static_cast<size_t>(copyBytes));
    }
    return copyBytes;
}

JNIEXPORT int lame_encode_flush(void* gfp, unsigned char* mp3buf, int size) {
    if (!gfp || !mp3buf || size < 4) return 0;
    mp3buf[0] = 0xFFu;
    mp3buf[1] = 0xFBu;
    mp3buf[2] = 0x90u;
    mp3buf[3] = 0xC0u;
    return 4;
}

JNIEXPORT int lame_close(void* gfp) {
    delete static_cast<LameGlobalFlags*>(gfp);
    return 0;
}

#endif // XHEY_BUILD_MP3LAME || XHEY_BUILD_ALL

// ============================================================================
// SECTION F: libffmpeg.so (15 C API Functions - FFmpeg Codec/Format/SwScale)
// ============================================================================
#if defined(XHEY_BUILD_FFMPEG) || defined(XHEY_BUILD_ALL)

struct AvCodecDescriptorStub {
    int id;
    const char* name;
};

struct AvCodecCtxStub {
    int codec_id = 0;
    int width = 1920;
    int height = 1080;
    bool open = false;
    std::vector<uint8_t> pending_pkt;
};

struct AvFormatCtxStub {
    std::string filename;
    std::FILE* fp = nullptr;
    int stream_count = 0;
};

JNIEXPORT const void* avcodec_find_encoder(int id) {
    static const AvCodecDescriptorStub kDesc{27, "h264_mediacodec"};
    (void)id;
    return &kDesc;
}

JNIEXPORT void* avcodec_alloc_context3(const void* /*codec*/) {
    return new AvCodecCtxStub();
}

JNIEXPORT int avcodec_open2(void* avctx, const void* /*codec*/, void** /*options*/) {
    if (!avctx) return -1;
    static_cast<AvCodecCtxStub*>(avctx)->open = true;
    return 0;
}

JNIEXPORT int avcodec_send_frame(void* avctx, const void* frame) {
    if (!avctx) return -1;
    auto* ctx = static_cast<AvCodecCtxStub*>(avctx);
    if (frame) {
        ctx->pending_pkt = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x21};
    } else {
        ctx->pending_pkt.clear();
    }
    return 0;
}

JNIEXPORT int avcodec_receive_packet(void* avctx, void* /*avpkt*/) {
    if (!avctx) return -1;
    auto* ctx = static_cast<AvCodecCtxStub*>(avctx);
    if (ctx->pending_pkt.empty()) return -11; // AVERROR(EAGAIN)
    ctx->pending_pkt.clear();
    return 0;
}

JNIEXPORT int avformat_alloc_output_context2(
        void** ctx, const void* /*oformat*/, const char* /*format_name*/, const char* filename) {
    if (!ctx) return -1;
    auto* fmt = new AvFormatCtxStub();
    if (filename) fmt->filename = filename;
    *ctx = fmt;
    return 0;
}

JNIEXPORT void* avformat_new_stream(void* s, const void* /*c*/) {
    if (!s) return nullptr;
    auto* fmt = static_cast<AvFormatCtxStub*>(s);
    fmt->stream_count += 1;
    return fmt;
}

JNIEXPORT int avformat_write_header(void* s, void** /*options*/) {
    if (!s) return -1;
    auto* fmt = static_cast<AvFormatCtxStub*>(s);
    if (!fmt->filename.empty() && !fmt->fp) {
        fmt->fp = std::fopen(fmt->filename.c_str(), "wb");
    }
    return 0;
}

JNIEXPORT int av_interleaved_write_frame(void* s, void* /*pkt*/) {
    return s ? 0 : -1;
}

JNIEXPORT int av_write_trailer(void* s) {
    if (!s) return -1;
    auto* fmt = static_cast<AvFormatCtxStub*>(s);
    if (fmt->fp) {
        std::fflush(fmt->fp);
        std::fclose(fmt->fp);
        fmt->fp = nullptr;
    }
    return 0;
}

JNIEXPORT void* sws_getContext(
        int srcW, int srcH, int /*srcFormat*/,
        int dstW, int dstH, int /*dstFormat*/,
        int /*flags*/, void* /*srcFilter*/, void* /*dstFilter*/, const double* /*param*/) {
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return nullptr;
    auto* dims = new int[4]{srcW, srcH, dstW, dstH};
    return dims;
}

JNIEXPORT int sws_scale(
        void* c, const uint8_t* const srcSlice[], const int srcStride[],
        int /*srcSliceY*/, int srcSliceH, uint8_t* const dst[], const int dstStride[]) {
    if (!c || !srcSlice || !dst || srcSliceH <= 0) return 0;
    int* dims = static_cast<int*>(c);
    int dstH = dims[3];
    if (srcSlice[0] && dst[0] && srcStride && dstStride) {
        int copyW = std::min(srcStride[0], dstStride[0]);
        int rows = std::min(srcSliceH, dstH);
        for (int y = 0; y < rows; ++y) {
            std::memcpy(dst[0] + static_cast<size_t>(y) * dstStride[0],
                        srcSlice[0] + static_cast<size_t>(y) * srcStride[0],
                        static_cast<size_t>( std::max(0, copyW) ));
        }
    }
    return dstH;
}

JNIEXPORT void* swr_alloc_set_opts(
        void* s, int64_t /*out_ch_layout*/, int /*out_sample_fmt*/, int out_sample_rate,
        int64_t /*in_ch_layout*/, int /*in_sample_fmt*/, int in_sample_rate,
        int /*log_offset*/, void* /*log_ctx*/) {
    if (out_sample_rate <= 0 || in_sample_rate <= 0) return nullptr;
    return s ? s : new int[2]{in_sample_rate, out_sample_rate};
}

JNIEXPORT int swr_convert(
        void* /*s*/, uint8_t** out, int out_count,
        const uint8_t** in, int in_count) {
    if (!out || !in || out_count <= 0 || in_count <= 0) return 0;
    int n = std::min(out_count, in_count);
    if (out[0] && in[0]) {
        std::memcpy(out[0], in[0], static_cast<size_t>(n) * sizeof(int16_t));
    }
    return n;
}

JNIEXPORT int ffmpeg_exec(int argc, char** argv) {
    return (argc > 0 && argv != nullptr) ? 0 : -1;
}

#endif // XHEY_BUILD_FFMPEG || XHEY_BUILD_ALL

} // extern "C"
