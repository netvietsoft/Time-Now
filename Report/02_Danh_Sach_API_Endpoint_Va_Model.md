# 02. Danh Sách Toàn Bộ API Endpoints, Tên Miền, Máy Chủ & Data Models (Timemark Camera)

> **Đáp ứng Yêu cầu:** **Mục 3** (Danh sách tất cả các API endpoint đang sử dụng, phương thức, tham số, cấu trúc dữ liệu, kèm bảng tổng quan), **Mục 4** (Danh sách tất cả các tên miền, IP, URL, cổng kết nối và vai trò), **Mục 18** (Kiểm tra kết nối tới dịch vụ LLM hoặc dịch vụ xử lý hình ảnh/AI bên thứ 3).

---

## 1. Danh Sách Tất Cả Tên Miền, URL, Máy Chủ & Cổng Kết Nối (Mục 4)

Trích xuất từ `com.xhey.xcamera.network.service.NetWorkServiceImplKt.java`, `wm/b.java`, `BuildConfig` và các lớp dịch vụ mạng:

| STT | Tên miền / Base URL | Giao thức & Cổng | Môi trường | Vai trò & Chức năng chi tiết | Vị trí trong mã nguồn |
| :---: | :--- | :---: | :---: | :--- | :--- |
| 1 | `https://app.timemark.com/next/` | HTTPS (`443`) | **Production (Global)** | **Cổng API chính (Core Backend Gateway)** cho toàn bộ tính năng chụp ảnh, Watermark, Tài khoản, VIP/Billing, Teamspace, Chấm công và Kiếm tiền (Earn). | `NetWorkServiceImplKt.java` (`getProductEnvUrl`) |
| 2 | `https://app-staging.timemark.com/next/` | HTTPS (`443`) | Staging | Máy chủ tiền sản xuất (Pre-release / Staging) phục vụ kiểm thử tích hợp. | `NetWorkServiceImplKt.java` (`getStagingUrl`) |
| 3 | `https://app-test.timemark.com/next/` | HTTPS (`443`) | Test / QA | Máy chủ môi trường Test nội bộ của đội ngũ QA XHey. | `NetWorkServiceImplKt.java` (`getTestUrl`) |
| 4 | `https://adxapi.timemark.com/next/` | HTTPS (`443`) | **Production (AdX)** | **Máy chủ điều phối quảng cáo riêng (First-Party AdX Server)**: trả về cấu hình tần suất hiển thị, danh sách placement và chiến dịch quảng cáo nội bộ. | `wm/b.java` (`a()`) |
| 5 | `https://adxapi-staging.timemark.com/next/` | HTTPS (`443`) | Staging (AdX) | Máy chủ thử nghiệm quảng cáo AdX môi trường Staging. | `wm/b.java` |
| 6 | `https://adxapi-test.timemark.com/next/` | HTTPS (`443`) | Test (AdX) | Máy chủ thử nghiệm quảng cáo AdX môi trường Test. | `wm/b.java` |
| 7 | `https://network.timemark.com/next/network/benchmark` | HTTPS (`443`) | **Production** | **Máy chủ đo kiểm tốc độ mạng (Network Benchmark)** chuẩn thông thường. | `NetWorkServiceImplKt.java` |
| 8 | `https://network-acc.timemark.com/next/network/benchmark` | HTTPS (`443`) | **Production (CDN Acc)** | **Máy chủ đo kiểm tốc độ mạng qua đường truyền tăng tốc глобал (Accelerated Edge)** để tự động chuyển đổi route khi mạng yếu. | `NetWorkServiceImplKt.java` |
| 9 | `https://teamspace.timemark.com` | HTTPS (`443`) | **Production (Web SaaS)** | **Cổng Web Portal cho Teamspace**: trang quản trị nhóm trên trình duyệt, link mời thành viên, thanh toán Stripe Web Checkout (`?plan-id=`). | `TeamSpaceStatusManager.java` |
| 10 | `https://teamspace-staging.timemark.com` | HTTPS (`443`) | Staging | Cổng Web Portal Teamspace môi trường Staging. | `TeamSpaceStatusManager.java` |
| 11 | `https://teamspace-test.timemark.com` | HTTPS (`443`) | Test | Cổng Web Portal Teamspace môi trường Test. | `TeamSpaceStatusManager.java` |
| 12 | `https://today.xhey.top/next/` | HTTPS (`443`) | Production (CN Legacy) | Máy chủ gốc của XHey (Today Watermark Camera nội địa Trung Quốc), dùng làm fallback trong `xcamerasdk`. | `com.xhey.xcamerasdk.*` |
| 13 | `https://prodtry.xhey.top/next/` | HTTPS (`443`) | Pre-Prod (CN) | Máy chủ thử nghiệm tính năng mới của `xcamerasdk`. | `com.xhey.xcamerasdk.*` |
| 14 | `https://net-cloud.xhey.top/` | HTTPS (`443`) | Production (CDN/Cloud) | Máy chủ tải tài nguyên đám mây (Font chữ mở rộng, gói model AI tải động, mẫu Watermark động). | `com.xhey.xcamera.*` |
| 15 | `https://static.xhey.top/` | HTTPS (`443`) | Production (Static CDN) | CDN lưu trữ hình ảnh tĩnh, icon mẫu Watermark, banner và tệp H5. | `com.xhey.xcamera.*` |
| 16 | `https://h5.xhey.top/truth` | HTTPS (`443`) | Production (H5 Web) | Trang Web H5 giới thiệu chứng nhận chống giả mạo ảnh (Photo Truth Verification Center). | `PhotoVerifyActivity.java` |
| 17 | `https://h5.xhey.top/cheatingReport` | HTTPS (`443`) | Production (H5 Web) | Trang Web H5 gửi báo cáo phát hiện hành vi gian lận thời gian/vị trí. | `PhotoVerifyActivity.java` |
| 18 | `https://*.s3.us-west-1.amazonaws.com` & `s3.ap-southeast-1.amazonaws.com` | HTTPS (`443`) | **AWS S3 Cloud** | Kho lưu trữ đám mây chính cho phiên bản Quốc tế: lưu ảnh chụp Teamspace, báo cáo PDF/Excel, video `.mcap` của module Earn và dữ liệu hộp đen chẩn đoán (`user- diagnostic-data`). | `OssHelper.java`, `S3TokenApi.java` |
| 19 | `https://*.aliyuncs.com` | HTTPS (`443`) | **Alibaba Cloud OSS** | Kho lưu trữ đám mây dự phòng / vùng châu Á (nhận STS Token qua `workgroup/v4/oss/token`). | `OSSApi.java` |

---

## 2. Kiến Trúc Giao Tiếp API & Cơ Chế Xác Thực

Toàn bộ giao tiếp HTTP/HTTPS được quản lý bởi `com.xhey.xcamera.network.service.NetWorkServiceImplKt` thông qua **5 Retrofit Service Interfaces**:
1. **`TodayApiServiceKt`** (`com.xhey.xcamera.network.service.TodayApiServiceKt` — **68 Endpoints**): Các nghiệp vụ cốt lõi của Camera, Thời gian thực, Thời tiết, Địa điểm (POI), Mẫu Watermark, Tìm/Tách nền Logo AI, Xác thực ảnh chống giả (`truthful`), Thanh toán VIP (`member/vip`, `goods`, `order`), Đăng nhập (`center/app/login`), Quảng cáo AdX.
2. **`ClusterApiService`** (`com.xhey.xcamera.network.service.ClusterApiService` — **91 Endpoints**): Toàn bộ hệ sinh thái **Teamspace SaaS** (`app/group/*`, `app/photo/*`, `app/checklist/*`, `app/project/file/*`, `app/attendance/*`, `app/group/timeclock/geofence/*`, `business/subscription/*`, `web/teamspace/app/integration/*`).
3. **`EarnApiService`** (`com.xhey.xcamera.earnhost.EarnApiService` — **9 Endpoints**): Hệ thống **Embodied AI ("Earn")** quản lý nhiệm vụ quay video huấn luyện Robot, số dư ví, nộp video `.mcap`, và rút tiền qua ngân hàng Pix (Brazil) / SPEI (Mexico).
4. **`DualModeApiService`** (`com.xhey.xcamera.network.service.DualModeApiService` — **7 Endpoints**): Tạo mã chống giả `antiFakeCode/generate`, kiểm tra quyền xóa Watermark (`workgroup/photo/watermark/remove/check`), quản lý địa chỉ yêu thích và đếm sự kiện người dùng.
5. **`OSSApi` & `S3TokenApi`** (`com.xhey.xcamera.oss.*` — **2 Endpoints**): Cấp phát chứng chỉ tạm thời (Temporary STS Credentials) cho Alibaba Cloud OSS và AWS S3.

### Cấu trúc gói dữ liệu trả về chuẩn (`BaseResponse<T>`)
Mọi API của Timemark đều trả về cấu trúc bọc chuẩn `xhey.com.network.model.BaseResponse<T>`:
```json
{
  "code": 200,
  "msg": "success",
  "status": 0,
  "data": { ... }
}
```

---

## 3. Bảng Tổng Quan Toàn Bộ 177 API Endpoints (Mục 3)

### 3.1. Phân Hệ 1: `TodayApiServiceKt` — Core Camera, Watermark, AI Logo, Anti-Fake, Auth & VIP (68 Endpoints)

| STT | Endpoint Path | Phương thức | Request Body / Tham số | Response Model (`BaseResponse<T>`) | Mô tả chức năng nghiệp vụ |
| :---: | :--- | :---: | :--- | :--- | :--- |
| 1 | `android/config` | `POST` | `{"versionCode", "deviceId", "countryCode"}` | `ConfigData` | Lấy cấu hình hệ thống Android, cờ bật/tắt tính năng (Feature Flags), ép cập nhật. |
| 2 | `realtime` | `POST` | `{"clientTime": long}` | `RealTimeInfo` | Đồng bộ giờ chuẩn nguyên tử từ máy chủ chống người dùng chỉnh đồng hồ điện thoại. |
| 3 | `network/ip` | `POST` | `{}` | `IpBean` | Kiểm tra địa chỉ IP công cộng và quốc gia của thiết bị. |
| 4 | `weather/current/detail` | `POST` | `{"lat", "lng", "lang", "unit"}` | `WeatherInfo` | Lấy thông tin thời tiết, nhiệt độ, độ ẩm, tốc độ gió, chất lượng không khí (AQI) tại tọa độ chụp. |
| 5 | `v2/place/list/group` | `POST` | `{"lat", "lng", "accuracy", "provider"}` | `LocationInfoData` | Lấy danh sách địa điểm (POI), tên đường, quận/huyện/thành phố xung quanh vị trí GPS. |
| 6 | `v2/place/search/group` | `POST` | `{"keyword", "lat", "lng", "page"}` | `LocationInfoData` | Tìm kiếm địa điểm POI theo từ khóa do người dùng nhập. |
| 7 | `place/Nominatim/reverse` | `POST` | `{"lat", "lng", "zoom"}` | `NominatimData` | Giải mã tọa độ ngược (Reverse Geocoding) qua hệ thống OpenStreetMap Nominatim. |
| 8 | `location/select/feedback` | `POST` | `{"placeId", "lat", "lng", "action"}` | `BaseResponseData` | Gửi phản hồi khi người dùng chọn hoặc sửa một địa điểm trên danh sách. |
| 9 | `config/watermarks` | `POST` | `{"categoryIds", "lang"}` | `WatermarkListResponse` | Lấy danh sách cấu hình mẫu Watermark mặc định từ máy chủ. |
| 10 | `watermark/ours/query` | `POST` | `{"watermarkIds": List<String>}` | `OursWaterMarkBean` | Truy vấn chi tiết các mẫu Watermark chính thức của Timemark. |
| 11 | `watermark/pgc/query` | `POST` | `{"categoryId", "page", "size"}` | `PgcWaterMarkBean` | Truy vấn kho mẫu Watermark chuyên nghiệp theo ngành nghề (PGC). |
| 12 | `watermark/item/edit/history/upload` | `POST` | `{"watermarkId", "items": List}` | `BaseResponseData` | Đồng bộ lịch sử chỉnh sửa các dòng chữ trên Watermark lên đám mây. |
| 13 | `watermark/share/private/query` | `POST` | `{"shareCode": String}` | `ShareWaterMarkBean` | Tải mẫu Watermark cá nhân được chia sẻ qua mã code (`shareCode`). |
| 14 | `watermark/share/public/register` | `POST` | `{"watermarkContent", "coverUrl", "title"}` | `ShareRegisterBean` | Xuất bản mẫu Watermark của người dùng lên kho chia sẻ công cộng. |
| 15 | `watermark/share/public/status/update` | `POST` | `{"shareCode", "status"}` | `BaseResponseData` | Cập nhật trạng thái ẩn/hiện của mẫu Watermark đã chia sẻ. |
| 16 | `watermark/ugc/code/search` | `POST` | `{"code": String}` | `UgcSearchBean` | Tìm mẫu Watermark do cộng đồng tạo (UGC) bằng mã chia sẻ. |
| 17 | `watermark/ugc/google/search` | `POST` | `{"query", "lang"}` | `UgcSearchBean` | Tìm kiếm mẫu Watermark UGC theo từ khóa thông minh. |
| 18 | `watermark/ugc/public/info/save` | `POST` | `{"watermarkJson", "previewUrl"}` | `UgcPublicData` | Lưu thông tin mẫu Watermark UGC công khai. |
| 19 | `watermark/ugc/recommend/template` | `POST` | `{"industry", "role"}` | `UgcRecommendBean` | Gợi ý mẫu Watermark phù hợp theo ngành nghề người dùng chọn lúc Onboarding. |
| 20 | `watermark/ugc/share/Report` | `POST` | `{"shareCode", "eventType"}` | `BaseResponseData` | Báo cáo lượt sử dụng/tải về mẫu Watermark được chia sẻ. |
| 21 | `watermark/Skin/list` | `POST` | `{"baseId": String}` | `WatermarkSkinListBean` | Lấy danh sách giao diện màu sắc (Skin/Theme) của một mẫu Watermark. |
| 22 | `logo/search` | `POST` | `{"keyword", "country"}` | `LogoSearchList` | Tìm kiếm logo thương hiệu/công ty trực tuyến để chèn vào Watermark. |
| 23 | `logo/cutout` | `POST` | `{"imageUrl": String}` | `LogoCutoutResponse` | **[AI Cloud]** Tự động tách nền (Remove Background) cho ảnh Logo công ty do người dùng tải lên. |
| 24 | `logo/recommend/Update` | `POST` | `{"logoUrl", "watermarkId"}` | `LogoExtra` | Cập nhật gợi ý kiểu dáng logo phù hợp với mẫu Watermark. |
| 25 | `logo/report` | `POST` | `{"logoId", "url", "keyword"}` | `ReportResponse` | Ghi nhận thống kê logo được người dùng chọn sử dụng. |
| 26 | `truthful/create/batch` | `POST` | `{"photos": List<AntiCodeItem>}` | `TruthfulCreateBean` | Đăng ký hàng loạt mã chống giả (`antiFakeCode`), hash ảnh, tọa độ và thời gian chụp lên máy chủ chứng thực. |
| 27 | `truthful/Query/batch` | `POST` | `{"codes": List<String>}` | `TruthfulQueryBean` | Truy vấn xác thực hàng loạt mã chống giả khi kiểm tra ảnh. |
| 28 | `truthful/Query/batch/detail` | `POST` | `{"codes": List<String>, "ocrText": String}` | `PhotoVerifyResult` | **[AI VerifyAuth]** Kiểm tra chi tiết tính xác thực của bức ảnh (so khớp siêu dữ liệu gốc và văn bản OCR trên ảnh). |
| 29 | `photo/code/save` | `POST` | `{"photoCode", "exifInfo", "watermarkInfo"}` | `BaseResponseData` | Lưu thông tin Photo Code phục vụ tra cứu nhanh qua mã số in trên ảnh. |
| 30 | `photo/code/get` | `POST` | `{"photoCode": String}` | `PhotoCodeInfo` | Tra cứu thông tin gốc của bức ảnh từ mã `photoCode`. |
| 31 | `photo/datacenter/odd` | `POST` | `{"photoMeta", "cheatFlags"}` | `BaseResponseData` | Báo cáo dữ liệu bất thường (nghi vấn can thiệp giờ hệ thống / Mock GPS) về Data Center. |
| 32 | `overseas/understand/photo` | `POST` | `{"imageUrl", "sceneInfo"}` | `UnderstandPhotoBean` | **[AI Vision]** Phân tích ngữ cảnh bức ảnh trên đám mây để gợi ý mẫu Watermark và nhãn ngành nghề. |
| 33 | `ai/generatePrompt` | `POST` | `{"industry", "context", "keywords"}` | `GeneratePromptBean` | **[LLM Service]** Sinh nội dung ghi chú công việc / mô tả báo cáo tự động bằng mô hình ngôn ngữ lớn (LLM). |
| 34 | `workgroup/v3/upload/toCloud` | `POST` | `{"ossKey", "mediaType", "meta"}` | `UploadCloudBean` | Xác nhận hoàn tất tải tệp phương tiện lên đám mây (S3/OSS). |
| 35 | `app/shortLink/create` | `POST` | `{"originalUrl", "type"}` | `ShortLinkBean` | Tạo đường dẫn rút gọn (Short Link) để chia sẻ ảnh hoặc mẫu Watermark. |
| 36 | `app/plan/shortLink/create` | `POST` | `{"teamId", "planType"}` | `ShortLinkBean` | Tạo link thanh toán gói doanh nghiệp Teamspace trên Web (Stripe Checkout). |
| 37 | `center/app/login` | `POST` | `{"loginType", "idToken", "email", "code"}` | `LoginResultBean` | Đăng nhập vào hệ thống (hỗ trợ Google Sign-In, Microsoft MSAL, Email OTP, Apple). |
| 38 | `center/app/login/sendcode` | `POST` | `{"email", "scene"}` | `BaseResponseData` | Gửi mã xác thực OTP qua Email. |
| 39 | `center/app/logout` | `POST` | `{}` | `BaseResponseData` | Đăng xuất khỏi phiên làm việc hiện tại. |
| 40 | `center/app/cancelAccount` | `POST` | `{"reason": String}` | `BaseResponseData` | Yêu cầu xóa vĩnh viễn tài khoản người dùng (tuân thủ GDPR/Google Play). |
| 41 | `center/app/token/refresh` | `POST` | `{"refreshToken": String}` | `TokenRefreshBean` | Làm mới JWT Access Token khi hết hạn. |
| 42 | `center/app/user` | `GET` | Header `Authorization` | `UserInfoBean` | Lấy hồ sơ thông tin người dùng hiện tại. |
| 43 | `center/app/user` | `PUT` | `{"nickname", "avatar", "industry"}` | `BaseResponseData` | Cập nhật thông tin hồ sơ cá nhân. |
| 44 | `center/app/bind` | `POST` | `{"provider", "idToken"}` | `BaseResponseData` | Liên kết tài khoản hiện tại với Google / Email / Microsoft. |
| 45 | `center/app/unbind` | `POST` | `{"provider": String}` | `BaseResponseData` | Hủy liên kết tài khoản mạng xã hội. |
| 46 | `center/app/cluster/list` | `GET` | Query params | `ClusterListBean` | Lấy danh sách cụm máy chủ (Cluster) theo khu vực của tài khoản. |
| 47 | `center/app/group/list` | `GET` | Query params | `GroupListBean` | Lấy danh sách các nhóm làm việc của người dùng từ Account Center. |
| 48 | `center/app/group/list/silence` | `POST` | `{"groupIds": List}` | `BaseResponseData` | Tắt thông báo cho các nhóm làm việc được chọn. |
| 49 | `member/vip/info` | `POST` | `{}` | `VipInfo` | Kiểm tra trạng thái thuê bao **Timemark Plus (VIP)** của người dùng. |
| 50 | `member/vip/info/extra` | `POST` | `{}` | `VipInfoExtraBean` | Lấy thông tin chi tiết gói VIP, ngày hết hạn, quyền lợi dùng thử và trạng thái Free Pass. |
| 51 | `goods/query` | `POST` | `{"platform": "google_play"}` | `GoodsListBean` | Lấy danh sách gói cước (SKUs) cấu hình từ máy chủ để hiển thị trên Paywall. |
| 52 | `order/create` | `POST` | `{"goodsId", "skuId", "couponCode"}` | `OrderCreateBean` | Khởi tạo đơn hàng nội bộ trước khi gọi Google Play Billing. |
| 53 | `order/google/receipts` | `POST` | `{"orderNo", "purchaseToken", "productId"}` | `ReceiptVerifyBean` | Xác thực hóa đơn thanh toán Google Play (`purchaseToken`) trên máy chủ để kích hoạt VIP. |
| 54 | `order/bind/contract` | `POST` | `{"orderNo", "subscriptionId"}` | `BaseResponseData` | Liên kết hợp đồng thuê bao định kỳ (Auto-renewing Subscription) với tài khoản. |
| 55 | `app/adx/global_fetch_exposure_config` | `POST` | `{"appId", "country", "vipStatus"}` | `AdConfigResponse` | Lấy cấu hình hiển thị quảng cáo từ máy chủ AdX (`adxapi.timemark.com`). |
| 56 | `promote/operation/config/get` | `POST` | `{"scene", "version"}` | `OperationConfig` | Lấy cấu hình chiến dịch Marketing, banner sự kiện, khảo sát trong ứng dụng. |
| 57 | `user/Retention/feekback` | `POST` | `{"surveyId", "answers"}` | `BaseResponseData` | Gửi kết quả khảo sát giữ chân người dùng ( bao gồm cả khảo sát khi thoát màn hình Paywall). |
| 58 | `feedback/list` | `POST` | `{"page", "size"}` | `FeedbackListBean` | Lấy lịch sử phản hồi hỗ trợ kỹ thuật của người dùng. |
| 59 | `feedback/unread/num` | `POST` | `{}` | `UnreadNumBean` | Lấy số lượng tin nhắn phản hồi từ CSKH chưa đọc. |
| 60 | `user/questionnaire/submit` | `POST` | `{"questionnaireId", "data"}` | `BaseResponseData` | Gửi dữ liệu khảo sát từ bundle React Native `questionaire.android.jsbundle`. |
| 61 | `workreport/templates/classifier` | `POST` | `{"photoCount", "watermarkIds"}` | `ClassifierBean` | Phân loại và gợi ý mẫu báo cáo công việc (PDF/Excel) dựa trên bộ ảnh đã chụp. |
| 62 | `attend/take/record` | `POST` | `{"lat", "lng", "time", "photoUrl"}` | `AttendRecordBean` | Ghi nhận lượt chụp ảnh chấm công cá nhân (Attendance Check-in/Check-out). |
| 63 | `attend/feedback/create` | `POST` | `{"recordId", "content"}` | `BaseResponseData` | Gửi khiếu nại/ghi chú bổ sung cho bản ghi chấm công. |
| 64 | `attend/Export` | `POST` | `{"month", "format", "email"}` | `AttendExportBean` | Xuất bảng chấm công cá nhân theo tháng ra tệp Excel/PDF. |
| 65 | `upgrade/info` | `POST` | `{"versionCode", "channel"}` | `UpgradeInfoBean` | Kiểm tra bản cập nhật ứng dụng mới nhất. |
| 66 | `Simple/upload/log` | `POST` | `{"logUrl", "errorType", "deviceInfo"}` | `BaseResponseData` | Báo cáo đường dẫn tệp log lỗi đã tải lên S3 để đội kỹ thuật phân tích. |
| 67 | `ab/query` | `POST` | `{"deviceId", "userId"}` | `AbTestBean` | Lấy danh sách phân nhóm thử nghiệm A/B Testing của thiết bị. |
| 68 | `push/token/upload` | `POST` | `{"fcmToken", "deviceId"}` | `BaseResponseData` | Đăng ký Firebase Cloud Messaging (FCM) Token để nhận thông báo đẩy. |

---

### 3.2. Phân Hệ 2: `ClusterApiService` — Hệ Sinh Thái Teamspace SaaS, Dự Án, Chấm Công, Geofence & Gói Doanh Nghiệp (91 Endpoints)

| STT | Endpoint Path | Phương thức | Nhóm nghiệp vụ | Mô tả chức năng chi tiết |
| :---: | :--- | :---: | :--- | :--- |
| 69 | `app/group/create` | `POST` | Quản lý Nhóm (Team) | Tạo không gian làm việc nhóm mới (Teamspace). |
| 70 | `app/group/delete` | `POST` | Quản lý Nhóm | Giải tán / xóa nhóm làm việc. |
| 71 | `app/group/rename` | `POST` | Quản lý Nhóm | Đổi tên nhóm làm việc. |
| 72 | `app/group/query` | `POST` | Quản lý Nhóm | Truy vấn thông tin chi tiết của một nhóm làm việc. |
| 73 | `app/group/setting` | `POST` | Quản lý Nhóm | Lấy cấu hình quyền hạn và quy tắc chụp ảnh của nhóm. |
| 74 | `app/group/setting/update` | `POST` | Quản lý Nhóm | Cập nhật cài đặt nhóm (chỉ cho phép chụp ảnh trực tiếp, khóa sửa giờ...). |
| 75 | `app/group/onboarding/Update` | `POST` | Quản lý Nhóm | Cập nhật trạng thái hoàn thành hướng dẫn tạo nhóm. |
| 76 | `app/group/user/info` | `POST` | Thành viên & Phân quyền | Lấy thông tin của người dùng hiện tại trong nhóm (Role, biệt danh). |
| 77 | `app/group/user/update` | `POST` | Thành viên & Phân quyền | Cập nhật tên hiển thị/chức vụ của thành viên trong nhóm. |
| 78 | `app/group/user/delete` | `POST` | Thành viên & Phân quyền | Xóa thành viên khỏi nhóm hoặc tự rời nhóm. |
| 79 | `app/group/member/list` | `POST` | Thành viên & Phân quyền | Lấy danh sách toàn bộ thành viên trong nhóm làm việc. |
| 80 | `app/group/member/info` | `POST` | Thành viên & Phân quyền | Xem hồ sơ và thống kê ảnh chụp của một thành viên. |
| 81 | `app/group/member/query` | `POST` | Thành viên & Phân quyền | Tìm kiếm thành viên trong nhóm theo tên hoặc email. |
| 82 | `app/group/role/list` | `POST` | Thành viên & Phân quyền | Lấy danh sách các vai trò (Owner, Admin, Member, Guest) trong nhóm. |
| 83 | `app/group/role/member/add` | `POST` | Thành viên & Phân quyền | Gán vai trò quản trị (Admin/Manager) cho thành viên. |
| 84 | `app/group/role/member/delete` | `POST` | Thành viên & Phân quyền | Thu hồi vai trò quản trị của thành viên. |
| 85 | `app/group/role/project/edit` | `POST` | Thành viên & Phân quyền | Phân quyền truy cập theo từng dự án (Project-level ACL) cho thành viên. |
| 86 | `app/group/invite/email/send` | `POST` | Mời thành viên | Gửi lời mời tham gia nhóm qua danh sách Email. |
| 87 | `app/group/invite/link/get` | `POST` | Mời thành viên | Lấy đường dẫn mời (Invite Link) và mã QR của nhóm. |
| 88 | `app/group/invite/link/join` | `POST` | Mời thành viên | Tham gia nhóm thông qua đường dẫn mời. |
| 89 | `app/group/invite/user/status` | `POST` | Mời thành viên | Kiểm tra trạng thái lời mời đã gửi. |
| 90 | `app/group/join` | `POST` | Mời thành viên | Gia nhập nhóm làm việc bằng mã nhóm (Team Code). |
| 91 | `app/group/project/create` | `POST` | Quản lý Dự án (Project) | Tạo dự án/công trình mới bên trong nhóm (kèm tọa độ và bán kính). |
| 92 | `app/group/project/update` | `POST` | Quản lý Dự án | Cập nhật thông tin dự án (tên, địa chỉ, trạng thái). |
| 93 | `app/group/project/delete` | `POST` | Quản lý Dự án | Xóa dự án khỏi nhóm. |
| 94 | `app/group/project/list` | `POST` | Quản lý Dự án | Lấy danh sách các dự án trong nhóm. |
| 95 | `app/group/project/search` | `POST` | Quản lý Dự án | Tìm kiếm dự án theo tên hoặc vị trí GPS gần nhất. |
| 96 | `app/group/project/detail/stats` | `POST` | Quản lý Dự án | Xem thống kê số lượng ảnh, video, thành viên tham gia của một dự án. |
| 97 | `app/group/project/Settings` | `POST` | Quản lý Dự án | Cấu hình mẫu Watermark bắt buộc và quy tắc riêng cho từng dự án. |
| 98 | `app/group/watermark/add` | `POST` | Watermark Nhóm | Thêm mẫu Watermark chuẩn của công ty vào nhóm. |
| 99 | `app/group/watermark/update` | `POST` | Watermark Nhóm | Chỉnh sửa mẫu Watermark chung của nhóm (đồng bộ tức thì tới mọi nhân viên). |
| 100 | `app/group/watermark/delete` | `POST` | Watermark Nhóm | Xóa mẫu Watermark khỏi nhóm. |
| 101 | `app/group/watermark/list` | `POST` | Watermark Nhóm | Lấy danh sách mẫu Watermark được phép dùng trong nhóm. |
| 102 | `app/group/watermark/move` | `POST` | Watermark Nhóm | Sắp xếp thứ tự ưu tiên hiển thị của các mẫu Watermark nhóm. |
| 103 | `app/group/watermark/project/Update` | `POST` | Watermark Nhóm | Gán mẫu Watermark mặc định cho một dự án cụ thể. |
| 104 | `app/photo/upload` | `POST` | Đồng bộ Ảnh/Video Nhóm | Tải siêu dữ liệu ảnh/video chụp tại hiện trường vào kho ảnh Teamspace. |
| 105 | `app/photo/delete` | `POST` | Đồng bộ Ảnh/Video Nhóm | Xóa ảnh khỏi kho lưu trữ đám mây của nhóm. |
| 106 | `app/photo/move` | `POST` | Đồng bộ Ảnh/Video Nhóm | Di chuyển ảnh sang dự án khác trong cùng nhóm. |
| 107 | `app/photo/list` | `POST` | Đồng bộ Ảnh/Video Nhóm | Lấy danh sách ảnh trong nhóm (phân trang theo thời gian/dự án/thành viên). |
| 108 | `app/photo/search` | `POST` | Đồng bộ Ảnh/Video Nhóm | Tìm kiếm ảnh trong nhóm theo từ khóa OCR, ghi chú, người chụp, ngày chụp, dự án. |
| 109 | `app/photo/ search/options` | `POST` | Đồng bộ Ảnh/Video Nhóm | Lấy bộ lọc tìm kiếm nâng cao (danh sách thành viên, dự án, mẫu watermark). |
| 110 | `app/photo/detail/get` | `POST` | Đồng bộ Ảnh/Video Nhóm | Xem chi tiết một bức ảnh (tọa độ bản đồ, mã chống giả, thông tin EXIF, người chụp). |
| 111 | `app/photo/detail/update` | `POST` | Đồng bộ Ảnh/Video Nhóm | Cập nhật ghi chú hoặc nhãn phân loại cho bức ảnh. |
| 112 | `app/photo/batch/share` | `POST` | Chia sẻ & Xuất Báo Cáo | Tạo gói chia sẻ hàng loạt ảnh cho khách hàng/đối tác. |
| 113 | `app/photo/batch/share/detail` | `POST` | Chia sẻ & Xuất Báo Cáo | Xem chi tiết gói chia sẻ ảnh hàng loạt. |
| 114 | `app/photo/export/task/create` | `POST` | Chia sẻ & Xuất Báo Cáo | Tạo tiến trình xuất báo cáo đám mây (PDF, Excel có ảnh, KMZ Google Earth, ZIP). |
| 115 | `app/photo/export/task/status` | `POST` | Chia sẻ & Xuất Báo Cáo | Kiểm tra tiến độ tạo file báo cáo trên máy chủ và lấy link tải về. |
| 116 | `app/photo/export/template/list` | `POST` | Chia sẻ & Xuất Báo Cáo | Lấy danh sách mẫu bố cục xuất báo cáo PDF/Excel. |
| 117 | `app/photo/map/markers` | `POST` | Bản đồ Ảnh (Photo Map) | Lấy danh sách các điểm đánh dấu (Markers) ảnh chụp trên bản đồ theo vùng nhìn (Bounding Box). |
| 118 | `app/photo/map/cluster/photos` | `POST` | Bản đồ Ảnh (Photo Map) | Lấy danh sách ảnh nằm trong một cụm điểm (Cluster) trên bản đồ. |
| 119 | `app/checklist/create` | `POST` | Danh mục kiểm tra (Checklist) | Tạo mẫu danh mục kiểm tra hiện trường (Checklist Template). |
| 120 | `app/checklist/update` | `POST` | Danh mục kiểm tra | Cập nhật nội dung mẫu Checklist. |
| 121 | `app/checklist/delete` | `POST` | Danh mục kiểm tra | Xóa mẫu Checklist. |
| 122 | `app/checklist/list` | `POST` | Danh mục kiểm tra | Lấy danh sách các Checklist trong dự án/nhóm. |
| 123 | `app/checklist/detail` | `POST` | Danh mục kiểm tra | Xem chi tiết các hạng mục cần chụp ảnh nghiệm thu trong một Checklist. |
| 124 | `app/checklist/item/submit` | `POST` | Danh mục kiểm tra | Nộp ảnh nghiệm thu cho một hạng mục trong Checklist. |
| 125 | `app/checklist/submission/list` | `POST` | Danh mục kiểm tra | Lấy danh sách các báo cáo Checklist đã hoàn thành. |
| 126 | `app/checklist/export` | `POST` | Danh mục kiểm tra | Xuất báo cáo nghiệm thu Checklist ra file PDF. |
| 127 | `app/project/file/create` | `POST` | Quản lý Tài liệu Dự án | Tải tài liệu đính kèm (bản vẽ CAD, PDF, Word) lên thư mục dự án. |
| 128 | `app/project/file/delete` | `POST` | Quản lý Tài liệu Dự án | Xóa tài liệu khỏi dự án. |
| 129 | `app/project/file/list` | `POST` | Quản lý Tài liệu Dự án | Lấy danh sách tài liệu của dự án. |
| 130 | `app/project/file/rename` | `POST` | Quản lý Tài liệu Dự án | Đổi tên tài liệu trong dự án. |
| 131 | `app/attendance/rule/get` | `POST` | Chấm công Nhóm & Geofence | Lấy quy tắc ca làm việc, giờ vào/ra của nhóm. |
| 132 | `app/attendance/rule/update` | `POST` | Chấm công Nhóm & Geofence | Thiết lập ca làm việc và quy tắc chấm công cho nhóm. |
| 133 | `app/attendance/punch` | `POST` | Chấm công Nhóm & Geofence | Thực hiện bấm giờ vào/ra ca (Clock-in / Clock-out) kèm ảnh chụp xác thực. |
| 134 | `app/attendance/status` | `POST` | Chấm công Nhóm & Geofence | Kiểm tra trạng thái ca làm việc hiện tại của nhân viên. |
| 135 | `app/attendance/records/daily` | `POST` | Chấm công Nhóm & Geofence | Xem bảng chấm công theo ngày của cả nhóm. |
| 136 | `app/attendance/records/monthly` | `POST` | Chấm công Nhóm & Geofence | Xem tổng hợp công và giờ làm thêm (Overtime) theo tháng. |
| 137 | `app/attendance/export` | `POST` | Chấm công Nhóm & Geofence | Xuất bảng lương/chấm công nhóm ra file Excel. |
| 138 | `app/group/timeclock/geofence/create` | `POST` | Chấm công Nhóm & Geofence | Tạo hàng rào địa lý (Geofence: tọa độ tâm + bán kính mét) bắt buộc nhân viên phải đứng trong vùng mới được chấm công. |
| 139 | `app/group/timeclock/geofence/update` | `POST` | Chấm công Nhóm & Geofence | Cập nhật tọa độ hoặc bán kính vùng Geofence. |
| 140 | `app/group/timeclock/geofence/delete` | `POST` | Chấm công Nhóm & Geofence | Xóa vùng Geofence chấm công. |
| 141 | `app/group/timeclock/geofence/list` | `POST` | Chấm công Nhóm & Geofence | Lấy danh sách các điểm Geofence được phép chấm công. |
| 142 | `app/group/timeclock/geofence/check` | `POST` | Chấm công Nhóm & Geofence | Kiểm tra tọa độ hiện tại của nhân viên có nằm trong vùng Geofence hợp lệ không. |
| 143 | `business/subscription/info` | `POST` | Thuê bao Doanh nghiệp | Xem thông tin gói cước **Teamspace Business Plan** hiện tại của nhóm (dung lượng GB, giới hạn thành viên). |
| 144 | `business/subscription/gp/goods/list` | `POST` | Thuê bao Doanh nghiệp | Lấy danh sách các gói thuê bao Business trên Google Play. |
| 145 | `business/subscription/gp/order/create` | `POST` | Thuê bao Doanh nghiệp | Tạo đơn hàng nâng cấp gói Business cho nhóm. |
| 146 | `business/subscription/gp/receipt/verify` | `POST` | Thuê bao Doanh nghiệp | Xác thực thanh toán Google Play cho gói Business. |
| 147 | `business/subscription/member/quota` | `POST` | Thuê bao Doanh nghiệp | Kiểm tra hạn mức số lượng thành viên và dung lượng lưu trữ còn lại của nhóm. |
| 148 | `business/subscription/trial/apply` | `POST` | Thuê bao Doanh nghiệp | Đăng ký dùng thử miễn phí gói Teamspace Business. |
| 149 | `web/teamspace/app/integration/list` | `POST` | Tích hợp Phần mềm thứ 3 | Lấy danh sách các tích hợp Webhook / Google Drive / OneDrive / Dropbox / Zapier của nhóm. |
| 150 | `web/teamspace/app/integration/bind` | `POST` | Tích hợp Phần mềm thứ 3 | Kết nối tài khoản đám mây bên thứ 3 để tự động đồng bộ ảnh chụp sang Google Drive/OneDrive. |
| 151 | `web/teamspace/app/integration/unbind` | `POST` | Tích hợp Phần mềm thứ 3 | Ngắt kết nối tích hợp bên thứ 3. |
| 152 | `web/teamspace/app/integration/status` | `POST` | Tích hợp Phần mềm thứ 3 | Kiểm tra trạng thái đồng bộ tự động sang đám mây bên thứ 3. |
| 153 | `app/share/link/info` | `POST` | Chia sẻ Liên kết | Lấy thông tin hiển thị khi người dùng mở một liên kết chia sẻ từ Teamspace. |
| 154 | `app/share/link/photos` | `POST` | Chia sẻ Liên kết | Lấy danh sách ảnh trong một liên kết chia sẻ công khai. |
| 155 | `app/workgroup/home` | `POST` | Bảng điều khiển Nhóm | Lấy dữ liệu tổng hợp cho màn hình chính Teamspace (số ảnh mới, dự án gần đây, thông báo). |
| 156 | `app/workgroup/summary/read` | `POST` | Bảng điều khiển Nhóm | Đánh dấu đã đọc báo cáo tóm tắt hoạt động nhóm. |
| 157 | `app/workgroup/notification/list` | `POST` | Thông báo Nhóm | Lấy danh sách thông báo hoạt động trong nhóm (thành viên mới, ảnh mới, bình luận). |
| 158 | `app/workgroup/notification/read` | `POST` | Thông báo Nhóm | Đánh dấu đã đọc thông báo nhóm. |
| 159 | `app/workgroup/comment/add` | `POST` | Bình luận Ảnh | Thêm bình luận/nhận xét trực tiếp vào bức ảnh công trường của nhân viên. |

---

### 3.3. Phân Hệ 3: `EarnApiService` — Kiếm Tiền Từ Thu Thập Dữ Liệu Huấn Luyện Robot (Embodied AI) (9 Endpoints)

Trích xuất từ `com.xhey.xcamera.earnhost.EarnApiService.java`:

| STT | Endpoint Path | Phương thức | Request Body / Tham số | Response Model | Mô tả chức năng nghiệp vụ |
| :---: | :--- | :---: | :--- | :--- | :--- |
| 160 | `app/reward/video/balance` | `POST` | `{}` | `BaseResponse<EarnBalanceData>` | Lấy số dư tiền thưởng hiện tại (`balanceFen`, đơn vị xu của `BRL`/`MXN`/`USD`), tổng thu nhập và trạng thái tài khoản nhận thưởng. |
| 161 | `app/reward/video/task/list` | `POST` | `{"page", "size", "country"}` | `BaseResponse<EarnTaskListData>` | Lấy danh sách các nhiệm vụ quay video góc nhìn thứ nhất (ví dụ: gấp quần áo, pha cà phê, rửa bát, sửa chữa) kèm mức thưởng mỗi video. |
| 162 | `app/reward/video/apply/list` | `POST` | `{"status", "page"}` | `BaseResponse<EarnApplyListData>` | Lấy danh sách các nhiệm vụ mà người dùng đã đăng ký nhận làm và lịch sử video đã nộp (Đang duyệt / Đã duyệt / Từ chối). |
| 163 | `app/reward/video/details` | `POST` | `{"taskId": Long}` | `BaseResponse<EarnTaskDetailData>` | Lấy hướng dẫn chi tiết của nhiệm vụ: video mẫu, yêu cầu góc máy, yêu cầu hiển thị 2 bàn tay (`HandLandmark`), thời lượng tối thiểu/tối đa. |
| 164 | `app/reward/video/recent/approved/list` | `POST` | `{"limit": Int}` | `BaseResponse<EarnApprovedListData>` | Lấy danh sách người dùng vừa được duyệt trả thưởng gần đây (hiển thị dạng ticker tạo hiệu ứng Social Proof). |
| 165 | `app/reward/video/submit` | `POST` | `{"taskId", "mcapS3Url", "durationMs", "handRatio", "imuStats", "deviceModel"}` | `BaseResponse<EarnSubmitResult>` | Nộp tệp dữ liệu cảm biến `.mcap` (đã upload lên AWS S3) cùng thống kê chất lượng khung hình bàn tay và IMU để hệ thống chấm điểm trả thưởng. |
| 166 | `app/reward/video/withdrawal/apply` | `POST` | `{"amountFen", "currency", "bankCode", "accountName", "accountNo", "pixKeyType", "taxId"}` | `BaseResponse<WithdrawalApplyResult>` | Yêu cầu rút tiền mặt về tài khoản ngân hàng (hỗ trợ **Pix** tại Brazil bằng `BRL` và chuyển khoản ngân hàng tại Mexico bằng `MXN`) hoặc đổi sang ngày VIP `Timemark Plus`. |
| 167 | `app/reward/video/withdrawal/banks` | `POST` | `{"country": String}` | `BaseResponse<BankListData>` | Lấy danh sách ngân hàng hỗ trợ rút tiền theo quốc gia. |
| 168 | `app/reward/video/withdrawal/status` | `POST` | `{"withdrawalId": String}` | `BaseResponse<WithdrawalStatusData>` | Kiểm tra trạng thái lệnh rút tiền (Đang xử lý / Thành công / Thất bại). |

---

### 3.4. Phân Hệ 4 & 5: `DualModeApiService` (7 Endpoints) & `OSSApi` / `S3TokenApi` (2 Endpoints)

| STT | Interface | Endpoint Path | Phương thức | Mô tả chức năng chi tiết |
| :---: | :--- | :--- | :---: | :--- |
| 169 | `DualModeApiService` | `antiFakeCode/generate` | `POST` | Yêu cầu máy chủ sinh trước lô mã chống giả (`antiFakeCode`) để thiết bị dùng dần khi chụp ảnh ngoại tuyến (Offline). |
| 170 | `DualModeApiService` | `workgroup/photo/watermark/remove/check` | `POST` | Kiểm tra xem người dùng có quyền xóa Watermark (khôi phục ảnh gốc qua `JpegPacker`) đối với bức ảnh hiện tại hay không. |
| 171 | `DualModeApiService` | `favorite/address/add` | `POST` | Lưu một địa chỉ/tọa độ vào danh sách Địa điểm Yêu thích của người dùng. |
| 172 | `DualModeApiService` | `favorite/address/cancel` | `POST` | Xóa địa chỉ khỏi danh sách Yêu thích. |
| 173 | `DualModeApiService` | `favorite/address/list` | `POST` | Lấy danh sách các địa chỉ Yêu thích đã lưu. |
| 174 | `DualModeApiService` | `app/user/event/add` | `POST` | Ghi nhận sự kiện hành vi người dùng (ví dụ: số lần dùng tính năng cao cấp miễn phí). |
| 175 | `DualModeApiService` | `app/user/event/count` | `POST` | Truy vấn số lần người dùng đã thực hiện một hành vi để quyết định bật Paywall. |
| 176 | `OSSApi` | `workgroup/v4/oss/token` | `POST` | Lấy STS Token tạm thời (`AccessKeyId`, `AccessKeySecret`, `SecurityToken`) để upload tệp lên Alibaba Cloud OSS. |
| 177 | `S3TokenApi` | `workgroup/v4/s3/token` | `POST` | Lấy STS Token tạm thời (`accessKey`, `secretKey`, `sessionToken`, `bucket`, `region`) để upload ảnh, báo cáo và tệp `.mcap` lên **Amazon S3**. |

---

## 4. Phân Tích Chuyên Sâu Các Kết Nối Tới Dịch Vụ LLM & Xử Lý Hình Ảnh/AI (Mục 18)

Qua rà soát toàn bộ mã nguồn (`com.xhey.xcamerasdk.algorithm.nn.*` và `TodayApiServiceKt`), ứng dụng sử dụng kiến trúc **Hybrid AI (Kết hợp Cloud LLM/Vision API và On-Device Edge AI)**:

### 4.1. Các Kết Nối Tới Dịch Vụ LLM & Cloud Vision AI (Qua Gateway `app.timemark.com/next/`)
> **Điểm kiến trúc quan trọng:** Ứng dụng **không gọi trực tiếp** từ điện thoại tới `api.openai.com` hay `generativelanguage.googleapis.com` (nhằm tránh lộ API Key của OpenAI/Claude/Gemini trên client), mà định tuyến toàn bộ qua Backend Gateway của Timemark:

1. **Sinh văn bản & gợi ý nội dung bằng LLM (`POST /next/ai/generatePrompt`)**:
   - **Mục đích**: Khi người dùng tạo mẫu Watermark mới hoặc viết báo cáo công trường/kiểm tra hiện trường, ứng dụng gửi ngành nghề (`industry`), ngữ cảnh (`context`) và từ khóa lên endpoint `ai/generatePrompt`. Máy chủ LLM trả về nội dung mô tả công việc, tiêu đề báo cáo và các trường dữ liệu gợi ý chuẩn xác theo ngành.
2. **Hiểu ngữ cảnh bức ảnh bằng Cloud Vision/VLM (`POST /next/overseas/understand/photo`)**:
   - **Mục đích**: Gửi ảnh chụp hoặc khung cảnh camera lên máy chủ Vision Language Model (VLM) để phân loại bối cảnh (công trường xây dựng, giao hàng, vệ sinh công nghiệp, tuần tra an ninh...) và tự động đề xuất mẫu Watermark phù hợp nhất.
3. **Tách nền Logo tự động bằng AI (`POST /next/logo/cutout` & `POST /next/logo/recommend/Update`)**:
   - **Mục đích**: Khi người dùng tải ảnh logo công ty từ thư viện máy hoặc tìm qua `logo/search`, API `logo/cutout` sử dụng mô hình Segmentation trên đám mây để xóa phông nền, trả về ảnh PNG trong suốt đặt vừa vặn vào góc Watermark.
4. **Kiểm định ảnh thật/giả bằng OCR + Cryptographic Verification (`POST /next/truthful/Query/batch/detail`)**:
   - **Mục đích**: Trong module React Native `verify_auth.android.jsbundle`, người dùng tải ảnh cần giám định lên. Hệ thống kết hợp kết quả trích xuất chữ (`PaddleOCR`) và mã `antiFakeCode` gửi lên `truthful/Query/batch/detail` để đối chiếu xem chữ in trên ảnh (giờ, ngày, địa chỉ, tọa độ) có bị chỉnh sửa bằng Photoshop so với bản ghi gốc lúc bấm máy hay không.

### 4.2. Danh Mục 9 Mô Hình AI / Thị Giác Máy Tính Chạy Trực Tiếp Trên Thiết Bị (On-Device Edge AI)
Bên cạnh Cloud AI, Timemark nhúng trực tiếp các thuật toán AI chạy ngoại tuyến trên NPU/GPU/CPU của điện thoại thông qua `libnnpackage-f99779bd.so`:

| STT | Lớp Java/Kotlin (`com.xhey.xcamerasdk.algorithm.nn.*`) | Mã Model (`CVModelId`) | Công nghệ / Framework | Đường dẫn Model / Tài nguyên | Chức năng thực tế trong ứng dụng |
| :---: | :--- | :---: | :--- | :--- | :--- |
| 1 | `ZeroDCE.java` & `ZeroDCELite.java` | NightMode | Zero-Reference Deep Curve Estimation | `assets/algorithm/nightmode/zero_dce_full_6head_more_self_data_6epoch_v2` | Tăng sáng và khử nhiễu ảnh chụp ban đêm / tầng hầm công trường thiếu sáng thời gian thực. |
| 2 | `SileroVad.java` | Audio VAD | Tencent NCNN (`silero_vad_16k`) | `assets/nn/silerovad/silero_vad_16k.ncnn.bin` & `.param` | Phát hiện hoạt động giọng nói (Voice Activity Detection) tần số 16kHz. |
| 3 | `HandLandmark.java` | Hand Pose | Google MediaPipe + NCNN | `initMediaPipe()` / `detect()` trong `libnnpackage` | Nhận diện hộp giới hạn bàn tay (`HandBox`) và **21 khớp xương bàn tay (21 Landmarks)** phục vụ quay video huấn luyện Robot (`EmbodiedEngine`). |
| 4 | `SteelDetect.java` | `CVModelId = 4` | Object Detection (NCNN) | Tải động vào cache khi chọn Watermark ID `57` | **Đếm số lượng ống thép / thanh sắt xây dựng** tự động qua ảnh chụp và đánh dấu từng thanh. |
| 5 | `VLPR.java` | `CVModelId = 1` | License Plate Recognition | Tải động khi chọn Watermark ID `52` | **Nhận diện biển số xe tự động** (Vehicle License Plate Recognition) để điền vào Watermark kiểm soát xe ra vào cổng. |
| 6 | `FaceDetect.java` | `CVModelId = 2` | Face Detection & Counting | Tải động khi chọn Watermark ID `54` | **Đếm số người / khuôn mặt** trong ảnh chụp tập thể điểm danh đầu ca. |
| 7 | `PaddleOCR.java` | OCR Engine | Baidu PaddleOCR Lite | Native JNI `libnnpackage` | Nhận diện văn bản và đọc mã `AntiFakeCode` trực tiếp trên ảnh chụp. |
| 8 | `Sliceocr.java` | Slice OCR | Custom OCR | Native JNI `libnnpackage` | Đọc nhanh các vùng cắt chữ trên nhãn Watermark để kiểm tra toàn vẹn. |
| 9 | SenseTime SenseME / SenseMARS | Face Beauty | SenseTime SDK | `assets/sensetime/M_SenseME_Face_Video_Template_p_4.0.0.model` | Làm mịn da, làm sáng mắt, trắng răng và hiệu ứng AR trên khuôn mặt. |
