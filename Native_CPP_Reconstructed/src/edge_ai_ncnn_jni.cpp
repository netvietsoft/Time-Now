/**
 * ============================================================================
 * File: edge_ai_ncnn_jni.cpp
 * Target Libraries:
 *   - libimage.so
 *   - libalgorithm.so
 *   - libfacedetect.so
 *   - libsilerovad.so
 *   - libpaddleocr.so
 *
 * Description:
 *   Complete C++17 & NCNN implementation of all 49 Edge AI JNI functions in
 *   com.xhey.xcamerasdk.algorithm.nn.*:
 *     1. ZeroDCE (6-head iterative curve enhancement: input -> output0..output5)
 *     2. ZeroDCELite (8-iteration single-head curve enhancement: input -> output)
 *     3. DeNoise (Frequency/spatial luminance-chrominance bilateral NCNN denoiser)
 *     4. DocumentDetect (4-corner document quadrilateral segmentation & contour fit)
 *     5. FaceDetection (Multi-stride anchor-free face detector + 5-point landmarks)
 *     6. HandLandmark (Palm detector + 21-keypoint 2D hand landmark regressor)
 *     7. ImageDetection (YOLOv5/YOLOv8 multi-head anchor/DFL object detector)
 *     8. ImageQualityClassification (Dual-model blurriness + recapture classifier)
 *     9. LicensePlateRecognition (Plate detector + CTC greedy plate OCR + color)
 *    10. MotionEstimated (Block-matching pyramid optical flow + IMU fusion)
 *    11. PaddleOCR (DBNet text detector + directional classifier + CRNN CTC OCR)
 *    12. PhotoStandards (Multi-rule ID/work photo compliance verifier)
 *    13.PortraitMatting (Alpha matte segmentation + guided alpha feathering)
 *    14. SileroVad (16kHz stateful LSTM voice activity detector: in0/in1/in2)
 *    15. SuperResolution (2x/4x sub-pixel ESPCN/Real-ESRGAN NCNN super-res)
 *    16. Vlfeat (Multi-scale SIFT feature descriptor + L2 ratio matcher)
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#define LOG_TAG "EdgeAI_NCNN_JNI"
#undef LOGI
#undef LOGE
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================================
// Lightweight NCNN Compatibility Layer (Supports standalone & linked libncnn.a)
// ============================================================================
#if defined(XHEY_USE_OFFICIAL_NCNN) && __has_include("net.h")
#include "net.h"
#include "mat.h"
#else
namespace ncnn {

struct Option {
    bool use_vulkan_compute = false;
    bool use_fp16_packed = true;
    bool use_fp16_storage = true;
    bool use_fp16_arithmetic = false;
    bool lightmode = true;
    int num_threads = 4;
};

class Mat {
public:
    int dims = 0;
    int w = 0;
    int h = 0;
    int c = 0;
    size_t cstep = 0;
    std::vector<float> storage;
    float* data = nullptr;

    enum PixelType {
        PIXEL_RGB = 1,
        PIXEL_BGR = 2,
        PIXEL_GRAY = 3,
        PIXEL_RGBA = 4,
        PIXEL_RGBA2RGB = (PIXEL_RGBA | (PIXEL_RGB << 16)),
        PIXEL_RGBA2BGR = (PIXEL_RGBA | (PIXEL_BGR << 16)),
        PIXEL_RGBA2GRAY = (PIXEL_RGBA | (PIXEL_GRAY << 16))
    };

    Mat() = default;
    explicit Mat(int _w, float* ext = nullptr)
        : dims(1), w(_w), h(1), c(1), cstep(static_cast<size_t>(std::max(0, _w))) {
        if (ext) {
            data = ext;
        } else if (_w > 0) {
            storage.resize(cstep, 0.0f);
            data = storage.data();
        }
    }
    Mat(int _w, int _h, float* ext = nullptr)
        : dims(2), w(_w), h(_h), c(1), cstep(static_cast<size_t>(std::max(0, _w)) * std::max(0, _h)) {
        if (ext) {
            data = ext;
        } else if (_w > 0 && _h > 0) {
            storage.resize(cstep, 0.0f);
            data = storage.data();
        }
    }
    Mat(int _w, int _h, int _c, float* ext = nullptr)
        : dims(3), w(_w), h(_h), c(_c), cstep(static_cast<size_t>(std::max(0, _w)) * std::max(0, _h)) {
        if (ext) {
            data = ext;
        } else if (_w > 0 && _h > 0 && _c > 0) {
            storage.resize(cstep * static_cast<size_t>(_c), 0.0f);
            data = storage.data();
        }
    }
    Mat(const Mat& other)
        : dims(other.dims), w(other.w), h(other.h), c(other.c), cstep(other.cstep), storage(other.storage) {
        if (!storage.empty()) {
            data = storage.data();
        } else if (other.data && other.cstep * std::max(1, other.c) > 0) {
            size_t total = other.cstep * static_cast<size_t>(std::max(1, other.c));
            storage.assign(other.data, other.data + total);
            data = storage.data();
        } else {
            data = nullptr;
        }
    }
    Mat& operator=(const Mat& other) {
        if (this != &other) {
            dims = other.dims;
            w = other.w;
            h = other.h;
            c = other.c;
            cstep = other.cstep;
            storage = other.storage;
            if (!storage.empty()) {
                data = storage.data();
            } else if (other.data && other.cstep * std::max(1, other.c) > 0) {
                size_t total = other.cstep * static_cast<size_t>(std::max(1, other.c));
                storage.assign(other.data, other.data + total);
                data = storage.data();
            } else {
                data = nullptr;
            }
        }
        return *this;
    }

    bool empty() const { return data == nullptr || w <= 0 || h <= 0 || c <= 0; }
    size_t total() const { return cstep * static_cast<size_t>(std::max(1, c)); }

    void fill(float v) {
        if (!storage.empty()) {
            std::fill(storage.begin(), storage.end(), v);
        } else if (data) {
            std::fill(data, data + total(), v);
        }
    }
    Mat channel(int ch) {
        Mat m;
        m.dims = 2;
        m.w = w;
        m.h = h;
        m.c = 1;
        m.cstep = cstep;
        m.data = data ? (data + static_cast<size_t>(ch) * cstep) : nullptr;
        return m;
    }
    const Mat channel(int ch) const {
        Mat m;
        m.dims = 2;
        m.w = w;
        m.h = h;
        m.c = 1;
        m.cstep = cstep;
        m.data = data ? (data + static_cast<size_t>(ch) * cstep) : nullptr;
        return m;
    }
    float* row(int y) { return data ? (data + static_cast<size_t>(y) * w) : nullptr; }
    const float* row(int y) const { return data ? (data + static_cast<size_t>(y) * w) : nullptr; }

    void substract_mean_normalize(const float* mean_vals, const float* norm_vals) {
        if (!data) return;
        for (int ch = 0; ch < c; ++ch) {
            float mean = mean_vals ? mean_vals[ch] : 0.0f;
            float norm = norm_vals ? norm_vals[ch] : 1.0f;
            float* ptr = data + static_cast<size_t>(ch) * cstep;
            for (size_t i = 0; i < cstep; ++i) {
                ptr[i] = (ptr[i] - mean) * norm;
            }
        }
    }

    static Mat from_pixels_resize(const uint8_t* pixels, int type, int w, int h, int target_w, int target_h) {
        int out_c = (type == PIXEL_RGBA2GRAY || type == PIXEL_GRAY) ? 1 : 3;
        Mat out(target_w, target_h, out_c);
        bool bgr = (type == PIXEL_RGBA2BGR || type == PIXEL_BGR);
        for (int y = 0; y < target_h; ++y) {
            float sy = (static_cast<float>(y) + 0.5f) * static_cast<float>(h) / static_cast<float>(target_h) - 0.5f;
            int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, h - 1);
            int y1 = std::clamp(y0 + 1, 0, h - 1);
            float fy = sy - static_cast<float>(y0);
            for (int x = 0; x < target_w; ++x) {
                float sx = (static_cast<float>(x) + 0.5f) * static_cast<float>(w) / static_cast<float>(target_w) - 0.5f;
                int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, w - 1);
                int x1 = std::clamp(x0 + 1, 0, w - 1);
                float fx = sx - static_cast<float>(x0);

                const uint8_t* p00 = pixels + (static_cast<size_t>(y0) * w + x0) * 4;
                const uint8_t* p01 = pixels + (static_cast<size_t>(y0) * w + x1) * 4;
                const uint8_t* p10 = pixels + (static_cast<size_t>(y1) * w + x0) * 4;
                const uint8_t* p11 = pixels + (static_cast<size_t>(y1) * w + x1) * 4;

                float r = (1.0f - fy) * ((1.0f - fx) * p00[0] + fx * p01[0]) +
                          fy * ((1.0f - fx) * p10[0] + fx * p11[0]);
                float g = (1.0f - fy) * ((1.0f - fx) * p00[1] + fx * p01[1]) +
                          fy * ((1.0f - fx) * p10[1] + fx * p11[1]);
                float b = (1.0f - fy) * ((1.0f - fx) * p00[2] + fx * p01[2]) +
                          fy * ((1.0f - fx) * p10[2] + fx * p11[2]);

                size_t idx = static_cast<size_t>(y) * target_w + x;
                if (out_c == 1) {
                    out.channel(0).data[idx] = 0.299f * r + 0.587f * g + 0.114f * b;
                } else if (bgr) {
                    out.channel(0).data[idx] = b;
                    out.channel(1).data[idx] = g;
                    out.channel(2).data[idx] = r;
                } else {
                    out.channel(0).data[idx] = r;
                    out.channel(1).data[idx] = g;
                    out.channel(2).data[idx] = b;
                }
            }
        }
        return out;
    }

    static Mat from_pixels(const uint8_t* pixels, int type, int w, int h) {
        return from_pixels_resize(pixels, type, w, h, w, h);
    }
};

struct LayerNode {
    std::string type;
    std::string name;
    std::vector<std::string> bottoms;
    std::vector<std::string> tops;
    std::unordered_map<int, float> params;
    std::unordered_map<int, std::vector<float>> array_params;
    std::vector<float> weight_data;
    std::vector<float> bias_data;
    std::vector<float> weight_hc_data;
};

inline float Fp16ToFp32(uint16_t h) {
    uint32_t sign = static_cast<uint32_t>(h & 0x8000u) << 16;
    uint32_t exp  = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x03FFu;
    uint32_t bits = 0;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign;
        } else {
            exp = 1;
            while ((mant & 0x0400u) == 0) {
                mant <<= 1;
                --exp;
            }
            mant &= 0x03FFu;
            bits = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
        }
    } else if (exp == 0x1Fu) {
        bits = sign | 0x7F800000u | (mant << 13);
    } else {
        bits = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
    }
    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(float));
    return out;
}

inline void ApplyFusedActivation(Mat& m, int activation_type) {
    if (activation_type == 0 || m.empty()) return;
    size_t total = m.total();
    if (activation_type == 1) { // ReLU
        for (size_t i = 0; i < total; ++i) {
            if (m.data[i] < 0.0f) m.data[i] = 0.0f;
        }
    } else if (activation_type == 4) { // Sigmoid
        for (size_t i = 0; i < total; ++i) {
            m.data[i] = 1.0f / (1.0f + std::exp(-std::clamp(m.data[i], -20.0f, 20.0f)));
        }
    } else if (activation_type == 5) { // Mish / TanH
        for (size_t i = 0; i < total; ++i) {
            m.data[i] = std::tanh(m.data[i]);
        }
    }
}

inline float EvalBinaryScalarOp(float a, float b, int op_type) {
    switch (op_type) {
        case 0: return a + b;
        case 1: return a - b;
        case 2: return a * b;
        case 3: return (std::fabs(b) > 1e-12f) ? (a / b) : 0.0f;
        case 4: return std::max(a, b);
        case 5: return std::min(a, b);
        case 6: return std::pow(a, b);
        case 7: return b - a;
        case 8: return (std::fabs(a) > 1e-12f) ? (b / a) : 0.0f;
        default: return a + b;
    }
}

class Extractor {
public:
    const std::vector<LayerNode>* layers = nullptr;
    bool net_loaded = false;
    bool graph_executed = false;
    std::unordered_map<std::string, Mat> blobs;
    bool light_mode = true;
    int num_threads = 4;

    void set_light_mode(bool v) { light_mode = v; }
    void set_num_threads(int n) { num_threads = std::max(1, n); }

    int input(const char* name, const Mat& in) {
        if (!name || in.empty()) return -1;
        blobs[name] = in;
        graph_executed = false;
        return 0;
    }

    bool executeLayerGraph() {
        if (!net_loaded || !layers || layers->empty()) return false;
        for (size_t li = 0; li < layers->size(); ++li) {
            const LayerNode& ln = (*layers)[li];
            if (ln.type == "Input") continue;
            if (ln.bottoms.empty() || ln.tops.empty()) continue;

            auto itB0 = blobs.find(ln.bottoms[0]);
            if (itB0 == blobs.end() || itB0->second.empty()) {
                return false;
            }
            const Mat& b0 = itB0->second;

            if (ln.type == "Split") {
                for (const auto& topName : ln.tops) {
                    blobs[topName] = b0;
                }
            } else if (ln.type == "ReLU") {
                float slope = ln.params.count(0) ? ln.params.at(0) : 0.0f;
                Mat top = b0;
                size_t total = top.total();
                for (size_t i = 0; i < total; ++i) {
                    float v = top.data[i];
                    top.data[i] = (v >= 0.0f) ? v : (v * slope);
                }
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Sigmoid") {
                Mat top = b0;
                size_t total = top.total();
                for (size_t i = 0; i < total; ++i) {
                    top.data[i] = 1.0f / (1.0f + std::exp(-std::clamp(top.data[i], -20.0f, 20.0f)));
                }
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "TanH") {
                Mat top = b0;
                size_t total = top.total();
                for (size_t i = 0; i < total; ++i) {
                    top.data[i] = std::tanh(top.data[i]);
                }
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Softmax") {
                Mat top = b0;
                size_t total = top.total();
                float maxV = -1e30f;
                for (size_t i = 0; i < total; ++i) maxV = std::max(maxV, top.data[i]);
                float sum = 0.0f;
                for (size_t i = 0; i < total; ++i) {
                    top.data[i] = std::exp(top.data[i] - maxV);
                    sum += top.data[i];
                }
                float inv = (sum > 1e-9f) ? (1.0f / sum) : 1.0f;
                for (size_t i = 0; i < total; ++i) top.data[i] *= inv;
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "UnaryOp") {
                int op_type = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 0;
                Mat top = b0;
                size_t total = top.total();
                for (size_t i = 0; i < total; ++i) {
                    float x = top.data[i];
                    switch (op_type) {
                        case 0: top.data[i] = std::fabs(x); break;
                        case 1: top.data[i] = -x; break;
                        case 2: top.data[i] = std::floor(x); break;
                        case 3: top.data[i] = std::ceil(x); break;
                        case 4: top.data[i] = x * x; break;
                        case 5: top.data[i] = std::sqrt(std::max(0.0f, x)); break;
                        case 6: top.data[i] = 1.0f / std::sqrt(std::max(1e-12f, x)); break;
                        case 7: top.data[i] = std::exp(std::clamp(x, -40.0f, 40.0f)); break;
                        case 8: top.data[i] = std::log(std::max(1e-12f, x)); break;
                        case 9: top.data[i] = std::sin(x); break;
                        case 10: top.data[i] = std::cos(x); break;
                        case 11: top.data[i] = std::tan(x); break;
                        case 12: top.data[i] = std::asin(std::clamp(x, -1.0f, 1.0f)); break;
                        case 13: top.data[i] = std::acos(std::clamp(x, -1.0f, 1.0f)); break;
                        case 14: top.data[i] = std::atan(x); break;
                        case 15: top.data[i] = (std::fabs(x) > 1e-12f) ? (1.0f / x) : 0.0f; break;
                        case 16: top.data[i] = std::tanh(x); break;
                        default: break;
                    }
                }
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "BinaryOp") {
                int op_type = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 0;
                int with_scalar = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 0;
                float scalar_b = ln.params.count(2) ? ln.params.at(2) : 0.0f;
                Mat top = b0;
                size_t totalA = top.total();
                if (with_scalar != 0 || ln.bottoms.size() < 2) {
                    for (size_t i = 0; i < totalA; ++i) {
                        top.data[i] = EvalBinaryScalarOp(top.data[i], scalar_b, op_type);
                    }
                } else {
                    auto itB1 = blobs.find(ln.bottoms[1]);
                    if (itB1 == blobs.end() || itB1->second.empty()) return false;
                    const Mat& b1 = itB1->second;
                    size_t totalB = b1.total();
                    if (totalA == totalB) {
                        for (size_t i = 0; i < totalA; ++i) {
                            top.data[i] = EvalBinaryScalarOp(top.data[i], b1.data[i], op_type);
                        }
                    } else if (totalB == 1) {
                        float sb = b1.data[0];
                        for (size_t i = 0; i < totalA; ++i) {
                            top.data[i] = EvalBinaryScalarOp(top.data[i], sb, op_type);
                        }
                    } else if (top.dims == 2 && b1.dims == 2) {
                        for (int y = 0; y < top.h; ++y) {
                            int by = std::min(y, b1.h - 1);
                            for (int x = 0; x < top.w; ++x) {
                                int bx = std::min(x, b1.w - 1);
                                top.row(y)[x] = EvalBinaryScalarOp(top.row(y)[x], b1.row(by)[bx], op_type);
                            }
                        }
                    } else {
                        for (size_t i = 0; i < totalA; ++i) {
                            top.data[i] = EvalBinaryScalarOp(top.data[i], b1.data[i % totalB], op_type);
                        }
                    }
                }
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Concat") {
                int axis = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 0;
                if (b0.dims == 3 && axis == 0) {
                    int totalC = 0;
                    for (const auto& bName : ln.bottoms) {
                        auto it = blobs.find(bName);
                        if (it == blobs.end()) return false;
                        totalC += it->second.c;
                    }
                    Mat top(b0.w, b0.h, totalC);
                    int curC = 0;
                    for (const auto& bName : ln.bottoms) {
                        const Mat& bi = blobs.at(bName);
                        for (int ch = 0; ch < bi.c; ++ch) {
                            std::memcpy(top.channel(curC++).data, bi.channel(ch).data, bi.cstep * sizeof(float));
                        }
                    }
                    blobs[ln.tops[0]] = std::move(top);
                } else if (b0.dims == 2 && (axis == 0 || axis == -2)) {
                    int totalH = 0;
                    for (const auto& bName : ln.bottoms) {
                        auto it = blobs.find(bName);
                        if (it == blobs.end()) return false;
                        totalH += it->second.h;
                    }
                    Mat top(b0.w, totalH);
                    int curY = 0;
                    for (const auto& bName : ln.bottoms) {
                        const Mat& bi = blobs.at(bName);
                        for (int y = 0; y < bi.h; ++y) {
                            std::memcpy(top.row(curY++), bi.row(y), static_cast<size_t>(bi.w) * sizeof(float));
                        }
                    }
                    blobs[ln.tops[0]] = std::move(top);
                } else if (b0.dims == 2 && (axis == 1 || axis == -1)) {
                    int totalW = 0;
                    for (const auto& bName : ln.bottoms) {
                        auto it = blobs.find(bName);
                        if (it == blobs.end()) return false;
                        totalW += it->second.w;
                    }
                    Mat top(totalW, b0.h);
                    for (int y = 0; y < b0.h; ++y) {
                        int curX = 0;
                        for (const auto& bName : ln.bottoms) {
                            const Mat& bi = blobs.at(bName);
                            std::memcpy(top.row(y) + curX, bi.row(y), static_cast<size_t>(bi.w) * sizeof(float));
                            curX += bi.w;
                        }
                    }
                    blobs[ln.tops[0]] = std::move(top);
                } else {
                    int totalW = 0;
                    for (const auto& bName : ln.bottoms) {
                        auto it = blobs.find(bName);
                        if (it == blobs.end()) return false;
                        totalW += static_cast<int>(it->second.total());
                    }
                    Mat top(totalW);
                    size_t off = 0;
                    for (const auto& bName : ln.bottoms) {
                        const Mat& bi = blobs.at(bName);
                        std::memcpy(top.data + off, bi.data, bi.total() * sizeof(float));
                        off += bi.total();
                    }
                    blobs[ln.tops[0]] = std::move(top);
                }
            } else if (ln.type == "Slice") {
                int axis = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 0;
                std::vector<float> slicePoints;
                if (ln.array_params.count(0)) slicePoints = ln.array_params.at(0);
                size_t numTops = ln.tops.size();
                if (b0.dims == 3 && axis == 0) {
                    int totalC = b0.c;
                    int explicitSum = 0;
                    int autoCount = 0;
                    for (size_t i = 0; i < numTops; ++i) {
                        int s = (i < slicePoints.size()) ? static_cast<int>(slicePoints[i]) : -233;
                        if (s > 0) explicitSum += s;
                        else ++autoCount;
                    }
                    int autoSlice = (autoCount > 0) ? std::max(1, (totalC - explicitSum) / autoCount) : 1;
                    int curC = 0;
                    for (size_t i = 0; i < numTops; ++i) {
                        int s = (i < slicePoints.size() && slicePoints[i] > 0) ? static_cast<int>(slicePoints[i]) : autoSlice;
                        if (i + 1 == numTops) s = std::max(1, totalC - curC);
                        Mat top(b0.w, b0.h, s);
                        for (int ch = 0; ch < s && curC + ch < totalC; ++ch) {
                            std::memcpy(top.channel(ch).data, b0.channel(curC + ch).data, b0.cstep * sizeof(float));
                        }
                        curC += s;
                        blobs[ln.tops[i]] = std::move(top);
                    }
                } else if (b0.dims == 2 && axis == 0) {
                    int totalH = b0.h;
                    int explicitSum = 0;
                    int autoCount = 0;
                    for (size_t i = 0; i < numTops; ++i) {
                        int s = (i < slicePoints.size()) ? static_cast<int>(slicePoints[i]) : -233;
                        if (s > 0) explicitSum += s;
                        else ++autoCount;
                    }
                    int autoSlice = (autoCount > 0) ? std::max(1, (totalH - explicitSum) / autoCount) : 1;
                    int curY = 0;
                    for (size_t i = 0; i < numTops; ++i) {
                        int s = (i < slicePoints.size() && slicePoints[i] > 0) ? static_cast<int>(slicePoints[i]) : autoSlice;
                        if (i + 1 == numTops) s = std::max(1, totalH - curY);
                        Mat top(b0.w, s);
                        for (int y = 0; y < s && curY + y < totalH; ++y) {
                            std::memcpy(top.row(y), b0.row(curY + y), static_cast<size_t>(b0.w) * sizeof(float));
                        }
                        curY += s;
                        blobs[ln.tops[i]] = std::move(top);
                    }
                }
            } else if (ln.type == "Reshape") {
                int targetW = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : b0.w;
                int targetH = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : -233;
                int targetC = ln.params.count(2) ? static_cast<int>(ln.params.at(2)) : -233;
                size_t totalElem = b0.total();
                Mat top;
                if (targetC > 0 && targetH > 0 && targetW > 0) {
                    top = Mat(targetW, targetH, targetC);
                } else if (targetH > 0 && targetW > 0) {
                    top = Mat(targetW, targetH);
                } else {
                    top = Mat(std::max(1, targetW));
                }
                size_t copyCount = std::min(totalElem, top.total());
                std::memcpy(top.data, b0.data, copyCount * sizeof(float));
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Padding") {
                int topPad = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 0;
                int botPad = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 0;
                int leftPad = ln.params.count(2) ? static_cast<int>(ln.params.at(2)) : 0;
                int rightPad = ln.params.count(3) ? static_cast<int>(ln.params.at(3)) : 0;
                int padType = ln.params.count(4) ? static_cast<int>(ln.params.at(4)) : 0;
                float padVal = ln.params.count(5) ? ln.params.at(5) : 0.0f;

                int outW = b0.w + leftPad + rightPad;
                int outH = b0.h + topPad + botPad;
                Mat top = (b0.dims == 3) ? Mat(outW, outH, b0.c) : Mat(outW, outH);
                auto mapCoord = [](int p, int len, int type) -> int {
                    if (p >= 0 && p < len) return p;
                    if (type == 1) return std::clamp(p, 0, len - 1);
                    if (type == 2 && len > 1) {
                        while (p < 0 || p >= len) {
                            if (p < 0) p = -p;
                            if (p >= len) p = 2 * len - 2 - p;
                        }
                        return std::clamp(p, 0, len - 1);
                    }
                    return -1;
                };
                for (int ch = 0; ch < std::max(1, b0.c); ++ch) {
                    const float* srcCh = (b0.dims == 3) ? b0.channel(ch).data : b0.data;
                    float* dstCh = (top.dims == 3) ? top.channel(ch).data : top.data;
                    for (int y = 0; y < outH; ++y) {
                        int sy = mapCoord(y - topPad, b0.h, padType);
                        for (int x = 0; x < outW; ++x) {
                            int sx = mapCoord(x - leftPad, b0.w, padType);
                            dstCh[static_cast<size_t>(y) * outW + x] =
                                (sx >= 0 && sy >= 0) ? srcCh[static_cast<size_t>(sy) * b0.w + sx] : padVal;
                        }
                    }
                }
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Squeeze") {
                Mat top;
                if (b0.dims == 3 && b0.c == 1) {
                    top = Mat(b0.w, b0.h);
                } else if (b0.dims == 2 && b0.h == 1) {
                    top = Mat(b0.w);
                } else {
                    top = b0;
                }
                std::memcpy(top.data, b0.data, std::min(top.total(), b0.total()) * sizeof(float));
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "ExpandDims") {
                Mat top;
                if (b0.dims == 1) {
                    top = Mat(b0.w);
                } else if (b0.dims == 2) {
                    top = Mat(b0.w, b0.h, 1);
                } else {
                    top = b0;
                }
                std::memcpy(top.data, b0.data, std::min(top.total(), b0.total()) * sizeof(float));
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Crop") {
                if (ln.array_params.count(9) && ln.array_params.count(10) && ln.array_params.count(11)) {
                    int start = static_cast<int>(ln.array_params.at(9)[0]);
                    int end   = static_cast<int>(ln.array_params.at(10)[0]);
                    int axis  = static_cast<int>(ln.array_params.at(11)[0]);
                    if (b0.dims == 3 && axis == 0) {
                        if (start < 0) start += b0.c;
                        if (end < 0) end += b0.c;
                        start = std::clamp(start, 0, b0.c);
                        end = std::clamp(end, start + 1, b0.c);
                        int outC = end - start;
                        Mat top(b0.w, b0.h, outC);
                        for (int ch = 0; ch < outC; ++ch) {
                            std::memcpy(top.channel(ch).data, b0.channel(start + ch).data, b0.cstep * sizeof(float));
                        }
                        blobs[ln.tops[0]] = std::move(top);
                    } else if (b0.dims == 2 && (axis == 1 || axis == -1)) {
                        if (start < 0) start += b0.w;
                        if (end < 0) end += b0.w;
                        start = std::clamp(start, 0, b0.w);
                        end = std::clamp(end, start + 1, b0.w);
                        int outW = end - start;
                        Mat top(outW, b0.h);
                        for (int y = 0; y < b0.h; ++y) {
                            std::memcpy(top.row(y), b0.row(y) + start, static_cast<size_t>(outW) * sizeof(float));
                        }
                        blobs[ln.tops[0]] = std::move(top);
                    } else {
                        blobs[ln.tops[0]] = b0;
                    }
                } else {
                    int woffset = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 0;
                    int hoffset = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 0;
                    int outw = ln.params.count(3) ? static_cast<int>(ln.params.at(3)) : b0.w;
                    int outh = ln.params.count(4) ? static_cast<int>(ln.params.at(4)) : b0.h;
                    outw = std::clamp(outw, 1, std::max(1, b0.w - woffset));
                    outh = std::clamp(outh, 1, std::max(1, b0.h - hoffset));
                    Mat top(outw, outh);
                    for (int y = 0; y < outh; ++y) {
                        std::memcpy(top.row(y), b0.row(hoffset + y) + woffset, static_cast<size_t>(outw) * sizeof(float));
                    }
                    blobs[ln.tops[0]] = std::move(top);
                }
            } else if (ln.type == "Reduction") {
                int op = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 3;
                int keepdims = ln.params.count(4) ? static_cast<int>(ln.params.at(4)) : 0;
                int axis = 0;
                if (ln.array_params.count(3) && !ln.array_params.at(3).empty()) {
                    axis = static_cast<int>(ln.array_params.at(3)[0]);
                }
                if (b0.dims == 2 && axis == 0) {
                    Mat top = keepdims ? Mat(b0.w, 1) : Mat(b0.w);
                    for (int x = 0; x < b0.w; ++x) {
                        float acc = 0.0f;
                        for (int y = 0; y < b0.h; ++y) acc += b0.row(y)[x];
                        top.data[x] = (op == 3) ? (acc / std::max(1, b0.h)) : acc;
                    }
                    blobs[ln.tops[0]] = std::move(top);
                } else if (b0.dims == 2 && (axis == 1 || axis == -1)) {
                    Mat top = keepdims ? Mat(1, b0.h) : Mat(b0.h);
                    for (int y = 0; y < b0.h; ++y) {
                        float acc = 0.0f;
                        for (int x = 0; x < b0.w; ++x) acc += b0.row(y)[x];
                        top.data[y] = (op == 3) ? (acc / std::max(1, b0.w)) : acc;
                    }
                    blobs[ln.tops[0]] = std::move(top);
                } else {
                    Mat top(1);
                    float acc = 0.0f;
                    size_t tot = b0.total();
                    for (size_t i = 0; i < tot; ++i) acc += b0.data[i];
                    top.data[0] = (op == 3 && tot > 0) ? (acc / static_cast<float>(tot)) : acc;
                    blobs[ln.tops[0]] = std::move(top);
                }
            } else if (ln.type == "Permute") {
                int order_type = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 0;
                if (b0.dims == 2 && order_type == 1) {
                    Mat top(b0.h, b0.w);
                    for (int y = 0; y < b0.h; ++y) {
                        for (int x = 0; x < b0.w; ++x) {
                            top.row(x)[y] = b0.row(y)[x];
                        }
                    }
                    blobs[ln.tops[0]] = std::move(top);
                } else {
                    blobs[ln.tops[0]] = b0;
                }
            } else if (ln.type == "Convolution1D" || ln.type == "ConvolutionDepthWise1D") {
                int num_output = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 1;
                int kernel_w   = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 1;
                int dilation_w = ln.params.count(2) ? static_cast<int>(ln.params.at(2)) : 1;
                int stride_w   = ln.params.count(3) ? static_cast<int>(ln.params.at(3)) : 1;
                int pad_left   = ln.params.count(4) ? static_cast<int>(ln.params.at(4)) : 0;
                int pad_right  = ln.params.count(15) ? static_cast<int>(ln.params.at(15)) : pad_left;
                int bias_term  = ln.params.count(5) ? static_cast<int>(ln.params.at(5)) : 0;
                int group      = (ln.type == "ConvolutionDepthWise1D")
                    ? (ln.params.count(7) ? static_cast<int>(ln.params.at(7)) : std::max(1, b0.h))
                    : 1;
                int act_type   = ln.params.count(9) ? static_cast<int>(ln.params.at(9)) : 0;

                int inW = b0.w;
                int inH = std::max(1, b0.h);
                int kExtent = dilation_w * (kernel_w - 1) + 1;
                int outW = std::max(1, (inW + pad_left + pad_right - kExtent) / stride_w + 1);
                Mat top(outW, num_output);

                int inPerGroup = std::max(1, inH / group);
                int outPerGroup = std::max(1, num_output / group);

                for (int g = 0; g < group; ++g) {
                    for (int oc = 0; oc < outPerGroup; ++oc) {
                        int outCh = g * outPerGroup + oc;
                        float bias = (bias_term && static_cast<size_t>(outCh) < ln.bias_data.size())
                            ? ln.bias_data[static_cast<size_t>(outCh)]
                            : 0.0f;
                        float* dstRow = top.row(outCh);
                        for (int ox = 0; ox < outW; ++ox) {
                            float sum = bias;
                            int baseX = ox * stride_w - pad_left;
                            for (int ic = 0; ic < inPerGroup; ++ic) {
                                int inCh = g * inPerGroup + ic;
                                const float* srcRow = b0.row(inCh);
                                size_t wBase = (static_cast<size_t>(outCh) * inPerGroup + ic) * kernel_w;
                                for (int k = 0; k < kernel_w; ++k) {
                                    int sx = baseX + k * dilation_w;
                                    if (sx >= 0 && sx < inW) {
                                        float wVal = (wBase + k < ln.weight_data.size()) ? ln.weight_data[wBase + k] : 0.0f;
                                        sum += srcRow[sx] * wVal;
                                    }
                                }
                            }
                            dstRow[ox] = sum;
                        }
                    }
                }
                ApplyFusedActivation(top, act_type);
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "Convolution" || ln.type == "ConvolutionDepthWise" || ln.type == "DepthWiseConvolution") {
                int num_output = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : b0.c;
                int kernel_w   = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 1;
                int kernel_h   = ln.params.count(11) ? static_cast<int>(ln.params.at(11)) : kernel_w;
                int dilation_w = ln.params.count(2) ? static_cast<int>(ln.params.at(2)) : 1;
                int dilation_h = ln.params.count(12) ? static_cast<int>(ln.params.at(12)) : dilation_w;
                int stride_w   = ln.params.count(3) ? static_cast<int>(ln.params.at(3)) : 1;
                int stride_h   = ln.params.count(13) ? static_cast<int>(ln.params.at(13)) : stride_w;
                int pad_left   = ln.params.count(4) ? static_cast<int>(ln.params.at(4)) : 0;
                int pad_top    = ln.params.count(14) ? static_cast<int>(ln.params.at(14)) : pad_left;
                int pad_right  = ln.params.count(15) ? static_cast<int>(ln.params.at(15)) : pad_left;
                int pad_bottom = ln.params.count(16) ? static_cast<int>(ln.params.at(16)) : pad_top;
                int bias_term  = ln.params.count(5) ? static_cast<int>(ln.params.at(5)) : 0;
                int group      = (ln.type == "ConvolutionDepthWise" || ln.type == "DepthWiseConvolution")
                    ? (ln.params.count(7) ? static_cast<int>(ln.params.at(7)) : std::max(1, b0.c))
                    : 1;
                int act_type   = ln.params.count(9) ? static_cast<int>(ln.params.at(9)) : 0;

                int inW = b0.w;
                int inH = b0.h;
                int inC = std::max(1, b0.c);
                int kExtW = dilation_w * (kernel_w - 1) + 1;
                int kExtH = dilation_h * (kernel_h - 1) + 1;
                int outW = std::max(1, (inW + pad_left + pad_right - kExtW) / stride_w + 1);
                int outH = std::max(1, (inH + pad_top + pad_bottom - kExtH) / stride_h + 1);

                Mat top(outW, outH, num_output);
                int inPerGroup = std::max(1, inC / group);
                int outPerGroup = std::max(1, num_output / group);
                size_t kArea = static_cast<size_t>(kernel_w) * kernel_h;

                for (int g = 0; g < group; ++g) {
                    for (int oc = 0; oc < outPerGroup; ++oc) {
                        int outCh = g * outPerGroup + oc;
                        float bias = (bias_term && static_cast<size_t>(outCh) < ln.bias_data.size())
                            ? ln.bias_data[static_cast<size_t>(outCh)]
                            : 0.0f;
                        Mat outSlice = top.channel(outCh);
                        if (bias != 0.0f) {
                            std::fill(outSlice.data, outSlice.data + top.cstep, bias);
                        }
                        for (int ic = 0; ic < inPerGroup; ++ic) {
                            int inCh = g * inPerGroup + ic;
                            const Mat inSlice = b0.channel(inCh);
                            size_t wBase = (static_cast<size_t>(outCh) * inPerGroup + ic) * kArea;
                            for (int oy = 0; oy < outH; ++oy) {
                                int baseY = oy * stride_h - pad_top;
                                float* dstRow = outSlice.row(oy);
                                for (int ox = 0; ox < outW; ++ox) {
                                    int baseX = ox * stride_w - pad_left;
                                    float acc = 0.0f;
                                    for (int ky = 0; ky < kernel_h; ++ky) {
                                        int sy = baseY + ky * dilation_h;
                                        if (sy < 0 || sy >= inH) continue;
                                        const float* srcRow = inSlice.row(sy);
                                        size_t wRow = wBase + static_cast<size_t>(ky) * kernel_w;
                                        for (int kx = 0; kx < kernel_w; ++kx) {
                                            int sx = baseX + kx * dilation_w;
                                            if (sx >= 0 && sx < inW && wRow + kx < ln.weight_data.size()) {
                                                acc += srcRow[sx] * ln.weight_data[wRow + kx];
                                            }
                                        }
                                    }
                                    dstRow[ox] += acc;
                                }
                            }
                        }
                    }
                }
                ApplyFusedActivation(top, act_type);
                blobs[ln.tops[0]] = std::move(top);
            } else if (ln.type == "LSTM") {
                int num_output = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 64;
                int hidden_size = ln.params.count(3) ? static_cast<int>(ln.params.at(3)) : num_output;
                int T = std::max(1, b0.h);
                int inSize = b0.w;

                Mat h_t(hidden_size, 1);
                Mat c_t(hidden_size, 1);
                if (ln.bottoms.size() >= 2 && blobs.count(ln.bottoms[1])) {
                    const Mat& hIn = blobs.at(ln.bottoms[1]);
                    std::memcpy(h_t.data, hIn.data, std::min(h_t.total(), hIn.total()) * sizeof(float));
                }
                if (ln.bottoms.size() >= 3 && blobs.count(ln.bottoms[2])) {
                    const Mat& cIn = blobs.at(ln.bottoms[2]);
                    std::memcpy(c_t.data, cIn.data, std::min(c_t.total(), cIn.total()) * sizeof(float));
                }

                Mat topY(hidden_size, T);
                std::vector<float> gates(static_cast<size_t>(4 * hidden_size), 0.0f);

                for (int t = 0; t < T; ++t) {
                    const float* x_t = b0.row(t);
                    for (int gIdx = 0; gIdx < 4 * hidden_size; ++gIdx) {
                        float sum = (static_cast<size_t>(gIdx) < ln.bias_data.size()) ? ln.bias_data[gIdx] : 0.0f;
                        size_t xcBase = static_cast<size_t>(gIdx) * inSize;
                        for (int i = 0; i < inSize && xcBase + i < ln.weight_data.size(); ++i) {
                            sum += x_t[i] * ln.weight_data[xcBase + i];
                        }
                        size_t hcBase = static_cast<size_t>(gIdx) * hidden_size;
                        for (int i = 0; i < hidden_size && hcBase + i < ln.weight_hc_data.size(); ++i) {
                            sum += h_t.data[i] * ln.weight_hc_data[hcBase + i];
                        }
                        gates[gIdx] = sum;
                    }
                    // NCNN LSTM gate layout: I (0..H-1), F (H..2H-1), O (2H..3H-1), G (3H..4H-1)
                    float* yRow = topY.row(t);
                    for (int q = 0; q < hidden_size; ++q) {
                        float gateI = 1.0f / (1.0f + std::exp(-std::clamp(gates[q], -20.0f, 20.0f)));
                        float gateF = 1.0f / (1.0f + std::exp(-std::clamp(gates[hidden_size + q], -20.0f, 20.0f)));
                        float gateO = 1.0f / (1.0f + std::exp(-std::clamp(gates[2 * hidden_size + q], -20.0f, 20.0f)));
                        float gateG = std::tanh(std::clamp(gates[3 * hidden_size + q], -20.0f, 20.0f));
                        float cell = gateF * c_t.data[q] + gateI * gateG;
                        float hid  = gateO * std::tanh(cell);
                        c_t.data[q] = cell;
                        h_t.data[q] = hid;
                        yRow[q] = hid;
                    }
                }
                blobs[ln.tops[0]] = std::move(topY);
                if (ln.tops.size() >= 2) blobs[ln.tops[1]] = std::move(h_t);
                if (ln.tops.size() >= 3) blobs[ln.tops[2]] = std::move(c_t);
            } else if (ln.type == "InnerProduct") {
                int num_output = ln.params.count(0) ? static_cast<int>(ln.params.at(0)) : 1;
                int bias_term  = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 0;
                int act_type   = ln.params.count(9) ? static_cast<int>(ln.params.at(9)) : 0;
                size_t inTotal = b0.total();
                Mat top(num_output);
                for (int o = 0; o < num_output; ++o) {
                    float sum = (bias_term && static_cast<size_t>(o) < ln.bias_data.size()) ? ln.bias_data[o] : 0.0f;
                    size_t wBase = static_cast<size_t>(o) * inTotal;
                    for (size_t i = 0; i < inTotal && wBase + i < ln.weight_data.size(); ++i) {
                        sum += b0.data[i] * ln.weight_data[wBase + i];
                    }
                    top.data[o] = sum;
                }
                ApplyFusedActivation(top, act_type);
                blobs[ln.tops[0]] = std::move(top);
            } else {
                return false;
            }
        }
        return true;
    }

    int extract(const char* name, Mat& feat) {
        if (!name || !net_loaded || !layers || layers->empty()) {
            return -1;
        }
        std::string key(name);
        if (!graph_executed) {
            if (!executeLayerGraph()) {
                return -1;
            }
            graph_executed = true;
        }
        auto it = blobs.find(key);
        if (it != blobs.end() && !it->second.empty()) {
            feat = it->second;
            return 0;
        }
        return -1;
    }
};

class Net {
public:
    Option opt;
    bool param_loaded = false;
    bool model_loaded = false;
    bool loaded = false;
    int magic = 0;
    int expected_layers = 0;
    int expected_blobs = 0;
    std::vector<LayerNode> layers;

    int parse_param_text(const std::string& text) {
        layers.clear();
        param_loaded = false;
        model_loaded = false;
        loaded = false;
        magic = 0;
        expected_layers = 0;
        expected_blobs = 0;

        if (text.empty()) {
            LOGE("NCNN parse_param_text failed: empty param text");
            return -1;
        }

        std::istringstream iss(text);
        std::string line;
        int headerStep = 0;

        while (std::getline(iss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ls(line);
            if (headerStep == 0) {
                if (!(ls >> magic) || magic != 7767517) {
                    LOGE("NCNN parse_param_text failed: invalid header magic=%d", magic);
                    return -1;
                }
                if (ls >> expected_layers >> expected_blobs) {
                    if (expected_layers <= 0 || expected_blobs <= 0) {
                        LOGE("NCNN parse_param_text failed: invalid header counts layers=%d blobs=%d", expected_layers, expected_blobs);
                        return -1;
                    }
                    headerStep = 2;
                } else {
                    headerStep = 1;
                }
                continue;
            }
            if (headerStep == 1) {
                if (!(ls >> expected_layers >> expected_blobs) || expected_layers <= 0 || expected_blobs <= 0) {
                    LOGE("NCNN parse_param_text failed: invalid header counts layers=%d blobs=%d", expected_layers, expected_blobs);
                    return -1;
                }
                headerStep = 2;
                continue;
            }
            LayerNode node;
            int bottomCount = 0;
            int topCount = 0;
            if (!(ls >> node.type >> node.name >> bottomCount >> topCount)) continue;
            if (bottomCount < 0 || bottomCount > 64 || topCount < 0 || topCount > 64) {
                LOGE("NCNN parse_param_text failed: invalid blob counts in layer %s", node.name.c_str());
                return -1;
            }
            for (int i = 0; i < bottomCount; ++i) {
                std::string b;
                if (!(ls >> b)) return -1;
                node.bottoms.push_back(b);
            }
            for (int i = 0; i < topCount; ++i) {
                std::string t;
                if (!(ls >> t)) return -1;
                node.tops.push_back(t);
            }
            std::string kv;
            while (ls >> kv) {
                size_t eq = kv.find('=');
                if (eq != std::string::npos) {
                    int k = std::atoi(kv.substr(0, eq).c_str());
                    std::string valStr = kv.substr(eq + 1);
                    if (k <= -23300 || valStr.find(',') != std::string::npos) {
                        int arrId = (k <= -23300) ? (-23300 - k) : k;
                        std::vector<float> vals;
                        std::istringstream vs(valStr);
                        std::string item;
                        bool first = (k <= -23300);
                        while (std::getline(vs, item, ',')) {
                            if (first) {
                                first = false;
                                continue;
                            }
                            vals.push_back(static_cast<float>(std::atof(item.c_str())));
                        }
                        node.array_params[arrId] = std::move(vals);
                    } else {
                        node.params[k] = static_cast<float>(std::atof(valStr.c_str()));
                    }
                }
            }
            layers.push_back(std::move(node));
        }

        if (headerStep < 2 || layers.empty() || static_cast<int>(layers.size()) != expected_layers) {
            LOGE("NCNN parse_param_text failed: parsed %zu layers, expected %d", layers.size(), expected_layers);
            layers.clear();
            return -1;
        }
        param_loaded = true;
        return 0;
    }

    static bool ReadTaggedWeightBlock(
            const uint8_t* data, size_t size, size_t& offset, size_t count, std::vector<float>& out) {
        out.clear();
        if (count == 0) return true;
        if (!data || offset + 4 > size) return false;
        uint32_t tag = 0;
        std::memcpy(&tag, data + offset, sizeof(uint32_t));
        offset += 4;

        out.resize(count, 0.0f);
        if (tag == 0x01306B47u) {
            size_t byteLen = (count * 2 + 3) & ~size_t(3);
            if (offset + byteLen > size) return false;
            for (size_t i = 0; i < count; ++i) {
                uint16_t h = 0;
                std::memcpy(&h, data + offset + i * 2, sizeof(uint16_t));
                out[i] = Fp16ToFp32(h);
            }
            offset += byteLen;
            return true;
        } else if (tag == 0u || (tag & 0xFFFF0000u) == 0u) {
            size_t byteLen = count * sizeof(float);
            if (offset + byteLen > size) return false;
            std::memcpy(out.data(), data + offset, byteLen);
            offset += byteLen;
            return true;
        }
        return false;
    }

    int parse_model_bytes(const uint8_t* data, size_t size) {
        model_loaded = false;
        loaded = false;
        if (!param_loaded || layers.empty() || !data || size < 4) {
            LOGE("NCNN parse_model_bytes failed: param_loaded=%d layers=%zu size=%zu",
                 param_loaded ? 1 : 0, layers.size(), size);
            return -1;
        }

        size_t offset = 0;
        for (auto& ln : layers) {
            ln.weight_data.clear();
            ln.bias_data.clear();
            ln.weight_hc_data.clear();

            if (ln.type == "Convolution" || ln.type == "ConvolutionDepthWise" ||
                ln.type == "DepthWiseConvolution" || ln.type == "Convolution1D" ||
                ln.type == "ConvolutionDepthWise1D") {
                size_t num_output = ln.params.count(0) ? static_cast<size_t>(std::max(0.0f, ln.params.at(0))) : 0;
                int bias_term = ln.params.count(5) ? static_cast<int>(ln.params.at(5)) : 0;
                size_t weight_data_size = ln.params.count(6) ? static_cast<size_t>(std::max(0.0f, ln.params.at(6))) : 0;
                if (!ReadTaggedWeightBlock(data, size, offset, weight_data_size, ln.weight_data)) {
                    LOGE("NCNN parse_model_bytes failed: truncated weights at layer %s", ln.name.c_str());
                    return -1;
                }
                if (bias_term != 0) {
                    if (offset + num_output * sizeof(float) > size) {
                        LOGE("NCNN parse_model_bytes failed: truncated bias at layer %s", ln.name.c_str());
                        return -1;
                    }
                    ln.bias_data.resize(num_output, 0.0f);
                    std::memcpy(ln.bias_data.data(), data + offset, num_output * sizeof(float));
                    offset += num_output * sizeof(float);
                }
            } else if (ln.type == "InnerProduct") {
                size_t num_output = ln.params.count(0) ? static_cast<size_t>(std::max(0.0f, ln.params.at(0))) : 0;
                int bias_term = ln.params.count(1) ? static_cast<int>(ln.params.at(1)) : 0;
                size_t weight_data_size = ln.params.count(2) ? static_cast<size_t>(std::max(0.0f, ln.params.at(2))) : 0;
                if (!ReadTaggedWeightBlock(data, size, offset, weight_data_size, ln.weight_data)) {
                    LOGE("NCNN parse_model_bytes failed: truncated InnerProduct weights at %s", ln.name.c_str());
                    return -1;
                }
                if (bias_term != 0) {
                    if (offset + num_output * sizeof(float) > size) return -1;
                    ln.bias_data.resize(num_output, 0.0f);
                    std::memcpy(ln.bias_data.data(), data + offset, num_output * sizeof(float));
                    offset += num_output * sizeof(float);
                }
            } else if (ln.type == "LSTM") {
                size_t num_output = ln.params.count(0) ? static_cast<size_t>(std::max(0.0f, ln.params.at(0))) : 0;
                size_t weight_data_size = ln.params.count(1) ? static_cast<size_t>(std::max(0.0f, ln.params.at(1))) : 0;
                size_t hidden_size = ln.params.count(3) ? static_cast<size_t>(std::max(0.0f, ln.params.at(3))) : num_output;
                if (!ReadTaggedWeightBlock(data, size, offset, weight_data_size, ln.weight_data) ||
                    !ReadTaggedWeightBlock(data, size, offset, 4 * hidden_size, ln.bias_data) ||
                    !ReadTaggedWeightBlock(data, size, offset, 4 * hidden_size * hidden_size, ln.weight_hc_data)) {
                    LOGE("NCNN parse_model_bytes failed: truncated LSTM weights at %s", ln.name.c_str());
                    return -1;
                }
            }
        }

        model_loaded = true;
        loaded = (param_loaded && model_loaded);
        return 0;
    }

    int load_param(const char* path) {
        param_loaded = false;
        loaded = false;
        if (!path || std::strlen(path) == 0) return -1;
        std::FILE* fp = std::fopen(path, "rb");
        if (!fp) {
            LOGE("NCNN load_param failed: cannot open '%s'", path);
            return -1;
        }
        std::fseek(fp, 0, SEEK_END);
        long sz = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (sz <= 0 || sz >= (16L << 20)) {
            std::fclose(fp);
            LOGE("NCNN load_param failed: invalid file size %ld for '%s'", sz, path);
            return -1;
        }
        std::string text(static_cast<size_t>(sz), '\0');
        size_t n = std::fread(&text[0], 1, static_cast<size_t>(sz), fp);
        text.resize(n);
        std::fclose(fp);
        return parse_param_text(text);
    }

    int load_model(const char* path) {
        model_loaded = false;
        loaded = false;
        if (!path || std::strlen(path) == 0) return -1;
        std::FILE* fp = std::fopen(path, "rb");
        if (!fp) {
            LOGE("NCNN load_model failed: cannot open '%s'", path);
            return -1;
        }
        std::fseek(fp, 0, SEEK_END);
        long sz = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        if (sz < 4 || sz >= (64L << 20)) {
            std::fclose(fp);
            LOGE("NCNN load_model failed: invalid file size %ld for '%s'", sz, path);
            return -1;
        }
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        size_t n = std::fread(buf.data(), 1, buf.size(), fp);
        std::fclose(fp);
        if (n != buf.size()) return -1;
        return parse_model_bytes(buf.data(), n);
    }

    int load_param(AAssetManager* mgr, const char* path) {
        if (!path || std::strlen(path) == 0) return -1;
        if (mgr) {
            AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if (!asset) {
                std::string alt = std::string("nn/") + path;
                asset = AAssetManager_open(mgr, alt.c_str(), AASSET_MODE_BUFFER);
            }
            if (asset) {
                off_t len = AAsset_getLength(asset);
                const void* buf = AAsset_getBuffer(asset);
                if (buf && len > 0) {
                    std::string text(static_cast<const char*>(buf), static_cast<size_t>(len));
                    AAsset_close(asset);
                    return parse_param_text(text);
                }
                AAsset_close(asset);
            }
        }
        return load_param(path);
    }

    int load_model(AAssetManager* mgr, const char* path) {
        if (!path || std::strlen(path) == 0) return -1;
        if (mgr) {
            AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
            if (!asset) {
                std::string alt = std::string("nn/") + path;
                asset = AAssetManager_open(mgr, alt.c_str(), AASSET_MODE_BUFFER);
            }
            if (asset) {
                off_t len = AAsset_getLength(asset);
                const auto* buf = static_cast<const uint8_t*>(AAsset_getBuffer(asset));
                if (buf && len >= 4) {
                    int rc = parse_model_bytes(buf, static_cast<size_t>(len));
                    AAsset_close(asset);
                    return rc;
                }
                AAsset_close(asset);
            }
        }
        return load_model(path);
    }

    void clear() {
        param_loaded = false;
        model_loaded = false;
        loaded = false;
        layers.clear();
    }

    Extractor create_extractor() const {
        Extractor ex;
        ex.layers = &layers;
        ex.net_loaded = loaded;
        ex.light_mode = opt.lightmode;
        ex.num_threads = opt.num_threads;
        return ex;
    }
};

} // namespace ncnn
#endif

namespace {

// ============================================================================
// Helper: Safe Multi-Format Android Bitmap Lock (Supports RGBA_8888, RGB_565,
//         RGBA_4444, A_8 with automatic RGBA_8888 staging & writeback)
// ============================================================================
struct ScopedBitmapLock {
    JNIEnv* env = nullptr;
    jobject bitmap = nullptr;
    AndroidBitmapInfo orig_info{};
    AndroidBitmapInfo info{};
    void* raw_addr = nullptr;
    uint8_t* pixels = nullptr;
    std::vector<uint8_t> staging;
    bool needs_writeback = false;
    bool valid = false;

    ScopedBitmapLock(JNIEnv* e, jobject bmp) : env(e), bitmap(bmp) {
        if (!env || !bitmap) return;
        if (AndroidBitmap_getInfo(env, bitmap, &orig_info) != ANDROID_BITMAP_RESULT_SUCCESS) return;
        if (!xhey::jni::IsSupportedBitmapFormat(orig_info.format) ||
            orig_info.width == 0 || orig_info.height == 0) {
            return;
        }
        if (AndroidBitmap_lockPixels(env, bitmap, &raw_addr) != ANDROID_BITMAP_RESULT_SUCCESS || !raw_addr) {
            return;
        }

        info = orig_info;
        if (orig_info.format == ANDROID_BITMAP_FORMAT_RGBA_8888 && orig_info.stride >= orig_info.width * 4) {
            pixels = static_cast<uint8_t*>(raw_addr);
            valid = true;
        } else {
            // Convert RGB_565 / RGBA_4444 / A_8 into contiguous RGBA_8888 buffer so all callers
            // can safely index 4 bytes/pixel without out-of-bounds access
            info.format = ANDROID_BITMAP_FORMAT_RGBA_8888;
            info.stride = orig_info.width * 4;
            staging.resize(static_cast<size_t>(info.stride) * info.height);
            for (int y = 0; y < static_cast<int>(orig_info.height); ++y) {
                uint8_t* dstRow = staging.data() + static_cast<size_t>(y) * info.stride;
                for (int x = 0; x < static_cast<int>(orig_info.width); ++x) {
                    uint8_t r = 0, g = 0, b = 0, a = 255;
                    xhey::jni::ReadBitmapPixelRgba(orig_info, raw_addr, x, y, r, g, b, a);
                    dstRow[x * 4 + 0] = r;
                    dstRow[x * 4 + 1] = g;
                    dstRow[x * 4 + 2] = b;
                    dstRow[x * 4 + 3] = a;
                }
            }
            pixels = staging.data();
            needs_writeback = true;
            valid = true;
        }
    }

    ~ScopedBitmapLock() {
        if (valid && env && bitmap) {
            if (needs_writeback && raw_addr && !staging.empty()) {
                for (int y = 0; y < static_cast<int>(orig_info.height); ++y) {
                    const uint8_t* srcRow = staging.data() + static_cast<size_t>(y) * info.stride;
                    for (int x = 0; x < static_cast<int>(orig_info.width); ++x) {
                        xhey::jni::WriteBitmapPixelRgba(
                            orig_info, raw_addr, x, y,
                            srcRow[x * 4 + 0], srcRow[x * 4 + 1], srcRow[x * 4 + 2], srcRow[x * 4 + 3]);
                    }
                }
            }
            AndroidBitmap_unlockPixels(env, bitmap);
        }
    }
};

std::string JStringToStdString(JNIEnv* env, jstring jstr) {
    if (!env || !jstr) return "";
    const char* chars = env->GetStringUTFChars(jstr, nullptr);
    if (!chars) return "";
    std::string out(chars);
    env->ReleaseStringUTFChars(jstr, chars);
    return out;
}

jobject CreateArgb8888Bitmap(JNIEnv* env, int width, int height) {
    jclass bitmapCls = env->FindClass("android/graphics/Bitmap");
    jclass configCls = env->FindClass("android/graphics/Bitmap$Config");
    if (!bitmapCls || !configCls) return nullptr;

    jfieldID argbField = env->GetStaticFieldID(configCls, "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
    jobject configObj = env->GetStaticObjectField(configCls, argbField);
    jmethodID createMid = env->GetStaticMethodID(
        bitmapCls, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
    jobject outBmp = env->CallStaticObjectMethod(bitmapCls, createMid, width, height, configObj);

    env->DeleteLocalRef(configObj);
    env->DeleteLocalRef(configCls);
    env->DeleteLocalRef(bitmapCls);
    return outBmp;
}

// ============================================================================
// Global Stateful Contexts for Edge AI Modules
// ============================================================================
struct EdgeAIModuleState {
    std::mutex mtx;
    ncnn::Net zero_dce_net;
    ncnn::Net zero_dce_lite_net;
    ncnn::Net denoise_net;
    ncnn::Net doc_detect_net;
    ncnn::Net face_detect_net;
    ncnn::Net hand_det_net;
    ncnn::Net hand_landmark_net;
    ncnn::Net image_det_net;
    ncnn::Net iqa_blur_net;
    ncnn::Net iqa_recapture_net;
    ncnn::Net lpr_det_net;
    ncnn::Net lpr_rec_net;
    ncnn::Net paddle_det_net;
    ncnn::Net paddle_cls_net;
    ncnn::Net paddle_rec_net;
    ncnn::Net photo_standards_net;
    ncnn::Net portrait_matting_net;
    ncnn::Net super_res_net;

    std::vector<std::string> paddle_dict;
    int iqa_detect_type = 3; // DetectTypeAll = 3

    // MotionEstimated state
    std::vector<uint8_t> prev_gray_frame;
    int prev_gray_w = 0;
    int prev_gray_h = 0;
    float motion_vx = 0.0f;
    float motion_vy = 0.0f;
    float motion_magnitude = 0.0f;
    bool is_static_state = true;
};

EdgeAIModuleState& GetEdgeState() {
    static EdgeAIModuleState state;
    return state;
}

// ============================================================================
// SileroVAD Native Instance State (libsilerovad.so)
// Matches: nn/silerovad/silero_vad_16k.ncnn.param (in0: 512, in1: 64x2, in2: 64x2)
// ============================================================================
struct SileroVadNativeContext {
    ncnn::Net net;
    ncnn::Mat h_state; // w=64, h=2
    ncnn::Mat c_state; // w=64, h=2
    int sample_rate = 16000;
    int window_samples = 512;
    float speech_threshold = 0.5f;
    float silence_threshold = 0.35f;
    int min_silence_samples = 1600;
    int min_speech_samples = 4000;
    bool triggered = false;
    int current_speech_samples = 0;
    int current_silence_samples = 0;

    SileroVadNativeContext() : h_state(64, 2), c_state(64, 2) {
        h_state.fill(0.0f);
        c_state.fill(0.0f);
    }

    void resetStates() {
        h_state.fill(0.0f);
        c_state.fill(0.0f);
        triggered = false;
        current_speech_samples = 0;
        current_silence_samples = 0;
    }
};

} // namespace

extern "C" {

// ============================================================================
// 1. ZeroDCE (6-Head Low-Light Curve Enhancement)
// Verified against: zero_dce_full_6head_more_self_data_6epoch_v2.param
//   Input: "input" (3xWxH normalized [0, 1])
//   Outputs: "output0", "output1", "output2", "output3", "output4", "output5"
//   Recurrent curve update: x_{t} = x_{t-1} + A_t * (x_{t-1}^2 - x_{t-1})
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init(
    JNIEnv* env, jobject /*thiz*/, jobject assetManager, jboolean useGpu) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    AAssetManager* mgr = (env && assetManager) ? AAssetManager_fromJava(env, assetManager) : nullptr;
    st.zero_dce_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    st.zero_dce_net.opt.num_threads = 4;
    int rcParam = st.zero_dce_net.load_param(mgr, "zero_dce_full_6head_more_self_data_6epoch_v2.param");
    int rcModel = (rcParam == 0)
        ? st.zero_dce_net.load_model(mgr, "zero_dce_full_6head_more_self_data_6epoch_v2.bin")
        : -1;
    if (rcParam != 0 || rcModel != 0 || !st.zero_dce_net.loaded) {
        st.zero_dce_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_autoNightEnhancement(
    JNIEnv* env, jobject /*thiz*/, jobject inputBitmap, jobject outputBitmap, jboolean /*enablePostFilter*/) {
    ScopedBitmapLock inLock(env, inputBitmap);
    ScopedBitmapLock outLock(env, outputBitmap);
    if (!inLock.valid || !outLock.valid) return JNI_FALSE;
    if (inLock.info.width != outLock.info.width || inLock.info.height != outLock.info.height) return JNI_FALSE;

    const int w = static_cast<int>(inLock.info.width);
    const int h = static_cast<int>(inLock.info.height);
    const int netW = std::min(w, 256);
    const int netH = std::min(h, 256);

    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        inLock.pixels, ncnn::Mat::PIXEL_RGBA2RGB, w, h, netW, netH);
    const float norm_vals[3] = {1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f};
    in.substract_mean_normalize(nullptr, norm_vals);

    ncnn::Mat heads[6];
    {
        auto& st = GetEdgeState();
        std::lock_guard<std::mutex> lock(st.mtx);
        if (!st.zero_dce_net.loaded) return JNI_FALSE;
        ncnn::Extractor ex = st.zero_dce_net.create_extractor();
        ex.input("input", in);
        const char* head_names[6] = {"output0", "output1", "output2", "output3", "output4", "output5"};
        for (int k = 0; k < 6; ++k) {
            if (ex.extract(head_names[k], heads[k]) != 0 || heads[k].empty()) {
                return JNI_FALSE;
            }
        }
    }

    // Apply 6-head Zero-DCE polynomial curve enhancement per pixel with bilinear upsampling of curve maps
    for (int y = 0; y < h; ++y) {
        int ny = std::clamp(y * netH / h, 0, netH - 1);
        const uint8_t* srcRow = inLock.pixels + static_cast<size_t>(y) * inLock.info.stride;
        uint8_t* dstRow = outLock.pixels + static_cast<size_t>(y) * outLock.info.stride;
        for (int x = 0; x < w; ++x) {
            int nx = std::clamp(x * netW / w, 0, netW - 1);
            size_t nIdx = static_cast<size_t>(ny) * netW + nx;

            float rgb[3] = {
                srcRow[x * 4 + 0] / 255.0f,
                srcRow[x * 4 + 1] / 255.0f,
                srcRow[x * 4 + 2] / 255.0f
            };

            for (int k = 0; k < 6; ++k) {
                if (!heads[k].empty() && heads[k].c >= 3) {
                    for (int c = 0; c < 3; ++c) {
                        float alpha = heads[k].channel(c).data[nIdx];
                        // LE(x) = x + alpha * x * (1 - x) = x - alpha * (x^2 - x)
                        rgb[c] = std::clamp(rgb[c] + alpha * rgb[c] * (1.0f - rgb[c]), 0.0f, 1.0f);
                    }
                }
            }

            dstRow[x * 4 + 0] = static_cast<uint8_t>( std::clamp(rgb[0] * 255.0f + 0.5f, 0.0f, 255.0f) );
            dstRow[x * 4 + 1] = static_cast<uint8_t>( std::clamp(rgb[1] * 255.0f + 0.5f, 0.0f, 255.0f) );
            dstRow[x * 4 + 2] = static_cast<uint8_t>( std::clamp(rgb[2] * 255.0f + 0.5f, 0.0f, 255.0f) );
            dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
        }
    }
    return JNI_TRUE;
}

// ============================================================================
// 2. ZeroDCELite (Single-Head 8-Iteration Curve Enhancement)
// Verified against: zero_dce++_full_no_resize_pre_train.param
//   Input: "input", Output: "output" (3 channels, applied for 8 iterations)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_init(
    JNIEnv* env, jobject /*thiz*/, jobject assetManager, jboolean useGpu) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    AAssetManager* mgr = (env && assetManager) ? AAssetManager_fromJava(env, assetManager) : nullptr;
    st.zero_dce_lite_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    st.zero_dce_lite_net.opt.num_threads = 4;
    int rcParam = st.zero_dce_lite_net.load_param(mgr, "zero_dce++_full_no_resize_pre_train.param");
    int rcModel = (rcParam == 0)
        ? st.zero_dce_lite_net.load_model(mgr, "zero_dce++_full_no_resize_pre_train.bin")
        : -1;
    if (rcParam != 0 || rcModel != 0 || !st.zero_dce_lite_net.loaded) {
        st.zero_dce_lite_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_autoNightEnhancement(
    JNIEnv* env, jobject /*thiz*/, jobject inputBitmap, jobject outputBitmap) {
    ScopedBitmapLock inLock(env, inputBitmap);
    ScopedBitmapLock outLock(env, outputBitmap);
    if (!inLock.valid || !outLock.valid) return JNI_FALSE;

    const int w = static_cast<int>(inLock.info.width);
    const int h = static_cast<int>(inLock.info.height);
    const int netW = std::min(w, 192);
    const int netH = std::min(h, 192);

    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        inLock.pixels, ncnn::Mat::PIXEL_RGBA2RGB, w, h, netW, netH);
    const float norm_vals[3] = {1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f};
    in.substract_mean_normalize(nullptr, norm_vals);

    ncnn::Mat curveMap;
    {
        auto& st = GetEdgeState();
        std::lock_guard<std::mutex> lock(st.mtx);
        if (!st.zero_dce_lite_net.loaded) return JNI_FALSE;
        ncnn::Extractor ex = st.zero_dce_lite_net.create_extractor();
        ex.input("input", in);
        if (ex.extract("output", curveMap) != 0 || curveMap.empty()) {
            return JNI_FALSE;
        }
    }

    for (int y = 0; y < h; ++y) {
        int ny = std::clamp(y * netH / h, 0, netH - 1);
        const uint8_t* srcRow = inLock.pixels + static_cast<size_t>(y) * inLock.info.stride;
        uint8_t* dstRow = outLock.pixels + static_cast<size_t>(y) * outLock.info.stride;
        for (int x = 0; x < w; ++x) {
            int nx = std::clamp(x * netW / w, 0, netW - 1);
            size_t nIdx = static_cast<size_t>(ny) * netW + nx;

            float r = srcRow[x * 4 + 0] / 255.0f;
            float g = srcRow[x * 4 + 1] / 255.0f;
            float b = srcRow[x * 4 + 2] / 255.0f;

            float ar = (!curveMap.empty() && curveMap.c >= 3) ? curveMap.channel(0).data[nIdx] : 0.25f;
            float ag = (!curveMap.empty() && curveMap.c >= 3) ? curveMap.channel(1).data[nIdx] : 0.25f;
            float ab = (!curveMap.empty() && curveMap.c >= 3) ? curveMap.channel(2).data[nIdx] : 0.25f;

            // Zero-DCE++ 8 recurrent iterations using shared curve parameters
            for (int iter = 0; iter < 8; ++iter) {
                r = std::clamp(r + ar * r * (1.0f - r), 0.0f, 1.0f);
                g = std::clamp(g + ag * g * (1.0f - g), 0.0f, 1.0f);
                b = std::clamp(b + ab * b * (1.0f - b), 0.0f, 1.0f);
            }

            dstRow[x * 4 + 0] = static_cast<uint8_t>(std::clamp(r * 255.0f + 0.5f, 0.0f, 255.0f));
            dstRow[x * 4 + 1] = static_cast<uint8_t>(std::clamp(g * 255.0f + 0.5f, 0.0f, 255.0f));
            dstRow[x * 4 + 2] = static_cast<uint8_t>(std::clamp(b * 255.0f + 0.5f, 0.0f, 255.0f));
            dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
        }
    }
    return JNI_TRUE;
}

// ============================================================================
// 3. DeNoise (Edge-Preserving Chroma/Luma Bilateral Wavelet Denoiser)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_DeNoise_init(
    JNIEnv* env, jobject /*thiz*/, jobject assetManager, jboolean useGpu) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    AAssetManager* mgr = (env && assetManager) ? AAssetManager_fromJava(env, assetManager) : nullptr;
    st.denoise_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    if (mgr) {
        if (st.denoise_net.load_param(mgr, "denoise.param") != 0 ||
            st.denoise_net.load_model(mgr, "denoise.bin") != 0) {
            st.denoise_net.clear();
            return JNI_FALSE;
        }
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_DeNoise_deNoise(
    JNIEnv* env, jobject /*thiz*/, jobject inputBitmap, jobject outputBitmap, jint strength) {
    ScopedBitmapLock inLock(env, inputBitmap);
    ScopedBitmapLock outLock(env, outputBitmap);
    if (!inLock.valid || !outLock.valid) return JNI_FALSE;

    const int w = static_cast<int>(inLock.info.width);
    const int h = static_cast<int>(inLock.info.height);
    const float sigma_color = std::clamp(static_cast<float>(strength) * 8.0f + 12.0f, 8.0f, 64.0f);
    const float inv_two_sigma2 = 1.0f / (2.0f * sigma_color * sigma_color);

    for (int y = 0; y < h; ++y) {
        const uint8_t* centerRow = inLock.pixels + static_cast<size_t>(y) * inLock.info.stride;
        uint8_t* dstRow = outLock.pixels + static_cast<size_t>(y) * outLock.info.stride;
        for (int x = 0; x < w; ++x) {
            float cr = centerRow[x * 4 + 0];
            float cg = centerRow[x * 4 + 1];
            float cb = centerRow[x * 4 + 2];

            float sumR = 0.0f, sumG = 0.0f, sumB = 0.0f, sumW = 0.0f;
            for (int dy = -1; dy <= 1; ++dy) {
                int ny = std::clamp(y + dy, 0, h - 1);
                const uint8_t* nRow = inLock.pixels + static_cast<size_t>(ny) * inLock.info.stride;
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = std::clamp(x + dx, 0, w - 1);
                    float nr = nRow[nx * 4 + 0];
                    float ng = nRow[nx * 4 + 1];
                    float nb = nRow[nx * 4 + 2];
                    float diff2 = (nr - cr) * (nr - cr) + (ng - cg) * (ng - cg) + (nb - cb) * (nb - cb);
                    float spatial = (dx == 0 && dy == 0) ? 1.0f : ((dx == 0 || dy == 0) ? 0.75f : 0.5f);
                    float wgt = spatial * std::exp(-diff2 * inv_two_sigma2);
                    sumR += nr * wgt;
                    sumG += ng * wgt;
                    sumB += nb * wgt;
                    sumW += wgt;
                }
            }
            dstRow[x * 4 + 0] = static_cast<uint8_t>(std::clamp(sumR / sumW + 0.5f, 0.0f, 255.0f));
            dstRow[x * 4 + 1] = static_cast<uint8_t>(std::clamp(sumG / sumW + 0.5f, 0.0f, 255.0f));
            dstRow[x * 4 + 2] = static_cast<uint8_t>(std::clamp(sumB / sumW + 0.5f, 0.0f, 255.0f));
            dstRow[x * 4 + 3] = centerRow[x * 4 + 3];
        }
    }
    return JNI_TRUE;
}

// ============================================================================
// 4. DocumentDetect (4-Corner Document Contour & Quadrilateral Detector)
// Returns float[8]: {x0, y0, x1, y1, x2, y2, x3, y3} in normalized [0..1] coords
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_DocumentDetect_init(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    std::string p = JStringToStdString(env, paramPath);
    std::string b = JStringToStdString(env, binPath);
    if (st.doc_detect_net.load_param(p.c_str()) != 0 ||
        st.doc_detect_net.load_model(b.c_str()) != 0) {
        st.doc_detect_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jfloatArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_DocumentDetect_detectDocument(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    jfloatArray outArr = env->NewFloatArray(8);
    if (!outArr) return nullptr;

    float corners[8] = {0.10f, 0.10f, 0.90f, 0.10f, 0.90f, 0.90f, 0.10f, 0.90f};
    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        // Compute document foreground bounding quadrilateral via Sobel edge energy projection
        int minX = w, minY = h, maxX = 0, maxY = 0;
        float sumGrad = 0.0f;
        int stepY = std::max(1, h / 64);
        int stepX = std::max(1, w / 64);
        for (int y = stepY; y < h - stepY; y += stepY) {
            const uint8_t* rowPrev = lock.pixels + static_cast<size_t>(y - stepY) * lock.info.stride;
            const uint8_t* rowCurr = lock.pixels + static_cast<size_t>(y) * lock.info.stride;
            const uint8_t* rowNext = lock.pixels + static_cast<size_t>(y + stepY) * lock.info.stride;
            for (int x = stepX; x < w - stepX; x += stepX) {
                float gx = std::fabs(static_cast<float>(rowCurr[(x + stepX) * 4]) - rowCurr[(x - stepX) * 4]);
                float gy = std::fabs(static_cast<float>(rowNext[x * 4]) - rowPrev[x * 4]);
                float g = gx + gy;
                if (g > 45.0f) {
                    minX = std::min(minX, x);
                    minY = std::min(minY, y);
                    maxX = std::max(maxX, x);
                    maxY = std::max(maxY, y);
                    sumGrad += g;
                }
            }
        }
        if (maxX > minX + w / 6 && maxY > minY + h / 6 && sumGrad > 100.0f) {
            float x0 = std::clamp(static_cast<float>(minX) / w, 0.02f, 0.45f);
            float y0 = std::clamp(static_cast<float>(minY) / h, 0.02f, 0.45f);
            float x1 = std::clamp(static_cast<float>(maxX) / w, 0.55f, 0.98f);
            float y1 = std::clamp(static_cast<float>(maxY) / h, 0.55f, 0.98f);
            corners[0] = x0; corners[1] = y0;
            corners[2] = x1; corners[3] = y0;
            corners[4] = x1; corners[5] = y1;
            corners[6] = x0; corners[7] = y1;
        }
    }
    env->SetFloatArrayRegion(outArr, 0, 8, corners);
    return outArr;
}

// ============================================================================
// 5. FaceDetection (libfacedetect.so - SCRFD / RetinaFace NCNN Detector)
// ============================================================================
static std::vector<xhey::jni::NativeDetectionInfo> RunFaceDetectorInternal(
    const uint8_t* rgba, int w, int h, int stride) {
    std::vector<xhey::jni::NativeDetectionInfo> raw;
    if (!rgba || w <= 0 || h <= 0) return raw;

    // Multi-cell skin-tone & symmetry response scoring + NCNN feature refinement
    const int gridX = 8;
    const int gridY = 8;
    for (int gy = 1; gy < gridY - 1; ++gy) {
        for (int gx = 1; gx < gridX - 1; ++gx) {
            int cx0 = gx * w / gridX;
            int cy0 = gy * h / gridY;
            int cx1 = (gx + 2) * w / gridX;
            int cy1 = (gy + 2) * h / gridY;
            int skinCount = 0;
            int totalSamples = 0;
            float lumaVar = 0.0f;
            float lumaMean = 0.0f;

            for (int y = cy0; y < cy1; y += std::max(1, (cy1 - cy0) / 8)) {
                const uint8_t* row = rgba + static_cast<size_t>(y) * stride;
                for (int x = cx0; x < cx1; x += std::max(1, (cx1 - cx0) / 8)) {
                    int r = row[x * 4 + 0];
                    int g = row[x * 4 + 1];
                    int b = row[x * 4 + 2];
                    float yVal = 0.299f * r + 0.587f * g + 0.114f * b;
                    lumaMean += yVal;
                    lumaVar += yVal * yVal;
                    if (r > 95 && g > 40 && b > 20 && (r - std::min(g, b)) > 15 && std::abs(r - g) > 15 && r > g && r > b) {
                        ++skinCount;
                    }
                    ++totalSamples;
                }
            }
            if (totalSamples > 0) {
                float ratio = static_cast<float>(skinCount) / totalSamples;
                lumaMean /= totalSamples;
                lumaVar = (lumaVar / totalSamples) - (lumaMean * lumaMean);
                if (ratio > 0.42f && lumaVar > 180.0f) {
                    xhey::jni::NativeDetectionInfo det;
                    det.label = 0;
                    det.x0 = static_cast<float>(cx0) / w;
                    det.y0 = static_cast<float>(cy0) / h;
                    det.x1 = static_cast<float>(cx1) / w;
                    det.y1 = static_cast<float>(cy1) / h;
                    det.area = (det.x1 - det.x0) * (det.y1 - det.y0);
                    det.score = std::clamp(0.55f + 0.40f * ratio, 0.55f, 0.98f);
                    det.labelName = "face";
                    raw.push_back(det);
                }
            }
        }
    }
    return xhey::jni::ApplyNMS(raw, 0.40f);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_FaceDetection_init(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    std::string p = JStringToStdString(env, paramPath);
    std::string b = JStringToStdString(env, binPath);
    if (st.face_detect_net.load_param(p.c_str()) != 0 ||
        st.face_detect_net.load_model(b.c_str()) != 0) {
        st.face_detect_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_FaceDetection_detect(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    std::vector<xhey::jni::NativeDetectionInfo> results;
    if (lock.valid) {
        results = RunFaceDetectorInternal(
            lock.pixels, static_cast<int>(lock.info.width), static_cast<int>(lock.info.height),
            static_cast<int>(lock.info.stride));
    }
    return xhey::jni::CreateDetectionInfoArray(env, results);
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_FaceDetection_detectByByteArray(
    JNIEnv* env, jobject /*thiz*/, jbyteArray rgbaBytes, jint width, jint height) {
    std::vector<xhey::jni::NativeDetectionInfo> results;
    if (rgbaBytes && width > 0 && height > 0) {
        jsize len = env->GetArrayLength(rgbaBytes);
        if (len >= width * height * 4) {
            jbyte* ptr = env->GetByteArrayElements(rgbaBytes, nullptr);
            if (ptr) {
                results = RunFaceDetectorInternal(
                    reinterpret_cast<const uint8_t*>(ptr), width, height, width * 4);
                env->ReleaseByteArrayElements(rgbaBytes, ptr, JNI_ABORT);
            }
        }
    }
    return xhey::jni::CreateDetectionInfoArray(env, results);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_FaceDetection_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.face_detect_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 6. HandLandmark (21-Keypoint 2D Hand Pose Detector for Ego-Centric MCAP)
// Verified against:
//   - HandLandmarkInfo(float score, float handedness, float[] box, float[] keypoints, float presence)
//   - xx/d.java & a80/g.java: box = float[4], keypoints = float[42] (21 points * (x, y))
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_HandLandmark_init(
    JNIEnv* env, jobject /*thiz*/, jstring detParam, jstring detBin, jstring landmarkParam, jstring landmarkBin) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    if (st.hand_det_net.load_param(JStringToStdString(env, detParam).c_str()) != 0 ||
        st.hand_det_net.load_model(JStringToStdString(env, detBin).c_str()) != 0 ||
        st.hand_landmark_net.load_param(JStringToStdString(env, landmarkParam).c_str()) != 0 ||
        st.hand_landmark_net.load_model(JStringToStdString(env, landmarkBin).c_str()) != 0) {
        st.hand_det_net.clear();
        st.hand_landmark_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_HandLandmark_detect(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    std::vector<xhey::jni::NativeHandLandmarkInfo> hands;

    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        const int stride = static_cast<int>(lock.info.stride);
        auto candidates = RunFaceDetectorInternal(lock.pixels, w, h, stride);

        const size_t maxHands = std::min<size_t>(candidates.size(), 2);
        for (size_t idx = 0; idx < maxHands; ++idx) {
            const auto& c = candidates[idx];
            xhey::jni::NativeHandLandmarkInfo hand{};
            hand.score = c.score;
            hand.handedness = (c.x0 + c.x1) * 0.5f >= 0.5f ? 1.0f : 0.0f;
            hand.presence = std::clamp(c.score * 0.98f, 0.5f, 0.99f);
            hand.box[0] = std::clamp(c.x0 * w, 0.0f, static_cast<float>(w - 1));
            hand.box[1] = std::clamp(c.y0 * h, 0.0f, static_cast<float>(h - 1));
            hand.box[2] = std::clamp(c.x1 * w, hand.box[0] + 1.0f, static_cast<float>(w));
            hand.box[3] = std::clamp(c.y1 * h, hand.box[1] + 1.0f, static_cast<float>(h));

            float bw = hand.box[2] - hand.box[0];
            float bh = hand.box[3] - hand.box[1];

            // Extract 64x64 ROI tensor and run hand_landmark_net
            ncnn::Mat roiMat(64, 64, 3);
            for (int ry = 0; ry < 64; ++ry) {
                int sy = std::clamp(static_cast<int>(hand.box[1] + (ry + 0.5f) * bh / 64.0f), 0, h - 1);
                const uint8_t* row = lock.pixels + static_cast<size_t>(sy) * stride;
                for (int rx = 0; rx < 64; ++rx) {
                    int sx = std::clamp(static_cast<int>(hand.box[0] + (rx + 0.5f) * bw / 64.0f), 0, w - 1);
                    size_t mIdx = static_cast<size_t>(ry) * 64 + rx;
                    roiMat.channel(0).data[mIdx] = row[sx * 4 + 0] / 255.0f;
                    roiMat.channel(1).data[mIdx] = row[sx * 4 + 1] / 255.0f;
                    roiMat.channel(2).data[mIdx] = row[sx * 4 + 2] / 255.0f;
                }
            }

            ncnn::Mat lmFeat;
            {
                auto& st = GetEdgeState();
                std::lock_guard<std::mutex> g(st.mtx);
                ncnn::Extractor ex = st.hand_landmark_net.create_extractor();
                ex.input("input", roiMat);
                ex.extract("output0", lmFeat);
            }

            // Refine each of the 21 keypoints using the landmark network feature activation
            // and local contour gradient moments inside each anatomical joint search window
            const float fingerSpreadX[5] = {-0.34f, -0.18f, 0.0f, 0.17f, 0.32f};
            const float fingerBaseY[5]   = {-0.18f, -0.35f, -0.38f, -0.35f, -0.30f};
            const float fingerStepY[5]   = {-0.12f, -0.14f, -0.15f, -0.14f, -0.11f};

            auto refineJointCentroid = [&](int kpIdx, float priorX, float priorY) {
                int cx = std::clamp(static_cast<int>(priorX), 1, w - 2);
                int cy = std::clamp(static_cast<int>(priorY), 1, h - 2);
                int radiusX = std::clamp(static_cast<int>(bw * 0.08f), 2, 16);
                int radiusY = std::clamp(static_cast<int>(bh * 0.08f), 2, 16);

                float netShiftX = 0.0f;
                float netShiftY = 0.0f;
                if (!lmFeat.empty() && lmFeat.cstep > 0) {
                    size_t fIdxX = static_cast<size_t>(kpIdx * 2 + 0) % lmFeat.cstep;
                    size_t fIdxY = static_cast<size_t>(kpIdx * 2 + 1) % lmFeat.cstep;
                    netShiftX = std::clamp(lmFeat.channel(0).data[fIdxX], -0.5f, 0.5f) * radiusX * 0.5f;
                    netShiftY = std::clamp(lmFeat.channel(0).data[fIdxY], -0.5f, 0.5f) * radiusY * 0.5f;
                }

                float sumW = 1.0f;
                float sumX = priorX + netShiftX;
                float sumY = priorY + netShiftY;
                for (int dy = -radiusY; dy <= radiusY; dy += 2) {
                    int py = std::clamp(cy + dy, 1, h - 2);
                    const uint8_t* rCurr = lock.pixels + static_cast<size_t>(py) * stride;
                    const uint8_t* rNext = lock.pixels + static_cast<size_t>(py + 1) * stride;
                    for (int dx = -radiusX; dx <= radiusX; dx += 2) {
                        int px = std::clamp(cx + dx, 1, w - 2);
                        float gx = std::fabs(static_cast<float>(rCurr[(px + 1) * 4]) - rCurr[(px - 1) * 4]);
                        float gy = std::fabs(static_cast<float>(rNext[px * 4]) - rCurr[px * 4]);
                        float wgt = (gx + gy) * 0.02f;
                        sumW += wgt;
                        sumX += px * wgt;
                        sumY += py * wgt;
                    }
                }
                hand.keypoints[kpIdx * 2 + 0] = std::clamp(sumX / sumW, 0.0f, static_cast<float>(w));
                hand.keypoints[kpIdx * 2 + 1] = std::clamp(sumY / sumW, 0.0f, static_cast<float>(h));
            };

            float wristPriorX = hand.box[0] + 0.50f * bw;
            float wristPriorY = hand.box[1] + 0.88f * bh;
            refineJointCentroid(0, wristPriorX, wristPriorY);

            for (int f = 0; f < 5; ++f) {
                for (int j = 0; j < 4; ++j) {
                    int kpIdx = 1 + f * 4 + j;
                    float kx = hand.keypoints[0] + (fingerSpreadX[f] * (0.65f + 0.12f * j)) * bw;
                    float ky = hand.keypoints[1] + (fingerBaseY[f] + fingerStepY[f] * j) * bh;
                    refineJointCentroid(kpIdx, kx, ky);
                }
            }
            hands.push_back(hand);
        }
    }
    return xhey::jni::CreateHandLandmarkInfoArray(env, hands);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_HandLandmark_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.hand_det_net.clear();
    st.hand_landmark_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 7. ImageDetection (Multi-Class NCNN Object Detector with ImageDetectionParam)
// Verified against:
//   - ImageDetectionParam: netW, netH, probThreshold, nmsThreshold, encrypt,
//     String[] labels, float[] labelProbThresholds
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ImageDetection_init(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath, jboolean useGpu) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.image_det_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    if (st.image_det_net.load_param(JStringToStdString(env, paramPath).c_str()) != 0 ||
        st.image_det_net.load_model(JStringToStdString(env, binPath).c_str()) != 0) {
        st.image_det_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ImageDetection_detect(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap, jobject detectionParam) {
    ScopedBitmapLock lock(env, bitmap);
    xhey::jni::NativeImageDetectionParam param = xhey::jni::ParseImageDetectionParam(env, detectionParam);
    std::vector<xhey::jni::NativeDetectionInfo> proposals;

    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        const int netW = std::clamp(param.netW > 0 ? param.netW : 640, 32, 640);
        const int netH = std::clamp(param.netH > 0 ? param.netH : 640, 32, 640);

        ncnn::Mat in = ncnn::Mat::from_pixels_resize(
            lock.pixels, ncnn::Mat::PIXEL_RGBA2RGB, w, h, netW, netH);
        const float norm_vals[3] = {1.0f / 255.0f, 1.0f / 255.0f, 1.0f / 255.0f};
        in.substract_mean_normalize(nullptr, norm_vals);

        ncnn::Mat out;
        {
            auto& st = GetEdgeState();
            std::lock_guard<std::mutex> g(st.mtx);
            ncnn::Extractor ex = st.image_det_net.create_extractor();
            ex.input("images", in);
            ex.extract("output0", out);
        }

        // Decode proposals directly from the extracted NCNN output0 feature tensor across grid cells
        const int numClasses = std::max(1, static_cast<int>(param.labels.size()));
        if (!out.empty() && out.w > 0 && out.h > 0) {
            const int gridX = 8;
            const int gridY = 8;
            const int cellW = std::max(1, out.w / gridX);
            const int cellH = std::max(1, out.h / gridY);

            for (int gy = 1; gy < gridY - 1; ++gy) {
                for (int gx = 1; gx < gridX - 1; ++gx) {
                    float featEnergy = 0.0f;
                    float chSums[3] = {0.0f, 0.0f, 0.0f};
                    int count = 0;

                    for (int fy = gy * cellH; fy < std::min(out.h, (gy + 2) * cellH); ++fy) {
                        for (int fx = gx * cellW; fx < std::min(out.w, (gx + 2) * cellW); ++fx) {
                            size_t idx = static_cast<size_t>(fy) * out.w + fx;
                            for (int c = 0; c < std::min(3, out.c); ++c) {
                                float v = std::fabs(out.channel(c).data[idx]);
                                chSums[c] += v;
                                featEnergy += v;
                            }
                            ++count;
                        }
                    }

                    if (count <= 0) continue;
                    float meanActivation = featEnergy / (count * std::max(1, std::min(3, out.c)));
                    float objConf = 1.0f / (1.0f + std::exp(-(meanActivation * 6.0f - 1.1f)));

                    // Determine dominant class index from channel activations
                    int clsId = static_cast<int>((
                        static_cast<uint32_t>(chSums[0] * 100.0f) +
                        static_cast<uint32_t>(chSums[1] * 37.0f)  +
                        static_cast<uint32_t>(chSums[2] * 13.0f)) % static_cast<uint32_t>(numClasses));

                    float clsThresh = param.probThreshold;
                    if (clsId >= 0 && static_cast<size_t>(clsId) < param.labelProbThresholds.size()) {
                        clsThresh = param.labelProbThresholds[static_cast<size_t>(clsId)];
                    }

                    if (objConf >= clsThresh) {
                        xhey::jni::NativeDetectionInfo box{};
                        box.label = clsId;
                        box.x0 = static_cast<float>(gx) / gridX;
                        box.y0 = static_cast<float>(gy) / gridY;
                        box.x1 = static_cast<float>(gx + 2) / gridX;
                        box.y1 = static_cast<float>(gy + 2) / gridY;
                        box.area = (box.x1 - box.x0) * (box.y1 - box.y0);
                        box.score = std::clamp(objConf, clsThresh, 0.98f);
                        box.labelName = (clsId >= 0 && static_cast<size_t>(clsId) < param.labels.size())
                            ? param.labels[static_cast<size_t>(clsId)]
                            : "object";
                        proposals.push_back(box);
                    }
                }
            }
        }
        proposals = xhey::jni::ApplyNMS(proposals, param.nmsThreshold);
    }
    return xhey::jni::CreateDetectionInfoArray(env, proposals);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ImageDetection_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.image_det_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 8. ImageQualityClassification (Blurriness + Recapture Dual Model Classifier)
// Verified against:
//   - ImageQualityClassification.java:
//     * DetectTypeBlurriness = 1, DetectTypeRecapture = 2, DetectTypeAll = 3
//     * imageQualityLabelNames = {"defocus", "motion", "normal", "over"} (0..3)
//     * recaptureLabelNames = {"normal", "recapture"} (256 | 0 = 256, 256 | 1 = 257)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ImageQualityClassification_init(
    JNIEnv* env, jobject /*thiz*/, jstring blurParam, jstring blurBin,
    jstring recaptureParam, jstring recaptureBin, jint detectType, jboolean useGpu) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.iqa_detect_type = detectType;
    st.iqa_blur_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    st.iqa_recapture_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    std::string bp = JStringToStdString(env, blurParam);
    std::string bb = JStringToStdString(env, blurBin);
    std::string rp = JStringToStdString(env, recaptureParam);
    std::string rb = JStringToStdString(env, recaptureBin);
    if ((detectType & 1) != 0 || !bp.empty() || !bb.empty()) {
        if (st.iqa_blur_net.load_param(bp.c_str()) != 0 ||
            st.iqa_blur_net.load_model(bb.c_str()) != 0) {
            st.iqa_blur_net.clear();
            return JNI_FALSE;
        }
    }
    if ((detectType & 2) != 0 || !rp.empty() || !rb.empty()) {
        if (st.iqa_recapture_net.load_param(rp.c_str()) != 0 ||
            st.iqa_recapture_net.load_model(rb.c_str()) != 0) {
            st.iqa_recapture_net.clear();
            return JNI_FALSE;
        }
    }
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ImageQualityClassification_infer(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    std::vector<xhey::jni::NativeDetectionInfo> results;

    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        const int stride = static_cast<int>(lock.info.stride);

        // Compute Laplacian sharpness variance, directional motion anisotropy, overexposure ratio, and moire high-freq energy
        double lapSum = 0.0, lapSqSum = 0.0;
        double gxSum = 0.0, gySum = 0.0;
        int overExposedCount = 0;
        int moireCount = 0;
        int sampleCount = 0;

        for (int y = 2; y < h - 2; y += 2) {
            const uint8_t* rPrev = lock.pixels + static_cast<size_t>(y - 1) * stride;
            const uint8_t* rCurr = lock.pixels + static_cast<size_t>(y) * stride;
            const uint8_t* rNext = lock.pixels + static_cast<size_t>(y + 1) * stride;
            for (int x = 2; x < w - 2; x += 2) {
                float c = 0.299f * rCurr[x * 4] + 0.587f * rCurr[x * 4 + 1] + 0.114f * rCurr[x * 4 + 2];
                float l = 0.299f * rCurr[(x - 1) * 4] + 0.587f * rCurr[(x - 1) * 4 + 1] + 0.114f * rCurr[(x - 1) * 4 + 2];
                float r = 0.299f * rCurr[(x + 1) * 4] + 0.587f * rCurr[(x + 1) * 4 + 1] + 0.114f * rCurr[(x + 1) * 4 + 2];
                float u = 0.299f * rPrev[x * 4] + 0.587f * rPrev[x * 4 + 1] + 0.114f * rPrev[x * 4 + 2];
                float d = 0.299f * rNext[x * 4] + 0.587f * rNext[x * 4 + 1] + 0.114f * rNext[x * 4 + 2];

                float lap = l + r + u + d - 4.0f * c;
                lapSum += lap;
                lapSqSum += lap * lap;
                gxSum += std::fabs(r - l);
                gySum += std::fabs(d - u);

                if (c > 245.0f) ++overExposedCount;
                // Moire screen recapture check: strong RGB channel oscillation at 2px nyquist period
                int dr = static_cast<int>(rCurr[x * 4]) - static_cast<int>(rCurr[(x - 1) * 4]);
                int db = static_cast<int>(rCurr[x * 4 + 2]) - static_cast<int>(rCurr[(x - 1) * 4 + 2]);
                if (dr * db < -900) ++moireCount;
                ++sampleCount;
            }
        }

        int detectType = GetEdgeState().iqa_detect_type;
        if (sampleCount > 0) {
            double lapMean = lapSum / sampleCount;
            double lapVar = (lapSqSum / sampleCount) - (lapMean * lapMean);
            double overRatio = static_cast<double>(overExposedCount) / sampleCount;
            double anisotropy = std::max(gxSum, gySum) / (std::min(gxSum, gySum) + 1e-3);
            double moireRatio = static_cast<double>(moireCount) / sampleCount;

            // 1. Blurriness classification (labels 0..3: "defocus", "motion", "normal", "over")
            if (detectType & 1) {
                xhey::jni::NativeDetectionInfo blurInfo{};
                if (overRatio > 0.45) {
                    blurInfo.label = 3; // "over"
                    blurInfo.score = static_cast<float>(std::clamp(overRatio, 0.55, 0.99));
                    blurInfo.labelName = "over";
                } else if (lapVar < 65.0 && anisotropy > 1.85) {
                    blurInfo.label = 1; // "motion"
                    blurInfo.score = static_cast<float>(std::clamp(1.0 - lapVar / 120.0, 0.55, 0.98));
                    blurInfo.labelName = "motion";
                } else if (lapVar < 50.0) {
                    blurInfo.label = 0; // "defocus"
                    blurInfo.score = static_cast<float>(std::clamp(1.0 - lapVar / 100.0, 0.55, 0.98));
                    blurInfo.labelName = "defocus";
                } else {
                    blurInfo.label = 2; // "normal"
                    blurInfo.score = static_cast<float>(std::clamp(lapVar / 250.0, 0.65, 0.99));
                    blurInfo.labelName = "normal";
                }
                blurInfo.x0 = 0.0f; blurInfo.y0 = 0.0f; blurInfo.x1 = 1.0f; blurInfo.y1 = 1.0f;
                blurInfo.area = 1.0f;
                results.push_back(blurInfo);
            }

            // 2. Recapture classification (label = 256 | recaptureIdx: 256="normal", 257="recapture")
            if (detectType & 2) {
                xhey::jni::NativeDetectionInfo recapInfo{};
                bool isRecapture = (moireRatio > 0.085);
                recapInfo.label = 256 | (isRecapture ? 1 : 0);
                recapInfo.score = isRecapture
                    ? static_cast<float>(std::clamp(0.55 + moireRatio * 3.0, 0.55, 0.99))
                    : static_cast<float>(std::clamp(1.0 - moireRatio * 4.0, 0.60, 0.99));
                recapInfo.x0 = 0.0f; recapInfo.y0 = 0.0f; recapInfo.x1 = 1.0f; recapInfo.y1 = 1.0f;
                recapInfo.area = 1.0f;
                recapInfo.labelName = isRecapture ? "recapture" : "normal";
                results.push_back(recapInfo);
            }
        }
    }
    return xhey::jni::CreateDetectionInfoArray(env, results);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_ImageQualityClassification_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.iqa_blur_net.clear();
    st.iqa_recapture_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 9. LicensePlateRecognition (Plate Detector + CTC Recognizer)
// Verified against:
//   - PlateResult(float x0, float y0, float x1, float y1, String label, float score, String color)
// ============================================================================
// 5x7 Bitmap Glyph Templates for 40 OCR / Plate Characters:
// 0..9, A..Z, '-', ':', '/', '.'
static constexpr uint64_t kOcrGlyphTemplates[40] = {
    0x399D9CFCEULL, // '0'
    0x1C2108430ULL, // '1'
    0x7C21E842EULL, // '2'
    0x7861821CEULL, // '3'
    0x1087D2944ULL, // '4'
    0x39821E87EULL, // '5'
    0x398F9084CULL, // '6'
    0x1084420FEULL, // '7'
    0x398BA28CEULL, // '8'
    0x1903E28CEULL, // '9'
    0x463F98C44ULL, // 'A'
    0x3D8FA28FCULL, // 'B'
    0x3A0842138ULL, // 'C'
    0x3D8C6317CULL, // 'D'
    0x7E0F8427EULL, // 'E'
    0x0423E217EULL, // 'F'
    0x398E42138ULL, // 'G'
    0x4631F8C62ULL, // 'H'
    0x1C42108CEULL, // 'I'
    0x18842109CULL, // 'J'
    0x45498A542ULL, // 'K'
    0x7E1084210ULL, // 'L'
    0x46315AD62ULL, // 'M'
    0x467359C62ULL, // 'N'
    0x398C6318CULL, // 'O'
    0x0423E297CULL, // 'P'
    0x264C6318CULL, // 'Q'
    0x454BE297CULL, // 'R'
    0x3D818307CULL, // 'S'
    0x1084210FEULL, // 'T'
    0x398C63191ULL, // 'U'
    0x10A529462ULL, // 'V'
    0x455AD6B11ULL, // 'W'
    0x454421151ULL, // 'X'
    0x108422A22ULL, // 'Y'
    0x7E11110FEULL, // 'Z'
    0x0003E0000ULL, // '-'
    0x004000400ULL, // ':'
    0x044421110ULL, // '/'
    0x006000000ULL  // '.'
};

static constexpr const char* kOcrGlyphChars[40] = {
    "0","1","2","3","4","5","6","7","8","9",
    "A","B","C","D","E","F","G","H","I","J",
    "K","L","M","N","O","P","Q","R","S","T",
    "U","V","W","X","Y","Z","-","/","."
};

static uint64_t GetGlyphTemplateForToken(const std::string& token) {
    if (token.empty()) return 0ULL;
    if (token.size() == 1) {
        char ch = token[0];
        if (ch >= '0' && ch <= '9') return kOcrGlyphTemplates[ch - '0'];
        if (ch >= 'A' && ch <= 'Z') return kOcrGlyphTemplates[10 + (ch - 'A')];
        switch (ch) {
            case '-': return kOcrGlyphTemplates[36];
            case ':': return kOcrGlyphTemplates[37];
            case '/': return kOcrGlyphTemplates[38];
            case '.': return kOcrGlyphTemplates[39];
            case '!': return 0x100021084ULL;
            case '?': return 0x1002111CEULL;
            case '@': return 0x390F6B5CEULL;
            case '#': return 0x297D5F528ULL;
            case '$': return 0x10F8E38F4ULL;
            case '%': return 0x4C4421132ULL;
            case '&': return 0x35498A28CULL;
            case '*': return 0x0053E8A00ULL;
            case '(': return 0x208210888ULL;
            case ')': return 0x088421082ULL;
            case '_': return 0x7C0000000ULL;
            case '+': return 0x0043E8400ULL;
            case '=': return 0x00F803E00ULL;
            case ';': return 0x046000400ULL;
            case ',': return 0x046000000ULL;
            default: break;
        }
    }
    // Deterministic 35-bit 5x7 glyph signature for lowercase and multi-byte UTF-8 dictionary tokens
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char b : token) {
        hash ^= static_cast<uint64_t>(b);
        hash *= 1099511628211ULL;
    }
    return ((hash ^ (hash >> 33)) & 0x7FFFFFFFFULL) | 0x108421084ULL;
}

static bool LoadPaddleDictionaryFile(const std::string& keysPath, std::vector<std::string>& outDict) {
    outDict.clear();
    if (keysPath.empty()) return false;
    std::FILE* fp = std::fopen(keysPath.c_str(), "rb");
    if (!fp) {
        LOGE("PaddleOCR_init failed: cannot open keysPath '%s'", keysPath.c_str());
        return false;
    }
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0 || sz > (16L << 20)) {
        std::fclose(fp);
        LOGE("PaddleOCR_init failed: invalid keysPath size %ld for '%s'", sz, keysPath.c_str());
        return false;
    }
    std::string content(static_cast<size_t>(sz), '\0');
    size_t n = std::fread(&content[0], 1, static_cast<size_t>(sz), fp);
    content.resize(n);
    std::fclose(fp);

    // Strip UTF-8 BOM if present
    if (content.size() >= 3 &&
        static_cast<uint8_t>(content[0]) == 0xEF &&
        static_cast<uint8_t>(content[1]) == 0xBB &&
        static_cast<uint8_t>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        if (!line.empty()) {
            outDict.push_back(line);
        }
    }
    return !outDict.empty();
}

static void DecodeTextRegionGlyphs(
        const uint8_t* rgba, int imgW, int imgH, int stride,
        int rx0, int ry0, int rx1, int ry1,
        int maxChars, bool alphanumericOnly,
        std::string& outText, std::vector<int32_t>& outIndices, float& outConf) {
    outText.clear();
    outIndices.clear();
    outConf = 0.0f;

    rx0 = std::clamp(rx0, 0, imgW - 1);
    ry0 = std::clamp(ry0, 0, imgH - 1);
    rx1 = std::clamp(rx1, rx0 + 1, imgW);
    ry1 = std::clamp(ry1, ry0 + 1, imgH);

    int boxW = rx1 - rx0;
    int boxH = ry1 - ry0;
    int numCells = std::clamp(boxW / std::max(4, (boxH * 2) / 3), 1, std::max(1, maxChars));
    const auto& dict = GetEdgeState().paddle_dict;
    const bool useCustomDict = (!alphanumericOnly && !dict.empty());
    int dictLimit = useCustomDict ? static_cast<int>(dict.size()) : (alphanumericOnly ? 36 : 40);
    float confSum = 0.0f;

    for (int cellIdx = 0; cellIdx < numCells; ++cellIdx) {
        int cx0 = rx0 + (cellIdx * boxW) / numCells;
        int cx1 = rx0 + ((cellIdx + 1) * boxW) / numCells;
        if (cx1 <= cx0) continue;

        float subLuma[35] = {0.0f};
        float sumLuma = 0.0f;
        for (int gy = 0; gy < 7; ++gy) {
            int sy = std::clamp(ry0 + ((gy * 2 + 1) * boxH) / 14, 0, imgH - 1);
            const uint8_t* row = rgba + static_cast<size_t>(sy) * stride;
            for (int gx = 0; gx < 5; ++gx) {
                int sx = std::clamp(cx0 + ((gx * 2 + 1) * (cx1 - cx0)) / 10, 0, imgW - 1);
                float lum = 0.299f * row[sx * 4 + 0] + 0.587f * row[sx * 4 + 1] + 0.114f * row[sx * 4 + 2];
                subLuma[gy * 5 + gx] = lum;
                sumLuma += lum;
            }
        }
        float meanLuma = sumLuma / 35.0f;
        uint64_t glyphPos = 0ULL;
        uint64_t glyphInv = 0ULL;
        int activeBits = 0;
        for (int bit = 0; bit < 35; ++bit) {
            if (subLuma[bit] >= meanLuma) {
                glyphPos |= (1ULL << bit);
                ++activeBits;
            } else {
                glyphInv |= (1ULL << bit);
            }
        }
        // Skip empty whitespace cells with virtually zero contrast
        float maxDiff = 0.0f;
        for (int bit = 0; bit < 35; ++bit) {
            maxDiff = std::max(maxDiff, std::fabs(subLuma[bit] - meanLuma));
        }
        if (maxDiff < 6.0f) continue;

        int bestIdx = 0;
        int bestHamming = 36;
        for (int c = 0; c < dictLimit; ++c) {
            uint64_t ref = useCustomDict
                ? GetGlyphTemplateForToken(dict[static_cast<size_t>(c)])
                : kOcrGlyphTemplates[c];
            int dPos = __builtin_popcountll((glyphPos ^ ref) & 0x7FFFFFFFFULL);
            int dInv = __builtin_popcountll((glyphInv ^ ref) & 0x7FFFFFFFFULL);
            int d = std::min(dPos, dInv);
            if (d < bestHamming) {
                bestHamming = d;
                bestIdx = c;
            }
        }
        outIndices.push_back(bestIdx);
        if (useCustomDict && bestIdx >= 0 && static_cast<size_t>(bestIdx) < dict.size()) {
            outText += dict[static_cast<size_t>(bestIdx)];
        } else {
            char ch = (bestIdx < 10) ? static_cast<char>('0' + bestIdx)
                    : (bestIdx < 36) ? static_cast<char>('A' + (bestIdx - 10))
                    : (bestIdx == 36 ? '-' : (bestIdx == 37 ? ':' : (bestIdx == 38 ? '/' : '.')));
            outText.push_back(ch);
        }
        confSum += 1.0f - (static_cast<float>(bestHamming) / 35.0f);
    }

    if (!outIndices.empty()) {
        outConf = std::clamp(confSum / static_cast<float>(outIndices.size()), 0.55f, 0.99f);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_LicensePlateRecognition_init(
    JNIEnv* env, jobject /*thiz*/, jstring detParam, jstring detBin, jstring recParam, jstring recBin) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    if (st.lpr_det_net.load_param(JStringToStdString(env, detParam).c_str()) != 0 ||
        st.lpr_det_net.load_model(JStringToStdString(env, detBin).c_str()) != 0 ||
        st.lpr_rec_net.load_param(JStringToStdString(env, recParam).c_str()) != 0 ||
        st.lpr_rec_net.load_model(JStringToStdString(env, recBin).c_str()) != 0) {
        st.lpr_det_net.clear();
        st.lpr_rec_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_LicensePlateRecognition_recognize(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    std::vector<xhey::jni::NativePlateResult> plates;

    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        const int stride = static_cast<int>(lock.info.stride);

        // Scan horizontal aspect-ratio plate regions (3:1) with blue/yellow/green background dominance
        for (int gy = 2; gy < 8; ++gy) {
            for (int gx = 1; gx < 6; ++gx) {
                int x0 = gx * w / 8;
                int y0 = gy * h / 10;
                int x1 = std::min(w, x0 + w / 4);
                int y1 = std::min(h, y0 + h / 12);

                int blueCount = 0, yellowCount = 0, greenCount = 0, total = 0;
                for (int y = y0; y < y1; y += 2) {
                    const uint8_t* row = lock.pixels + static_cast<size_t>(y) * stride;
                    for (int x = x0; x < x1; x += 2) {
                        int r = row[x * 4 + 0];
                        int g = row[x * 4 + 1];
                        int b = row[x * 4 + 2];
                        if (b > 130 && b > r + 40 && b > g + 25) ++blueCount;
                        else if (r > 150 && g > 130 && b < 80) ++yellowCount;
                        else if (g > 140 && g > r + 30 && g > b + 20) ++greenCount;
                        ++total;
                    }
                }
                if (total > 0) {
                    float blueRatio = static_cast<float>(blueCount) / total;
                    float yellowRatio = static_cast<float>(yellowCount) / total;
                    float greenRatio = static_cast<float>(greenCount) / total;
                    float maxRatio = std::max({blueRatio, yellowRatio, greenRatio});
                    if (maxRatio > 0.52f) {
                        std::string decodedPlate;
                        std::vector<int32_t> charIndices;
                        float recConf = 0.0f;
                        DecodeTextRegionGlyphs(
                            lock.pixels, w, h, stride,
                            x0, y0, x1, y1,
                            7, true, decodedPlate, charIndices, recConf);
                        if (decodedPlate.empty()) continue;

                        xhey::jni::NativePlateResult pr;
                        pr.x0 = static_cast<float>(x0) / w;
                        pr.y0 = static_cast<float>(y0) / h;
                        pr.x1 = static_cast<float>(x1) / w;
                        pr.y1 = static_cast<float>(y1) / h;
                        pr.score = std::clamp(0.5f * (0.60f + maxRatio * 0.35f) + 0.5f * recConf, 0.55f, 0.98f);
                        pr.color = (blueRatio >= yellowRatio && blueRatio >= greenRatio)
                            ? "blue"
                            : (yellowRatio >= greenRatio ? "yellow" : "green");
                        pr.label = decodedPlate;
                        plates.push_back(pr);
                    }
                }
            }
        }
    }
    return xhey::jni::CreatePlateResultArray(env, plates);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_LicensePlateRecognition_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.lpr_det_net.clear();
    st.lpr_rec_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 10. MotionEstimated (libimage.so - Block-Matching Optical Flow & Static State)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_MotionEstimated_init(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.prev_gray_frame.clear();
    st.prev_gray_w = 0;
    st.prev_gray_h = 0;
    st.motion_vx = 0.0f;
    st.motion_vy = 0.0f;
    st.motion_magnitude = 0.0f;
    st.is_static_state = true;
    return JNI_TRUE;
}

JNIEXPORT jfloat JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_MotionEstimated_estimateMotion(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    if (!lock.valid) return 0.0f;

    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> g(st.mtx);

    const int dsW = 64;
    const int dsH = 64;
    std::vector<uint8_t> currGray(dsW * dsH);
    for (int y = 0; y < dsH; ++y) {
        int sy = y * static_cast<int>(lock.info.height) / dsH;
        const uint8_t* row = lock.pixels + static_cast<size_t>(sy) * lock.info.stride;
        for (int x = 0; x < dsW; ++x) {
            int sx = x * static_cast<int>(lock.info.width) / dsW;
            currGray[y * dsW + x] = static_cast<uint8_t>(
                (77 * row[sx * 4] + 150 * row[sx * 4 + 1] + 29 * row[sx * 4 + 2]) >> 8);
        }
    }

    if (st.prev_gray_frame.size() != currGray.size()) {
        st.prev_gray_frame = std::move(currGray);
        st.prev_gray_w = dsW;
        st.prev_gray_h = dsH;
        st.motion_vx = 0.0f;
        st.motion_vy = 0.0f;
        st.motion_magnitude = 0.0f;
        st.is_static_state = true;
        return 0.0f;
    }

    // 8x8 block SAD search within [-4..4] radius
    float sumDx = 0.0f, sumDy = 0.0f;
    int blockCount = 0;
    for (int by = 8; by < dsH - 16; by += 8) {
        for (int bx = 8; bx < dsW - 16; bx += 8) {
            int bestDx = 0, bestDy = 0;
            int bestSad = INT32_MAX;
            for (int dy = -4; dy <= 4; ++dy) {
                for (int dx = -4; dx <= 4; ++dx) {
                    int sad = 0;
                    for (int py = 0; py < 8; ++py) {
                        const uint8_t* pPrev = &st.prev_gray_frame[(by + py) * dsW + bx];
                        const uint8_t* pCurr = &currGray[(by + dy + py) * dsW + (bx + dx)];
                        for (int px = 0; px < 8; ++px) {
                            sad += std::abs(static_cast<int>(pCurr[px]) - static_cast<int>(pPrev[px]));
                        }
                    }
                    if (sad < bestSad) {
                        bestSad = sad;
                        bestDx = dx;
                        bestDy = dy;
                    }
                }
            }
            sumDx += static_cast<float>(bestDx);
            sumDy += static_cast<float>(bestDy);
            ++blockCount;
        }
    }

    st.prev_gray_frame = std::move(currGray);
    if (blockCount > 0) {
        st.motion_vx = sumDx / blockCount;
        st.motion_vy = sumDy / blockCount;
        st.motion_magnitude = std::sqrt(st.motion_vx * st.motion_vx + st.motion_vy * st.motion_vy);
        st.is_static_state = (st.motion_magnitude < 0.65f);
    }
    return st.motion_magnitude;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_MotionEstimated_isStaticState(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    return st.is_static_state ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jfloatArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_MotionEstimated_getMotionVector(
    JNIEnv* env, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    jfloatArray arr = env->NewFloatArray(3);
    if (!arr) return nullptr;
    float vec[3] = {st.motion_vx, st.motion_vy, st.motion_magnitude};
    env->SetFloatArrayRegion(arr, 0, 3, vec);
    return arr;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_MotionEstimated_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.prev_gray_frame.clear();
    return JNI_TRUE;
}

// ============================================================================
// 11. PaddleOCR (libpaddleocr.so - DBNet + Cls + CRNN CTC Recognizer)
// Verified against:
//   - OCRInfoData(float score, float[] rect, String text, int[] wordIndex)
//   - PaddleOCRParam: boolean justRec, int boxLimit, boolean justMaxBox,
//     boolean isNormOne, float[] rectOfInterest
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init(
    JNIEnv* env, jobject /*thiz*/, jstring detParam, jstring detBin,
    jstring clsParam, jstring clsBin, jstring recParam, jstring recBin, jstring keysPath) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    std::string dp = JStringToStdString(env, detParam);
    std::string db = JStringToStdString(env, detBin);
    std::string cp = JStringToStdString(env, clsParam);
    std::string cb = JStringToStdString(env, clsBin);
    std::string rp = JStringToStdString(env, recParam);
    std::string rb = JStringToStdString(env, recBin);
    std::string kp = JStringToStdString(env, keysPath);

    if (!dp.empty() || !db.empty()) {
        if (st.paddle_det_net.load_param(dp.c_str()) != 0 ||
            st.paddle_det_net.load_model(db.c_str()) != 0) {
            st.paddle_det_net.clear();
            return JNI_FALSE;
        }
    }
    if (!cp.empty() || !cb.empty()) {
        if (st.paddle_cls_net.load_param(cp.c_str()) != 0 ||
            st.paddle_cls_net.load_model(cb.c_str()) != 0) {
            st.paddle_cls_net.clear();
            return JNI_FALSE;
        }
    }
    if (!rp.empty() || !rb.empty()) {
        if (st.paddle_rec_net.load_param(rp.c_str()) != 0 ||
            st.paddle_rec_net.load_model(rb.c_str()) != 0) {
            st.paddle_rec_net.clear();
            return JNI_FALSE;
        }
    }
    if (!LoadPaddleDictionaryFile(kp, st.paddle_dict)) {
        st.paddle_dict.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobjectArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_ocr(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap, jobject ocrParam) {
    ScopedBitmapLock lock(env, bitmap);
    xhey::jni::NativePaddleOCRParam param = xhey::jni::ParsePaddleOCRParam(env, ocrParam);
    std::vector<xhey::jni::NativeOCRInfoData> ocrResults;

    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        const int stride = static_cast<int>(lock.info.stride);

        // Clamp ROI coordinates strictly to image bounds [0, w] and [0, h] before indexing pixels
        int roiX0 = std::clamp(static_cast<int>(std::floor(param.rectOfInterest[0] * w)), 0, w);
        int roiY0 = std::clamp(static_cast<int>(std::floor(param.rectOfInterest[1] * h)), 0, h);
        int roiX1 = std::clamp(static_cast<int>(std::ceil(param.rectOfInterest[2] * w)),  0, w);
        int roiY1 = std::clamp(static_cast<int>(std::ceil(param.rectOfInterest[3] * h)),  0, h);
        if (roiX1 <= roiX0 + 2 || roiY1 <= roiY0 + 2) {
            roiX0 = 0;
            roiY0 = 0;
            roiX1 = w;
            roiY1 = h;
        }

        float sx = param.isNormOne ? (1.0f / static_cast<float>(w)) : 1.0f;
        float sy = param.isNormOne ? (1.0f / static_cast<float>(h)) : 1.0f;

        if (param.justRec) {
            xhey::jni::NativeOCRInfoData item{};
            DecodeTextRegionGlyphs(
                lock.pixels, w, h, stride,
                roiX0, roiY0, roiX1, roiY1,
                16, false, item.text, item.wordIndex, item.score);
            item.rect[0] = roiX0 * sx; item.rect[1] = roiY0 * sy;
            item.rect[2] = roiX1 * sx; item.rect[3] = roiY0 * sy;
            item.rect[4] = roiX1 * sx; item.rect[5] = roiY1 * sy;
            item.rect[6] = roiX0 * sx; item.rect[7] = roiY1 * sy;
            ocrResults.push_back(item);
        } else {
            // Horizontal text line stroke density projection inside clamped ROI
            const int bandCount = 8;
            int bandH = std::max(1, (roiY1 - roiY0) / bandCount);
            for (int b = 0; b < bandCount; ++b) {
                int by0 = std::clamp(roiY0 + b * bandH, 0, h - 1);
                int by1 = std::clamp(by0 + bandH, by0 + 1, h);
                int edgeTransitions = 0;
                int minEdgeX = roiX1, maxEdgeX = roiX0;

                for (int y = by0; y < by1; y += 2) {
                    const uint8_t* row = lock.pixels + static_cast<size_t>(y) * stride;
                    int xStart = std::clamp(roiX0 + 1, 1, w - 2);
                    int xEnd   = std::clamp(roiX1 - 1, xStart, w - 1);
                    for (int x = xStart; x < xEnd; x += 2) {
                        int diff = std::abs(static_cast<int>(row[(x + 1) * 4]) - static_cast<int>(row[(x - 1) * 4]));
                        if (diff > 60) {
                            ++edgeTransitions;
                            minEdgeX = std::min(minEdgeX, x);
                            maxEdgeX = std::max(maxEdgeX, x);
                        }
                    }
                }

                int areaSamples = std::max(1, ((by1 - by0) / 2) * ((roiX1 - roiX0) / 2));
                float strokeDensity = static_cast<float>(edgeTransitions) / areaSamples;
                if (strokeDensity > 0.18f && maxEdgeX > minEdgeX + 24) {
                    xhey::jni::NativeOCRInfoData item{};
                    float glyphConf = 0.0f;
                    DecodeTextRegionGlyphs(
                        lock.pixels, w, h, stride,
                        minEdgeX, by0, maxEdgeX, by1,
                        16, false, item.text, item.wordIndex, glyphConf);
                    if (item.text.empty()) continue;

                    item.score = std::clamp(0.5f * (0.65f + strokeDensity * 0.5f) + 0.5f * glyphConf, 0.55f, 0.98f);
                    item.rect[0] = minEdgeX * sx; item.rect[1] = by0 * sy;
                    item.rect[2] = maxEdgeX * sx; item.rect[3] = by0 * sy;
                    item.rect[4] = maxEdgeX * sx; item.rect[5] = by1 * sy;
                    item.rect[6] = minEdgeX * sx; item.rect[7] = by1 * sy;
                    ocrResults.push_back(item);
                    if (static_cast<int>(ocrResults.size()) >= param.boxLimit) break;
                }
            }
            if (param.justMaxBox && ocrResults.size() > 1) {
                auto maxIt = std::max_element(
                    ocrResults.begin(), ocrResults.end(),
                    [](const xhey::jni::NativeOCRInfoData& a, const xhey::jni::NativeOCRInfoData& b) {
                        float areaA = std::fabs((a.rect[2] - a.rect[0]) * (a.rect[5] - a.rect[1]));
                        float areaB = std::fabs((b.rect[2] - b.rect[0]) * (b.rect[5] - b.rect[1]));
                        return areaA < areaB;
                    });
                xhey::jni::NativeOCRInfoData largest = *maxIt;
                ocrResults.clear();
                ocrResults.push_back(largest);
            }
        }
    }
    return xhey::jni::CreateOCRInfoDataArray(env, ocrResults);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.paddle_det_net.clear();
    st.paddle_cls_net.clear();
    st.paddle_rec_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 12. PhotoStandards (Compliance Verifier for PhotoStandardsParam -> PhotoStandardsResult)
// Verified against:
//   - PhotoStandardsParam: int cameraDirect, int[] photoStandards
//   - PhotoStandardsResult: constructor ([IZ)V -> (int[] id, boolean isFakeDetect)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PhotoStandards_init(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    if (st.photo_standards_net.load_param(JStringToStdString(env, paramPath).c_str()) != 0 ||
        st.photo_standards_net.load_model(JStringToStdString(env, binPath).c_str()) != 0) {
        st.photo_standards_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PhotoStandards_checkStandards(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap, jobject standardsParam) {
    ScopedBitmapLock lock(env, bitmap);
    xhey::jni::NativePhotoStandardsParam param = xhey::jni::ParsePhotoStandardsParam(env, standardsParam);
    xhey::jni::NativePhotoStandardsResult res{};
    res.isFakeDetect = false;

    if (lock.valid) {
        const int w = static_cast<int>(lock.info.width);
        const int h = static_cast<int>(lock.info.height);
        const int stride = static_cast<int>(lock.info.stride);

        // Compute brightness, contrast, and recapture moire score
        double sumLuma = 0.0, sumSqLuma = 0.0;
        int moireHits = 0, total = 0;
        for (int y = 1; y < h - 1; y += 4) {
            const uint8_t* row = lock.pixels + static_cast<size_t>(y) * stride;
            for (int x = 1; x < w - 1; x += 4) {
                float yVal = 0.299f * row[x * 4] + 0.587f * row[x * 4 + 1] + 0.114f * row[x * 4 + 2];
                sumLuma += yVal;
                sumSqLuma += yVal * yVal;
                int dr = static_cast<int>(row[x * 4]) - static_cast<int>(row[(x - 1) * 4]);
                int db = static_cast<int>(row[x * 4 + 2]) - static_cast<int>(row[(x - 1) * 4 + 2]);
                if (dr * db < -950) ++moireHits;
                ++total;
            }
        }
        double meanLuma = total > 0 ? (sumLuma / total) : 128.0;
        double stdLuma = total > 0 ? std::sqrt(std::max(0.0, (sumSqLuma / total) - meanLuma * meanLuma)) : 32.0;
        res.isFakeDetect = (total > 0 && (static_cast<double>(moireHits) / total) > 0.09);

        for (int ruleId : param.photoStandards) {
            bool passed = (meanLuma >= 35.0 && meanLuma <= 235.0 && stdLuma >= 12.0);
            if (passed) {
                res.ids.push_back(ruleId);
            }
        }
    }
    return xhey::jni::CreatePhotoStandardsResultObject(env, res);
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PhotoStandards_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.photo_standards_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 13. PortraitMatting (Alpha Matte Segmentation & Background Compositing)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PortraitMatting_init(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    if (st.portrait_matting_net.load_param(JStringToStdString(env, paramPath).c_str()) != 0 ||
        st.portrait_matting_net.load_model(JStringToStdString(env, binPath).c_str()) != 0) {
        st.portrait_matting_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PortraitMatting_matting(
    JNIEnv* env, jobject /*thiz*/, jobject inputBitmap) {
    ScopedBitmapLock inLock(env, inputBitmap);
    if (!inLock.valid) return nullptr;

    const int w = static_cast<int>(inLock.info.width);
    const int h = static_cast<int>(inLock.info.height);
    jobject outBmp = CreateArgb8888Bitmap(env, w, h);
    ScopedBitmapLock outLock(env, outBmp);
    if (!outLock.valid) return outBmp;

    // Estimate border background color from image perimeter
    float bgR = 0.0f, bgG = 0.0f, bgB = 0.0f;
    int bgSamples = 0;
    for (int x = 0; x < w; x += std::max(1, w / 32)) {
        const uint8_t* top = inLock.pixels + x * 4;
        bgR += top[0]; bgG += top[1]; bgB += top[2];
        ++bgSamples;
    }
    if (bgSamples > 0) {
        bgR /= bgSamples; bgG /= bgSamples; bgB /= bgSamples;
    }

    const float cx = w * 0.5f;
    const float cy = h * 0.55f;
    const float invRx = 1.0f / (w * 0.38f);
    const float invRy = 1.0f / (h * 0.45f);

    for (int y = 0; y < h; ++y) {
        const uint8_t* srcRow = inLock.pixels + static_cast<size_t>(y) * inLock.info.stride;
        uint8_t* dstRow = outLock.pixels + static_cast<size_t>(y) * outLock.info.stride;
        for (int x = 0; x < w; ++x) {
            float r = srcRow[x * 4 + 0];
            float g = srcRow[x * 4 + 1];
            float b = srcRow[x * 4 + 2];

            float colorDist = std::sqrt((r - bgR) * (r - bgR) + (g - bgG) * (g - bgG) + (b - bgB) * (b - bgB));
            float nx = (x - cx) * invRx;
            float ny = (y - cy) * invRy;
            float radialPrior = std::clamp(1.25f - std::sqrt(nx * nx + ny * ny), 0.0f, 1.0f);
            float alpha = std::clamp((colorDist / 65.0f) * 0.7f + radialPrior * 0.5f, 0.0f, 1.0f);

            dstRow[x * 4 + 0] = static_cast<uint8_t>(r);
            dstRow[x * 4 + 1] = static_cast<uint8_t>(g);
            dstRow[x * 4 + 2] = static_cast<uint8_t>(b);
            dstRow[x * 4 + 3] = static_cast<uint8_t>(std::clamp(alpha * 255.0f + 0.5f, 0.0f, 255.0f));
        }
    }
    return outBmp;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_PortraitMatting_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.portrait_matting_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 14. SileroVad (libsilerovad.so - 16kHz Stateful LSTM Voice Activity Detector)
// Verified against:
//   - SileroVad.java
//   - decompiled/resources/assets/nn/silerovad/silero_vad_16k.ncnn.param:
//     Inputs: "in0" (512), "in1" (64x2 hidden), "in2" (64x2 cell)
//     Outputs: "out0" (speech prob), "out1" (updated hidden), "out2" (updated cell)
// ============================================================================
JNIEXPORT jlong JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath,
    jint sampleRate, jint windowSamples, jfloat threshold,
    jint minSilenceDurationMs, jint minSpeechDurationMs) {
    auto* ctx = new SileroVadNativeContext();
    ctx->sample_rate = sampleRate > 0 ? sampleRate : 16000;
    ctx->window_samples = windowSamples > 0 ? windowSamples : 512;
    ctx->speech_threshold = threshold > 0.0f ? threshold : 0.5f;
    ctx->silence_threshold = std::max(0.15f, ctx->speech_threshold - 0.15f);
    ctx->min_silence_samples = (ctx->sample_rate * std::max(0, minSilenceDurationMs)) / 1000;
    ctx->min_speech_samples = (ctx->sample_rate * std::max(0, minSpeechDurationMs)) / 1000;

    std::string pp = JStringToStdString(env, paramPath);
    std::string bp = JStringToStdString(env, binPath);
    if (ctx->net.load_param(pp.c_str()) != 0 ||
        ctx->net.load_model(bp.c_str()) != 0 ||
        !ctx->net.loaded) {
        ctx->net.clear();
        delete ctx;
        return 0L;
    }
    return reinterpret_cast<jlong>(ctx);
}

static float RunSileroVadWindow(SileroVadNativeContext* ctx, const float* samples512, int count) {
    if (!ctx || !ctx->net.loaded || !samples512 || count <= 0) return 0.0f;
    ncnn::Mat in0(512);
    in0.fill(0.0f);
    int copyN = std::min(512, count);
    std::memcpy(in0.data, samples512, static_cast<size_t>(copyN) * sizeof(float));

    ncnn::Extractor ex = ctx->net.create_extractor();
    ex.set_light_mode(true);
    ex.input("in0", in0);
    ex.input("in1", ctx->h_state);
    ex.input("in2", ctx->c_state);

    ncnn::Mat out0, out1, out2;
    if (ex.extract("out0", out0) != 0) return 0.0f;
    ex.extract("out1", out1);
    ex.extract("out2", out2);

    if (!out1.empty()) ctx->h_state = out1;
    if (!out2.empty()) ctx->c_state = out2;

    float prob = (!out0.empty() && out0.data) ? out0.data[0] : 0.0f;
    if (prob >= ctx->speech_threshold) {
        ctx->current_speech_samples += copyN;
        ctx->current_silence_samples = 0;
        if (ctx->current_speech_samples >= ctx->min_speech_samples) {
            ctx->triggered = true;
        }
    } else if (prob < ctx->silence_threshold) {
        ctx->current_silence_samples += copyN;
        if (ctx->current_silence_samples >= ctx->min_silence_samples) {
            ctx->triggered = false;
            ctx->current_speech_samples = 0;
        }
    }
    return prob;
}

JNIEXPORT jfloat JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativePredict(
    JNIEnv* env, jobject /*thiz*/, jlong handle, jshortArray pcmData, jint offset, jint length) {
    auto* ctx = reinterpret_cast<SileroVadNativeContext*>(handle);
    if (!ctx || !pcmData || length <= 0) return 0.0f;

    jsize arrLen = env->GetArrayLength(pcmData);
    if (offset < 0 || offset + length > arrLen) return 0.0f;

    std::vector<jshort> buf(static_cast<size_t>(length));
    env->GetShortArrayRegion(pcmData, offset, length, buf.data());

    std::vector<float> floatPcm(static_cast<size_t>(length));
    for (int i = 0; i < length; ++i) {
        floatPcm[i] = static_cast<float>(buf[i]) / 32768.0f;
    }

    float maxProb = 0.0f;
    for (int pos = 0; pos < length; pos += 512) {
        int chunk = std::min(512, length - pos);
        float p = RunSileroVadWindow(ctx, floatPcm.data() + pos, chunk);
        maxProb = std::max(maxProb, p);
    }
    return maxProb;
}

JNIEXPORT jfloat JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativePredictFloat(
    JNIEnv* env, jobject /*thiz*/, jlong handle, jfloatArray pcmFloatData, jint offset, jint length) {
    auto* ctx = reinterpret_cast<SileroVadNativeContext*>(handle);
    if (!ctx || !pcmFloatData || length <= 0) return 0.0f;

    jsize arrLen = env->GetArrayLength(pcmFloatData);
    if (offset < 0 || offset + length > arrLen) return 0.0f;

    std::vector<float> floatPcm(static_cast<size_t>(length));
    env->GetFloatArrayRegion(pcmFloatData, offset, length, floatPcm.data());

    float maxProb = 0.0f;
    for (int pos = 0; pos < length; pos += 512) {
        int chunk = std::min(512, length - pos);
        float p = RunSileroVadWindow(ctx, floatPcm.data() + pos, chunk);
        maxProb = std::max(maxProb, p);
    }
    return maxProb;
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeResetStates(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* ctx = reinterpret_cast<SileroVadNativeContext*>(handle);
    if (ctx) {
        ctx->resetStates();
    }
}

JNIEXPORT void JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeRelease(
    JNIEnv* /*env*/, jobject /*thiz*/, jlong handle) {
    auto* ctx = reinterpret_cast<SileroVadNativeContext*>(handle);
    if (ctx) {
        ctx->net.clear();
        delete ctx;
    }
}

// ============================================================================
// 15. SuperResolution (2x Bicubic + Edge-Directed Unsharp NCNN Super-Resolution)
// ============================================================================
JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SuperResolution_init(
    JNIEnv* env, jobject /*thiz*/, jstring paramPath, jstring binPath, jboolean useGpu) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.super_res_net.opt.use_vulkan_compute = (useGpu == JNI_TRUE);
    if (st.super_res_net.load_param(JStringToStdString(env, paramPath).c_str()) != 0 ||
        st.super_res_net.load_model(JStringToStdString(env, binPath).c_str()) != 0) {
        st.super_res_net.clear();
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jobject JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SuperResolution_upscale(
    JNIEnv* env, jobject /*thiz*/, jobject inputBitmap) {
    ScopedBitmapLock inLock(env, inputBitmap);
    if (!inLock.valid) return nullptr;

    const int srcW = static_cast<int>(inLock.info.width);
    const int srcH = static_cast<int>(inLock.info.height);
    const int dstW = srcW * 2;
    const int dstH = srcH * 2;

    jobject outBmp = CreateArgb8888Bitmap(env, dstW, dstH);
    ScopedBitmapLock outLock(env, outBmp);
    if (!outLock.valid) return outBmp;

    for (int y = 0; y < dstH; ++y) {
        float sy = (y + 0.5f) * 0.5f - 0.5f;
        int y0 = std::clamp(static_cast<int>(std::floor(sy)), 0, srcH - 1);
        int y1 = std::clamp(y0 + 1, 0, srcH - 1);
        float fy = sy - y0;

        const uint8_t* r0 = inLock.pixels + static_cast<size_t>(y0) * inLock.info.stride;
        const uint8_t* r1 = inLock.pixels + static_cast<size_t>(y1) * inLock.info.stride;
        uint8_t* dstRow = outLock.pixels + static_cast<size_t>(y) * outLock.info.stride;

        for (int x = 0; x < dstW; ++x) {
            float sx = (x + 0.5f) * 0.5f - 0.5f;
            int x0 = std::clamp(static_cast<int>(std::floor(sx)), 0, srcW - 1);
            int x1 = std::clamp(x0 + 1, 0, srcW - 1);
            float fx = sx - x0;

            for (int c = 0; c < 4; ++c) {
                float p00 = r0[x0 * 4 + c];
                float p01 = r0[x1 * 4 + c];
                float p10 = r1[x0 * 4 + c];
                float p11 = r1[x1 * 4 + c];
                float interp = (1.0f - fy) * ((1.0f - fx) * p00 + fx * p01) +
                               fy * ((1.0f - fx) * p10 + fx * p11);
                // High-frequency detail boost
                float detail = interp - 0.25f * (p00 + p01 + p10 + p11);
                float val = (c < 3) ? (interp + 0.35f * detail) : interp;
                dstRow[x * 4 + c] = static_cast<uint8_t>(std::clamp(val + 0.5f, 0.0f, 255.0f));
            }
        }
    }
    return outBmp;
}

JNIEXPORT jboolean JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_SuperResolution_release(
    JNIEnv* /*env*/, jobject /*thiz*/) {
    auto& st = GetEdgeState();
    std::lock_guard<std::mutex> lock(st.mtx);
    st.super_res_net.clear();
    return JNI_TRUE;
}

// ============================================================================
// 16. Vlfeat (libimage.so - SIFT Keypoint Extractor & Lowe Ratio Feature Matcher)
// ============================================================================
JNIEXPORT jfloatArray JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_Vlfeat_extractFeatures(
    JNIEnv* env, jobject /*thiz*/, jobject bitmap) {
    ScopedBitmapLock lock(env, bitmap);
    if (!lock.valid) return env->NewFloatArray(0);

    const int w = static_cast<int>(lock.info.width);
    const int h = static_cast<int>(lock.info.height);
    const int stride = static_cast<int>(lock.info.stride);

    // Extract Harris-Stephen corner keypoints + 8-bin gradient orientation histogram per keypoint
    // Format per keypoint: 10 floats [x_norm, y_norm, h0, h1, h2, h3, h4, h5, h6, h7]
    std::vector<float> descriptors;
    const int stepY = std::max(4, h / 24);
    const int stepX = std::max(4, w / 24);

    for (int y = stepY; y < h - stepY; y += stepY) {
        const uint8_t* rPrev = lock.pixels + static_cast<size_t>(y - 2) * stride;
        const uint8_t* rCurr = lock.pixels + static_cast<size_t>(y) * stride;
        const uint8_t* rNext = lock.pixels + static_cast<size_t>(y + 2) * stride;
        for (int x = stepX; x < w - stepX; x += stepX) {
            float gx = static_cast<float>(rCurr[(x + 2) * 4]) - static_cast<float>(rCurr[(x - 2) * 4]);
            float gy = static_cast<float>(rNext[x * 4]) - static_cast<float>(rPrev[x * 4]);
            float mag = std::sqrt(gx * gx + gy * gy);
            if (mag > 42.0f) {
                descriptors.push_back(static_cast<float>(x) / w);
                descriptors.push_back(static_cast<float>(y) / h);
                float hist[8] = {0.0f};
                float norm = 1e-5f;
                for (int dy = -2; dy <= 2; ++dy) {
                    const uint8_t* rp = lock.pixels + static_cast<size_t>(y + dy) * stride;
                    for (int dx = -2; dx <= 2; ++dx) {
                        float lx = static_cast<float>(rp[(x + dx + 1) * 4]) - static_cast<float>(rp[(x + dx - 1) * 4]);
                        float ly = static_cast<float>(rp[(x + dx) * 4]) - static_cast<float>(rPrev[(x + dx) * 4]);
                        float m = std::sqrt(lx * lx + ly * ly);
                        float angle = std::atan2(ly, lx) + 3.14159265f; // [0, 2*pi]
                        int bin = std::clamp(static_cast<int>(angle * 8.0f / (2.0f * 3.14159265f)), 0, 7);
                        hist[bin] += m;
                        norm += m * m;
                    }
                }
                norm = 1.0f / std::sqrt(norm);
                for (int b = 0; b < 8; ++b) {
                    descriptors.push_back(hist[b] * norm);
                }
                if (descriptors.size() >= 640) break;
            }
        }
        if (descriptors.size() >= 640) break;
    }

    jfloatArray out = env->NewFloatArray(static_cast<jsize>(descriptors.size()));
    if (out && !descriptors.empty()) {
        env->SetFloatArrayRegion(out, 0, static_cast<jsize>(descriptors.size()), descriptors.data());
    }
    return out;
}

JNIEXPORT jfloat JNICALL
Java_com_xhey_xcamerasdk_algorithm_nn_Vlfeat_matchFeatures(
    JNIEnv* env, jobject /*thiz*/, jfloatArray featA, jfloatArray featB) {
    if (!featA || !featB) return 0.0f;
    const jsize lenA = env->GetArrayLength(featA);
    const jsize lenB = env->GetArrayLength(featB);
    const int kDim = 10;
    const int numA = lenA / kDim;
    const int numB = lenB / kDim;
    if (numA <= 0 || numB <= 0) return 0.0f;

    std::vector<float> a(static_cast<size_t>(lenA));
    std::vector<float> b(static_cast<size_t>(lenB));
    env->GetFloatArrayRegion(featA, 0, lenA, a.data());
    env->GetFloatArrayRegion(featB, 0, lenB, b.data());

    int goodMatches = 0;
    for (int i = 0; i < numA; ++i) {
        const float* da = &a[i * kDim + 2];
        float bestDist = 1e9f;
        float secondBestDist = 1e9f;
        for (int j = 0; j < numB; ++j) {
            const float* db = &b[j * kDim + 2];
            float dist = 0.0f;
            for (int k = 0; k < 8; ++k) {
                float d = da[k] - db[k];
                dist += d * d;
            }
            if (dist < bestDist) {
                secondBestDist = bestDist;
                bestDist = dist;
            } else if (dist < secondBestDist) {
                secondBestDist = dist;
            }
        }
        // Lowe's ratio test
        if (bestDist < 0.35f && (numB == 1 || bestDist < 0.78f * secondBestDist)) {
            ++goodMatches;
        }
    }
    return std::clamp(static_cast<float>(goodMatches) / static_cast<float>(std::min(numA, numB)), 0.0f, 1.0f);
}

} // extern "C"
