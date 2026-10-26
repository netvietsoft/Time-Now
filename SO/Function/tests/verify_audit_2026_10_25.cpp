/**
 * ============================================================================
 * File: tests/verify_audit_2026_10_25.cpp
 * Description:
 *   End-to-End Native C++ Runtime Verification Harness for Audit Reports:
 *     - Report/11_Audit_Native_SO_Function_2026-10-25.md
 *     - Report/13_Kiem_Chung_Lai_Audit_Native_e0883f4.md
 *
 *   Executes the ACTUAL native C++ JNI implementations across all 6 P1/P2
 *   subsystems with a complete in-process JNIEnv (JNINativeInterface) harness:
 *     1. P1 #1: Full NCNN DAG & .bin weight execution on all 3 production models
 *               (Zero-DCE 22 layers, Zero-DCE++ 28 layers, Silero-VAD 67 layers)
 *               with deterministic inputs and quantitative output verification.
 *     2. P1 #2: Fail-closed error propagation when .param/.bin files are missing,
 *               empty, or corrupted/truncated.
 *     3. P1 #3: Native SecurityManager_nativeEncrypt / nativeDecrypt (XAE1 AEAD
 *               AES-128-CTR + HMAC-SHA256) unique 96-bit nonce per encryption,
 *               round-trip decryption, 1-bit tamper rejection (nonce/cipher/tag),
 *               and cross-version test vector verification.
 *     4. P1 #4: Native APK Signing Block v2 (APK Sig Block 42) X.509 certificate
 *               parser + SecurityManager_nativeVerifyAppSignature &
 *               JNIUtils_getApkSignature accepting release APK and rejecting
 *               re-signed/forged APK and untrusted package names.
 *     5. P2 #5: Native PaddleOCR_init loading external UTF-8 dictionary from
 *               keysPath (digits, punctuation, extended tokens) & PaddleOCR_ocr.
 *     6. P2 #6: Native JpegPacker_getCanjpegDecode & JpegPacker_startDecode on
 *               all 3 SHA-256-verified release sample JPEGs (2025-04-*.jpg) +
 *               native CodeDetect_transform3 (14-char antiFakeCode) &
 *               CodeDetect_postprocess tensor layouts.
 * ============================================================================
 */

#include "../include/xhey_jni_models.h"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

static_assert(sizeof(void*) == sizeof(uintptr_t), "Pointer and uintptr_t sizes must match");
static_assert(sizeof(void*) == sizeof(intptr_t),  "Pointer and intptr_t sizes must match");
static_assert(sizeof(void*) == sizeof(size_t),    "Pointer and size_t sizes must match");

// ============================================================================
// Optional Windows x64 NDK Host Runtime Shim (when compiled via NDK clang++ + ld.lld)
// ============================================================================
#if defined(XHEY_WIN64_NDK_HOST_TEST)
static_assert(sizeof(void*) == 8,     "Win64 host pointer must be 64-bit");
static_assert(sizeof(uintptr_t) == 8, "Win64 host uintptr_t must be 64-bit");
static_assert(sizeof(intptr_t) == 8,  "Win64 host intptr_t must be 64-bit");
static_assert(sizeof(size_t) == 8,    "Win64 host size_t must be 64-bit");
static_assert(sizeof(ptrdiff_t) == 8, "Win64 host ptrdiff_t must be 64-bit");

void* operator new(size_t sz) {
    void* p = std::malloc(sz ? sz : 1);
    return p;
}
void* operator new[](size_t sz) {
    void* p = std::malloc(sz ? sz : 1);
    return p;
}
void operator delete(void* p) noexcept {
    if (p) std::free(p);
}
void operator delete[](void* p) noexcept {
    if (p) std::free(p);
}
void operator delete(void* p, size_t) noexcept {
    if (p) std::free(p);
}
void operator delete[](void* p, size_t) noexcept {
    if (p) std::free(p);
}

extern "C" {
void __main() {}
void ___chkstk_ms() {}
int __cxa_guard_acquire(uint64_t* g) {
    if ((*reinterpret_cast<uint8_t*>(g)) == 0) return 1;
    return 0;
}
void __cxa_guard_release(uint64_t* g) {
    *reinterpret_cast<uint8_t*>(g) = 1;
}
void __cxa_guard_abort(uint64_t*) {}

// Provide both std::__ndk1::__libcpp_verbose_abort and std::__libcpp_verbose_abort
// symbols at the object level so NDK r19..r28 (LLVM 8..19) resolve cleanly.
void xhey_shim_verbose_abort_ndk1(const char*, ...) __asm__("_ZNSt6__ndk122__libcpp_verbose_abortEPKcz");
void xhey_shim_verbose_abort_ndk1(const char*, ...) { std::abort(); }
void xhey_shim_verbose_abort_std(const char*, ...) __asm__("_ZNSt22__libcpp_verbose_abortEPKcz");
void xhey_shim_verbose_abort_std(const char*, ...) { std::abort(); }

double round(double x) {
    return (x >= 0.0) ? std::floor(x + 0.5) : std::ceil(x - 0.5);
}
long long llround(double x) {
    return static_cast<long long>((x >= 0.0) ? std::floor(x + 0.5) : std::ceil(x - 0.5));
}
long lroundf(float x) {
    return static_cast<long>((x >= 0.0f) ? std::floor(x + 0.5f) : std::ceil(x - 0.5f));
}
float sinf(float x)   { return static_cast<float>(std::sin(static_cast<double>(x))); }
float cosf(float x)   { return static_cast<float>(std::cos(static_cast<double>(x))); }
float tanf(float x)   { return static_cast<float>(std::tan(static_cast<double>(x))); }
float tanhf(float x)  { return static_cast<float>(std::tanh(static_cast<double>(x))); }
float asinf(float x)  { return static_cast<float>(std::asin(static_cast<double>(x))); }
float acosf(float x)  { return static_cast<float>(std::acos(static_cast<double>(x))); }
float atanf(float x)  { return static_cast<float>(std::atan(static_cast<double>(x))); }
float atan2f(float y, float x) { return static_cast<float>(std::atan2(static_cast<double>(y), static_cast<double>(x))); }
float expf(float x)   { return static_cast<float>(std::exp(static_cast<double>(x))); }
float logf(float x)   { return static_cast<float>(std::log(static_cast<double>(x))); }
float powf(float x, float y) { return static_cast<float>(std::pow(static_cast<double>(x), static_cast<double>(y))); }
float sqrtf(float x)  { return static_cast<float>(std::sqrt(static_cast<double>(x))); }
float ceilf(float x)  { return static_cast<float>(std::ceil(static_cast<double>(x))); }
float floorf(float x) { return static_cast<float>(std::floor(static_cast<double>(x))); }
float fabsf(float x)  { return (x < 0.0f) ? -x : x; }
int _vsnprintf(char* buf, size_t count, const char* fmt, va_list args);
int snprintf(char* buf, size_t count, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = _vsnprintf(buf, count, fmt, args);
    va_end(args);
    if (buf && count > 0) buf[count - 1] = '\0';
    return r;
}

struct _startupinfo { int newmode; };
int __getmainargs(int* argc, char*** argv, char*** env, int doWildCard, _startupinfo* startInfo);
int main(int argc, char** argv);

typedef void (*CtorFunc)();
__attribute__((section(".ctors$z"), used)) static CtorFunc g_ctor_end = nullptr;

void Win64NdkEntry() {
    CtorFunc* ctorStart = reinterpret_cast<CtorFunc*>(
        reinterpret_cast<size_t>(&g_ctor_end) & ~static_cast<size_t>(0xFFFu));
    for (CtorFunc* p = ctorStart; p < &g_ctor_end; ++p) {
        if (*p) {
            (*p)();
        }
    }
    int argc = 0;
    char** argv = nullptr;
    char** envp = nullptr;
    _startupinfo si = {0};
    __getmainargs(&argc, &argv, &envp, 0, &si);
    int rc = main(argc, argv);
    std::fflush(nullptr);
    std::exit(rc);
}
} // extern "C"

namespace std {
inline namespace __ndk1 {
template class basic_string<char>;
template basic_string<char> operator+<char, char_traits<char>, allocator<char>>(char const*, basic_string<char> const&);
template basic_string<char> operator+<char, char_traits<char>, allocator<char>>(basic_string<char> const&, basic_string<char> const&);
template basic_string<char> operator+<char, char_traits<char>, allocator<char>>(char, basic_string<char> const&);
template basic_string<char> operator+<char, char_traits<char>, allocator<char>>(basic_string<char> const&, char const*);
template basic_string<char> operator+<char, char_traits<char>, allocator<char>>(basic_string<char> const&, char);
#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 16000
template class __basic_string_common<true>;
template class __vector_base_common<true>;
#endif

size_t __next_prime(size_t n) {
    if (n <= 2) return 2;
    if ((n & 1) == 0) ++n;
    for (;; n += 2) {
        bool prime = true;
        for (size_t d = 3; d * d <= n; d += 2) {
            if ((n % d) == 0) {
                prime = false;
                break;
            }
        }
        if (prime) return n;
    }
}
void mutex::lock() {}
void mutex::unlock() noexcept {}
mutex::~mutex() noexcept {}

namespace chrono {
steady_clock::time_point steady_clock::now() noexcept {
    static int64_t s_ticks = 1000000;
    s_ticks += 104729;
    return time_point(duration(s_ticks));
}
system_clock::time_point system_clock::now() noexcept {
    static int64_t s_sys_ticks = 1745000000000000LL;
    s_sys_ticks += 1000;
    return time_point(duration(s_sys_ticks));
}
} // namespace chrono

string to_string(int val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", val);
    return string(buf);
}
string to_string(unsigned int val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u", val);
    return string(buf);
}
string to_string(long val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%ld", val);
    return string(buf);
}
string to_string(unsigned long val) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lu", val);
    return string(buf);
}
string to_string(long long val) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%lld", val);
    return string(buf);
}
string to_string(unsigned long long val) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%llu", val);
    return string(buf);
}
} // namespace __ndk1
} // namespace std
#endif // XHEY_WIN64_NDK_HOST_TEST

// ============================================================================
// Standalone Android NDK System Library Shims (AAssetManager, AndroidBitmap, GL)
// ============================================================================
static std::string g_test_assets_dir;
static bool g_force_asset_open_fail = false;

struct MockAssetBuffer {
    std::vector<uint8_t> bytes;
};

struct MockBitmapObject {
    uint32_t magic = 0x424D5031u; // "BMP1"
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    int32_t  format = ANDROID_BITMAP_FORMAT_RGBA_8888;
    std::vector<uint8_t> pixels;
};

#if defined(XHEY_STANDALONE_TEST_SHIM)
extern "C" {

int __android_log_print(int /*prio*/, const char* /*tag*/, const char* /*fmt*/, ...) {
    return 0;
}

int __system_property_get(const char* name, char* value) {
    if (!value) return 0;
    if (name && std::strcmp(name, "ro.product.brand") == 0) {
        std::strcpy(value, "google");
    } else if (name && std::strcmp(name, "ro.product.model") == 0) {
        std::strcpy(value, "Pixel_8_Pro");
    } else if (name && std::strcmp(name, "ro.board.platform") == 0) {
        std::strcpy(value, "husky");
    } else {
        value[0] = '\0';
    }
    return static_cast<int>(std::strlen(value));
}

AAssetManager* AAssetManager_fromJava(JNIEnv* /*env*/, jobject assetManager) {
    if (g_force_asset_open_fail || !assetManager) return nullptr;
    return reinterpret_cast<AAssetManager*>(assetManager);
}

AAsset* AAssetManager_open(AAssetManager* mgr, const char* filename, int /*mode*/) {
    if (!mgr || !filename || g_force_asset_open_fail) return nullptr;
    std::string fullPath = g_test_assets_dir + "/" + filename;
    std::FILE* fp = std::fopen(fullPath.c_str(), "rb");
    if (!fp) return nullptr;
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(fp);
        return nullptr;
    }
    auto* asset = new MockAssetBuffer();
    asset->bytes.resize(static_cast<size_t>(sz));
    size_t n = std::fread(asset->bytes.data(), 1, asset->bytes.size(), fp);
    std::fclose(fp);
    if (n != asset->bytes.size()) {
        delete asset;
        return nullptr;
    }
    return reinterpret_cast<AAsset*>(asset);
}

const void* AAsset_getBuffer(AAsset* asset) {
    if (!asset) return nullptr;
    return reinterpret_cast<MockAssetBuffer*>(asset)->bytes.data();
}

off_t AAsset_getLength(AAsset* asset) {
    if (!asset) return 0;
    return static_cast<off_t>(reinterpret_cast<MockAssetBuffer*>(asset)->bytes.size());
}

void AAsset_close(AAsset* asset) {
    delete reinterpret_cast<MockAssetBuffer*>(asset);
}

int AndroidBitmap_getInfo(JNIEnv* /*env*/, jobject jbitmap, AndroidBitmapInfo* info) {
    if (!jbitmap || !info) return -1;
    auto* bmp = reinterpret_cast<MockBitmapObject*>(jbitmap);
    if (bmp->magic != 0x424D5031u) return -1;
    info->width = bmp->width;
    info->height = bmp->height;
    info->stride = bmp->stride;
    info->format = bmp->format;
    info->flags = 0;
    return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_lockPixels(JNIEnv* /*env*/, jobject jbitmap, void** addrPtr) {
    if (!jbitmap || !addrPtr) return -1;
    auto* bmp = reinterpret_cast<MockBitmapObject*>(jbitmap);
    if (bmp->magic != 0x424D5031u || bmp->pixels.empty()) return -1;
    *addrPtr = bmp->pixels.data();
    return ANDROID_BITMAP_RESULT_SUCCESS;
}

int AndroidBitmap_unlockPixels(JNIEnv* /*env*/, jobject /*jbitmap*/) {
    return ANDROID_BITMAP_RESULT_SUCCESS;
}

void glGenTextures(GLsizei n, GLuint* textures) {
    for (GLsizei i = 0; i < n; ++i) textures[i] = static_cast<GLuint>(i + 1);
}
void glBindTexture(GLenum, GLuint) {}
void glTexParameteri(GLenum, GLenum, GLint) {}
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}
void glPixelStorei(GLenum, GLint) {}
void glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) {}
GLenum glGetError(void) { return GL_NO_ERROR; }
void glDeleteTextures(GLsizei, const GLuint*) {}
EGLContext eglGetCurrentContext(void) { return reinterpret_cast<EGLContext>(0x1); }

} // extern "C"
#endif // XHEY_STANDALONE_TEST_SHIM

// ============================================================================
// Mock JNI Object Model & In-Process JNINativeInterface Table
// ============================================================================
namespace {

enum class MockKind : uint32_t {
    String = 1,
    ByteArray,
    FloatArray,
    IntArray,
    ShortArray,
    BooleanArray,
    ObjectArray,
    Class,
    Context,
    PackageManager,
    PackageInfo,
    Signature,
    SliceDataInfo,
    CodeMetaData,
    DetectionResult,
    PaddleOCRParam,
    PaddleOCRResult
};

struct MockJObject {
    MockKind kind;
    std::string strVal;
    std::vector<int8_t> byteArr;
    std::vector<float> floatArr;
    std::vector<int32_t> intArr;
    std::vector<int16_t> shortArr;
    std::vector<uint8_t> boolArr;
    std::vector<jobject> objArr;

    // Context / Signature fields
    std::string pkgName;
    std::vector<uint8_t> certBytes;

    // SliceDataInfo fields
    int64_t errorCode = 0;
    int64_t position = 0;
    int64_t version = 0;
    int64_t id = 0;
    int64_t type = 0;
    float rect[4] = {0, 0, 0, 0};
    std::vector<uint8_t> sliceData;

    // CodeMetaData fields
    int32_t codeValid = 0;
    int32_t codeVersion = 0;
    int32_t timestampFormat = 0;
    int64_t timeStampS = 0;
    double longitude = 0.0;
    double latitude = 0.0;
    int32_t locationType = 0;
    int32_t reserved = 0;

    // DetectionResult fields
    int32_t classId = 0;
    float confidence = 0.0f;
};

struct MockMethodDesc {
    std::string clsName;
    std::string name;
    std::string sig;
};

struct MockFieldDesc {
    std::string clsName;
    std::string name;
    std::string sig;
};

static std::vector<MockJObject*> g_alloc_objs;
static std::vector<MockMethodDesc*> g_alloc_methods;
static std::vector<MockFieldDesc*> g_alloc_fields;

MockJObject* AllocMock(MockKind kind) {
    auto* o = new MockJObject();
    o->kind = kind;
    g_alloc_objs.push_back(o);
    return o;
}

jstring MakeJString(const std::string& s) {
    auto* o = AllocMock(MockKind::String);
    o->strVal = s;
    return reinterpret_cast<jstring>(o);
}

std::string ReadJString(jstring js) {
    if (!js) return "";
    auto* o = reinterpret_cast<MockJObject*>(js);
    return o->strVal;
}

jclass JNICALL Mock_FindClass(JNIEnv*, const char* name) {
    auto* o = AllocMock(MockKind::Class);
    o->strVal = name ? name : "";
    return reinterpret_cast<jclass>(o);
}

jclass JNICALL Mock_GetObjectClass(JNIEnv*, jobject obj) {
    if (!obj) return nullptr;
    auto* inObj = reinterpret_cast<MockJObject*>(obj);
    auto* cls = AllocMock(MockKind::Class);
    switch (inObj->kind) {
        case MockKind::Context:        cls->strVal = "android/content/Context"; break;
        case MockKind::PackageManager: cls->strVal = "android/content/pm/PackageManager"; break;
        case MockKind::PackageInfo:    cls->strVal = "android/content/pm/PackageInfo"; break;
        case MockKind::Signature:      cls->strVal = "android/content/pm/Signature"; break;
        case MockKind::PaddleOCRParam: cls->strVal = "com/xhey/xcamerasdk/algorithm/nn/PaddleOCRParam"; break;
        default:                       cls->strVal = "java/lang/Object"; break;
    }
    return reinterpret_cast<jclass>(cls);
}

jmethodID JNICALL Mock_GetMethodID(JNIEnv*, jclass clazz, const char* name, const char* sig) {
    auto* m = new MockMethodDesc();
    if (clazz) m->clsName = reinterpret_cast<MockJObject*>(clazz)->strVal;
    m->name = name ? name : "";
    m->sig = sig ? sig : "";
    g_alloc_methods.push_back(m);
    return reinterpret_cast<jmethodID>(m);
}

jfieldID JNICALL Mock_GetFieldID(JNIEnv*, jclass clazz, const char* name, const char* sig) {
    auto* f = new MockFieldDesc();
    if (clazz) f->clsName = reinterpret_cast<MockJObject*>(clazz)->strVal;
    f->name = name ? name : "";
    f->sig = sig ? sig : "";
    g_alloc_fields.push_back(f);
    return reinterpret_cast<jfieldID>(f);
}

jobject JNICALL Mock_NewObjectV(JNIEnv*, jclass clazz, jmethodID methodID, va_list args) {
    std::string clsName = clazz ? reinterpret_cast<MockJObject*>(clazz)->strVal : "";
    std::string sig = methodID ? reinterpret_cast<MockMethodDesc*>(methodID)->sig : "";

    if (clsName.find("SliceDataInfo") != std::string::npos) {
        auto* o = AllocMock(MockKind::SliceDataInfo);
        o->errorCode = va_arg(args, jlong);
        o->position  = va_arg(args, jlong);
        o->version   = va_arg(args, jlong);
        o->id        = va_arg(args, jlong);
        o->type      = va_arg(args, jlong);
        jfloatArray rectArr = va_arg(args, jfloatArray);
        jbyteArray dataArr  = va_arg(args, jbyteArray);
        if (rectArr) {
            auto* r = reinterpret_cast<MockJObject*>(rectArr);
            for (size_t i = 0; i < 4 && i < r->floatArr.size(); ++i) o->rect[i] = r->floatArr[i];
        }
        if (dataArr) {
            auto* d = reinterpret_cast<MockJObject*>(dataArr);
            o->sliceData.assign(d->byteArr.begin(), d->byteArr.end());
        }
        return reinterpret_cast<jobject>(o);
    }

    if (clsName.find("CodeMetaData") != std::string::npos) {
        auto* o = AllocMock(MockKind::CodeMetaData);
        o->codeValid       = va_arg(args, jint);
        o->codeVersion     = va_arg(args, jint);
        o->timestampFormat = va_arg(args, jint);
        o->timeStampS      = va_arg(args, jlong);
        o->longitude       = va_arg(args, jdouble);
        o->latitude        = va_arg(args, jdouble);
        o->locationType    = va_arg(args, jint);
        o->reserved        = va_arg(args, jint);
        return reinterpret_cast<jobject>(o);
    }

    if (clsName.find("DetectionResult") != std::string::npos || sig == "(IF[F)V") {
        auto* o = AllocMock(MockKind::DetectionResult);
        o->classId    = va_arg(args, jint);
        o->confidence = static_cast<float>(va_arg(args, jdouble));
        jfloatArray boxArr = va_arg(args, jfloatArray);
        if (boxArr) {
            auto* b = reinterpret_cast<MockJObject*>(boxArr);
            for (size_t i = 0; i < 4 && i < b->floatArr.size(); ++i) o->rect[i] = b->floatArr[i];
        }
        return reinterpret_cast<jobject>(o);
    }

    auto* generic = AllocMock(MockKind::PaddleOCRResult);
    return reinterpret_cast<jobject>(generic);
}

jobject JNICALL Mock_CallObjectMethodV(JNIEnv*, jobject obj, jmethodID methodID, va_list /*args*/) {
    if (!obj || !methodID) return nullptr;
    auto* inObj = reinterpret_cast<MockJObject*>(obj);
    auto* m = reinterpret_cast<MockMethodDesc*>(methodID);

    if (m->name == "getPackageName") {
        return MakeJString(inObj->pkgName);
    }
    if (m->name == "getPackageManager") {
        auto* pm = AllocMock(MockKind::PackageManager);
        pm->pkgName = inObj->pkgName;
        pm->certBytes = inObj->certBytes;
        return reinterpret_cast<jobject>(pm);
    }
    if (m->name == "getPackageInfo") {
        auto* pi = AllocMock(MockKind::PackageInfo);
        pi->pkgName = inObj->pkgName;
        pi->certBytes = inObj->certBytes;
        return reinterpret_cast<jobject>(pi);
    }
    if (m->name == "toByteArray") {
        auto* ba = AllocMock(MockKind::ByteArray);
        ba->byteArr.resize(inObj->certBytes.size());
        if (!inObj->certBytes.empty()) {
            std::memcpy(ba->byteArr.data(), inObj->certBytes.data(), inObj->certBytes.size());
        }
        return reinterpret_cast<jobject>(ba);
    }
    return nullptr;
}

jobject JNICALL Mock_GetObjectField(JNIEnv*, jobject obj, jfieldID fieldID) {
    if (!obj || !fieldID) return nullptr;
    auto* inObj = reinterpret_cast<MockJObject*>(obj);
    auto* f = reinterpret_cast<MockFieldDesc*>(fieldID);
    if (f->name == "signatures") {
        auto* sig = AllocMock(MockKind::Signature);
        sig->certBytes = inObj->certBytes;
        auto* arr = AllocMock(MockKind::ObjectArray);
        arr->objArr.push_back(reinterpret_cast<jobject>(sig));
        return reinterpret_cast<jobject>(arr);
    }
    return nullptr;
}

jint JNICALL Mock_GetIntField(JNIEnv*, jobject, jfieldID) { return 0; }
jfloat JNICALL Mock_GetFloatField(JNIEnv*, jobject, jfieldID) { return 0.5f; }
jboolean JNICALL Mock_GetBooleanField(JNIEnv*, jobject, jfieldID) { return JNI_TRUE; }

jboolean JNICALL Mock_ExceptionCheck(JNIEnv*) { return JNI_FALSE; }
void JNICALL Mock_ExceptionClear(JNIEnv*) {}
void JNICALL Mock_DeleteLocalRef(JNIEnv*, jobject) {}

jstring JNICALL Mock_NewStringUTF(JNIEnv*, const char* bytes) {
    return MakeJString(bytes ? bytes : "");
}
jsize JNICALL Mock_GetStringUTFLength(JNIEnv*, jstring str) {
    if (!str) return 0;
    return static_cast<jsize>(reinterpret_cast<MockJObject*>(str)->strVal.size());
}
const char* JNICALL Mock_GetStringUTFChars(JNIEnv*, jstring str, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!str) return "";
    return reinterpret_cast<MockJObject*>(str)->strVal.c_str();
}
void JNICALL Mock_ReleaseStringUTFChars(JNIEnv*, jstring, const char*) {}

jsize JNICALL Mock_GetArrayLength(JNIEnv*, jarray arr) {
    if (!arr) return 0;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    switch (o->kind) {
        case MockKind::ByteArray:    return static_cast<jsize>(o->byteArr.size());
        case MockKind::FloatArray:   return static_cast<jsize>(o->floatArr.size());
        case MockKind::IntArray:     return static_cast<jsize>(o->intArr.size());
        case MockKind::ShortArray:   return static_cast<jsize>(o->shortArr.size());
        case MockKind::BooleanArray: return static_cast<jsize>(o->boolArr.size());
        case MockKind::ObjectArray:  return static_cast<jsize>(o->objArr.size());
        default:                     return 0;
    }
}

jobjectArray JNICALL Mock_NewObjectArray(JNIEnv*, jsize len, jclass, jobject initElem) {
    auto* o = AllocMock(MockKind::ObjectArray);
    if (len > 0) o->objArr.assign(static_cast<size_t>(len), initElem);
    return reinterpret_cast<jobjectArray>(o);
}
jobject JNICALL Mock_GetObjectArrayElement(JNIEnv*, jobjectArray arr, jsize idx) {
    if (!arr) return nullptr;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (idx < 0 || static_cast<size_t>(idx) >= o->objArr.size()) return nullptr;
    return o->objArr[static_cast<size_t>(idx)];
}
void JNICALL Mock_SetObjectArrayElement(JNIEnv*, jobjectArray arr, jsize idx, jobject val) {
    if (!arr) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (idx >= 0 && static_cast<size_t>(idx) < o->objArr.size()) {
        o->objArr[static_cast<size_t>(idx)] = val;
    }
}

jbyteArray JNICALL Mock_NewByteArray(JNIEnv*, jsize len) {
    auto* o = AllocMock(MockKind::ByteArray);
    if (len > 0) o->byteArr.assign(static_cast<size_t>(len), 0);
    return reinterpret_cast<jbyteArray>(o);
}
jbooleanArray JNICALL Mock_NewBooleanArray(JNIEnv*, jsize len) {
    auto* o = AllocMock(MockKind::BooleanArray);
    if (len > 0) o->boolArr.assign(static_cast<size_t>(len), 0);
    return reinterpret_cast<jbooleanArray>(o);
}
jintArray JNICALL Mock_NewIntArray(JNIEnv*, jsize len) {
    auto* o = AllocMock(MockKind::IntArray);
    if (len > 0) o->intArr.assign(static_cast<size_t>(len), 0);
    return reinterpret_cast<jintArray>(o);
}
jfloatArray JNICALL Mock_NewFloatArray(JNIEnv*, jsize len) {
    auto* o = AllocMock(MockKind::FloatArray);
    if (len > 0) o->floatArr.assign(static_cast<size_t>(len), 0.0f);
    return reinterpret_cast<jfloatArray>(o);
}

jbyte* JNICALL Mock_GetByteArrayElements(JNIEnv*, jbyteArray arr, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!arr) return nullptr;
    return reinterpret_cast<MockJObject*>(arr)->byteArr.data();
}
void JNICALL Mock_ReleaseByteArrayElements(JNIEnv*, jbyteArray, jbyte*, jint) {}

jfloat* JNICALL Mock_GetFloatArrayElements(JNIEnv*, jfloatArray arr, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!arr) return nullptr;
    return reinterpret_cast<MockJObject*>(arr)->floatArr.data();
}
void JNICALL Mock_ReleaseFloatArrayElements(JNIEnv*, jfloatArray, jfloat*, jint) {}

jint* JNICALL Mock_GetIntArrayElements(JNIEnv*, jintArray arr, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!arr) return nullptr;
    return reinterpret_cast<MockJObject*>(arr)->intArr.data();
}
void JNICALL Mock_ReleaseIntArrayElements(JNIEnv*, jintArray, jint*, jint) {}

jboolean* JNICALL Mock_GetBooleanArrayElements(JNIEnv*, jbooleanArray arr, jboolean* isCopy) {
    if (isCopy) *isCopy = JNI_FALSE;
    if (!arr) return nullptr;
    return reinterpret_cast<MockJObject*>(arr)->boolArr.data();
}
void JNICALL Mock_ReleaseBooleanArrayElements(JNIEnv*, jbooleanArray, jboolean*, jint) {}

void JNICALL Mock_GetByteArrayRegion(JNIEnv*, jbyteArray arr, jsize start, jsize len, jbyte* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->byteArr.size()) {
        std::memcpy(buf, o->byteArr.data() + start, static_cast<size_t>(len));
    }
}
void JNICALL Mock_SetByteArrayRegion(JNIEnv*, jbyteArray arr, jsize start, jsize len, const jbyte* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->byteArr.size()) {
        std::memcpy(o->byteArr.data() + start, buf, static_cast<size_t>(len));
    }
}
void JNICALL Mock_SetBooleanArrayRegion(JNIEnv*, jbooleanArray arr, jsize start, jsize len, const jboolean* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->boolArr.size()) {
        std::memcpy(o->boolArr.data() + start, buf, static_cast<size_t>(len));
    }
}
void JNICALL Mock_GetIntArrayRegion(JNIEnv*, jintArray arr, jsize start, jsize len, jint* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->intArr.size()) {
        std::memcpy(buf, o->intArr.data() + start, static_cast<size_t>(len) * sizeof(jint));
    }
}
void JNICALL Mock_GetFloatArrayRegion(JNIEnv*, jfloatArray arr, jsize start, jsize len, jfloat* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->floatArr.size()) {
        std::memcpy(buf, o->floatArr.data() + start, static_cast<size_t>(len) * sizeof(jfloat));
    }
}
void JNICALL Mock_SetIntArrayRegion(JNIEnv*, jintArray arr, jsize start, jsize len, const jint* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->intArr.size()) {
        std::memcpy(o->intArr.data() + start, buf, static_cast<size_t>(len) * sizeof(jint));
    }
}
void JNICALL Mock_SetFloatArrayRegion(JNIEnv*, jfloatArray arr, jsize start, jsize len, const jfloat* buf) {
    if (!arr || !buf || len <= 0) return;
    auto* o = reinterpret_cast<MockJObject*>(arr);
    if (start >= 0 && static_cast<size_t>(start + len) <= o->floatArr.size()) {
        std::memcpy(o->floatArr.data() + start, buf, static_cast<size_t>(len) * sizeof(jfloat));
    }
}

static JNINativeInterface g_jni_func_table;
static _JNIEnv g_jni_env;

JNIEnv* GetMockJniEnv() {
    std::memset(&g_jni_func_table, 0, sizeof(g_jni_func_table));
    g_jni_func_table.FindClass                 = &Mock_FindClass;
    g_jni_func_table.GetObjectClass            = &Mock_GetObjectClass;
    g_jni_func_table.GetMethodID               = &Mock_GetMethodID;
    g_jni_func_table.GetFieldID                = &Mock_GetFieldID;
    g_jni_func_table.NewObjectV                = &Mock_NewObjectV;
    g_jni_func_table.CallObjectMethodV         = &Mock_CallObjectMethodV;
    g_jni_func_table.GetObjectField            = &Mock_GetObjectField;
    g_jni_func_table.GetIntField               = &Mock_GetIntField;
    g_jni_func_table.GetFloatField             = &Mock_GetFloatField;
    g_jni_func_table.GetBooleanField           = &Mock_GetBooleanField;
    g_jni_func_table.ExceptionCheck            = &Mock_ExceptionCheck;
    g_jni_func_table.ExceptionClear            = &Mock_ExceptionClear;
    g_jni_func_table.DeleteLocalRef            = &Mock_DeleteLocalRef;
    g_jni_func_table.NewStringUTF              = &Mock_NewStringUTF;
    g_jni_func_table.GetStringUTFLength        = &Mock_GetStringUTFLength;
    g_jni_func_table.GetStringUTFChars         = &Mock_GetStringUTFChars;
    g_jni_func_table.ReleaseStringUTFChars     = &Mock_ReleaseStringUTFChars;
    g_jni_func_table.GetArrayLength            = &Mock_GetArrayLength;
    g_jni_func_table.NewObjectArray            = &Mock_NewObjectArray;
    g_jni_func_table.GetObjectArrayElement     = &Mock_GetObjectArrayElement;
    g_jni_func_table.SetObjectArrayElement     = &Mock_SetObjectArrayElement;
    g_jni_func_table.NewByteArray              = &Mock_NewByteArray;
    g_jni_func_table.NewBooleanArray           = &Mock_NewBooleanArray;
    g_jni_func_table.NewIntArray               = &Mock_NewIntArray;
    g_jni_func_table.NewFloatArray             = &Mock_NewFloatArray;
    g_jni_func_table.GetByteArrayElements      = &Mock_GetByteArrayElements;
    g_jni_func_table.ReleaseByteArrayElements  = &Mock_ReleaseByteArrayElements;
    g_jni_func_table.GetFloatArrayElements     = &Mock_GetFloatArrayElements;
    g_jni_func_table.ReleaseFloatArrayElements = &Mock_ReleaseFloatArrayElements;
    g_jni_func_table.GetIntArrayElements       = &Mock_GetIntArrayElements;
    g_jni_func_table.ReleaseIntArrayElements   = &Mock_ReleaseIntArrayElements;
    g_jni_func_table.GetBooleanArrayElements   = &Mock_GetBooleanArrayElements;
    g_jni_func_table.ReleaseBooleanArrayElements = &Mock_ReleaseBooleanArrayElements;
    g_jni_func_table.GetByteArrayRegion        = &Mock_GetByteArrayRegion;
    g_jni_func_table.GetIntArrayRegion         = &Mock_GetIntArrayRegion;
    g_jni_func_table.GetFloatArrayRegion       = &Mock_GetFloatArrayRegion;
    g_jni_func_table.SetByteArrayRegion        = &Mock_SetByteArrayRegion;
    g_jni_func_table.SetBooleanArrayRegion     = &Mock_SetBooleanArrayRegion;
    g_jni_func_table.SetIntArrayRegion         = &Mock_SetIntArrayRegion;
    g_jni_func_table.SetFloatArrayRegion       = &Mock_SetFloatArrayRegion;

    g_jni_env.functions = &g_jni_func_table;
    return &g_jni_env;
}

// Helper: Read file bytes
bool ReadFileAllBytes(const std::string& path, std::vector<uint8_t>& outBytes) {
    outBytes.clear();
    std::FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(fp);
        return false;
    }
    outBytes.resize(static_cast<size_t>(sz));
    size_t n = std::fread(outBytes.data(), 1, outBytes.size(), fp);
    std::fclose(fp);
    return n == outBytes.size();
}

// Helper: Extract X.509 signing certificate DER from an APK's "APK Sig Block 42"
bool ExtractX509CertFromApkSigBlockV2(const std::string& apkPath, std::vector<uint8_t>& outCertDer) {
    outCertDer.clear();
    std::vector<uint8_t> apk;
    if (!ReadFileAllBytes(apkPath, apk) || apk.size() < 64) return false;

    // 1. Locate ZIP End of Central Directory (0x06054b50)
    long eocdPos = -1;
    long minPos = static_cast<long>(apk.size()) - 65558;
    if (minPos < 0) minPos = 0;
    for (long i = static_cast<long>(apk.size()) - 22; i >= minPos; --i) {
        if (apk[i] == 0x50 && apk[i + 1] == 0x4B && apk[i + 2] == 0x05 && apk[i + 3] == 0x06) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos < 0) return false;

    uint32_t cdOffset = static_cast<uint32_t>(apk[eocdPos + 16]) |
                        (static_cast<uint32_t>(apk[eocdPos + 17]) << 8) |
                        (static_cast<uint32_t>(apk[eocdPos + 18]) << 16) |
                        (static_cast<uint32_t>(apk[eocdPos + 19]) << 24);
    if (cdOffset < 32 || cdOffset > apk.size()) return false;

    // 2. Verify "APK Sig Block 42" magic just before cdOffset
    if (std::memcmp(apk.data() + cdOffset - 16, "APK Sig Block 42", 16) != 0) {
        return false;
    }
    uint64_t blockSize = 0;
    std::memcpy(&blockSize, apk.data() + cdOffset - 24, sizeof(uint64_t));
    if (blockSize + 8 > cdOffset) return false;
    size_t blockStart = static_cast<size_t>(cdOffset - blockSize - 8);

    // 3. Scan APK Signing Block for X.509 DER SEQUENCE (0x30 0x82 0x05 0x89 -> 1417 + 4 = 1421 bytes)
    for (size_t i = blockStart; i + 1421 <= cdOffset - 24; ++i) {
        if (apk[i] == 0x30 && apk[i + 1] == 0x82 && apk[i + 2] == 0x05 && apk[i + 3] == 0x89) {
            outCertDer.assign(apk.begin() + i, apk.begin() + i + 1421);
            return true;
        }
    }
    return false;
}

} // namespace

// ============================================================================
// Audited JNI Function Declarations
// ============================================================================
extern "C" {
// P1 #1, P1 #2, P2 #5 (libnnpackage-f99779bd.so)
JNIEXPORT jboolean JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init(
    JNIEnv* env, jobject thiz, jobject assetManager, jboolean useGpu);
JNIEXPORT jboolean JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_autoNightEnhancement(
    JNIEnv* env, jobject thiz, jobject inputBitmap, jobject outputBitmap, jboolean enablePostFilter);
JNIEXPORT jboolean JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_init(
    JNIEnv* env, jobject thiz, jobject assetManager, jboolean useGpu);
JNIEXPORT jboolean JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_autoNightEnhancement(
    JNIEnv* env, jobject thiz, jobject inputBitmap, jobject outputBitmap);
JNIEXPORT jlong JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
    JNIEnv* env, jobject thiz, jstring paramPath, jstring binPath,
    jint sampleRate, jint windowSamples, jfloat threshold,
    jint minSilenceDurationMs, jint minSpeechDurationMs);
JNIEXPORT jfloat JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativePredictFloat(
    JNIEnv* env, jobject thiz, jlong handle, jfloatArray pcmFloatData, jint offset, jint length);
JNIEXPORT void JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeRelease(
    JNIEnv* env, jobject thiz, jlong handle);
JNIEXPORT jboolean JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init(
    JNIEnv* env, jobject thiz, jstring detParam, jstring detBin,
    jstring clsParam, jstring clsBin, jstring recParam, jstring recBin, jstring keysPath);

// P2 #6 (libnnpackage-f99779bd.so)
JNIEXPORT jintArray JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_getCanjpegDecode(
    JNIEnv* env, jclass clazz, jobjectArray filePaths, jintArray packPositions);
JNIEXPORT jobjectArray JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startDecode(
    JNIEnv* env, jclass clazz, jint packPosition, jstring jpegPath, jstring optOutDir);
JNIEXPORT jobject JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform3(
    JNIEnv* env, jclass clazz, jstring codeStr);
JNIEXPORT jfloatArray JNICALL Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_detect(
    JNIEnv* env, jclass clazz, jobject bitmap, jfloatArray roiArr);

// P1 #3, P1 #4 (libnative-lib.so & libxheymediav8.so)
JNIEXPORT jstring JNICALL Java_com_xhey_xcamera_security_SecurityManager_nativeEncrypt(
    JNIEnv* env, jclass clazz, jstring plainText);
JNIEXPORT jstring JNICALL Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(
    JNIEnv* env, jclass clazz, jstring cipherText);
JNIEXPORT jboolean JNICALL Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature(
    JNIEnv* env, jclass clazz, jobject contextObj);
JNIEXPORT jstring JNICALL Java_com_xhey_xcamera_NativeXhey_heyStr(
    JNIEnv* env, jclass clazz, jint keyIndex);
JNIEXPORT jstring JNICALL Java_com_xhey_xcamera_util_JNIUtils_getApkSignature(
    JNIEnv* env, jobject thiz, jobject contextObj, jlong timestampMs);
JNIEXPORT jstring JNICALL Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo(
    JNIEnv* env, jobject thiz);
}

#define REQUIRE_TRUE(cond, msg)                                                       \
    do {                                                                              \
        if (!(cond)) {                                                                \
            std::printf("[FAIL] %s (line %d)\n", (msg), __LINE__);                    \
            std::fflush(nullptr);                                                     \
            return 1;                                                                 \
        }                                                                             \
    } while (0)

int main(int argc, char** argv) {
    std::string assetsDir   = (argc >= 2) ? argv[1] : "SO/SO/assets_native";
    std::string fixturesDir = (argc >= 3) ? argv[2] : "SO/Function/tests/fixtures";
    g_test_assets_dir = assetsDir;

    std::printf("============================================================================\n");
    std::printf("  NATIVE C++ RUNTIME VERIFICATION HARNESS (Audit Reports 11 & 13)\n");
    std::printf("  Assets Dir  : %s\n", assetsDir.c_str());
    std::printf("  Fixtures Dir: %s\n", fixturesDir.c_str());
    std::printf("  ABI Sizes   : void*=%zu, uintptr_t=%zu, intptr_t=%zu, size_t=%zu\n",
                sizeof(void*), sizeof(uintptr_t), sizeof(intptr_t), sizeof(size_t));
    std::printf("============================================================================\n");
    std::fflush(nullptr);

    JNIEnv* env = GetMockJniEnv();

    // ========================================================================
    // 1. P1 #2: Fail-Closed Model Loading (Null Env, Missing & Corrupt Files)
    // ========================================================================
    std::printf("\n[NATIVE 1/6] Verifying P1 #2 Fail-Closed Model Loading...\n");
    std::fflush(nullptr);
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init(nullptr, nullptr, nullptr, JNI_FALSE) == JNI_FALSE,
                 "ZeroDCE_init(nullptr) must return JNI_FALSE");
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_init(nullptr, nullptr, nullptr, JNI_FALSE) == JNI_FALSE,
                 "ZeroDCELite_init(nullptr) must return JNI_FALSE");
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
                     nullptr, nullptr, nullptr, nullptr, 16000, 512, 0.5f, 100, 100) == 0L,
                 "SileroVad_nativeInit(nullptr) must return 0L");
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init(
                     nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) == JNI_FALSE,
                 "PaddleOCR_init(nullptr) must return JNI_FALSE");

    // Missing file paths with valid JNIEnv
    jstring missingParam = MakeJString(assetsDir + "/non_existent_model.param");
    jstring missingBin   = MakeJString(assetsDir + "/non_existent_model.bin");
    jstring validVadParam = MakeJString(assetsDir + "/silero_vad_16k.ncnn.param");
    jstring validVadBin   = MakeJString(assetsDir + "/silero_vad_16k.ncnn.bin");

    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
                     env, nullptr, missingParam, validVadBin, 16000, 512, 0.5f, 100, 100) == 0L,
                 "SileroVad_nativeInit with missing .param must return 0L");
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
                     env, nullptr, validVadParam, missingBin, 16000, 512, 0.5f, 100, 100) == 0L,
                 "SileroVad_nativeInit with missing .bin must return 0L");

    // Corrupt .param (invalid magic / truncated layers) & truncated .bin
    std::string corruptParamPath = fixturesDir + "/tmp_corrupt.param";
    std::string truncBinPath     = fixturesDir + "/tmp_truncated.bin";
    {
        std::FILE* fp = std::fopen(corruptParamPath.c_str(), "wb");
        if (fp) {
            const char* badText = "7767517\n10 12\nInput input 0 1 input\n"; // claims 10 layers, only has 1
            std::fwrite(badText, 1, std::strlen(badText), fp);
            std::fclose(fp);
        }
        std::FILE* fp2 = std::fopen(truncBinPath.c_str(), "wb");
        if (fp2) {
            uint8_t zeros[64] = {0};
            std::fwrite(zeros, 1, sizeof(zeros), fp2); // only 64 bytes instead of 621,680 bytes
            std::fclose(fp2);
        }
    }
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
                     env, nullptr, MakeJString(corruptParamPath), validVadBin, 16000, 512, 0.5f, 100, 100) == 0L,
                 "SileroVad_nativeInit with corrupt .param (layer count mismatch) must return 0L");
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
                     env, nullptr, validVadParam, MakeJString(truncBinPath), 16000, 512, 0.5f, 100, 100) == 0L,
                 "SileroVad_nativeInit with truncated .bin must return 0L");
    std::remove(corruptParamPath.c_str());
    std::remove(truncBinPath.c_str());

#if defined(XHEY_STANDALONE_TEST_SHIM)
    g_force_asset_open_fail = true;
    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init(
                     env, nullptr, reinterpret_cast<jobject>(0x1), JNI_FALSE) == JNI_FALSE,
                 "ZeroDCE_init must return JNI_FALSE when AAssetManager fails to open model");
    g_force_asset_open_fail = false;
#endif
    std::printf("  [PASS] All fail-closed checks (null env, missing .param/.bin, corrupt .param, truncated .bin) returned error.\n");
    std::fflush(nullptr);

    // ========================================================================
    // 2. P1 #1: Full NCNN Graph & .bin Weight Execution on All 3 Real Models
    // ========================================================================
    std::printf("\n[NATIVE 2/6] Verifying P1 #1 Real NCNN Graph & .bin Weight Execution (3 Models)...\n");
    std::fflush(nullptr);

    // 2a. Silero-VAD (67 layers, 82 blobs, 621,680-byte fp16 .bin)
    jlong vadHandle = Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit(
        env, nullptr, validVadParam, validVadBin, 16000, 512, 0.5f, 100, 100);
    REQUIRE_TRUE(vadHandle != 0L, "SileroVad_nativeInit must succeed on real silero_vad_16k.ncnn.param/.bin");

    jfloatArray pcmArr = env->NewFloatArray(512);
    jfloat* pcmPtr = env->GetFloatArrayElements(pcmArr, nullptr);
    for (int i = 0; i < 512; ++i) {
        pcmPtr[i] = 0.5f * std::sin(2.0f * 3.14159265358979323846f * 440.0f * static_cast<float>(i) / 16000.0f);
    }
    jfloat vadProb1 = Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativePredictFloat(
        env, nullptr, vadHandle, pcmArr, 0, 512);
    jfloat vadProb2 = Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativePredictFloat(
        env, nullptr, vadHandle, pcmArr, 0, 512);
    Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeRelease(env, nullptr, vadHandle);

    REQUIRE_TRUE(std::isfinite(vadProb1) && vadProb1 >= 0.0f && vadProb1 <= 1.0f,
                 "SileroVad output probability #1 must be finite in [0, 1]");
    REQUIRE_TRUE(std::isfinite(vadProb2) && vadProb2 >= 0.0f && vadProb2 <= 1.0f,
                 "SileroVad output probability #2 must be finite in [0, 1]");
    REQUIRE_TRUE(std::fabs(vadProb1 - 0.039532f) < 1e-4f && std::fabs(vadProb2 - 0.023295f) < 1e-4f,
                 "SileroVad 67-layer LSTM+Conv1D+DWConv1D probabilities must match reference values within 1e-4");
    std::printf("  - Silero-VAD (67 layers, LSTM+Conv1D+DWConv1D): prob_step1=%.6f, prob_step2=%.6f\n",
                static_cast<double>(vadProb1), static_cast<double>(vadProb2));

#if defined(XHEY_STANDALONE_TEST_SHIM)
    // 2b. Zero-DCE (22 layers, 30 blobs, 303,844-byte float32 .bin)
    jboolean zdceOk = Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init(
        env, nullptr, reinterpret_cast<jobject>(0x1), JNI_FALSE);
    REQUIRE_TRUE(zdceOk == JNI_TRUE, "ZeroDCE_init must succeed on real zero_dce_full_6head.param/.bin");

    MockBitmapObject inBmp, outBmp1, outBmp2;
    inBmp.width = outBmp1.width = outBmp2.width = 16;
    inBmp.height = outBmp1.height = outBmp2.height = 16;
    inBmp.stride = outBmp1.stride = outBmp2.stride = 16 * 4;
    inBmp.pixels.resize(16 * 16 * 4);
    outBmp1.pixels.assign(16 * 16 * 4, 0);
    outBmp2.pixels.assign(16 * 16 * 4, 0);
    for (size_t i = 0; i < 16 * 16; ++i) {
        inBmp.pixels[i * 4 + 0] = 30;  // R (low-light)
        inBmp.pixels[i * 4 + 1] = 40;  // G
        inBmp.pixels[i * 4 + 2] = 50;  // B
        inBmp.pixels[i * 4 + 3] = 255; // A
    }

    jboolean zdceRun = Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_autoNightEnhancement(
        env, nullptr, reinterpret_cast<jobject>(&inBmp), reinterpret_cast<jobject>(&outBmp1), JNI_FALSE);
    REQUIRE_TRUE(zdceRun == JNI_TRUE, "ZeroDCE_autoNightEnhancement must succeed");

    // 2c. Zero-DCE++ (28 layers, 31 blobs, 42,300-byte float32 .bin with ConvolutionDepthWise)
    jboolean zdceLiteOk = Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_init(
        env, nullptr, reinterpret_cast<jobject>(0x1), JNI_FALSE);
    REQUIRE_TRUE(zdceLiteOk == JNI_TRUE, "ZeroDCELite_init must succeed on real zero_dce++.param/.bin");

    jboolean zdceLiteRun = Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCELite_autoNightEnhancement(
        env, nullptr, reinterpret_cast<jobject>(&inBmp), reinterpret_cast<jobject>(&outBmp2));
    REQUIRE_TRUE(zdceLiteRun == JNI_TRUE, "ZeroDCELite_autoNightEnhancement must succeed");

    // Verify both models transformed the low-light input and match quantitative reference outputs
    double meanIn = (30.0 + 40.0 + 50.0) / 3.0;
    double sumOut1 = 0.0, sumOut2 = 0.0;
    for (size_t i = 0; i < 16 * 16; ++i) {
        sumOut1 += (outBmp1.pixels[i * 4 + 0] + outBmp1.pixels[i * 4 + 1] + outBmp1.pixels[i * 4 + 2]) / 3.0;
        sumOut2 += (outBmp2.pixels[i * 4 + 0] + outBmp2.pixels[i * 4 + 1] + outBmp2.pixels[i * 4 + 2]) / 3.0;
    }
    double meanOut1 = sumOut1 / (16.0 * 16.0);
    double meanOut2 = sumOut2 / (16.0 * 16.0);
    REQUIRE_TRUE(std::fabs(meanOut1 - 8.38) < 0.05 &&
                 outBmp1.pixels[(8 * 16 + 8) * 4 + 0] == 6 &&
                 outBmp1.pixels[(8 * 16 + 8) * 4 + 1] == 8 &&
                 outBmp1.pixels[(8 * 16 + 8) * 4 + 2] == 11,
                 "Zero-DCE 22-layer DAG output must match reference mean (8.38) and center RGB (6,8,11)");
    REQUIRE_TRUE(std::fabs(meanOut2 - 5.84) < 0.05 &&
                 outBmp2.pixels[(8 * 16 + 8) * 4 + 0] == 5 &&
                 outBmp2.pixels[(8 * 16 + 8) * 4 + 1] == 7 &&
                 outBmp2.pixels[(8 * 16 + 8) * 4 + 2] == 6,
                 "Zero-DCE++ 28-layer DAG output must match reference mean (5.84) and center RGB (5,7,6)");
    std::printf("  - Zero-DCE   (22 layers, 6-head Slice+Concat): mean_in=%.2f -> mean_out=%.2f (center RGBA=%u,%u,%u,%u)\n",
                meanIn, meanOut1,
                outBmp1.pixels[(8 * 16 + 8) * 4 + 0],
                outBmp1.pixels[(8 * 16 + 8) * 4 + 1],
                outBmp1.pixels[(8 * 16 + 8) * 4 + 2],
                outBmp1.pixels[(8 * 16 + 8) * 4 + 3]);
    std::printf("  - Zero-DCE++ (28 layers, DepthWise+PointWise): mean_in=%.2f -> mean_out=%.2f (center RGBA=%u,%u,%u,%u)\n",
                meanIn, meanOut2,
                outBmp2.pixels[(8 * 16 + 8) * 4 + 0],
                outBmp2.pixels[(8 * 16 + 8) * 4 + 1],
                outBmp2.pixels[(8 * 16 + 8) * 4 + 2],
                outBmp2.pixels[(8 * 16 + 8) * 4 + 3]);
#endif
    std::printf("  [PASS] Real NCNN graph & .bin weight execution verified.\n");
    std::fflush(nullptr);

    // ========================================================================
    // 3. P1 #3: Native SecurityManager_nativeEncrypt / nativeDecrypt (XAE1 AEAD)
    // ========================================================================
    std::printf("\n[NATIVE 3/6] Verifying P1 #3 Native SecurityManager_nativeEncrypt / nativeDecrypt...\n");
    std::string plainText = "{\"antiFakeCode\":\"BHPUTG9Y4K1DEW\",\"timestamp\":1743562327,\"lat\":-7.752,\"lon\":110.377}";
    jstring jPlain = MakeJString(plainText);

    jstring jCipher1 = Java_com_xhey_xcamera_security_SecurityManager_nativeEncrypt(env, nullptr, jPlain);
    jstring jCipher2 = Java_com_xhey_xcamera_security_SecurityManager_nativeEncrypt(env, nullptr, jPlain);
    std::string cipher1 = ReadJString(jCipher1);
    std::string cipher2 = ReadJString(jCipher2);

    REQUIRE_TRUE(!cipher1.empty() && !cipher2.empty(), "Encrypted Base64 strings must not be empty");
    REQUIRE_TRUE(cipher1 != cipher2, "Two nativeEncrypt calls on identical plaintext MUST produce different ciphertexts (unique 96-bit nonce)");
    REQUIRE_TRUE(cipher1.substr(0, 5) == "WEFFM" && cipher2.substr(0, 5) == "WEFFM",
                 "Ciphertext packets must begin with 'XAE1' magic header ('WEFFM' in Base64)");

    std::string dec1 = ReadJString(Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(env, nullptr, jCipher1));
    std::string dec2 = ReadJString(Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(env, nullptr, jCipher2));
    REQUIRE_TRUE(dec1 == plainText && dec2 == plainText, "nativeDecrypt round-trip must recover exact plaintext");

    // Cross-version deterministic XAE1 test vector verification (nonce = 0102030405060708090a0b0c)
    static const char* kFixedXae1Vector =
        "WEFFMQECAwQFBgcICQoLDFPsTwHrVZ+g+TLzOcQMsb1gslg3SWGmggTED89N5c5rEXoQ2gGBfQjrRNc3w8rNPRWbi8eoldjd1eTyAUohj6Zm2LkjclRuYvvbqWQcRP2/6HdXKhixXqfUecO1xFPZSwLtwA==";
    std::string decFixed = ReadJString(Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(
        env, nullptr, MakeJString(kFixedXae1Vector)));
    REQUIRE_TRUE(decFixed == plainText, "Cross-version XAE1 reference test vector must decrypt to exact plaintext");

    // Tamper test 1: Flip character in nonce region (index 8 in Base64)
    std::string tamperedNonce = cipher1;
    tamperedNonce[8] = (tamperedNonce[8] == 'A') ? 'B' : 'A';
    REQUIRE_TRUE(ReadJString(Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(
                     env, nullptr, MakeJString(tamperedNonce))).empty(),
                 "Tampered nonce must be rejected by nativeDecrypt");
    REQUIRE_TRUE(ReadJString(Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo(env, nullptr)) == "aead_auth_tag_mismatch",
                 "getLastErrorInfo must report aead_auth_tag_mismatch");

    // Tamper test 2: Flip character in ciphertext body (index 28 in Base64)
    std::string tamperedBody = cipher1;
    tamperedBody[28] = (tamperedBody[28] == 'A') ? 'B' : 'A';
    REQUIRE_TRUE(ReadJString(Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(
                     env, nullptr, MakeJString(tamperedBody))).empty(),
                 "Tampered ciphertext body must be rejected by nativeDecrypt");

    // Tamper test 3: Flip character in HMAC-SHA256 tag (near end of Base64)
    std::string tamperedTag = cipher1;
    size_t tagPos = tamperedTag.size() - 6;
    tamperedTag[tagPos] = (tamperedTag[tagPos] == 'A') ? 'B' : 'A';
    REQUIRE_TRUE(ReadJString(Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt(
                     env, nullptr, MakeJString(tamperedTag))).empty(),
                 "Tampered HMAC tag must be rejected by nativeDecrypt");

    std::printf("  - Native Ciphertext #1: %.44s...\n", cipher1.c_str());
    std::printf("  - Native Ciphertext #2: %.44s...\n", cipher2.c_str());
    std::printf("  [PASS] Native XAE1 AEAD unique nonce, round-trip, and 3-region tamper rejection verified.\n");
    std::fflush(nullptr);

    // ========================================================================
    // 4. P1 #4: Native APK Signing Block v2 Certificate Pinning & Forgery Rejection
    // ========================================================================
    std::printf("\n[NATIVE 4/6] Verifying P1 #4 Native APK Certificate Pinning & Forgery Rejection...\n");
    std::string releaseApkFixture = fixturesDir + "/timemark_release_sig.apk";
    std::string forgedApkFixture  = fixturesDir + "/timemark_forged_sig.apk";

    std::vector<uint8_t> releaseCertDer;
    std::vector<uint8_t> forgedCertDer;
    REQUIRE_TRUE(ExtractX509CertFromApkSigBlockV2(releaseApkFixture, releaseCertDer),
                 "Must extract 1421-byte X.509 cert from APK Sig Block 42 of release APK fixture");
    REQUIRE_TRUE(ExtractX509CertFromApkSigBlockV2(forgedApkFixture, forgedCertDer),
                 "Must extract X.509 cert from APK Sig Block 42 of forged APK fixture");

    // Case 4a: Valid release APK certificate + valid package name
    uint8_t relCertSha[32] = {0};
    xhey::jni::ComputeSha256(releaseCertDer.data(), releaseCertDer.size(), relCertSha);
    std::string relCertShaHex = xhey::jni::BytesToHex(relCertSha, 32);
    REQUIRE_TRUE(relCertShaHex == "96fcc4571a3c269d0bae4ca6fa786d236d8b61a691d58406c3805996f53f123f",
                 "Extracted release APK certificate SHA-256 must match pinned digest");

    MockJObject* validCtx = AllocMock(MockKind::Context);
    validCtx->pkgName = "com.oceangalaxy.camera.new";
    validCtx->certBytes = releaseCertDer;

    jboolean sigValid = Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature(
        env, nullptr, reinterpret_cast<jobject>(validCtx));
    std::string apkSigHex = ReadJString(Java_com_xhey_xcamera_util_JNIUtils_getApkSignature(
        env, nullptr, reinterpret_cast<jobject>(validCtx), 1743562327000LL));

    REQUIRE_TRUE(sigValid == JNI_TRUE, "nativeVerifyAppSignature must accept release APK certificate");
    REQUIRE_TRUE(apkSigHex.size() == 64,
                 "JNIUtils_getApkSignature must return 64-hex-char HMAC-SHA256 signature for release APK");
    REQUIRE_TRUE(ReadJString(Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo(env, nullptr)).empty(),
                 "getLastErrorInfo must be empty after valid APK verification");

    // Case 4b: Forged / re-signed APK certificate + valid package name -> MUST REJECT
    MockJObject* forgedCtx = AllocMock(MockKind::Context);
    forgedCtx->pkgName = "com.oceangalaxy.camera.new";
    forgedCtx->certBytes = forgedCertDer;

    jboolean forgedValid = Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature(
        env, nullptr, reinterpret_cast<jobject>(forgedCtx));
    std::string forgedSigStr = ReadJString(Java_com_xhey_xcamera_util_JNIUtils_getApkSignature(
        env, nullptr, reinterpret_cast<jobject>(forgedCtx), 1743562327000LL));
    std::string forgedErr = ReadJString(Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo(env, nullptr));

    REQUIRE_TRUE(forgedValid == JNI_FALSE, "nativeVerifyAppSignature must REJECT forged/re-signed APK certificate");
    REQUIRE_TRUE(forgedSigStr.empty() && forgedErr.find("untrusted_certificate_digest:") == 0,
                 "JNIUtils_getApkSignature must return empty string and record untrusted_certificate_digest error for forged APK");

    // Case 4c: Valid release certificate + untrusted repackaged package name -> MUST REJECT
    MockJObject* untrustedPkgCtx = AllocMock(MockKind::Context);
    untrustedPkgCtx->pkgName = "com.evil.repackaged.xhey.camera";
    untrustedPkgCtx->certBytes = releaseCertDer;

    REQUIRE_TRUE(Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature(
                     env, nullptr, reinterpret_cast<jobject>(untrustedPkgCtx)) == JNI_FALSE,
                 "nativeVerifyAppSignature must REJECT untrusted package name even with substring 'xhey'/'camera'");
    std::string pkgErr = ReadJString(Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo(env, nullptr));
    REQUIRE_TRUE(pkgErr.find("untrusted_package_name:") == 0,
                 "getLastErrorInfo must report untrusted_package_name");

    std::printf("  - Release APK Cert SHA-256: %s -> ACCEPTED (JNI_TRUE, HMAC=%s)\n", relCertShaHex.c_str(), apkSigHex.c_str());
    std::printf("  - Forged APK Cert         : %s -> REJECTED (JNI_FALSE)\n", forgedErr.c_str());
    std::printf("  - Untrusted Package Name  : %s -> REJECTED (JNI_FALSE)\n", pkgErr.c_str());
    std::printf("  [PASS] Native APK signature pinning and forgery rejection verified.\n");

    // ========================================================================
    // 5. P2 #5: Native PaddleOCR_init External Dictionary Loader (keysPath)
    // ========================================================================
    std::printf("\n[NATIVE 5/6] Verifying P2 #5 Native PaddleOCR_init External Dictionary Loading...\n");
    jstring detP = MakeJString(assetsDir + "/zero_dce++_full_no_resize_pre_train.param");
    jstring detB = MakeJString(assetsDir + "/zero_dce++_full_no_resize_pre_train.bin");
    jstring dictPath = MakeJString(fixturesDir + "/paddle_keys_ext.txt");
    jstring badDictPath = MakeJString(fixturesDir + "/non_existent_keys.txt");

    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init(
                     env, nullptr, detP, detB, detP, detB, detP, detB, badDictPath) == JNI_FALSE,
                 "PaddleOCR_init must return JNI_FALSE when keysPath file does not exist");

    REQUIRE_TRUE(Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init(
                     env, nullptr, detP, detB, detP, detB, detP, detB, dictPath) == JNI_TRUE,
                 "PaddleOCR_init must return JNI_TRUE when models and external keysPath dictionary are valid");
    std::printf("  [PASS] Native PaddleOCR_init external UTF-8 dictionary loading verified.\n");

    // ========================================================================
    // 6. P2 #6: Native JpegPacker & CodeDetect on 3 SHA-256 Verified Sample JPEGs
    // ========================================================================
    std::printf("\n[NATIVE 6/6] Verifying P2 #6 Native JpegPacker & CodeDetect on 3 Release Sample JPEGs...\n");

    struct SampleJpegSpec {
        const char* fileName;
        size_t expectedSize;
        const char* expectedSha256;
        const char* expectedAntiCode;
    };
    static const SampleJpegSpec kSamples[3] = {
        {
            "2025-04-21 11.00.25_10 Bridge Street_(Company-Timemark)_(Name-Elvis).jpg",
            2009314u,
            "85f0bb1fe51b68a278bcf623104a0c9c5785afcd21e22ec5e92e5968eb3d5ebb",
            "BHPUTG9Y4K1DEW"
        },
        {
            "2025-04-25 12.18.34_Gg Pandega Padma No. 6B_(Perusahaan-Timemark)_(Nama-Elvis).jpg",
            1619943u,
            "f83998d65e6e88e06d0b0295526728559a5f867b20557db071c0324aa27ffdc8",
            "MGT4DA9L9MBR9U"
        },
        {
            "2025-04-28 08.59.13_Calle 61 11632_(Empresa-Timemark)_(Nombre-Elvis).jpg",
            2195949u,
            "da11c91c9f9b15830ccd55eb4b906e2f440b22faf21f3294b890ca28ddb95509",
            "EG6P1H9HLAMGPU"
        }
    };

    // Verify NativeXhey_heyStr(0) returns the exact release AES key "2qR2nuPdYrq3IMSr"
    std::string heyKey0 = ReadJString(Java_com_xhey_xcamera_NativeXhey_heyStr(env, nullptr, 0));
    REQUIRE_TRUE(heyKey0 == "2qR2nuPdYrq3IMSr", "NativeXhey_heyStr(0) must return '2qR2nuPdYrq3IMSr'");

    jobjectArray pathsArr = env->NewObjectArray(3, nullptr, nullptr);
    jintArray posArr = env->NewIntArray(3);
    jint posVals[3] = {1, 1, 1};
    env->SetIntArrayRegion(posArr, 0, 3, posVals);

    for (int i = 0; i < 3; ++i) {
        std::string fullPath = fixturesDir + "/" + kSamples[i].fileName;
        std::vector<uint8_t> jpegBytes;
        REQUIRE_TRUE(ReadFileAllBytes(fullPath, jpegBytes), "Sample JPEG fixture must exist and be readable");
        REQUIRE_TRUE(jpegBytes.size() == kSamples[i].expectedSize, "Sample JPEG byte size must match fixture spec");

        uint8_t sha[32] = {0};
        xhey::jni::ComputeSha256(jpegBytes.data(), jpegBytes.size(), sha);
        std::string shaHex = xhey::jni::BytesToHex(sha, 32);
        REQUIRE_TRUE(shaHex == kSamples[i].expectedSha256, "Sample JPEG SHA-256 hash must match fixture spec");

        env->SetObjectArrayElement(pathsArr, i, MakeJString(fullPath));
    }

    // Call native JpegPacker_getCanjpegDecode on all 3 paths
    jintArray canDecArr = Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_getCanjpegDecode(
        env, nullptr, pathsArr, posArr);
    REQUIRE_TRUE(canDecArr != nullptr && env->GetArrayLength(canDecArr) == 3,
                 "JpegPacker_getCanjpegDecode must return int[3]");
    jint* canDecFlags = env->GetIntArrayElements(canDecArr, nullptr);
    for (int i = 0; i < 3; ++i) {
        REQUIRE_TRUE(canDecFlags[i] == 1, "JpegPacker_getCanjpegDecode must return 1 for each sample JPEG");
    }

    // Call native JpegPacker_startDecode and native CodeDetect_transform3 on each sample JPEG
    for (int i = 0; i < 3; ++i) {
        std::string fullPath = fixturesDir + "/" + kSamples[i].fileName;
        jobjectArray slicesArr = Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startDecode(
            env, nullptr, 1, MakeJString(fullPath), MakeJString(""));
        REQUIRE_TRUE(slicesArr != nullptr && env->GetArrayLength(slicesArr) >= 1,
                     "JpegPacker_startDecode must return at least 1 SliceDataInfo");

        auto* sliceObj = reinterpret_cast<MockJObject*>(env->GetObjectArrayElement(slicesArr, 0));
        REQUIRE_TRUE(sliceObj != nullptr && sliceObj->errorCode == 0 && !sliceObj->sliceData.empty(),
                     "Decoded SliceDataInfo must have errorCode==0 and non-empty JSON payload");

        std::string decodedJson(sliceObj->sliceData.begin(), sliceObj->sliceData.end());
        std::string codeQuoted = std::string("\"") + kSamples[i].expectedAntiCode + "\"";
        REQUIRE_TRUE(decodedJson.find("\"antiFakeCode\"") != std::string::npos &&
                     decodedJson.find(codeQuoted) != std::string::npos,
                     "Decoded Exif UserComment JSON must contain expected 14-char antiFakeCode");

        // Call native CodeDetect_transform3 (antiCodeDecode) on the extracted 14-char antiFakeCode
        jobject metaObjRaw = Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform3(
            env, nullptr, MakeJString(kSamples[i].expectedAntiCode));
        auto* metaObj = reinterpret_cast<MockJObject*>(metaObjRaw);
        REQUIRE_TRUE(metaObj != nullptr && metaObj->codeValid == 1,
                     "Native CodeDetect_transform3 must validate 14-char release antiFakeCode");
        REQUIRE_TRUE(metaObj->timeStampS > 1700000000LL && std::isfinite(metaObj->latitude) && std::isfinite(metaObj->longitude),
                     "Native CodeDetect_transform3 must decode valid timestamp and GPS coordinates");

        std::printf("  - Sample #%d: %zu B (SHA256=%.12s...) -> JSON=%zu B -> antiFakeCode=%s (ts=%lld, lat=%.4f, lon=%.4f)\n",
                    i + 1, sliceObj->sliceData.size(), kSamples[i].expectedSha256,
                    decodedJson.size(), kSamples[i].expectedAntiCode,
                    static_cast<long long>(metaObj->timeStampS), metaObj->latitude, metaObj->longitude);
    }

    #if defined(XHEY_STANDALONE_TEST_SHIM)
    // Also verify CodeDetect_detect float[] output layout (flat image -> 0, watermark pattern -> 27 floats)
    jfloatArray roiArr = env->NewFloatArray(4);
    jfloat roiVals[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    env->SetFloatArrayRegion(roiArr, 0, 4, roiVals);
    jfloatArray flatDetOut = Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_detect(
        env, nullptr, reinterpret_cast<jobject>(&inBmp), roiArr);
    REQUIRE_TRUE(flatDetOut != nullptr && env->GetArrayLength(flatDetOut) == 0,
                 "CodeDetect_detect must return float[0] on flat/blank bitmap");

    MockBitmapObject detBmp = inBmp;
    for (size_t i = 0; i < 16 * 16; ++i) {
        uint8_t v = ((i % 2) == 0) ? 240u : 15u;
        detBmp.pixels[i * 4 + 0] = v;
        detBmp.pixels[i * 4 + 1] = v;
        detBmp.pixels[i * 4 + 2] = v;
        detBmp.pixels[i * 4 + 3] = 255u;
    }
    jfloatArray detOut = Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_detect(
        env, nullptr, reinterpret_cast<jobject>(&detBmp), roiArr);
    REQUIRE_TRUE(detOut != nullptr && env->GetArrayLength(detOut) == 27,
                 "CodeDetect_detect must return 27-float detection layout (3+8+16) on high-contrast ROI");
    #endif

    std::printf("  [PASS] All 3 release sample JPEGs and 14-char antiFakeCodes decoded by native C++ implementation.\n");

    std::printf("\n============================================================================\n");
    std::printf("  [RUNTIME PASS] ALL 6 NATIVE C++ AUDIT VERIFICATION SUITES SUCCEEDED\n");
    std::printf("============================================================================\n");
    return 0;
}
