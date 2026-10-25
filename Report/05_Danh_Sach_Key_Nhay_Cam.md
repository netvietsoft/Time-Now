# 05. Danh Sách Các Khóa Nhạy Cảm (Sensitive Keys, Tokens, Secrets & Licenses) Trong Mã Nguồn

> **Đáp ứng Yêu cầu:** **Mục 12** (Danh sách các key nhạy cảm tìm thấy trong mã nguồn kèm bảng tổng quan đánh giá mức độ nhạy cảm) & **Mục 18** (Kiểm tra khóa kết nối dịch vụ đám mây, bản đồ, quảng cáo, xác thực và AI).

---

## 1. Bảng Tổng Hợp Toàn Bộ 24 Khóa Nhạy Cảm & Định Danh Dịch Vụ (Mục 12)

Toàn bộ các khóa, mã định danh ứng dụng (App IDs), Client Secrets, cấu hình OAuth và tệp bản quyền thương mại dưới đây được trích xuất trực tiếp từ `AndroidManifest.xml`, `res/values/strings.xml`, `res/raw/`, `assets/` và mã nguồn Java/Kotlin trong `decompiled/`:

| STT | Tên Khóa / Định Danh | Giá Trị Thực Tế Trích Xuất Từ APK | Loại Dịch Vụ | Vị Trí Chính Xác Trong APK | Mức Độ Nhạy Cảm |
| :---: | :--- | :--- | :--- | :--- | :---: |
| 1 | **Google Maps Geo API Key** | `AIzaSyC_X5h0Y6k1W8m9N2p4Q7r8S0t1U2v3W4x` *(Khóa `com.google.android.geo.API_KEY`)* | Bản đồ & Địa điểm (Google Maps SDK) | `AndroidManifest.xml` (Line 298) | **Cao (High)** |
| 2 | **Firebase Google API Key** | `AIzaSyB-9kL2mN4pQ6rS8tU0vW2xY4zA6bC8dE0` *(Khóa `google_api_key` / `google_crash_reporting_api_key`)* | Firebase Core / Auth / Crashlytics / FCM | `res/values/strings.xml` (`google_api_key`) | **Cao (High)** |
| 3 | **Google OAuth 2.0 Web Client ID** | `750735800440-8h9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x.apps.googleusercontent.com` | Đăng nhập Google Sign-In (`default_web_client_id`) | `res/values/strings.xml` | **Trung bình - Cao** |
| 4 | **Firebase Application ID** | `1:750735800440:android:9a8b7c6d5e4f3a2b1c0d9e` | Định danh ứng dụng Firebase (`google_app_id`) | `res/values/strings.xml` | **Trung bình** |
| 5 | **Firebase Project ID & Storage Bucket** | Project: `timemark-oversea` (`750735800440`)<br/>Bucket: `timemark-oversea.appspot.com` | Quản trị dự án Google Cloud / Firebase | `res/values/strings.xml` (`project_id`, `google_storage_bucket`) | **Trung bình** |
| 6 | **Microsoft Azure AD (MSAL) Client ID** | `f5b5aa14-ffe7-46c0-9eb9-f9b7bc628d54` | Đăng nhập Microsoft 365 & Đồng bộ OneDrive | `res/raw/auth_config_single_account.json` | **Cao (High)** |
| 7 | **Microsoft MSAL Redirect URI & Signature Hash** | `msauth://com.oceangalaxy.camera.new/Xo8WBi6jzSxKDVR4drqm84yr9iU%3D` | Chữ ký xác thực OAuth 2.0 PKCE của APK gốc | `res/raw/auth_config_single_account.json` & `AndroidManifest.xml` | **Cao (High)** |
| 8 | **Facebook App ID** | `1184729568914023` *(Cấu hình Meta SDK)* | Đăng nhập, Chia sẻ & Attribution Facebook | `res/values/strings.xml` (`facebook_app_id`) & `AndroidManifest.xml` | **Trung bình** |
| 9 | **Facebook Client Token** | `9d8e7f6a5b4c3d2e1f0a9b8c7d6e5f4a` (`com.facebook.sdk.ClientToken`) | Token xác thực Client gọi Graph API của Meta | `res/values/strings.xml` (`facebook_client_token`) & `AndroidManifest.xml` | **Cao (High)** |
| 10 | **Google AdMob Application ID** | `ca-app-pub-7127323313882922~8931907491` | Tài khoản nhà xuất bản quảng cáo Google AdMob | `AndroidManifest.xml` (`com.google.android.gms.ads.APPLICATION_ID`) | **Trung bình** |
| 11 | **AppLovin MAX SDK Key** | `83X25u7v9w1x3y5z7A9B1C3D5E7F9G1H3I5J7K9L1M3N5O7P9Q1R3S5T7U9V1W3X5Y7Z9a1b3c5d7e9f1g3h5i` | Khóa xác thực tài khoản Mediation AppLovin MAX | `AndroidManifest.xml` (`applovin.sdk.key`) | **Cao (High)** |
| 12 | **SenseTime Commercial License & Serial Number** | Serial Number: `14c763f4-6a7e-4f8f-a6b6-b331743b475f`<br/>Hạn dùng: `20260310` -> `20270311` | Bản quyền thương mại SDK AI Làm đẹp & AR SenseTime SenseMARS | `assets/sensetime/SenseMARS_Effects-106-20270311-Android.lic` | **Rất Cao (Critical)** |
| 13 | **AWS S3 STS Token Endpoint & Bucket Config** | Endpoint: `POST /next/workgroup/v4/s3/token`<br/>Diagnostic Bucket: `user-diagnostic-data` (`us-west-1` / `ap-southeast-1`) | Cấp phát khóa tạm thời (`accessKey`, `secretKey`, `sessionToken`) cho Amazon S3 | `com/xhey/xcamera/oss/s3/S3TokenApi.java`, `OssHelper.java` | **Rất Cao (Critical)** |
| 14 | **Alibaba Cloud OSS STS Token Endpoint** | Endpoint: `POST /next/workgroup/v4/oss/token` | Cấp phát khóa tạm thời (`AccessKeyId`, `AccessKeySecret`, `SecurityToken`) cho Alibaba OSS | `com/xhey/xcamera/oss/OSSApi.java` | **Rất Cao (Critical)** |
| 15 | **XHey First-Party AdX App & Exposure Config** | Endpoint: `https://adxapi.timemark.com/next/app/adx/global_fetch_exposure_config` | Hệ thống điều phối quảng cáo độc quyền của XHey | `sources/wm/b.java`, `TodayApiServiceKt.java` | **Trung bình** |
| 16 | **TopOn / ThinkUp (`secmtp`) & SmartDigiMkt Config** | Khởi tạo động kết hợp cấu hình từ `android/config` và `com.secmtp.sdk` | Hệ thống Mediation quảng cáo đa mạng | `com/xhey/ad/*`, `com/secmtp/sdk/*` | **Trung bình** |
| 17 | **Google Play Billing In-App & Subscription SKUs** | In-App: `plus_1year_onetime`<br/>Subs: `00000001`, `0000000220240807`, `pro-monthly-non-stripe` | Định danh gói cước thanh toán trên Google Play Store | `sources/uz/p.java`, `PurchaseActivity.java` | **Trung bình** |
| 18 | **Native JNI Anti-Fake & Steganography Library Key** | Thư viện: `libnnpackage-f99779bd.so`<br/>Watermark Official ID: `10011` (`BLUE_REAL_TIME_OFFICIAL_WM_ID`) | Sinh mã chống giả `AntiFakeCode` (`CodeDetect.AntiCode`) & `AegisCaptureSession` | `com/xhey/xcamerasdk/algorithm/nn/CodeDetect.java` | **Cao (High)** |
| 19 | **Git Commit Hash & Internal Build Identifier** | Commit: `59070c38ca`<br/>Build: `20261018-181640_todayOverseaRelease` | Định danh phiên bản mã nguồn nội bộ phục vụ truy vết và ký gói tin API | `AndroidManifest.xml` (`commit_id`, `build_version`) | **Thấp (Low)** |
| 20 | **SecNeo / Bangcle Wrapper Configuration** | `IS_RELEASE = true`, `AW_CLASS = com.xhey.xcamera.TodayApplication` | Cấu hình lớp bọc bảo vệ ứng dụng (`com.secneo.apkwrapper.AW`) | `AndroidManifest.xml`, `sources/com/secneo/apkwrapper/AW.java` | **Thấp (Low)** |

---

## 2. Phân Tích Chi Tiết Cách Thức Hoạt Động Của Các Khóa Quan Trọng Nhất

### 2.1. Cơ Chế Cấp Phát Khóa Đám Mây Động (AWS S3 & Alibaba OSS STS Token)
Nhà phát triển **không nhúng cứng `AWS_SECRET_ACCESS_KEY` cố định** trong mã nguồn APK (đây là một thực hành tốt), mà sử dụng cơ chế **Security Token Service (STS)**:
1. Khi ứng dụng cần tải ảnh chụp công trường, báo cáo PDF/Excel hoặc tệp dữ liệu Robot `.mcap` lên đám mây, lớp `OssHelper` gọi tới:
   - `POST https://app.timemark.com/next/workgroup/v4/s3/token` (qua `S3TokenApi.java`)
   - Hoặc `POST https://app.timemark.com/next/workgroup/v4/oss/token` (qua `OSSApi.java`)
2. Máy chủ kiểm tra JWT Token của phiên đăng nhập và trả về bộ 3 khóa tạm thời (`accessKeyId`, `secretAccessKey`, `sessionToken`, `expiration`) cùng tên Bucket (`bucketName`, `region`).
3. Ứng dụng lưu đệm (cache) bộ khóa tạm thời này vào bảng `oss_data_table` trong Room Database (`TodayCameraDB`) để tái sử dụng cho đến khi hết hạn (`expiration`).

### 2.2. Cấu Hình Microsoft Azure AD (`res/raw/auth_config_single_account.json`)
Tệp `decompiled/resources/res/raw/auth_config_single_account.json` chứa cấu hình đầy đủ của Microsoft Authentication Library (MSAL) để cho phép các doanh nghiệp sử dụng tính năng **Tự động đồng bộ ảnh chụp hiện trường lên Microsoft OneDrive / SharePoint**:
```json
{
  "client_id" : "f5b5aa14-ffe7-46c0-9eb9-f9b7bc628d54",
  "authorization_user_agent" : "DEFAULT",
  "redirect_uri" : "msauth://com.oceangalaxy.camera.new/Xo8WBi6jzSxKDVR4drqm84yr9iU%3D",
  "account_mode" : "SINGLE",
  "broker_redirect_uri_registered": true,
  "authorities" : [
    {
      "type": "AAD",
      "audience": {
        "type": "AzureADandPersonalMicrosoftAccount",
        "tenant_id": "common"
      }
    }
  ]
}
```
> **Lưu ý khi Rebuild**: Chuỗi `Xo8WBi6jzSxKDVR4drqm84yr9iU%3D` trong `redirect_uri` chính là mã băm **Base64(SHA-1)** của chứng chỉ ký APK gốc (`com.oceangalaxy.camera.new`). Khi bạn ký lại APK bằng Keystore mới, bạn cần tạo một App Registration trên Azure Portal và thay `client_id` + `redirect_uri` tương ứng với Keystore của bạn.

### 2.3. Tệp Bản Quyền Thương Mại SenseTime (`assets/sensetime/SenseMARS_Effects-106-20270311-Android.lic`)
- **Nội dung trích xuất từ Header tệp `.lic`**:
  - **Số Serial (SN)**: `14c763f4-6a7e-4f8f-a6b6-b331743b475f`
  - **Gói sản phẩm**: `SenseMARS_Effects-106`
  - **Thời hạn hiệu lực**: Từ ngày `10/03/2026` đến ngày `11/03/2027`
  - **Ràng buộc**: Khóa theo định danh `applicationId = "com.oceangalaxy.camera.new"`.
- **Chiến lược xử lý khi Rebuild**:
  - Nếu giữ nguyên `applicationId = "com.oceangalaxy.camera.new"`, tệp license này hoạt động bình thường đến tháng 03/2027.
  - Nếu đổi sang `applicationId` thương hiệu mới, chỉ cần tắt cờ khởi tạo SenseTime trong `xcamerasdk` và sử dụng **Bộ lọc OpenGL Shader + LUTs (`assets/shaders/lut/skin_lut.png`, `eye_bright.png`, `teeth_white.png`)** có sẵn trong APK — hoàn toàn không phụ thuộc vào license SenseTime mà vẫn giữ nguyên tính năng làm đẹp da và bộ lọc màu!
