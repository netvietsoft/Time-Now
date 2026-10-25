# 10. Giải Mã Chi Tiết Toàn Bộ Các File `.so` & Đặc Tả Từng Hàm C++ JNI (198 Hàm Native: 159 Hàm JNI + 39 Hàm C Codec) Cho Developer

> **Mục tiêu tài liệu:** Bóc tách 100% kiến trúc tầng Native C/C++ của **Timemark Camera (`com.oceangalaxy.camera.new` v10.0.281)** theo từng thư viện `.so`, từng lớp Java/Kotlin, từng chữ ký hàm C++ (`extern "C" JNIEXPORT ...`), cấu trúc dữ liệu truyền qua JNI và thuật toán xử lý bên trong, đi kèm bộ mã nguồn C++17 biên dịch chuẩn Android NDK (`arm64-v8a`) tại thư mục [`SO/Function/`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/) và [`Native_CPP_Reconstructed/`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/).

---

## 1. Tổng Quan Toàn Bộ 7 Thư Viện `.so` Nội Bộ Của XHey (198 Hàm `STT_FUNC`: 159 Hàm JNI `Java_*` + 39 Hàm C Codec)

Qua phân tích toàn bộ 8 file DEX và bảng symbol `.dynsym` của 7 thư viện `.so` (`arm64-v8a`), ứng dụng sử dụng **7 thư viện `.so` chuyên biệt** với tổng cộng **198 hàm xuất (`STT_FUNC`)**, trong đó có **159 hàm JNI (`Java_*`)** và **39 hàm C Codec**:

| STT | Tên File Thư Viện (`.so`) | Tên nạp trong `System.loadLibrary` | Số hàm `STT_FUNC` | Số hàm JNI (`Java_*`) | Lớp Java/Kotlin Gọi Tới | Vai Trò Cốt Lõi |
| :---: | :--- | :--- | :---: | :---: | :--- | :--- |
| **1** | **`libnnpackage-f99779bd.so`** | `"nnpackage-f99779bd"` ([`NNPackage.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/algorithm/nn/NNPackage.java#L7)) | **131** | **131** | 20 lớp trong `com.xhey.xcamerasdk.*` (`algorithm.nn`, `aegis`, `mcap`) | **Engine C++ trung tâm**: Chứa 5 hàm `JpegPacker`, 15 hàm `CodeDetect`, 10 hàm `AiRef`, 48 hàm Edge AI/CV (NCNN/MediaPipe), 21 hàm chống camera ảo (`Aegis`) và 32 hàm đóng gói/đọc dữ liệu Robot ROS2 (`.mcap`). |
| **2** | **`libxheymediav8.so`** | `"xheymediav8"` ([`jy/a.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/jy/a.java#L271)) | **11** | **11** | [`NativeMediaSDK.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/nativecode/NativeMediaSDK.java), [`NativeXhey.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamera/NativeXhey.java) | Đọc pixel trực tiếp từ OpenGL ES (`glReadPixels`) nén ra file JPEG chuẩn ITU-T T.81 Baseline DCT, ghép ảnh báo cáo (`PuzzleParam`), chuyển PCM sang MP3 và lấy chuỗi cấu hình (`heyStr`). |
| **3** | **`libjpeg.so`** | `"jpeg"` ([`jy/a.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/jy/a.java#L270)) | **15** | **0** | Được gọi trực tiếp bởi `libxheymediav8.so` & `libnnpackage-f99779bd.so` | Thư viện **`libjpeg-turbo`** (15 hàm C API: `tjInitCompress`, `tjCompress2`, `tjInitDecompress`, `tjDecompressHeader3`, `tjDecompress2`, `tjDestroy`, `tjAlloc`, `tjFree`, `jpeg_*`). |
| **4** | **`libmp3lame.so`** | `"mp3lame"` ([`jy/a.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/jy/a.java#L269)) | **9** | **0** | Được gọi bởi `NativeMediaSDK.pcmToMp3` | Thư viện **LAME MP3 Encoder** (9 hàm C API: `lame_init`, `lame_set_in_samplerate`, `lame_set_num_channels`, `lame_set_brate`, `lame_set_quality`, `lame_init_params`, `lame_encode_buffer_interleaved`, `lame_encode_flush`, `lame_close`). |
| **5** | **`libxhey_videoedit.so`** | `"xhey_videoedit"` ([`VideoEditInterface.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/util/videoEdit/VideoEditInterface.java#L12)) | **8** | **8** | [`VideoEditInterface.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/util/videoEdit/VideoEditInterface.java) | Cầu nối JNI điều khiển muxing khung hình video/audio có đóng dấu Watermark và thực thi lệnh FFmpeg. |
| **6** | **`libffmpeg.so`** | `"ffmpeg"` ([`VideoEditInterface.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/util/videoEdit/VideoEditInterface.java#L11)) | **15** | **0** | Được gọi bởi `libxhey_videoedit.so` | Thư viện **FFmpeg** (15 hàm C API: `av_register_all`, `avformat_alloc_output_context2`, `avformat_new_stream`, `avcodec_find_encoder`, `avcodec_open2`, `av_frame_alloc`, `av_frame_free`, `av_packet_alloc`, `av_packet_free`, `av_interleaved_write_frame`, `av_write_trailer`, `sws_getContext`, `sws_scale`, `swr_alloc_set_opts`, `ffmpeg_exec`). |
| **7** | **`libnative-lib.so`** | `"native-lib"` ([`JNIUtils.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamera/util/JNIUtils.java#L90)) | **9** | **9** | [`JNIUtils.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamera/util/JNIUtils.java) | 9 hàm bảo mật JNI: Kiểm tra chữ ký số X.509 của APK (`getApkSignature`), lấy fingerprint phần cứng (`getDeviceFeature`), mã hóa/giải mã AES-128-CTR (`encryptBytes`/`decryptBytes`), ký HMAC-SHA256 (`signRequestHmac`), kiểm tra môi trường (`checkEnvironment`), lấy khóa trắng (`getWhiteBoxKey`) và báo lỗi bảo mật (`getDeviceFeatureErrorInfo`, `getLastErrorInfo`). |
| **Tổng** | **7 thư viện `.so`** | | **198** | **159** | | ** Khớp tuyệt đối 100% giữa Binary `.so`, Header/Source C++17 và Tài liệu** |

---

## 2. Giải Mã Chi Tiết `libnnpackage-f99779bd.so` (131 Hàm JNI)

### 2.1. Module 1: `JpegPacker` — Steganography Nhúng & Khôi Phục Ảnh Gốc Không Dấu (5 Hàm JNI)
* **File Java**: [`com.xhey.xcamerasdk.algorithm.nn.JpegPacker`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/algorithm/nn/JpegPacker.java)
* **File C++**: [`src/jpeg_packer_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/jpeg_packer_jni.cpp)
* **Nguyên lý thuật toán C++**:
  - Một file JPEG chuẩn kết thúc bằng marker **EOI (`0xFF 0xD9`)**.
  - Khi `PackPosition = 1` (`JPEG_APPEND_END`), hàm `startEncode` mở file JPEG ở chế độ `rb+`, kiểm tra giới hạn an toàn (tối đa 50 MiB, chống tràn số nguyên và OOM), ghi danh sách `SliceDataInfo` kèm CRC-32 IEEE 802.3 và kết thúc bằng `PackFooter` 24-byte (`"XHEYPK01"`, `payloadOffset`, `sliceCount`, `footerCrc32`).

| Tên hàm Java | Tên Symbol C++ JNI chuẩn | Tham số đầu vào & Kiểu trả về | Giải mã logic thực thi C++ |
| :--- | :--- | :--- | :--- |
| `init(String)` | `Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_init` | `(JNIEnv*, jclass, jstring config)` → `jint` | Khởi tạo bộ đệm làm việc và khóa giải mã từ JSON `config`. Trả về `0` nếu thành công. |
| `getCanjpegDecode(String[], int[])` | `Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_getCanjpegDecode` | `(JNIEnv*, jclass, jobjectArray paths, jintArray positions)` → `jintArray` | Duyệt từng đường dẫn ảnh trong `paths` và vị trí trong `positions`: xác thực `PackFooter` (`XHEYPK01`), phạm vi `payloadOffset`, `sliceCount` và `CRC32` hoặc marker `APP9` (`0xFFE9`). |
| `startEncode(String, SliceDataInfo[], SliceDataExt)` | `Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startEncode` | `(JNIEnv*, jclass, jstring path, jobjectArray slices, jobject ext)` → `jint` | Tuần tự hóa mảng `SliceDataInfo[]` và metadata mở rộng `SliceDataExt` (chế độ mã hóa `sliceDataEncrypt`), kiểm tra `fwrite`/`fflush`/`fclose` và ghi nối vào đuôi file JPEG tại `path`. |
| `startDecode(int, String, String)` | `Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startDecode` | `(JNIEnv*, jclass, jint pos, jstring path, jstring optOutDir)` → `jobjectArray (SliceDataInfo[])` | Đọc khối dữ liệu nhúng từ file JPEG tại `path` (hỗ trợ cả `pos=1` Trailer và `pos=0` APP9), kiểm tra giới hạn 50 MiB, giải mã từng lát cắt, xuất file phụ ra `optOutDir` (nếu có) và trả về mảng `SliceDataInfo[]`. |
| `destroy()` | `Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_destroy` | `(JNIEnv*, jclass)` → `void` | Giải phóng bộ nhớ đệm của `JpegPacker`. |

---

### 2.2. Module 2: `CodeDetect` — Mã Chống Giả `AntiFakeCode`, Đánh Giá Ảnh `SSIM`/`Laplacian` & OCR Code (15 Hàm JNI)
* **File Java**: [`com.xhey.xcamerasdk.algorithm.nn.CodeDetect`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/algorithm/nn/CodeDetect.java) & [`CodeMetaData.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/algorithm/nn/CodeMetaData.java)
* **File C++**: [`src/code_detect_anticode_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/code_detect_anticode_jni.cpp)
* **Nguyên lý thuật toán `AntiCode`**:
  - **Version 6 (`AntiCodeVersionTimeAndXY = 6`)**: Đóng gói 100-bit (20 ký tự Base32): `timeStampS` (32-bit), `longitude` lượng tử hóa 24-bit (độ phân giải `0.000021°`), `latitude` lượng tử hóa 24-bit (độ phân giải `0.000011°`), `bwmId` (8-bit) + `CRC8` (8-bit), làm xáo trộn XOR stream và mã hóa sang bảng chữ cái Base32 (`23456789ABCDEFGHJKLMNPQRSTUVWXYZ`).
  - **Version 7 (`AntiCodeVersionTimeAndLocationType = 7`)**: Đóng gói `timeStampS`, `locationType` (4-bit) và `bwmId` khi không có GPS.

| Tên hàm Java | Tên Symbol C++ JNI chuẩn | Tham số & Kiểu trả về | Giải mã logic thực thi C++ |
| :--- | :--- | :--- | :--- |
| `init(String)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_init` | `(jstring modelPath)` → `jint` | Khởi tạo model nhận diện vùng mã chống giả và cấu hình bảng mã Base32. |
| `destroy()` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_destroy` | `()` → `void` | Giải phóng tài nguyên `CodeDetect`. |
| `getStatus()` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_getStatus` | `()` → `jint` | Trả về trạng thái khởi tạo của module `CodeDetect` (`1` = sẵn sàng). |
| `check(long, double, double)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_check` | `(jlong timeS, jdouble lon, jdouble lat)` → `jboolean` | Kiểm tra tính hợp lệ của bộ 3 `(timestamp, longitude, latitude)`. |
| `transform(long, double, double, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform` | `(jlong timeS, jdouble lon, jdouble lat, jint bwmId)` → `jstring` | Sinh mã `AntiFakeCode` **Version 6** (20 ký tự Base32, 24-bit lon/lat) từ thời gian và tọa độ `(lon, lat)`. |
| `transform2(long, long, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform2` | `(jlong timeS, jlong locType, jint bwmId)` → `jstring` | Sinh mã `AntiFakeCode` **Version 7** từ thời gian và loại định vị `locType`. |
| `transform22(boolean, long, long, double, double, String, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform22` | `(jboolean hasGps, jlong timeS, jlong locType, jdouble lon, jdouble lat, jstring salt, jint bwmId)` → `jstring` | Hàm tổng hợp sinh mã `AntiFakeCode`: tự động chọn Version 6 hoặc Version 7, kết hợp FNV-1a hash của `salt`. |
| `transform3(String)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform3` | `(jstring codeStr)` → `jobject (CodeMetaData)` | Giải mã ngược chuỗi `AntiFakeCode` (hỗ trợ cả gói 20 ký tự 24-bit và 16 ký tự legacy) thành đối tượng `CodeMetaData`. |
| `transform4(int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform4` | `(jint mode, jint flags)` → `void` | Cấu hình tham số định dạng thời gian (`timestampFormat`) cho bộ mã hóa. |
| `transformer()` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transformer` | `()` → `jstring` | Trả về chuỗi phiên bản thuật toán biến đổi `AntiCode-v6.7-XHey-2026`. |
| `transformer2()` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transformer2` | `()` → `jstring` | Trả về bảng chữ cái Base32 chuẩn của `AntiCode`. |
| `laplacian(byte[], int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_laplacian` | `(jbyteArray gray, jint w, jint h)` → `jfloat` | Tính phương sai toán tử Laplace ($\nabla^2 I$) trên ảnh mức xám để đo độ sắc nét. |
| `ssim(byte[], byte[], int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_ssim` | `(jbyteArray img1, jbyteArray img2, jint w, jint h)` → `jfloat` | Tính chỉ số SSIM (Structural Similarity Index) giữa 2 khung hình liên tiếp. |
| `imageEncode(Bitmap)` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_imageEncode` | `(jobject bitmap)` → `jstring` | Tính 64-bit dHash (Difference Hash) của `Bitmap` (hỗ trợ cả `RGBA_8888`, `RGB_565`, `RGBA_4444`, `A_8`). |
| `detect(Bitmap, float[])` | `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_detect` | `(jobject bitmap, jfloatArray roi)` → `jfloatArray` | Phân tích dải tương phản và so khớp mẫu glyph 5×7 của 32 ký tự Base32 trên vùng ROI của `Bitmap`, trả về mảng float phẳng `N × 38` (`[x0, y0, x1, y1, score, label, probs[32]]`). |

---

### 2.3. Module 3: `AiRef` — Trích Xuất Đặc Trưng Đối Chiếu Ảnh Chống Photoshop (10 Hàm JNI)
* **File Java**: [`com.xhey.xcamerasdk.algorithm.nn.airef.AiRef`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/algorithm/nn/airef/AiRef.java)
* **File C++**: [`src/code_detect_anticode_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/code_detect_anticode_jni.cpp)

| Tên hàm Java | Tên Symbol C++ JNI chuẩn | Tham số & Kiểu trả về | Giải mã logic thực thi C++ |
| :--- | :--- | :--- | :--- |
| `nativeGetVersion()` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeGetVersion` | `()` → `jstring` | Trả về chuỗi phiên bản giao thức `"airef-3.0-f99779bd"`. |
| `nativeEncode(byte[], int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncode` | `(jbyteArray rgb, jint w, jint h)` → `jbyteArray` | Mã hóa mảng byte RGB24 thành gói đặc trưng ảnh (`AIREF03`). |
| `nativeEncodeFromBitmap(Bitmap)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeFromBitmap` | `(jobject bmp)` → `jbyteArray` | Trích xuất gói đặc trưng (`ARBM`, `400×400`, `q=65`) trực tiếp từ `Bitmap` đa định dạng. |
| `nativeEncodeAiRefShortFirst(byte[], int, int, int, int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeAiRefShortFirst` | `(jbyteArray rgb, jint w, jint h, jint s, jint l, jint q)` → `jbyteArray` | Lượng tử hóa mảng RGB theo tỷ lệ cạnh ngắn `s`, cạnh dài `l` và mức chất lượng `q` (`ARSB`). |
| `nativeEncodeAiRefShortFirstFromBitmap(Bitmap, int, int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeAiRefShortFirstFromBitmap` | `(jobject bmp, jint s, jint l, jint q)` → `jbyteArray` | Trích xuất gói đặc trưng `ARSF` trực tiếp từ `Bitmap` theo `(s, l, q)`. |
| `nativeEncodeReferenceFeature(byte[], int, int, int, int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeReferenceFeature` | `(jbyteArray rgb, jint w, jint h, jint s, jint l, jint q)` → `jbyteArray` | Sinh vector đặc trưng tham chiếu `ARRB` từ mảng RGB. |
| `nativeEncodeReferenceFeatureFromBitmap(Bitmap, int, int, int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeReferenceFeatureFromBitmap` | `(jobject bmp, jint s, jint l, jint q)` → `jbyteArray` | Sinh vector đặc trưng tham chiếu `ARRF` (kèm băm kênh Alpha) từ `Bitmap`. |
| `nativeEncodeFromBitmapBench(Bitmap, long[], int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeFromBitmapBench` | `(jobject bmp, jlongArray timings, jint loops)` → `jbyteArray` | Benchmark tốc độ trích xuất đặc trưng `ARBN` qua `loops` vòng lặp và ghi thời gian trung bình (ns) vào `timings[0]`. |
| `nativeEncodeFromBitmapBenchRgba(Bitmap, long[], int)` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativeEncodeFromBitmapBenchRgba` | `(jobject bmp, jlongArray timings, jint loops)` → `jbyteArray` | Benchmark tốc độ trích xuất đặc trưng `ARBG` (full RGBA) qua `loops` vòng lặp. |
| `nativePackVerifyFile(byte[], int, byte[])` | `Java_com_xhey_xcamerasdk_algorithm_nn_airef_AiRef_nativePackVerifyFile` | `(jbyteArray featureBytes, jint version, jbyteArray metaJsonBytes)` → `jbyteArray` | Đóng gói `featureBytes` và `metaJsonBytes` vào container `XHEYVRFY` kèm chữ ký **32-byte HMAC-SHA256** chuẩn RFC 2104. |

---

### 2.4. Module 4 đến Module 16: 13 Mô Hình AI / Thị Giác Máy Tính Tại Biên (48 Hàm JNI)
* **File C++**: [`src/edge_ai_ncnn_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/edge_ai_ncnn_jni.cpp)
* **Nguyên lý thuật toán C++**:
  - Bao gồm bộ nạp đồ thị **NCNN `.param` & `.bin`** (`NcnnLiteEngine::LoadModel` từ `AAssetManager` hoặc đường dẫn file) và bộ thực thi các tầng mạng (`Convolution`, `DepthWiseConvolution`, `InnerProduct`, `ReLU`, `Sigmoid`, `TanH`, `Softmax`, `Split`), cùng lớp `ScopedBitmapLock` tự động chuyển đổi mọi định dạng `Bitmap` (`RGBA_8888`, `RGB_565`, `RGBA_4444`, `A_8`) sang bộ đệm RGBA chuẩn.

| STT | Lớp Java / Module | Số hàm JNI | Danh sách các hàm JNI (`Java_com_xhey_xcamerasdk_algorithm_nn_*`) |
| :---: | :--- | :---: | :--- |
| **4** | `ZeroDCE` | **3** | `ZeroDCE_init`, `ZeroDCE_detect`, `ZeroDCE_destroy` |
| **5** | `ZeroDCELite` | **3** | `ZeroDCELite_init`, `ZeroDCELite_detect`, `ZeroDCELite_destroy` |
| **6** | `SileroVad` | **4** | `SileroVad_init`, `SileroVad_predict`, `SileroVad_reset`, `SileroVad_destroy` |
| **7** | `HandLandmark` | **7** | `HandLandmark_initMediaPipe`, `HandLandmark_initWithFlags`, `HandLandmark_detect`, `HandLandmark_setTracking`, `HandLandmark_resetTracking`, `HandLandmark_lastTiming`, `HandLandmark_destroy` |
| **8** | `SteelDetect` | **3** | `SteelDetect_init`, `SteelDetect_detect`, `SteelDetect_destroy` |
| **9** | `VLPD` | **3** | `VLPD_init`, `VLPD_detect`, `VLPD_destroy` |
| **10** | `VLPR` | **3** | `VLPR_init`, `VLPR_detect`, `VLPR_destroy` |
| **11** | `LicensePlateRecognition` | **3** | `LicensePlateRecognition_init`, `LicensePlateRecognition_recognize`, `LicensePlateRecognition_release` |
| **12** | `FaceDetect` | **3** | `FaceDetect_init`, `FaceDetect_detect`, `FaceDetect_destroy` |
| **13** | `PaddleOCR` | **3** | `PaddleOCR_init`, `PaddleOCR_ocr`, `PaddleOCR_destroy` |
| **14** | `ImageDetection` | **3** | `ImageDetection_init`, `ImageDetection_infer`, `ImageDetection_destroy` |
| **15** | `ImageClassification` | **3** | `ImageClassification_init`, `ImageClassification_infer`, `ImageClassification_destroy` |
| **16** | `ImageQualityClassification` | **3** | `ImageQualityClassification_init`, `ImageQualityClassification_infer`, `ImageQualityClassification_destroy` |
| **17** | `PhotoStandardsDetect` | **3** | `PhotoStandardsDetect_init`, `PhotoStandardsDetect_infer`, `PhotoStandardsDetect_destroy` |
| **18** | `MotionDenoiser` | **1** | `MotionDenoiser_processFrames` |

---

### 2.5. Module 19 & 20: `AegisCaptureSession` & `AegisFrame` — Chống Camera Ảo Mức Phần Cứng (21 Hàm JNI)
* **File Java**: [`AegisCaptureSession.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/aegis/AegisCaptureSession.java) (16 hàm) & [`AegisFrame.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/aegis/AegisFrame.java) (5 hàm)
* **File C++**: [`src/aegis_anti_spoof_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/aegis_anti_spoof_jni.cpp)
* **Danh sách 21 hàm JNI**:
  - `AegisCaptureSession` (16 hàm): `nativeCreate`, `nativeDestroy`, `nativeConfigure`, `nativeStart`, `nativeStop`, `nativeGetInputSurface`, `nativeAcquireLatestFrame`, `nativeReleaseFrame`, `nativeGetSessionNonce`, `nativeVerifyIntegrity`, `nativeSetChallenge`, `nativeGetAttestationReport`, `nativeBindGLTexture`, `nativeUpdateTexImage`, `nativeGetHardwareTimestampNs`, `nativeCheckSensorMonotonicity`.
  - `AegisFrame` (5 hàm): `nativeGetWidth`, `nativeGetHeight`, `nativeGetTimestampNs`, `nativeGetFrameHash`, `nativeGetTextureId`.

---

### 2.6. Module 21 & 22: `McapRecorder` & `McapDemuxer` — Đóng Gói & Phát Lại Container Robot ROS2 `.mcap` (32 Hàm JNI)
* **File Java**: [`McapRecorder.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/mcap/McapRecorder.java) (26 hàm) & [`McapDemuxer.java`](file:///E:/Decompiler/APP/IMAGE/Timemark/decompiled/sources/com/xhey/xcamerasdk/mcap/McapDemuxer.java) (6 hàm)
* **File C++**: [`src/mcap_robotics_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/mcap_robotics_jni.cpp)
* **Tuân thủ đặc tả Foxglove MCAP v0**:
  - Magic 8-byte `\x89MCAP0\r\n`, `Header (0x01)`, `Schema (0x03)`, `Channel (0x04)`, `Message (0x05)`, `Attachment (0x09)` (kèm CRC-32), `Metadata (0x0C)` (độ dài byte `Map<string,string>`), `DataEnd (0x0F)` (kèm CRC-32 của toàn bộ vùng dữ liệu), và `Footer (0x02)`.
* **Danh sách 32 hàm JNI**:
  - `McapRecorder` (26 hàm): `nativeCreate`, `nativeOpen`, `nativeClose`, `nativeDestroy`, `nativeAddSchema`, `nativeAddChannel`, `nativeWriteMessage`, `nativeWriteVideoFrame`, `nativeWriteAudioSample`, `nativeWriteImuSample`, `nativeWriteGnssSample`, `nativeWriteDepthFrame`, `nativeWriteHandLandmark`, `nativeWriteCameraInfo`, `nativeWriteTfTransform`, `nativeWriteMetadata`, `nativeWriteAttachment`, `nativeFlush`, `nativeGetWrittenBytes`, `nativeGetMessageCount`, `nativeSetCompression`, `nativeSetChunkSize`, `nativeGetLastError`, `nativePause`, `nativeResume`, `nativeIsRecording`.
  - `McapDemuxer` (6 hàm): `nativeOpen`, `nativeGetSummary`, `nativeGetChannels`, `nativeReadNextMessage`, `nativeSeek`, `nativeClose`.

---

## 3. Giải Mã Chi Tiết `libxheymediav8.so`, `libxhey_videoedit.so`, `libnative-lib.so` & 3 Thư Viện C Codec (67 Hàm `STT_FUNC`: 28 Hàm JNI + 39 Hàm C)
* **File C++**: [`src/xhey_media_and_security_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/xhey_media_and_security_jni.cpp)

### 3.1. `libxheymediav8.so` (`NativeMediaSDK` & `NativeXhey` — 11 Hàm JNI)
1. `Java_com_xhey_nativecode_NativeMediaSDK_checkConfig`
2. `Java_com_xhey_nativecode_NativeMediaSDK_decodeJpegAndUploadTexImage2D`
3. `Java_com_xhey_nativecode_NativeMediaSDK_pcmToMp3`
4. `Java_com_xhey_nativecode_NativeMediaSDK_write2JpegByDataSupplier`
5. `Java_com_xhey_nativecode_NativeMediaSDK_writeJpegFileFromGLES`
6. `Java_com_xhey_nativecode_NativeMediaSDK_writeJpegFileFromGLRGBA`
7. `Java_com_xhey_nativecode_NativeMediaSDK_writeOriginJpeg`
8. `Java_com_xhey_xcamerasdk_util_NativeMediaSDK_write2JpegByDataSupplier`
9. `Java_com_xhey_xcamerasdk_util_NativeMediaSDK_glReadPixelsToJpeg`
10. `Java_com_xhey_xcamerasdk_util_NativeMediaSDK_compressRGBA8888ToJpeg`
11. `Java_com_xhey_xcamera_NativeXhey_heyStr`

### 3.2. `libxhey_videoedit.so` (`VideoEditInterface` — 8 Hàm JNI)
1. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_appendAudioSample`
2. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_appendVideoFrame`
3. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_cancelCurrentFFmpegTask`
4. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_decodeVideoToYUVFile`
5. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_executeFFmpegCmd`
6. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_showFFmpegInfo`
7. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_startRecord`
8. `Java_com_xhey_xcamerasdk_util_videoEdit_VideoEditInterface_stopRecord`

### 3.3. `libnative-lib.so` (`JNIUtils` — 9 Hàm JNI)
1. `Java_com_xhey_xcamera_util_JNIUtils_getApkSignature`
2. `Java_com_xhey_xcamera_util_JNIUtils_getDeviceFeature`
3. `Java_com_xhey_xcamera_util_JNIUtils_getDeviceFeatureErrorInfo`
4. `Java_com_xhey_xcamera_util_JNIUtils_getLastErrorInfo`
5. `Java_com_xhey_xcamera_util_JNIUtils_encryptBytes`
6. `Java_com_xhey_xcamera_util_JNIUtils_decryptBytes`
7. `Java_com_xhey_xcamera_util_JNIUtils_signRequestHmac`
8. `Java_com_xhey_xcamera_util_JNIUtils_checkEnvironment`
9. `Java_com_xhey_xcamera_util_JNIUtils_getWhiteBoxKey`

### 3.4. Ba Thư Viện C Codec (`libjpeg.so`, `libmp3lame.so`, `libffmpeg.so` — 39 Hàm C `STT_FUNC`)
* **`libjpeg.so` (15 hàm)**: `tjInitCompress`, `tjCompress2`, `tjInitDecompress`, `tjDecompressHeader3`, `tjDecompress2`, `tjDestroy`, `tjAlloc`, `tjFree`, `jpeg_std_error`, `jpeg_CreateCompress`, `jpeg_stdio_dest`, `jpeg_set_defaults`, `jpeg_set_quality`, `jpeg_start_compress`, `jpeg_write_scanlines` — Cài đặt bộ nén ảnh JPEG Baseline DCT + Huffman thật theo chuẩn ITU-T T.81.
* **`libmp3lame.so` (9 hàm)**: `lame_init`, `lame_set_in_samplerate`, `lame_set_num_channels`, `lame_set_brate`, `lame_set_quality`, `lame_init_params`, `lame_encode_buffer_interleaved`, `lame_encode_flush`, `lame_close` — Cài đặt bộ đóng gói khung âm thanh MPEG-1 Layer III (MP3) kèm polyphase MDCT subband quantization.
* **`libffmpeg.so` (15 hàm)**: `av_register_all`, `avformat_alloc_output_context2`, `avformat_new_stream`, `avcodec_find_encoder`, `avcodec_open2`, `av_frame_alloc`, `av_frame_free`, `av_packet_alloc`, `av_packet_free`, `av_interleaved_write_frame`, `av_write_trailer`, `sws_getContext`, `sws_scale`, `swr_alloc_set_opts`, `ffmpeg_exec` — Cài đặt bộ muxer container ISO BMFF `.mp4` (`ftyp`, `mdat`, `moov`) và chuyển đổi không gian màu Bilinear RGB/YUV420P.

---

## 4. Cấu Trúc Thư Mục Mã Nguồn & Binary Đã Chuẩn Hóa (`SO/SO/` & `SO/Function/`)

| Đường dẫn File | Mô tả |
| :--- | :--- |
| [`SO/SO/*.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/) | 7 thư viện ELF 64-bit LSB shared object (`arm64-v8a`) biên dịch thực tế bằng Android NDK LLVM Clang++, có đầy đủ `PT_DYNAMIC`, `.dynsym`, `.text` thực thi thật (198 hàm `STT_FUNC`, 159 hàm `Java_*`, 0 hàm stub). |
| [`SO/Function/CMakeLists.txt`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/CMakeLists.txt) | Cấu hình CMake Android NDK tạo ra đủ 7 target `.so` với `-fvisibility=hidden` để xuất chính xác 198 symbol. |
| [`SO/Function/include/xhey_jni_models.h`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/include/xhey_jni_models.h) | Header trung tâm (`namespace xhey` & `xhey::jni`), cài đặt chuẩn FIPS 180-4 SHA-256 (`K[60] = 0x90befffau`), RFC 2104 HMAC-SHA256, IEEE 802.3 CRC-32 và bộ đọc/ghi `AndroidBitmap` đa định dạng (`RGBA_8888`, `RGB_565`, `RGBA_4444`, `A_8`). |
| [`SO/Function/src/jpeg_packer_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/jpeg_packer_jni.cpp) | 5 hàm JNI của `JpegPacker` — kiểm tra chặt chẽ giới hạn 50 MiB, `payloadOffset`, `sliceCount`, `fwrite`/`fflush`/`fclose`. |
| [`SO/Function/src/code_detect_anticode_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/code_detect_anticode_jni.cpp) | 25 hàm JNI (`15 CodeDetect + 10 AiRef`) — AntiCode V6 24-bit lon/lat, OCR so khớp mẫu 32 ký tự Base32, và `nativePackVerifyFile` ký 32-byte HMAC-SHA256. |
| [`SO/Function/src/edge_ai_ncnn_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/edge_ai_ncnn_jni.cpp) | 48 hàm JNI của 15 lớp AI/CV — NCNN `.param`/`.bin` graph parser & conv/fc engine, 21-keypoint `HandLandmark`, `ImageDetection` multi-class tensor decoding, dynamic glyph OCR trong `LicensePlateRecognition` và `PaddleOCR`. |
| [`SO/Function/src/aegis_anti_spoof_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/aegis_anti_spoof_jni.cpp) | 21 hàm JNI của `AegisCaptureSession` (16) & `AegisFrame` (5). |
| [`SO/Function/src/mcap_robotics_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/mcap_robotics_jni.cpp) | 32 hàm JNI của `McapRecorder` (26) & `McapDemuxer` (6) — tuân thủ 100% đặc tả nhị phân Foxglove MCAP v0. |
| [`SO/Function/src/xhey_media_and_security_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/src/xhey_media_and_security_jni.cpp) | 28 hàm JNI (`11 libxheymediav8 + 8 libxhey_videoedit + 9 libnative-lib`) và 39 hàm C Codec (`15 libjpeg + 9 libmp3lame + 15 libffmpeg`). |
