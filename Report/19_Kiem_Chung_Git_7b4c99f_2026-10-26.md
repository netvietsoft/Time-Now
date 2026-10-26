# Kiểm chứng commit `7b4c99f` — 26/10/2026

Đã kéo `origin/main` tới `7b4c99f`. Trên NDK `28.2.13676358` (Clang/LLD 19), script nguyên bản đạt `[1/8]`, `[2/8]` và `[3/8] [LINK PASS]` cho cả bảy `.so`. Lỗi kiểu `uintptr_t` trong `Report/17` đã hết: bước 3B biên dịch được các object của host runtime harness. Tuy nhiên **suite vẫn dừng trước khi chạy harness**, nên chưa xác nhận được `[RUNTIME PASS]` hay 8/8 trên môi trường này.

Lệnh tái hiện:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\SO\Function\tests\verify_audit_2026_10_25.ps1 -NdkPath 'C:\Users\PC.DESKTOP-81LIH38\AppData\Local\Android\Sdk\ndk\28.2.13676358'
```

Lỗi đầu tiên của bước link Windows x64:

```text
ld.lld: error: undefined symbol: std::__ndk1::basic_string<char, ...> std::__ndk1::operator+<char, ...>(char const*, std::__ndk1::basic_string<char, ...> const&)
>>> referenced by ...\xhey_media_and_security_jni.cpp.obj:((anonymous namespace)::VerifyPinnedApkPublisher(...))
Failed to link native runtime harness executable: ...\verify_audit_2026_10_25_win64.exe
```

Như vậy, thay đổi `-D_LIBCPP_DISABLE_EXTERN_TEMPLATE=1` và `template class basic_string<char>` trong `Report/18` chưa cung cấp đủ symbol libc++ mà mã native sử dụng trên NDK 28. Nhánh host build dùng `ld.lld` liên kết các object và import library `msvcrt` thủ công, nên cần xử lý đầy đủ các template/function phụ thuộc libc++ theo target Windows x64, hoặc dùng toolchain/host harness thống nhất để linker tự liên kết runtime phù hợp.

Đề nghị chạy lại script trên **clean checkout với NDK 28.2.13676358**, lưu log đầy đủ tới `[8/8]`, và chỉ tuyên bố môi trường này PASS sau khi executable vừa build chạy được sáu cụm test. Log trong `Report/18` ghi NDK Unity `19.0.5232133`, không phải kết quả NDK 28. Báo cáo này không kết luận các hàm Android native sai; lỗi hiện tại thuộc khâu link host harness.
