cmake_minimum_required(VERSION 3.22.1)
project("timemark_native_reconstructed" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_library(log-lib log)
find_library(android-lib android)
find_library(jnigraphics-lib jnigraphics)
find_library(glesv2-lib GLESv2)
find_library(egl-lib EGL)
find_library(mediandk-lib mediandk)

set(XHEY_COMMON_LIBS
    ${log-lib}
    ${android-lib}
    ${jnigraphics-lib}
    ${glesv2-lib}
    ${egl-lib}
    ${mediandk-lib}
)

# ==============================================================================
# 1. libnnpackage-f99779bd.so
#    Core C++ Edge AI, JpegPacker Steganography, CodeDetect Anti-Fake,
#    AiRef Feature Encoder, Aegis Hardware Anti-Spoofing, and ROS2 MCAP Engine
# ==============================================================================
add_library(
    nnpackage-f99779bd
    SHARED
    src/jpeg_packer_jni.cpp
    src/code_detect_anticode_jni.cpp
    src/edge_ai_ncnn_jni.cpp
    src/aegis_anti_spoof_jni.cpp
    src/mcap_robotics_jni.cpp
)
target_link_libraries(nnpackage-f99779bd ${XHEY_COMMON_LIBS})

# ==============================================================================
# 2. libxheymediav8.so
#    OpenGL ES / RGBA Baseline DCT JPEG Encoder & Decoder, LAME MP3 Bridge
# ==============================================================================
add_library(
    xheymediav8
    SHARED
    src/xhey_media_and_security_jni.cpp
)
target_compile_definitions(xheymediav8 PRIVATE XHEY_BUILD_MEDIAV8=1)
target_link_libraries(xheymediav8 ${XHEY_COMMON_LIBS})

# ==============================================================================
# 3. libxhey_videoedit.so
#    FFmpeg Video/Audio Muxer, YUV420 Decoder & CLI Command Executor
# ==============================================================================
add_library(
    xhey_videoedit
    SHARED
    src/xhey_media_and_security_jni.cpp
)
target_compile_definitions(xhey_videoedit PRIVATE XHEY_BUILD_VIDEOEDIT=1)
target_link_libraries(xhey_videoedit ${XHEY_COMMON_LIBS})

# ==============================================================================
# 4. libnative-lib.so
#    APK X.509 Signature Verifier, Device Feature Fingerprint & AES-128-CTR/HMAC
# ==============================================================================
add_library(
    native-lib
    SHARED
    src/xhey_media_and_security_jni.cpp
)
target_compile_definitions(native-lib PRIVATE XHEY_BUILD_NATIVELIB=1)
target_link_libraries(native-lib ${XHEY_COMMON_LIBS})

# ==============================================================================
# 5. libjpeg.so, libmp3lame.so & libffmpeg.so
#    Companion Native Codec Shared Libraries Loaded by NativeLibsLoader (jy/a)
#    and VideoEditInterface
# ==============================================================================
add_library(
    jpeg
    SHARED
    src/xhey_media_and_security_jni.cpp
)
target_compile_definitions(jpeg PRIVATE XHEY_BUILD_LIBJPEG=1)
target_link_libraries(jpeg ${XHEY_COMMON_LIBS})

add_library(
    mp3lame
    SHARED
    src/xhey_media_and_security_jni.cpp
)
target_compile_definitions(mp3lame PRIVATE XHEY_BUILD_MP3LAME=1)
target_link_libraries(mp3lame ${XHEY_COMMON_LIBS})

add_library(
    ffmpeg
    SHARED
    src/xhey_media_and_security_jni.cpp
)
target_compile_definitions(ffmpeg PRIVATE XHEY_BUILD_FFMPEG=1)
target_link_libraries(ffmpeg ${XHEY_COMMON_LIBS})
