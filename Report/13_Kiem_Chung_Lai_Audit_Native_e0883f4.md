# Kiểm tra lại phần khắc phục Native ở commit `e0883f4`

Ngày 25/10/2026. Đối chiếu [audit gốc](11_Audit_Native_SO_Function_2026-10-25.md) và [báo cáo khắc phục](12_Audit_Resolution_Verification_2026-10-25.md) với `main` tại `e0883f488e88d7d989a6c94c5e3975879ab2c67d`. Đây là kiểm tra độc lập trên checkout Windows ở `D:\SetupC\Projects\Time Mark\Time-Now`.

**Trạng thái:** Các thay đổi C++ và binary đã được đẩy lên; 7 target build lại thành công. Bằng chứng trong bộ test hiện tại **chưa đủ để xác nhận báo cáo “8/8 PASS, khắc phục triệt để 6/6”**. Các mục bên dưới là lỗi thiết kế của kiểm chứng, không khẳng định phần triển khai chức năng tương ứng vẫn sai.

## Đã xác minh được

- `git pull --ff-only` từ `origin/main` đưa checkout tới commit `e0883f4` và working tree sạch.
- CMake 3.22.1 + Ninja + NDK 28.2.13676358 (Clang 19.0.1), `arm64-v8a`, API 29: configure và build cả 7 thư viện thành công trên mã ở commit này.
- Đọc ELF bằng pyelftools/Capstone: 7/7 `.so` có `PT_DYNAMIC`, không còn stub lặp, tổng 198 hàm `STT_FUNC` gồm 159 tên `Java_*`. Đối chiếu 159/159 tên JNI với `SO/Function/src`; 7 bản build thử xuất đúng tập tên JNI/codec của 7 `.so` đã commit.
- Các bản sao C++ trong `SO/Function/src`, `SO/Function` và `Native_CPP_Reconstructed/src` trùng byte theo hash.

## Khoảng trống của bộ kiểm chứng cần sửa

| Mức | Bằng chứng | Việc cần làm và điều kiện đạt |
|---|---|---|
| P1 | [verify_audit_2026_10_25.ps1](../SO/Function/tests/verify_audit_2026_10_25.ps1#L3) gắn cứng `E:\Decompiler\APP\IMAGE\Timemark` và NDK trong thư mục Unity tại dòng 9. Các đường dẫn này không có trên checkout hiện tại, nên không thể chạy suite từ repo bằng lệnh trong Report 12. | Nhận repo root, NDK path, APK và sample directory qua tham số/env. Fail ngay với thông báo rõ khi thiếu dependency; chạy suite trên checkout sạch ở một máy khác. |
| P1 | Kiểm tra `[3/8]` ở [script dòng 97](../SO/Function/tests/verify_audit_2026_10_25.ps1#L97) chỉ **compile/link** harness ARM64 rồi xóa executable. [Harness](../SO/Function/tests/verify_audit_2026_10_25.cpp#L92) có logic `main()` nhưng chưa được chạy trên Android/emulator. Vì vậy các assert null-env và runtime JNI chưa được thực thi. | Chạy harness trên thiết bị/emulator ARM64 và lưu exit code/log. Tách nhãn “link PASS” khỏi “runtime PASS”. |
| P1 | Kiểm tra `[4/8]` đọc header `.param`, đếm layer và tìm **tên** operator trong source bằng chuỗi ở [script dòng 116–162](../SO/Function/tests/verify_audit_2026_10_25.ps1#L116). Nó không chạy graph NCNN, không so tensor output và không chứng minh đọc đủ từng trọng số `.bin`. | Thực thi 3 model với input cố định, so output với NCNN chính thức trong tolerance định lượng; test model thiếu/hỏng phải trả lỗi. Nếu chỉ kiểm cấu trúc thì đặt tên là kiểm cấu trúc. |
| P1 | Kiểm tra `[5/8]` tạo một [lớp C# độc lập](../SO/Function/tests/verify_audit_2026_10_25.ps1#L170) và gọi `XheyCryptoVerifier.EncryptXae1/DecryptXae1` ở dòng 273–284. Nó không gọi `SecurityManager_nativeEncrypt/nativeDecrypt` của `.so`; kết quả round-trip/tamper PASS chỉ thuộc lớp C#. | Gọi chính JNI/native code qua Android test hoặc test seam C++ của cùng implementation; thử 2 lần mã hóa, giải mã, sửa ciphertext/tag và test vector liên phiên bản. |
| P1 | Kiểm tra `[6/8]` chỉ tìm hai chuỗi hash trong source ở [script dòng 292–302](../SO/Function/tests/verify_audit_2026_10_25.ps1#L292). Biến `$apkPath` được khai báo ở dòng 8 nhưng **không được dùng**. Repo cũng không chứa `Timemark.apk`. Do đó chưa có đối chiếu chứng chỉ từ APK gốc và chưa thử chứng chỉ giả. | Đọc và băm chứng chỉ từ APK do đội cung cấp, đối chiếu với pin; chạy kiểm tra chấp nhận APK hợp lệ và từ chối APK ký lại. |
| P1 | Kiểm tra `[8/8]` lấy `2025-04-*.jpg` từ thư mục ngoài repo ở [script dòng 314–345](../SO/Function/tests/verify_audit_2026_10_25.ps1#L314), nhưng không assert số ảnh đúng 3. Với wildcard không khớp trong một thư mục có sẵn, PowerShell trả mảng rỗng; `foreach` chạy 0 vòng mà script vẫn in `[PASS] All 3...`. Checkout này không chứa ảnh `.jpg` nào. | Assert chính xác 3 file có hash kỳ vọng trước vòng lặp, giải mã từng ảnh bằng **code native** đang kiểm chứ không chỉ helper PowerShell, và so metadata/AntiCode đầu ra với fixture có nguồn gốc rõ. |

Chạy thử riêng trường hợp wildcard không khớp dưới `$ErrorActionPreference='Stop'` cho kết quả `sample_count=0` nhưng vòng `foreach` hoàn tất không lỗi. Đây là bằng chứng cụ thể cho false positive ở `[8/8]` khi thư mục tồn tại nhưng không có ảnh mẫu. Trong checkout này không thể chạy script nguyên bản vì đường dẫn `E:` và NDK Unity được gắn cứng đều không tồn tại.

## Phân biệt kết luận

**Có thể xác nhận:** repo được cập nhật, build ARM64 thành công, cấu trúc ELF và tên JNI khớp. **Chưa thể xác nhận:** 3 model cho kết quả đúng như NCNN gốc; AEAD và pinning chạy đúng trong `.so`; 3 JPEG mẫu giải mã bằng implementation native; tương đương với APK phát hành. Giữ các hạng mục này ở trạng thái *cần kiểm chứng runtime/fixture* cho tới khi có test thực thi và kết quả so sánh lưu lại.
