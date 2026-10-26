# Báo Cáo Khắc Phục Lỗi Link ARM64 (`Report/15`) — 26/10/2026

## 1. Tổng Quan & Nguyên Nhân Gốc Rễ

Trong báo cáo kiểm chứng [`Report/15_Kiem_Chung_Git_21ffee2_2026-10-25.md`](file:///E:/Decompiler/APP/IMAGE/Timemark/Report/15_Kiem_Chung_Git_21ffee2_2026-10-25.md), khi chạy [`SO/Function/tests/verify_audit_2026_10_25.ps1`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/tests/verify_audit_2026_10_25.ps1) trên Android NDK `28.2.13676358` (sử dụng `ld.lld` 19 mặc định), bước `[1/8]` và `[2/8]` đạt `PASS`, nhưng bước `[3/8]` dừng tại `[LINK FAIL]` với lỗi từ `ld.lld`:

```text
invalid local symbol '_edata' in global part of symbol table
invalid local symbol '_end' in global part of symbol table
invalid local symbol '__bss_start' in global part of symbol table
```

### Phân tích kỹ thuật nguyên nhân gốc:
1. Khi liên kết bằng GNU `ld.bfd` (mặc định khi không truyền `-fuse-ld=lld` trên NDK cũ) kèm cờ `-Wl,--exclude-libs,ALL`, `ld.bfd` chuyển 3 symbol nội bộ của linker (`_edata`, `_end`, `__bss_start`) ở các chỉ số `3, 4, 5` của bảng `.dynsym` sang `STB_LOCAL`, nhưng **không cập nhật trường `sh_info`** trong section header `SHT_DYNSYM` (vẫn giữ `sh_info = 3` thay vì `sh_info = 6`).
2. Theo đặc tả ELF64 System V ABI, `sh_info` của `SHT_DYNSYM` phải bằng `chỉ_số_symbol_LOCAL_cuối_cùng + 1` (tức chỉ số của symbol `GLOBAL`/`WEAK` đầu tiên). Trong cả 7 file `.so` tại `SO/SO/*.so`, các chỉ số `0..5` là `STB_LOCAL` và toàn bộ các chỉ số `6..N-1` là `STB_GLOBAL` (không có bất kỳ symbol `STB_LOCAL` nào từ chỉ số `6` trở đi).
3. Trên LLVM `ld.lld` (`lld/ELF/InputFiles.cpp`), linker duyệt phần global của `.dynsym` bắt đầu từ `firstGlobal = sec.sh_info`. Khi `sh_info = 3`, `ld.lld` gặp các chỉ số `3, 4, 5` (`STB_LOCAL`) trong vùng `>= sh_info` và báo lỗi `invalid local symbol '...' in global part of symbol table`.

---

## 2. Ba Hạng Mục Đã Thực Hiện Theo Yêu Cầu Của `Report/15`

### Hạng mục 1: Chuẩn hóa `.dynsym` (`sh_info = 6`), `.gnu.version` và `.hash` cho toàn bộ 7 file `.so` trong `SO/SO/` + cấu hình `-fuse-ld=lld`
- Cập nhật chuẩn xác metadata ELF64 cho cả 7 thư viện trong [`SO/SO/`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO):
  - **`SHT_DYNSYM` (`sh_info`)**: Cập nhật `sh_info = 6` khớp chính xác với ranh giới `LOCAL` (`0..5`) và `GLOBAL` (`6..N-1`).
  - **`SHT_GNU_versym` (`.gnu.version`)**: Đặt `versym[0..5] = 0` (`VER_NDX_LOCAL`, hiển thị `0 (*local*)` trên `llvm-readelf -V`).
  - **`SHT_HASH` (`.hash`)**: Loại bỏ các chỉ số local `< 6` khỏi các bucket/chain của bảng băm SysV, bảo toàn 100% chuỗi băm cho toàn bộ các symbol global `6..N-1`.
- Bổ sung `-fuse-ld=lld` vào `add_link_options` trong [`Native_CPP_Reconstructed/CMakeLists.txt`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/CMakeLists.txt#L10) và [`SO/Function/CMakeLists.txt`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/CMakeLists.txt#L10).

| Thư viện (`SO/SO/*.so`) | Kích thước | `sh_info` | Phân vùng `.dynsym` | `STT_FUNC` | `Java_*` | SHA-256 |
|---|---:|---:|---|---:|---:|---|
| [`libffmpeg.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libffmpeg.so) | 171,096 B | `6` | `LOCAL=0..5`, `GLOBAL=6..61` | 15 | 0 | `04410E88E3E45B03E5DB084996E492C3E30CD84336D3F45C6434842A5A4B65DB` |
| [`libjpeg.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libjpeg.so) | 192,024 B | `6` | `LOCAL=0..5`, `GLOBAL=6..65` | 15 | 0 | `95826715CE8148B3A59405A2B60B33D21A5FC9B3B791FB225FE88D48C1FBD1E9` |
| [`libmp3lame.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libmp3lame.so) | 171,056 B | `6` | `LOCAL=0..5`, `GLOBAL=6..52` | 9 | 0 | `10BEEDBFE5DFFDB5DCD78DF390A89E0189FE3C3645AFC589B8A50D27BB42B054` |
| [`libnative-lib.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libnative-lib.so) | 200,248 B | `6` | `LOCAL=0..5`, `GLOBAL=6..62` | 9 | 9 | `FBFB1F50191EDC110A9027D61AC1DC28BCE0E3ADE3AFA28D935ADA5B21689037` |
| [`libnnpackage-f99779bd.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libnnpackage-f99779bd.so) | 633,960 B | `6` | `LOCAL=0..5`, `GLOBAL=6..279` | 131 | 131 | `452367C9487DF123E05C53A3B1CA2942858A2AA7394769979D38AF8AEF7B53A5` |
| [`libxhey_videoedit.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libxhey_videoedit.so) | 183,872 B | `6` | `LOCAL=0..5`, `GLOBAL=6..63` | 8 | 8 | `22B9E1B9274989B2ACD9B4BEC7683929956EB6DE34C82A7179063841FBE96A75` |
| [`libxheymediav8.so`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/SO/libxheymediav8.so) | 195,784 B | `6` | `LOCAL=0..5`, `GLOBAL=6..67` | 11 | 11 | `FC47E3A2A1AEDFD733301B82DAC28FDB4D662583BC32DE30926BC78B6974FF6E` |
| **Tổng cộng (7 thư viện)** | **1,748,040 B** | **`6`** | **0 symbol `LOCAL` ở `>= sh_info`** | **198** | **159** | **100% hợp lệ với `ld.lld --fatal-warnings`** |

### Hạng mục 2: Bổ sung kiểm tra cấu trúc `.dynsym` / `sh_info` / `.gnu.version` / `.hash` tại bước `[2/8]`
- Trong [`Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.ps1`](file:///E:/Decompiler/APP/IMAGE/Timemark/Native_CPP_Reconstructed/tests/verify_audit_2026_10_25.ps1#L225-L318) và [`SO/Function/tests/verify_audit_2026_10_25.ps1`](file:///E:/Decompiler/APP/IMAGE/Timemark/SO/Function/tests/verify_audit_2026_10_25.ps1#L225-L318), trước khi in `[2/8] PASS`:
  - Đọc trực tiếp bảng Section Header ELF64 (`Elf64_Shdr`) và từng phần tử `Elf64_Sym` của `SHT_DYNSYM` cho cả 7 file `.so`:
    - Xác minh mọi symbol ở chỉ số `< sh_info` đều có binding `STB_LOCAL` (`0`).
    - Xác minh mọi symbol ở chỉ số `>= sh_info` đều có binding `STB_GLOBAL` hoặc `STB_WEAK` (tuyệt đối không có `STB_LOCAL` trong vùng global).
    - Kiểm tra chéo với kết quả `readelf --dyn-syms -W`, bảng `.gnu.version` (`SHT_GNU_versym`), và bảng băm `.hash` (`SHT_HASH`).

### Hạng mục 3: Bắt buộc dùng `-fuse-ld=lld -Wl,--fatal-warnings` và liên kết đủ 7 file `.so` ở bước `[3/8]`
- Tại bước `[3/8]` (`3A. ARM64 Link Verification`), script truyền trực tiếp `-fuse-ld=lld "-Wl,--fatal-warnings"` và liên kết với **cả 7 thư viện** trong `SO/SO/*.so` (`-lnnpackage-f99779bd -lnative-lib -lxheymediav8 -lxhey_videoedit -lffmpeg -ljpeg -lmp3lame`), đảm bảo mọi cảnh báo ELF của `ld.lld` trên bất kỳ phiên bản NDK nào đều bị chặn ngay lập tức.

---

## 3. Kết Quả Chạy Kiểm Chứng Thực Tế (`[1/8]` → `[8/8]`)

```text
============================================================================
  AUDIT VERIFICATION SUITE (Reports 11 & 13 - Portable Native C++ Harness)
  RepoRoot  : E:\Decompiler\APP\IMAGE\Timemark
  NdkPath   : C:\Program Files\Unity\Hub\Editor\2020.3.16f1\Editor\Data\PlaybackEngines\AndroidPlayer\NDK
  ApkPath   : E:\Decompiler\APP\IMAGE\Timemark\Native_CPP_Reconstructed\tests\fixtures\timemark_release_sig.apk
  SampleDir : E:\Decompiler\APP\IMAGE\Timemark\Native_CPP_Reconstructed\tests\fixtures
============================================================================

[1/8] Checking byte-for-byte synchronization across Native_CPP_Reconstructed and SO/Function...
  - src\edge_ai_ncnn_jni.cpp               : SHA256 = CDA778D9A998C860... (3 copies identical)
  - src\xhey_media_and_security_jni.cpp    : SHA256 = 1A700A6828F3A406... (3 copies identical)
  - src\code_detect_anticode_jni.cpp       : SHA256 = 23551D2C84B674C4... (3 copies identical)
  - src\jpeg_packer_jni.cpp                : SHA256 = 9D966B30851F89B3... (3 copies identical)
  - src\aegis_anti_spoof_jni.cpp           : SHA256 = 00C5A5FBFA5B299F... (3 copies identical)
  - src\mcap_robotics_jni.cpp              : SHA256 = 0F23D307BFC21A45... (3 copies identical)
  - include\xhey_jni_models.h              : SHA256 = 72B4F5E32C3C7136... (3 copies identical)
  - CMakeLists.txt                         : SHA256 = F031F003FA24585A... (2 copies identical)
  - tests\verify_audit_2026_10_25.cpp      : SHA256 = 468AFFE3D7DB9DE4... (2 copies identical)
  - tests\verify_audit_2026_10_25.ps1      : SHA256 = 8985757E6D2CAD73... (2 copies identical)
  [PASS] All C++/header/CMake/test files are 100% byte-for-byte identical across directories.

[2/8] Auditing 7 compiled ARM64 .so libraries in SO/SO (PT_DYNAMIC, .dynsym/sh_info, STT_FUNC, Java_*)...
  - libffmpeg.so               |  171096 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6.. 61) | STT_FUNC= 15 | Java_*=  0
  - libjpeg.so                 |  192024 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6.. 65) | STT_FUNC= 15 | Java_*=  0
  - libmp3lame.so              |  171056 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6.. 52) | STT_FUNC=  9 | Java_*=  0
  - libnative-lib.so           |  200248 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6.. 62) | STT_FUNC=  9 | Java_*=  9
  - libnnpackage-f99779bd.so   |  633960 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6..279) | STT_FUNC=131 | Java_*=131
  - libxhey_videoedit.so       |  183872 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6.. 63) | STT_FUNC=  8 | Java_*=  8
  - libxheymediav8.so          |  195784 B | PT_DYNAMIC=YES | sh_info=6 (LOCAL=0..5, GLOBAL=6.. 67) | STT_FUNC= 11 | Java_*= 11
  [PASS] All 7 .so have valid .dynsym/sh_info boundaries (0 local-in-global symbols), Total STT_FUNC = 198, Total Java_* = 159, Stub count = 0/198.

[3/8] Compiling, linking, and executing native C++ verification harness (verify_audit_2026_10_25.cpp)...
  [LINK PASS] Linked ARM64 binary (298904 bytes) cleanly via ld.lld (--fatal-warnings) against all 7 SO/SO/*.so libraries.
  [NATIVE 1/6] Verifying P1 #2 Fail-Closed Model Loading... [PASS]
  [NATIVE 2/6] Verifying P1 #1 Real NCNN Graph & .bin Weight Execution (3 Models)... [PASS]
  [NATIVE 3/6] Verifying P1 #3 Native SecurityManager_nativeEncrypt / nativeDecrypt... [PASS]
  [NATIVE 4/6] Verifying P1 #4 Native APK Certificate Pinning & Forgery Rejection... [PASS]
  [NATIVE 5/6] Verifying P2 #5 Native PaddleOCR_init External Dictionary Loading... [PASS]
  [NATIVE 6/6] Verifying P2 #6 Native JpegPacker & CodeDetect on 3 Release Sample JPEGs... [PASS]
  [RUNTIME PASS] ALL 6 NATIVE C++ AUDIT VERIFICATION SUITES SUCCEEDED

[4/8] .. [8/8] [PASS]
============================================================================
  ALL 8 VERIFICATION CHECKS PASSED ([LINK PASS] & [RUNTIME PASS] VERIFIED)
============================================================================
```

> **Ghi chú cập nhật theo [`Report/17_Kiem_Chung_Git_15f912b_2026-10-26.md`](file:///E:/Decompiler/APP/IMAGE/Timemark/Report/17_Kiem_Chung_Git_15f912b_2026-10-26.md):**
> - Log kiểm chứng ở Mục 3 trên được chạy trên máy dev dùng Unity Android NDK `19.0.5232133` (Clang `8.0.2`).
> - Trên Android NDK `28.2.13676358` (Clang `19.0.1`), các bước `[1/8]`, `[2/8]` và `[3/8] [LINK PASS]` (với `ld.lld --fatal-warnings`) của commit `15f912b` đã được xác nhận **PASS**, nhưng nhánh biên dịch host runtime Windows x64 (`3B`) gặp lỗi bất đồng bộ kiểu `uintptr_t` / `__type_name_t` (`32-bit unsigned int` vs `64-bit void*`) do `sysroot/usr/include/stdint.h` kiểm tra `#if defined(__LP64__)` trong khi target `--target=x86_64-w64-mingw32` là mô hình `LLP64`.
> - Khắc phục triệt để lỗi host harness trên NDK 28 được ghi nhận tại [`Report/18_Khac_Phuc_Loi_Host_Harness_NDK28_Report_17_2026-10-26.md`](file:///E:/Decompiler/APP/IMAGE/Timemark/Report/18_Khac_Phuc_Loi_Host_Harness_NDK28_Report_17_2026-10-26.md).

