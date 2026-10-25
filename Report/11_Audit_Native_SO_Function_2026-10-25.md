# Audit Native `.so` và mã C++ — 25/10/2026

Audit trên `main` tại commit `874f5c4012ac6ce9690ae92e5ad4ad5b2e7ddae4` của `netvietsoft/Time-Now`. Phạm vi: `SO/SO/*.so`, `SO/Function/src`, `SO/Function/include` và `SO/Function/CMakeLists.txt`. Đã đọc ELF AArch64, so sánh symbol và build thử bằng Android NDK. Chưa có APK/split `.so` phát hành gốc, DEX/Java gốc hoặc dữ liệu mẫu chuẩn để kiểm tra tương đương hành vi.

## Kết luận để đội tiếp tục

**Bản này đã sửa các lỗi đóng gói của bộ cũ và build được. Chưa thể kết luận rằng thuật toán được giải mã đúng từ ứng dụng gốc.** Những điểm dưới đây là lỗi hoặc khoảng trống của *implementation hiện tại*, không phải khẳng định về source nguyên bản của TimeMark.

| Kiểm tra | Kết quả trên commit này |
|---|---|
| 7 ELF ARM64 | Đủ `PT_DYNAMIC`; `.text` chứa mã máy khác nhau. Không còn 198 hàm stub cùng đặt `x0=0` rồi `ret` như bản lưu ngày 22/10. |
| Symbol | 198 hàm ELF `STT_FUNC`, trong đó 159 tên `Java_*`. Source có đúng 159 tên JNI tương ứng; không thiếu/thừa tên. |
| Build | CMake/Ninja, NDK `28.2.13676358` (Clang 19.0.1), ABI `arm64-v8a`, API 29: cả 7 target build thành công. Các binary build thử xuất đủ tên JNI/codec có trong bộ `.so` của repo. |
| Giới hạn | Trùng tên symbol và build thành công xác nhận cấu trúc JNI/C++; chưa xác nhận Java method signature, thuật toán, định dạng file hoặc output bằng bản phát hành gốc. |

Đã build các target `nnpackage-f99779bd`, `xheymediav8`, `xhey_videoedit`, `native-lib`, `jpeg`, `mp3lame`, `ffmpeg`. Có thể tái lập bằng:

```sh
cmake -S SO/Function -B build/native-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=<NDK>/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29
cmake --build build/native-arm64 -- -j4
```

## Việc cần làm theo mức ưu tiên

| Mức | Phát hiện, bằng chứng | Kết quả cần đạt |
|---|---|---|
| P1 | **AI mặc định không chạy NCNN chính thức.** [edge_ai_ncnn_jni.cpp](../SO/Function/src/edge_ai_ncnn_jni.cpp#L60) dùng lớp `ncnn` tự viết nếu thiếu `net.h`; [CMakeLists.txt](../SO/Function/CMakeLists.txt) không tìm hoặc link NCNN. Lớp này chỉ xử lý một số loại layer, còn layer khác copy input sang output ở [dòng 376](../SO/Function/src/edge_ai_ncnn_jni.cpp#L376). Khi không có output, nó tự tổng hợp output cho ảnh/VAD hoặc trả lại input ở [dòng 384](../SO/Function/src/edge_ai_ncnn_jni.cpp#L384). | Dùng NCNN thật với model tương ứng và kiểm thử output từng module (ZeroDCE, Silero, detection, OCR...) trên bộ ảnh/audio mẫu có kết quả chuẩn; nếu giữ fallback, đánh dấu rõ đây là chế độ thử nghiệm và không gọi đó là suy luận model gốc. |
| P1 | **Thiếu model vẫn có thể báo init thành công.** `load_param` và `load_model` trả `0`, đặt `loaded=true` khi không mở được file ở [dòng 542](../SO/Function/src/edge_ai_ncnn_jni.cpp#L542) và [dòng 562](../SO/Function/src/edge_ai_ncnn_jni.cpp#L562). `ZeroDCE_init` bỏ qua hai mã lỗi rồi trả `JNI_TRUE` ở [dòng 829](../SO/Function/src/edge_ai_ncnn_jni.cpp#L829). | File `.param`/`.bin` thiếu hoặc hỏng phải làm init thất bại và có log/error code rõ; test đủ trường hợp có model, thiếu một file và file lỗi. |
| P1 | **AES-CTR dùng cùng nonce cho mọi lần mã hóa.** [nativeEncrypt](../SO/Function/src/xhey_media_and_security_jni.cpp#L1372) và [nativeDecrypt](../SO/Function/src/xhey_media_and_security_jni.cpp#L1382) dùng cùng khóa biên dịch sẵn và nonce cố định `0x5848455932303235`. Như vậy hai plaintext được mã hóa bằng cùng dòng khóa. [NIST SP 800-38A](https://nvlpubs.nist.gov/nistpubs/legacy/sp/nistspecialpublication800-38a.pdf) yêu cầu counter block là duy nhất cho mỗi block dưới cùng một khóa, kể cả giữa các thông điệp. | Thiết kế nonce duy nhất mỗi lần mã hóa, lưu nonce cùng ciphertext, xác thực dữ liệu (hoặc dùng chế độ AEAD phù hợp) và thêm test mã hóa lặp lại/giải mã/sửa dữ liệu. Cần xem xét định dạng hiện tại trước khi đổi để không làm mất khả năng đọc dữ liệu cũ. |
| P1 | **Hàm xác minh APK chưa xác minh nhà phát hành.** [nativeVerifyAppSignature](../SO/Function/src/xhey_media_and_security_jni.cpp#L1402) chấp nhận package có chữ `xhey` hoặc `camera`, chứng chỉ dài ít nhất 128 byte và byte đầu hash khác 0. Không so sánh fingerprint hoặc public key với giá trị tin cậy. | Đối chiếu certificate digest/public key của APK với giá trị được xác nhận từ bản phát hành hợp lệ; test APK đúng chữ ký, package giả và APK ký bằng chứng chỉ khác. |
| P2 | **PaddleOCR bỏ qua `keysPath`.** [PaddleOCR_init](../SO/Function/src/edge_ai_ncnn_jni.cpp#L1966) gán từ điển 40 ký tự cố định và `(void)keysPath` ở [dòng 1984](../SO/Function/src/edge_ai_ncnn_jni.cpp#L1984). Với model có bảng ký tự khác, chỉ số CTC sẽ giải ra sai chữ. | Đọc từ điển đúng của model từ `keysPath`; kiểm thử model + keys trên mẫu có chữ số, dấu câu và ký tự không thuộc từ điển cũ. |
| P2 | **Tính tương thích format riêng còn chưa kiểm chứng.** [JpegPacker](../SO/Function/src/jpeg_packer_jni.cpp#L22) dùng trailer `XHEYPK01` và XOR `0x5A`; [AntiCode V6/V7](../SO/Function/src/code_detect_anticode_jni.cpp#L155) dùng định dạng Base32/CRC do source hiện tại định nghĩa. Build ra binary từ chính source này không chứng minh chúng đọc/ghi được dữ liệu từ ứng dụng phát hành gốc. | Thu thập JPEG/AntiCode mẫu từ đúng phiên bản gốc và làm test đọc, ghi rồi so byte/metadata, gồm dữ liệu hợp lệ, sai checksum và nhiều slice. Nếu format khác, sửa tài liệu và source theo mẫu thực. |

## Các lỗi cũ đã được sửa trong bản Git

- [CMakeLists.txt](../SO/Function/CMakeLists.txt) tạo đủ 7 thư viện và link GLES/AndroidBitmap; các lỗi compile namespace `xhey::jni` và link thiếu thư viện của bản lưu cũ không còn xảy ra.
- [AntiCode V6](../SO/Function/src/code_detect_anticode_jni.cpp#L155) lưu đủ byte lượng tử hóa kinh/vĩ độ; lỗi làm mất 11 bit vĩ độ trong bản cũ không còn ở đây.
- [JpegPacker](../SO/Function/src/jpeg_packer_jni.cpp#L22) giới hạn số slice/payload và kiểm tra footer trước cấp phát lớn.
- OCR và biển số không còn trả cố định chuỗi `TIMEMARK`/`PLATE` như bản cũ. Điều này chưa thay cho test accuracy.

## Bằng chứng và giới hạn

- Audit ELF: pyelftools + Capstone, so sánh toàn bộ thân hàm và symbol; 7 file mới đều có `PT_DYNAMIC`, các thân hàm trong từng file không còn cùng một stub. Đối chiếu chính xác 159 tên `Java_*` giữa ELF và source. Build thử 7 `.so` và so tên export với 7 `.so` trong repo: không thiếu tên JNI/codec.
- Không chạy trên thiết bị Android, không kiểm tra runtime JNI/Java và không so sánh với APK/DEX hoặc `.so` nguyên bản. Repo hiện không chứa các artifact gốc này; do đó yêu cầu “đúng 100% bản gốc” vẫn chưa có cơ sở chứng minh.
- Bộ build test: Windows, CMake 3.22.1, Ninja, NDK 28.2.13676358, Android API 29. Bản Git sạch trước audit; report này không thay đổi source hay binary.

Đề nghị đội tiếp theo ưu tiên bộ mẫu gốc và kiểm thử đối chiếu cho 4 nhóm: JNI/Java contract, ảnh JPEG + AntiCode, model AI/OCR/VAD, và bảo mật. Mỗi kết luận “đã giải mã đúng” nên gắn với mẫu đầu vào, đầu ra và phiên bản APK cụ thể.
