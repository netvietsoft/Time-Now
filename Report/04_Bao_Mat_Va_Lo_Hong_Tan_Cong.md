# 04. Đánh Giá Bảo Mật Kiến Trúc, Phân Tích Lỗ Hỗng & Giải Pháp Khắc Phục Phòng Thủ (Timemark Camera)

> **Đáp ứng Yêu cầu:** **Mục 10** (Phân tích các lỗ hổng bảo mật tiềm ẩn trong mã nguồn, cấu hình Manifest, cơ chế xác thực, lưu trữ dữ liệu và khuyến nghị vá lỗi phòng thủ khi tái xây dựng hệ thống).
> **Lưu ý Tuân thủ An toàn (Defensive Security Scope):** Báo cáo này được thực hiện nghiêm ngặt dưới góc độ **Kiểm toán Bảo mật Phòng thủ (Defensive Code Audit & Remediation)** nhằm giúp đội ngũ kỹ thuật hiểu rõ các điểm yếu trong thiết kế hiện tại và áp dụng các biện pháp gia cố (Hardening) chuẩn OWASP MASVS khi xây dựng lại ứng dụng.

---

## 1. Tổng Quan Ma Trận Rủi Ro Bảo Mật (OWASP Mobile Top 10 Assessment)

Qua kiểm toán tĩnh (Static Application Security Testing - SAST) trên `AndroidManifest.xml`, cấu hình mạng (`network_security_config.xml`), cơ sở dữ liệu nội bộ (`TodayCameraDB`) và các tầng nghiệp vụ của **Timemark v10.0.281**, chúng tôi ghi nhận **8 vấn đề bảo mật đáng chú ý**, phân bổ từ mức **Nghiêm trọng (Critical/High)** đến **Trung bình (Medium)**:

| Mã lỗi | Tên điểm yếu bảo mật | Danh mục OWASP MASVS | Mức độ rủi ro | Thành phần bị ảnh hưởng |
| :---: | :--- | :--- | :---: | :--- |
| **VULN-01** | Cho phép kết nối HTTP không mã hóa & Tin cậy Chứng chỉ Người dùng tự cài (User CAs) | MASVS-NETWORK-1 | **Cao (High)** | `res/xml/network_security_config.xml`, `AndroidManifest.xml` |
| **VULN-02** | Lộ nhiều Khóa API, OAuth Secret & License Thương mại dạng văn bản thuần (Hardcoded Secrets) | MASVS-STORAGE-1 | **Cao (High)** | `AndroidManifest.xml`, `res/values/strings.xml`, `res/raw/auth_config_single_account.json`, `assets/sensetime/` |
| **VULN-03** | Tồn tại 14 Component (`Activity`/`Service`/`Receiver`) có `exported="true"` thiếu kiểm tra quyền chặt chẽ | MASVS-PLATFORM-1 | **Trung bình - Cao** | `AndroidManifest.xml` (`VerifyAuthActivity`, `QuestionnaireActivity`, `PurchaseActivity`, `SensorPrivacyService`...) |
| **VULN-04** | Cơ chế "Xóa Watermark" (`JpegPacker`) nhúng nguyên vẹn vùng ảnh gốc chưa đóng dấu vào bên trong tệp JPEG | MASVS-CRYPTO-1 / PRIVACY | **Cao (Logic Nghiệp vụ)** | `com.xhey.xcamerasdk.algorithm.nn.JpegPacker` |
| **VULN-05** | Cơ sở dữ liệu Room (`TodayCameraDB` - 42 bảng) lưu trữ không mã hóa (Unencrypted SQLite) & lưu đệm STS Token | MASVS-STORAGE-2 | **Trung bình (Medium)** | `com.xhey.xcamera.room.TodayCameraDB_Impl`, bảng `oss_data_table`, `anti_code_entry` |
| **VULN-06** | Lớp bảo vệ Packer (`SecNeo/Bangcle`) bị vô hiệu hóa phần mã hóa DEX trong bản phát hành App Bundle | MASVS-RESILIENCE-3 | **Trung bình (Medium)** | `classes.dex` -> `classes8.dex`, `com.secneo.apkwrapper.AW` |
| **VULN-07** | Hai Bundle React Native (`verify_auth` & `questionaire`) đóng gói ở dạng Plain JavaScript không biên dịch Bytecode | MASVS-RESILIENCE-4 | **Trung bình (Medium)** | `assets/verify_auth.android.jsbundle`, `assets/questionaire.android.jsbundle` |
| **VULN-08** | Kiểm tra Trạng thái VIP (`Timemark Plus`) & Mở khóa Không Quảng Cáo (`AdFreePassState`) lưu đệm trên MMKV/SharedPreferences | MASVS-AUTH-2 | **Trung bình (Medium)** | `o90/l.java` (`AdFreePassState`), `com.xhey.xcamera.vip.VipManager` |

---

## 2. Phân Tích Kỹ Thuật Chi Tiết & Giải Pháp Khắc Phục (Remediation Blueprint)

### 2.1. [VULN-01] Cấu hình `network_security_config.xml` cho phép Cleartext HTTP & User Certificates (Mức độ: CAO)

- **Bằng chứng trong mã nguồn (`decompiled/resources/res/xml/network_security_config.xml` & `AndroidManifest.xml`)**:
  ```xml
  <!-- AndroidManifest.xml -->
  <application
      android:usesCleartextTraffic="true"
      android:networkSecurityConfig="@xml/network_security_config" ...>

  <!-- res/xml/network_security_config.xml -->
  <network-security-config>
      <base-config cleartextTrafficPermitted="true">
          <trust-anchors>
              <certificates src="system"/>
              <certificates src="user"/>
          </trust-anchors>
      </base-config>
  </network-security-config>
  ```
- **Phân tích nguyên nhân & Tác động**:
  1. Thuộc tính `cleartextTrafficPermitted="true"` cho phép ứng dụng truyền dữ liệu qua giao thức `http://` không mã hóa nếu có bất kỳ URL nào bị cấu hình nhầm hoặc hạ cấp (Downgrade).
  2. Việc khai báo `<certificates src="user"/>` trong `<base-config>` (thay vì chỉ giới hạn trong `<debug-overrides>`) khiến phiên bản Production tin cậy hoàn toàn các chứng chỉ gốc (Root CA) do người dùng tự cài đặt trên thiết bị. Điều này làm mất tác dụng của lớp bảo vệ TLS mặc định từ Android 7.0+, cho phép chặn bắt và sửa đổi lưu lượng API (ví dụ: chỉnh sửa tọa độ GPS trong gói tin `/next/v2/place/list/group` hoặc thời gian thực trong `/next/realtime`).
- **Giải pháp khắc phục phòng thủ (Defensive Remediation)**:
  - Loại bỏ `<certificates src="user"/>` khỏi `<base-config>` và chuyển vào thẻ `<debug-overrides>` (chỉ kích hoạt khi `android:debuggable="true"`).
  - Đặt `cleartextTrafficPermitted="false"` làm mặc định toàn cục.
  - Bổ sung **Certificate Pinning (Ghim chứng chỉ SHA-256 Public Key)** cho tên miền chính `app.timemark.com` và `teamspace.timemark.com`:
    ```xml
    <network-security-config>
        <base-config cleartextTrafficPermitted="false">
            <trust-anchors>
                <certificates src="system"/>
            </trust-anchors>
        </base-config>
        <domain-config cleartextTrafficPermitted="false">
            <domain includeSubdomains="true">timemark.com</domain>
            <pin-set expiration="2028-01-01">
                <pin digest="SHA-256">BASE64_PRIMARY_PUBLIC_KEY_HASH=</pin>
                <pin digest="SHA-256">BASE64_BACKUP_PUBLIC_KEY_HASH=</pin>
            </pin-set>
        </domain-config>
    </network-security-config>
    ```

---

### 2.2. [VULN-02] Lộ Khóa API Bản Đồ, OAuth & License Thương Mại Trong Tài Nguyên Tĩnh (Mức độ: CAO)

- **Bằng chứng trong mã nguồn**:
  - `AndroidManifest.xml`: Chứa trực tiếp khóa Google Maps API (`AIzaSy...`), AppLovin SDK Key (`83X25u...`), Facebook Client Token, Google AdMob App ID.
  - `res/values/strings.xml`: Chứa khóa Firebase API Key (`AIzaSy...`), Google Web Client ID (`750735800440-...`).
  - `res/raw/auth_config_single_account.json`: Chứa cấu hình Microsoft Azure AD (`client_id`: `f5b5aa14-ffe7-46c0-9eb9-f9b7bc628d54`).
  - `assets/sensetime/SenseMARS_Effects-106-20270311-Android.lic`: Chứa tệp cấp phép thương mại SenseTime có hiệu lực đến 11/03/2027.
- **Phân tích nguyên nhân & Tác động**:
  - Nếu khóa Google Maps API không được giới hạn chặt chẽ theo cặp `(Package Name + SHA-1 Signing Certificate)` trên Google Cloud Console, bên thứ ba có thể trích xuất khóa này để gọi Maps/Places API gây phát sinh chi phí lớn.
- **Giải pháp khắc phục phòng thủ**:
  1. Trên **Google Cloud Console**, bắt buộc bật **Application Restrictions -> Android Apps** và khóa cứng theo `Package Name` cùng dấu vân tay chứng chỉ ký `SHA-1` của bản Release; đồng thời bật **API Restrictions** chỉ cho phép đúng `Maps SDK for Android`.
  2. Đối với các khóa dịch vụ đám mây hoặc AI, tuyệt đối không nhúng khóa tĩnh trên Client mà phải cấp phát qua cơ chế **Short-lived Token** từ Backend sau khi đã xác thực JWT của người dùng.

---

### 2.3. [VULN-03] Các Activity & Service Xuất Công Khai (`android:exported="true"`) Thiếu Ràng Buộc Quyền (Mức độ: TRUNG BÌNH - CAO)

- **Bằng chứng trong mã nguồn (`AndroidManifest.xml`)**:
  Một số Activity và Service nội bộ được đặt `android:exported="true"` mà không yêu cầu quyền tùy chỉnh (`android:permission`) hoặc không có `<intent-filter>` cần thiết từ bên ngoài:
  - `com.xhey.xcamera.rn.verify.VerifyAuthActivity` (`exported="true"`)
  - `com.xhey.xcamera.rn.questionnaire.QuestionnaireActivity` (`exported="true"`)
  - `com.xhey.xcamera.ui.bill.PurchaseActivity` (`exported="true"`)
  - `com.xhey.xcamera.ui.webview.WebViewActivity` (`exported="true"`)
  - `com.xhey.xcamera.EachOtherTriggerService` (`exported="true"`)
- **Phân tích nguyên nhân & Tác động**:
  - Đặc biệt với `WebViewActivity` và `QuestionnaireActivity`, nếu một ứng dụng khác trên cùng thiết bị gửi một `Intent` tường minh kèm tham số URL tùy ý (Extra URL/Bundle), `WebViewActivity` có thể tải một trang web giả mạo bên trong ngữ cảnh của ứng dụng Timemark (nguy cơ **Cross-App Scripting / Intent Spoofing** nếu WebView có bật `addJavascriptInterface`).
- **Giải pháp khắc phục phòng thủ**:
  - Đặt `android:exported="false"` cho toàn bộ các Activity/Service chỉ phục vụ điều hướng nội bộ (`PurchaseActivity`, `VerifyAuthActivity`, `QuestionnaireActivity`, `EachOtherTriggerService`).
  - Đối với các Activity bắt buộc nhận Deep Link từ trình duyệt (như `TeamSpaceJoinActivity` hoặc `WebViewActivity`), phải kiểm tra danh sách trắng tên miền (`Whitelist Domain`: chỉ cho phép `*.timemark.com` và `*.xhey.top`) trước khi gọi `webView.loadUrl(url)`.

---

### 2.4. [VULN-04] Rủi Ro Rò Rỉ Dữ Liệu Ngầm & Toàn Vẹn Nghiệp Vụ Trong Cơ Chế `JpegPacker` (Mức độ: CAO - Nghiệp vụ)

- **Bằng chứng trong mã nguồn (`com.xhey.xcamerasdk.algorithm.nn.JpegPacker`)**:
  - Như đã giải mã ở Báo cáo 01, để phục vụ tính năng kiếm tiền *"Trả phí VIP để xóa Watermark khỏi ảnh đã chụp"*, hàm `JpegPacker.startEncode()` âm thầm cắt vùng điểm ảnh gốc (trước khi chèn khung Watermark) đóng gói vào cấu trúc `SliceDataInfo[]` và nhúng thẳng vào cuối tệp JPEG xuất ra.
- **Phân tích nguyên nhân & Tác động**:
  1. **Góc độ Quyền riêng tư (Privacy)**: Khi người dùng dùng Watermark hoặc khung che thông tin để che một phần cảnh vật ở góc ảnh rồi gửi file JPEG gốc đó cho bên thứ ba, vùng ảnh nền phía sau khung Watermark thực chất vẫn nằm nguyên bên trong file JPEG và có thể bị trích xuất bằng hàm `JpegPacker.startDecode()`.
  2. **Góc độ Doanh thu (Monetization)**: Vì vùng ảnh gốc đã nằm sẵn trong file JPEG trên máy người dùng, việc kiểm tra quyền VIP chỉ đóng vai trò là một chốt chặn giao diện (UI Gate) trước khi gọi hàm giải nén cục bộ `JpegPacker.Wrapper.unpack(path)`.
- **Giải pháp khắc phục phòng thủ**:
  - **Mã hóa vùng dữ liệu nhúng (`SliceDataInfo`) bằng AES-256-GCM**: Khi nhúng vùng ảnh gốc vào file JPEG, hãy mã hóa `sliceImageByte` bằng một khóa đối xứng ngẫu nhiên (Per-Photo Key) hoặc khóa dẫn xuất từ máy chủ. Khi người dùng VIP yêu cầu xóa Watermark, ứng dụng gửi mã định danh ảnh lên API `workgroup/photo/watermark/remove/check`; máy chủ xác nhận tài khoản có gói VIP hợp lệ mới trả về khóa giải mã để khôi phục ảnh gốc. Cách này vừa bảo vệ quyền riêng tư khi chia sẻ file JPEG, vừa bảo vệ tuyệt đối doanh thu gói VIP!

---

### 2.5. [VULN-05] Cơ Sở Dữ Liệu SQLite Nội Bộ (`TodayCameraDB`) Không Mã Hóa (Mức độ: TRUNG BÌNH)

- **Bằng chứng trong mã nguồn (`com.xhey.xcamera.room.TodayCameraDB_Impl`)**:
  - Cơ sở dữ liệu Room lưu trữ 42 bảng ở dạng SQLite tiêu chuẩn không mã hóa trong thư mục `/data/data/com.oceangalaxy.camera.new/databases/`.
  - Trong đó, bảng `anti_code_entry` lưu danh sách các mã chống giả dự phòng chụp ngoại tuyến, bảng `oss_data_table` và `upload_progress_task_entry` lưu đường dẫn tệp nội bộ và siêu dữ liệu dự án, bảng `prefer_place_entity` và `lat_long_poi_entity` lưu lịch sử tọa độ di chuyển chi tiết của người dùng.
- **Giải pháp khắc phục phòng thủ**:
  - Tích hợp **SQLCipher for Android (`net.zetetic:android-database-sqlcipher`)** vào `Room.databaseBuilder().openHelperFactory(SupportFactory(passphrase))` với khóa sinh từ **Android Keystore System (`MasterKey`)**.
  - Đối với các lô mã chống giả ngoại tuyến trong `anti_code_entry`, mỗi mã cần được ràng buộc криптографически (HMAC) với chuỗi băm của bức ảnh tại thời điểm bấm máy.

---

### 2.6. [VULN-06 & VULN-07] Rủi Ro Dịch Ngược Bytecode & Plain JS Bundle (Mức độ: TRUNG BÌNH)

- **Bằng chứng trong mã nguồn**:
  1. Toàn bộ 8 tệp DEX (`classes.dex` -> `classes8.dex`) không được mã hóa, và toàn bộ package `com.xhey.xcamera.network.service.*`, `com.xhey.xcamera.data.model.bean.*` giữ nguyên tên lớp/thuộc tính.
  2. Hai tệp React Native `assets/verify_auth.android.jsbundle` (716 KB) và `assets/questionaire.android.jsbundle` (599 KB) được đóng gói ở dạng văn bản JavaScript thuần túy (Metro Bundle), khác với `index.android.bundle` đã được biên dịch sang Hermes Bytecode v96.
- **Giải pháp khắc phục phòng thủ**:
  1. Bật biên dịch **Hermes Bytecode (`hermesEnabled=true`)** đồng nhất cho tất cả các sub-bundle React Native (`verify_auth` và `questionaire`) trong quy trình Gradle build trước khi đóng gói APK/AAB.
  2. Tối ưu hóa tệp `proguard-rules.pro`: sử dụng `@SerializedName("...")` cho tất cả các trường trong Data Model và cho phép R8 rút gọn tên lớp/tên biến của tầng Model và Network Service, chỉ giữ lại các giao diện JNI thật sự cần thiết.

---

### 2.7. [VULN-08] Gia Cố Cơ Chế Chống Giả Mạo Thời Gian, Vị Trí GPS & Trạng Thái VIP (Mức độ: TRUNG BÌNH)

- **Cơ chế phòng thủ hiện có của Timemark (Điểm mạnh cần kế thừa)**:
  - Ứng dụng đã triển khai nhiều lớp kiểm tra rất tốt:
    - Chặn chụp ảnh nếu phát hiện người dùng bật **Chế độ Nhà phát triển (Developer Options)** khi tổ chức yêu cầu nghiêm ngặt (`i_developer_mode_block_description`).
    - Kiểm tra cờ `Location.isFromMockProvider()` / `isMock()` và đối chiếu tọa độ GPS với vị trí trạm thu phát sóng/IP (`/next/network/ip`).
    - Lấy giờ chuẩn từ cả hai nguồn độc lập: **Máy chủ HTTPS (`/next/realtime`)** và **Thời gian vệ tinh GPS (`GPSTimeService`)**, so sánh độ lệch với `SystemClock.elapsedRealtime()` để chống việc người dùng ngắt mạng rồi chỉnh giờ điện thoại.
    - Sử dụng `AegisCaptureSession` ở tầng C++ gắn với `HardwareBuffer` để chống tiêm khung hình từ Camera ảo.
- **Điểm cần gia cố thêm khi xây dựng lại**:
  1. **Tích hợp Google Play Integrity API**: Đính kèm `IntegrityToken` của Google Play vào API `/next/truthful/create/batch` và `/next/app/attendance/punch` để máy chủ xác minh thiết bị không bị Root, không chạy trên trình giả lập đã can thiệp hệ thống và chữ ký APK là nguyên bản.
  2. **Xác thực quyền lợi VIP phía Server (Server-Side Entitlement Enforcement)**: Không lưu trạng thái mở khóa quảng cáo (`AdFreePassState`) hay trạng thái VIP dưới dạng boolean thuần trong `MMKV`. Thay vào đó, máy chủ trả về một **Signed Entitlement JWT (ký bằng khóa riêng RSA/ECDSA của Server)** có thời hạn ngắn, và ứng dụng xác minh chữ ký số của JWT đó trước khi mở khóa các tính năng cao cấp ngoại tuyến.
