# Kiểm chứng commit `21ffee2` — 25/10/2026

## Kết luận

Đã kéo `origin/main` tới `21ffee2dafe34aa48a339967850ebc4b27dce224`. Commit bổ sung test runtime C++ và fixture cho các khoảng trống trong `Report/13`, nhưng **bộ kiểm chứng hiện không đạt trên checkout này**. Lệnh kiểm chứng chính thức dừng tại `[3/8] [LINK FAIL]`, trước khi chạy sáu bài test runtime. Do đó không thể xác nhận kết luận “8/8 PASS” trong `Report/14` trên các `.so` đã commit.

## Cách tái hiện

Trên Windows, với Android NDK `28.2.13676358`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\SO\Function\tests\verify_audit_2026_10_25.ps1 -NdkPath 'C:\Users\PC.DESKTOP-81LIH38\AppData\Local\Android\Sdk\ndk\28.2.13676358'
```

`[1/8]` đồng bộ source và `[2/8]` thống kê ELF đều PASS. Tại bước link ARM64, `ld.lld` báo lỗi giống nhau cho `libnnpackage-f99779bd.so`, `libnative-lib.so`, `libxheymediav8.so`:

```text
invalid local symbol '_edata' in global part of symbol table
invalid local symbol '_end' in global part of symbol table
invalid local symbol '__bss_start' in global part of symbol table
[LINK FAIL] Failed to link ARM64 test harness against SO/SO/*.so
```

## Nguyên nhân được xác nhận

Trong `.dynsym` của `SO/SO/libnative-lib.so`, section header có `sh_info = 3` (số đầu tiên thuộc phần symbol global). Tuy nhiên `llvm-readelf --dyn-syms` hiển thị các symbol local ở chỉ số 3, 4, 5 (`_edata`, `_end`, `__bss_start`); symbol global đầu tiên thực tế là chỉ số 6. Hai thư viện còn lại cũng có ba symbol local này ở chỉ số 3–5. Đây là lỗi metadata ELF của binary đã commit, không phải lỗi tìm đường dẫn NDK. Việc chỉ đếm `STT_FUNC`/JNI exports ở `[2/8]` không phát hiện được lỗi này.

## Việc cần làm

1. Tạo lại hoặc sửa ba `.so` nói trên để `sh_info` của `.dynsym` phản ánh đúng ranh giới local/global; kiểm tra cả bảy `.so` sau bước xử lý binary cuối cùng (nếu có strip/patch).
2. Chạy lệnh kiểm chứng từ **clean checkout** trên NDK được hỗ trợ; lưu log link và runtime mới. Chỉ công bố `[RUNTIME PASS]` nếu bước link ARM64 đã PASS và harness thực sự chạy thành công.
3. Bổ sung kiểm tra cấu trúc `.dynsym`/`sh_info` trong suite trước khi in kết quả `[2/8] PASS`, để lỗi binary không bị bỏ sót.

Các kiểm thử runtime từ `[NATIVE 1/6]` đến `[NATIVE 6/6]` **chưa được chạy trong lần xác minh này** vì script dừng đúng tại lỗi link. Báo cáo này không kết luận chúng sai; chỉ ghi nhận rằng tuyên bố PASS của `Report/14` chưa tái lập được với artifact hiện tại.
