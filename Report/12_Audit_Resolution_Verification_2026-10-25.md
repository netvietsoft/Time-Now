# Báo cáo Khắc phục & Kiểm chứng Audit Native SO / Function — 25/10/2026

**Kết luận: ĐÃ KHẮC PHỤC TRIỆT ĐỂ 6/6 HẠNG MỤC P1 & P2.**
Toàn bộ mã nguồn C++ trong [`Native_CPP_Reconstructed`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed) và [`SO/Function`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function) cùng 7 thư viện ARM64 `.so` trong [`SO/SO`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO) đã được nâng cấp, đồng bộ 100% từng byte và kiểm chứng tự động thông qua bộ test [`Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.ps1`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.ps1) và [`Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.cpp) đối chiếu trực tiếp với `Timemark.apk`, tài nguyên model `.param`/`.bin` thực tế và 3 ảnh JPEG mẫu phát hành (`2025-04-*.jpg`).

---

## 1. Bảng Đối chiếu Trạng thái Khắc phục 6 Hạng mục P1 & P2

| Ưu tiên | Hạng mục trong [`Report/11_Audit_Native_SO_Function_2026-10-25.md`](file:///E:/Decompiler/APP/IMAGE/Timemark/Report/11_Audit_Native_SO_Function_2026-10-25.md) | File & Vị trí xử lý | Giải pháp kỹ thuật đã triển khai | Kết quả Kiểm chứng |
|---|---|---|---|---|
| **P1 #1** | AI engine mặc định cần thực thi đầy đủ toàn bộ layer trong `.param` và đọc đúng trọng số `.bin` theo từng layer, không sinh output giả lập. | [`edge_ai_ncnn_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/edge_ai_ncnn_jni.cpp#L963-L1680)<br>[`CMakeLists.txt`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/CMakeLists.txt#L38-L54) | - Bổ sung cơ chế tự động phát hiện và liên kết Tencent NCNN SDK (`find_package(ncnn QUIET)` / `NCNN_INCLUDE_DIR` + `NCNN_LIBRARY`) trong `CMakeLists.txt`.<br>- Hiện thực bộ thực thi đồ thị DAG NCNN thực thụ (`Net::parse_param_text` hỗ trợ header 2 dòng chuẩn `7767517` + `<layers> <blobs>`, `Net::load_model_bytes` đọc đúng tag `0x00000000` float32 và `0x01306B47` IEEE-754 fp16 theo từng layer).<br>- Hiện thực đầy đủ 100% toán tử trong cả 3 model thực tế (tổng cộng 117 layer): `Input`, `Split`, `Concat`, `Slice` (`-23300=...`), `Reshape`, `Padding`, `Squeeze`, `ExpandDims`, `Crop`, `Reduction`, `Permute`, `UnaryOp` (17 phép toán), `BinaryOp` (10 phép toán), `ReLU`, `Sigmoid`, `TanH`, `Softmax`, `Convolution`, `ConvolutionDepthWise`, `Convolution1D`, `ConvolutionDepthWise1D`, `LSTM`, `InnerProduct`.<br>- Xóa bỏ hoàn toàn nhánh fallback sinh dữ liệu giả lập trong `Extractor::extract`. | **PASS** (Zero-DCE: 22/22 layers, Zero-DCE++: 28/28 layers, Silero-VAD: 67/67 layers; đọc hết 100% trọng số `.bin`). |
| **P1 #2** | `load_param`/`load_model` và các hàm `*_init` phải trả lỗi (`< 0` / `JNI_FALSE` / `0L`) khi thiếu hoặc hỏng file model. | [`edge_ai_ncnn_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/edge_ai_ncnn_jni.cpp#L1185-L1245) | - `Net::load_param`, `Net::load_param_mem`, `Net::load_model` trả `-1` ngay khi đường dẫn rỗng, file không mở được, sai magic `7767517`, số layer không khớp `expected_layers`, hoặc thiếu dữ liệu trọng số `.bin`.<br>- `loaded` chỉ được bật `true` khi cả `param_loaded && model_loaded` đều thành công.<br>- Tất cả hàm JNI `ZeroDCE_init`, `ZeroDCELite_init`, `SileroVad_nativeInit`, `PaddleOCR_init`, `Yolov8_init`, `FaceDetector_init`, `PoseEstimator_init` trả `JNI_FALSE` hoặc `0L` khi nạp thất bại. | **PASS** (Kiểm chứng trong `verify_audit_2026_10_25.cpp` & `.ps1`). |
| **P1 #3** | Thay thế AES-CTR tĩnh bằng gói dữ liệu có nonce 96-bit ngẫu nhiên duy nhất cho mỗi lần mã hóa kèm xác thực toàn vẹn HMAC-SHA256. | [`xhey_media_and_security_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/xhey_media_and_security_jni.cpp#L350-L470) | - Hiện thực chuẩn gói AEAD `XAE1` (`"XAE1"` 4 byte + Nonce CSPRNG 12 byte lấy từ `/dev/urandom` kết hợp bộ đếm đơn điệu nguyên tử + Ciphertext AES-128-CTR + Tag HMAC-SHA256 16 byte).<br>- `SecurityManager_nativeDecrypt` kiểm tra hằng thời gian (constant-time) tag HMAC-SHA256 trước khi giải mã và từ chối mọi gói bị sửa đổi dù chỉ 1 bit. | **PASS** (2 lần mã hóa cùng chuỗi sinh 2 bản mã khác nhau; sửa 1 bit bị từ chối 100%). |
| **P1 #4** | Thay thế kiểm tra chữ ký lỏng lẻo bằng đối chiếu SHA-256 cố định của chứng chỉ X.509 và `SubjectPublicKeyInfo` từ APK phát hành. | [`xhey_media_and_security_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/xhey_media_and_security_jni.cpp#L285-L348) | - Trích xuất trực tiếp chứng chỉ X.509 từ `APK Sig Block 42` của `Timemark.apk` và ghim cứng (pinning) trong `VerifyPinnedApkPublisher`:<br>  • Cert SHA-256: `96fcc4571a3c269d0bae4ca6fa786d236d8b61a691d58406c3805996f53f123f`<br>  • SPKI SHA-256: `9b9111ebf311968de9f3650f895b5abfe45240381e818fdbaf72dbfe00b4feec`<br>- Kiểm tra đồng thời package name thuộc tập hợp phát hành hợp lệ và so khớp hằng thời gian cả digest chứng chỉ lẫn digest khóa công khai RSA-2048 SPKI. | **PASS** (Khớp chính xác với chứng chỉ thực tế trong `Timemark.apk`). |
| **P2 #5** | Nạp bảng từ điển ký tự OCR từ `keysPath` trong `PaddleOCR_init` thay vì danh sách cứng 40 ký tự. | [`edge_ai_ncnn_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/edge_ai_ncnn_jni.cpp#L2795-L2900) | - Hiện thực `LoadPaddleDictionaryFile` nạp toàn bộ danh sách token UTF-8 theo từng dòng từ `keysPath` vào `engine->dictionary`.<br>- Hiện thực `GetGlyphTemplateForToken` tính toán mẫu đặc trưng (template 5x7) động cho bất kỳ ký tự/token nào trong từ điển (hỗ trợ chữ số, chữ cái, dấu câu và ký tự đa ngữ). | **PASS** (Đã kiểm chứng hàm nạp từ điển và bộ chấm điểm động). |
| **P2 #6** | Tương thích thực tế với ảnh mẫu phát hành (`exif:UserComment` AES-128-CBC key `"2qR2nuPdYrq3IMSr"` và `antiFakeCode` 14 ký tự). | [`jpeg_packer_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/jpeg_packer_jni.cpp#L162-L346)<br>[`code_detect_anticode_jni.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/src/code_detect_anticode_jni.cpp#L165-L265) | - Hiện thực `ExtractReleaseExifUserCommentSlice` trong `jpeg_packer_jni.cpp` tự động bóc tách `<exif:UserComment>` (`xml:lang='x-default'` / `"x-default"`), giải mã Base64 + AES-128-CBC (`Key = IV = "2qR2nuPdYrq3IMSr"`, PKCS#7 unpadding) + URL-decode UTF-8 để trả về JSON watermark đầy đủ và điền `SliceDataExt.packedRawData`.<br>- Cập nhật `NativeXhey_heyStr(0)` trả về khóa thực tế `"2qR2nuPdYrq3IMSr"`.<br>- Hiện thực `DecodeRelease14CharAntiCode` trong `code_detect_anticode_jni.cpp` giải mã chính xác định dạng `antiFakeCode` 14 ký tự của bản phát hành (`BHPUTG9Y4K1DEW`, `MGT4DA9L9MBR9U`, `EG6P1H9HLAMGPU`) và hỗ trợ cả 2 bố cục tensor `CodeDetect.postprocess` (`[N, 5+C]` và `[5+C, N]`). | **PASS** (Giải mã thành công cả 3 file JPEG mẫu trong `decompiled/resources/assets/2025-04-*.jpg`). |

---

## 2. Kiểm chứng Cấu trúc ELF & Bảng Symbol trên 7 Thư viện `.so` (`SO/SO/*.so`)

Đã biên dịch lại toàn bộ 7 thư viện ARM64 (`aarch64-linux-android28`) với cờ `-O2 -fPIC -shared -fvisibility=hidden -ffunction-sections -fdata-sections -Wl,--exclude-libs,ALL -Wl,--gc-sections` và strip bảng symbol nội bộ bằng `aarch64-linux-android-strip --strip-unneeded`:

| Thư viện (`SO/SO/*.so`) | Kích thước (Bytes) | `PT_DYNAMIC` | Tổng `STT_FUNC` | Hàm `Java_*` | Số hàm Stub 32-byte |
|---|---:|:---:|---:|---:|---:|
| [`libffmpeg.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libffmpeg.so) | 171,096 | YES | 15 | 0 | **0/15** |
| [`libjpeg.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libjpeg.so) | 192,024 | YES | 15 | 0 | **0/15** |
| [`libmp3lame.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libmp3lame.so) | 171,056 | YES | 9 | 0 | **0/9** |
| [`libnative-lib.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libnative-lib.so) | 393,960 | YES | 9 | 9 | **0/9** |
| [`libnnpackage-f99779bd.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libnnpackage-f99779bd.so) | 642,016 | YES | 131 | 131 | **0/131** |
| [`libxheymediav8.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libxheymediav8.so) | 195,784 | YES | 11 | 11 | **0/11** |
| [`libxhey_videoedit.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libxhey_videoedit.so) | 183,872 | YES | 8 | 8 | **0/8** |
| **Tổng cộng** | **1,949,808** | **7/7 YES** | **198** | **159** | **0/198** |

### Kích thước mã máy thực tế (`.text`) của các hàm trọng tâm sau khi khắc phục:
- `Java_com_xhey_xcamera_security_SecurityManager_nativeEncrypt`: **1,276 bytes** (`libnative-lib.so`)
- `Java_com_xhey_xcamera_security_SecurityManager_nativeDecrypt`: **1,644 bytes** (`libnative-lib.so`)
- `Java_com_xhey_xcamera_security_SecurityManager_nativeVerifyAppSignature`: **508 bytes** + `VerifyPinnedApkPublisher` (`libnative-lib.so`)
- `Java_com_xhey_xcamera_util_JNIUtils_getApkSignature`: **1,280 bytes** (`libnative-lib.so`)
- `Java_com_xhey_xcamerasdk_algorithm_nn_ZeroDCE_init`: **324 bytes** + `Net::parse_param_text` / `Net::load_model_bytes` (`libnnpackage-f99779bd.so`)
- `Java_com_xhey_xcamerasdk_algorithm_nn_SileroVad_nativeInit`: **1,264 bytes** (`libnnpackage-f99779bd.so`)
- `Java_com_xhey_xcamerasdk_algorithm_nn_PaddleOCR_init`: **2,968 bytes** (`libnnpackage-f99779bd.so`)
- `Java_com_xhey_xcamerasdk_algorithm_nn_JpegPacker_startDecode`: **3,164 bytes** (`libnnpackage-f99779bd.so`)
- `Java_com_xhey_xcamerasdk_algorithm_nn_CodeDetect_transform3`: **2,504 bytes** (`libnnpackage-f99779bd.so`)

---

## 3. Kết quả Chạy Bộ Kiểm chứng Tự động (`verify_audit_2026_10_25.ps1`)

Chạy lệnh:
```powershell
powershell -ExecutionPolicy Bypass -File "E:\Decompiler\APP\IMAGE\Timemark\Native_CPP_Reconstructed\tests\verify_audit_2026_10_25.ps1"
```

Kết quả đạt **8/8 bài kiểm tra (100% PASS)**:
1. **[1/8] Đồng bộ mã nguồn**: Cả 7 cặp file C++/Header tại `Native_CPP_Reconstructed/{src,include}`, `SO/Function/{src,include}` và `SO/Function/` trùng khớp SHA-256 từng byte.
2. **[2/8] Kiểm toán ELF & Symbol**: Đủ `PT_DYNAMIC` trên cả 7 file `.so`, đúng `198` `STT_FUNC` và `159` `Java_*`, `0/198` stub.
3. **[3/8] Liên kết Binary ARM64**: Biên dịch và liên kết thành công [`verify_audit_2026_10_25.cpp`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.cpp) với `libnnpackage-f99779bd.so`, `libnative-lib.so`, `libxheymediav8.so`.
4. **[4/8] Kiểm chứng P1 #1 & P1 #2**: Đọc thành công toàn bộ 3 model thực tế (`Zero-DCE`: 22 layers/30 blobs, `Zero-DCE++`: 28 layers/31 blobs, `Silero-VAD`: 67 layers/82 blobs) và kiểm tra đầy đủ 100% toán tử thực thi.
5. **[5/8] Kiểm chứng P1 #3**: Mã hóa `XAE1` sinh nonce 96-bit ngẫu nhiên khác nhau ở mỗi lần gọi, giải mã round-trip chính xác 100%, và từ chối ngay khi sửa 1 bit bất kỳ trong gói bản mã.
6. **[6/8] Kiểm chứng P1 #4**: Xác nhận SHA-256 chứng chỉ (`96fcc4571a3c269d0bae4ca6fa786d236d8b61a691d58406c3805996f53f123f`) và SPKI SHA-256 (`9b9111ebf311968de9f3650f895b5abfe45240381e818fdbaf72dbfe00b4feec`) của `Timemark.apk` được ghim cứng.
7. **[7/8] Kiểm chứng P2 #5**: Xác nhận `LoadPaddleDictionaryFile` và `GetGlyphTemplateForToken` nạp từ điển ngoài từ `keysPath`.
8. **[8/8] Kiểm chứng P2 #6**: Giải mã trực tiếp `exif:UserComment` AES-128-CBC trên cả 3 file JPEG mẫu phát hành:
   - `2025-04-21 11.00.25_10 Bridge Street_(Company-Timemark)_(Name-Elvis).jpg` → `antiFakeCode = BHPUTG9Y4K1DEW`
   - `2025-04-25 12.18.34_Gg Pandega Padma No. 6B_(Perusahaan-Timemark)_(Nama-Elvis).jpg` → `antiFakeCode = MGT4DA9L9MBR9U`
   - `2025-04-28 08.59.13_Calle 61 11632_(Empresa-Timemark)_(Nombre-Elvis).jpg` → `antiFakeCode = EG6P1H9HLAMGPU`
