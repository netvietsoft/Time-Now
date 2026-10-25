# 09. Danh Sách Màn Hình, Cấu Trúc Giao Diện, Bảng Màu, Tọa Độ & Đặc Tả Redesign UI/UX (Timemark Camera)

> **Đáp ứng Yêu cầu:** **Mục 17** (Danh sách tất cả các màn hình, cấu trúc, màu sắc, tọa độ kích thước kèm bộ thiết kế lại UI/UX hiện đại trong thư mục `Redesign/`).

---

## 1. Bảng Màu Hệ Thống & Token Thiết Kế Trích Xuất Từ APK (`res/values/colors.xml` & `dimens.xml`)

Qua phân tích các tệp tài nguyên `decompiled/resources/res/values/colors.xml`, `styles.xml` và các mẫu Watermark JSON trong `assets/watermark/global/`, hệ thống thiết kế (Design System) của **Timemark** sử dụng bảng màu độ tương phản cao (High-Contrast Industrial Palette) để đảm bảo nhìn rõ dưới ánh nắng gắt ngoài công trường:

| Nhóm Màu | Tên Token Thiết Kế | Mã Màu Hex (Gốc APK) | Mã Màu Redesign (Nâng Cấp) | Vai Trò & Vị Trí Áp Dụng |
| :--- | :--- | :---: | :---: | :--- |
| **Brand Primary** | `colorPrimary` / `timemark_blue` | `#0066FF` | `#2563EB` | Nút hành động chính (CTA), Tab đang chọn, Huy hiệu xác thực thời gian thực (`BLUE_REAL_TIME_OFFICIAL_WM_ID`). |
| **Brand Accent / Industrial** | `timemark_orange` / `safety_amber` | `#FF6B00` | `#F59E0B` | Nhãn công trường, Nút tạo báo cáo nhanh, Cảnh báo ngoài vùng Geofence. |
| **VIP / Timemark Plus** | `vip_gold_primary` / `vip_gradient` | `#FFB800` -> `#FF8A00` | `#FBBF24` -> `#D97706` | Giao diện Paywall `Timemark Plus`, Huy hiệu mẫu Watermark VIP, Nút xóa Watermark. |
| **Anti-Tamper Verified** | `verified_emerald` | `#00C48C` | `#10B981` | Dấu tích xanh chứng nhận Giờ Chuẩn & GPS thật (`Verified by Timemark`), Trạng thái hoàn thành Checklist. |
| **Earn / Robotics AI** | `earn_purple_cyan` | `#7C3AED` / `#06B6D4` | `#6366F1` -> `#06B6D4` | Giao diện module kiếm tiền **Earn**, Khung xương bàn tay 21 khớp (`HandOverlayView`). |
| **Camera Surface (Dark)** | `camera_bg_dark` / `glass_overlay` | `#000000` / `#80000000` | `#090D16` / `rgba(15,23,42,0.72)` | Nền màn hình Camera, Thanh công cụ mờ trong suốt (Glassmorphism) đè lên khung ngắm. |
| **App Surface (Light)** | `bg_light_primary` / `card_surface` | `#F5F6FA` / `#FFFFFF` | `#F8FAFC` / `#FFFFFF` | Nền các màn hình Teamspace, Thư viện ảnh, Cài đặt và Quản lý dự án. |
| **Typography & Borders** | `text_primary` / `text_secondary` | `#111827` / `#6B7280` | `#0F172A` / `#64748B` | Văn bản tiêu đề, phụ đề, thông số EXIF/GPS và đường viền phân cách (`#E2E8F0`). |

---

## 2. Danh Mục Chi Tiết 12 Màn Hình Cốt Lõi, Cấu Trúc & Tọa Độ Kích Thước (Viewport 390×844 dp)

Bảng dưới đây mô tả chi tiết cấu trúc bố cục, hệ tọa độ (`[x, y, width, height]` trên khung chuẩn `390×844 dp`), file XML gốc và thông số thiết kế lại cho 12 màn hình quan trọng nhất của ứng dụng:

| STT | Tên Màn Hình | Class Activity / Fragment | File XML Gốc (`res/layout/`) | Cấu Trúc Vùng & Tọa Độ Chính (`x, y, w, h` theo dp) | Điểm Nhấn Trong Bản Redesign (`Redesign/preview_screens.html`) |
| :---: | :--- | :--- | :--- | :--- | :--- |
| 1 | **Camera & Live Watermark (Màn hình chính)** | `PreviewActivity` | `activity_preview.xml`, `fragment_preview_new.xml` | • **Top Bar**: `[0, 44, 390, 48]` (Flash, Ratio, HD, Filter, Lens, Settings)<br/>• **Viewfinder**: `[0, 92, 390, 560]`<br/>• **Mini-Map & Compass**: `[16, 108, 96, 96]`<br/>• **Live Watermark Card**: `[16, 468, 320, 168]`<br/>• **Anti-Fake Code Strip**: `[362, 468, 16, 168]`<br/>• **Mode Tabs**: `[0, 652, 390, 40]`<br/>• **Bottom Shutter Bar**: `[0, 692, 390, 118]` | Thiết kế Glassmorphism hiện đại, hiển thị rõ đồng hồ LED 7 đoạn (`Segment7`), huy hiệu `NTP & GPS Verified`, bản đồ vệ tinh bo góc và nút chuyển nhanh ống kính `0.5x / 1x / 2x`. |
| 2 | **Kho Mẫu & Chỉnh Sửa Watermark** | `WatermarkEditFragment` | `fragment_watermark_edit_new.xml`, `layout_watermark_store.xml` | • **Live Preview Header**: `[0, 44, 390, 240]`<br/>• **Category Tabs**: `[0, 284, 390, 44]`<br/>• **Field Toggle List / Template Grid**: `[0, 328, 390, 440]` (Grid 2 cột, mỗi card `174×132 dp`)<br/>• **Action Footer**: `[0, 768, 390, 76]` | Tách biệt rõ 2 chế độ: **Chọn mẫu theo ngành** (Xây dựng, Giao hàng, Chấm công, Đếm thép AI) và **Tuỳ chỉnh từng trường** (Bật/tắt Tọa độ UTM, Thời tiết, Tách nền Logo AI). |
| 3 | **Không Gian Nhóm (Teamspace SaaS Home)** | `TeamSpaceHomeParentActivity` | `activity_team_space_home_parent.xml`, `fragment_team_space_home.xml` | • **Team Header & Quota Bar**: `[0, 44, 390, 72]`<br/>• **Search & Filter Chips**: `[16, 124, 358, 44]`<br/>• **Project Summary Cards**: `[16, 176, 358, 96]`<br/>• **Photo Feed Stream**: `[16, 284, 358, 480]`<br/>• **Bottom Navigation (4 Tabs)**: `[0, 764, 390, 80]` | Hiển thị trực quan thanh dung lượng đám mây S3, bộ lọc ảnh theo Dự án/Thành viên/OCR, thẻ ảnh hiện trường kèm vị trí GPS và nút xuất báo cáo 1 chạm. |
| 4 | **Thư Viện Nội Bộ & Xuất Báo Cáo (PDF/Excel/KMZ)** | `LocalPreviewActivity` | `activity_local_preview.xml`, `fragment_local_preview.xml` | • **Top Toolbar & Batch Counter**: `[0, 44, 390, 52]`<br/>• **Photo Preview / Multi-Select Grid**: `[0, 96, 390, 520]`<br/>• **Export Format Selector Sheet**: `[0, 616, 390, 228]` (4 nút lớn: `PDF Report`, `Excel .xlsx`, `Google Earth .KMZ`, `Remove Watermark`) | Trực quan hóa bố cục trang xuất báo cáo PDF (1/2/4 ảnh mỗi trang), bảng dữ liệu Excel Apache POI và nút khôi phục ảnh gốc bằng `JpegPacker`. |
| 5 | **Trung Tâm Giám Định Ảnh Thật/Giả (VerifyAuth)** | `VerifyAuthActivity` (RN) & `PhotoVerifyActivity` | `assets/verify_auth.android.jsbundle`, `activity_photo_verify.xml` | • **Header & Security Badge**: `[0, 44, 390, 64]`<br/>• **Upload / Scan Photo Box**: `[16, 120, 358, 200]`<br/>• **Cryptographic Audit Card**: `[16, 336, 358, 280]` (Đối chiếu 4 thông số: `AntiFakeCode`, `NTP Time`, `GPS & Mock Check`, `Aegis Hardware Nonce`)<br/>• **Certificate Export Button**: `[16, 632, 358, 52]` | Giao diện kiểm toán số chuyên nghiệp: hiển thị bảng đối chiếu giữa chữ OCR trên ảnh và bản ghi gốc trên máy chủ `/truthful/Query/batch/detail`. |
| 6 | **Màn Hình Paywall Kép (Timemark Plus & Business)** | `PurchaseActivity` | `activity_purchase_new.xml`, `fragment_purchase_pro.xml` | • **Dual Mode Switcher (`Plus` / `Business`)**: `[45, 56, 300, 40]`<br/>• **Feature Comparison Hero**: `[16, 112, 358, 260]`<br/>• **SKU Pricing Cards (3 Gói)**: `[16, 388, 358, 260]` (`Annual $29.99`, `Monthly $6.99`, `1-Year One-time $34.99`)<br/>• **Subscribe CTA & Stripe Link**: `[16, 664, 358, 120]` | Chuyển đổi mượt mà giữa giao diện **Vàng Kim (Cá nhân)** và **Xanh Doanh Nghiệp (Teamspace)**, kèm thông báo dùng thử miễn phí và tùy chọn thanh toán qua Stripe Web. |
| 7 | **Kiếm Tiền Huấn Luyện Robot (Earn & AR Collect)** | `EarnHomeActivity` & `ArCollectActivity` | `activity_earn_home.xml`, `activity_ar_collect.xml` | • **Wallet Balance & Pix/SPEI Card**: `[16, 56, 358, 112]`<br/>• **AR Viewfinder + 21 Hand Landmarks**: `[16, 180, 358, 280]`<br/>• **IMU 4-Sensor Telemetry Bar**: `[16, 472, 358, 64]` (Accel, Gyro, Gravity, 48kHz Mic)<br/>• **Task List & MCAP Upload Queue**: `[16, 548, 358, 240]` | Mô phỏng trực tiếp khung xương bàn tay 21 điểm (`HandOverlayView`), sóng cảm biến IMU thời gian thực, trạng thái đóng gói `.mcap` và rút tiền qua Pix (`BRL`) / đổi ngày VIP. |
| 8 | **Chấm Công Geofence & Nghiệm Thu Checklist** | `TeamSpaceAttendance` & `ChecklistFragment` | `fragment_team_space_attendance.xml` | • **Geofence Map Circle**: `[16, 108, 358, 200]`<br/>• **Shift Clock-In/Out Card**: `[16, 320, 358, 140]`<br/>• **Inspection Checklist Items**: `[16, 476, 358, 280]` | Hiển thị vòng tròn bán kính hợp lệ trên bản đồ công trường, trạng thái trong/ngoài vùng chấm công và danh mục hạng mục cần chụp ảnh nghiệm thu. |
| 9 | **Bản Đồ Cụm Ảnh Công Trường (Photo Map)** | `TeamSpaceMapFragment` | `fragment_team_space_map.xml` | • **Full Interactive Map**: `[0, 44, 390, 560]`<br/>• **Photo Cluster Markers**: Tọa độ động theo GPS<br/>• **Bottom Photo Carousel**: `[0, 604, 390, 160]` | Gom cụm ảnh chụp theo vị trí địa lý trên bản đồ vệ tinh, chạm vào từng ghim để xem chuỗi ảnh theo trình tự thời gian. |
| 10 | **Đếm Thép & Nhận Diện Biển Số AI (Edge CV Mode)** | `SteelDetect` & `VLPR` (trong `PreviewActivity`) | `layout_watermark_cv_overlay.xml` | • **AI Bounding Boxes Overlay**: `[0, 92, 390, 560]`<br/>• **Auto Counter Badge**: `[16, 580, 180, 56]` | Đánh dấu từng đầu ống thép xây dựng bằng chấm tròn đánh số tự động (`CVModelId=4`) và điền tổng số lượng vào trực tiếp mẫu Watermark. |
| 11 | **Tìm & Tách Nền Logo Tự Động (AI Logo Cutout)** | `LogoSearchActivity` | `activity_logo_search.xml` | • **Search Input Bar**: `[16, 56, 358, 48]`<br/>• **Before/After Cutout Preview**: `[16, 120, 358, 220]`<br/>• **Brand Logo Grid**: `[16, 356, 358, 420]` | So sánh trực quan ảnh Logo gốc và ảnh sau khi tách nền bằng AI (`logo/cutout`), cho phép chỉnh độ trong suốt và vị trí đặt trên ảnh. |
| 12 | **Cài Đặt, Đồng Bộ Đám Mây & Chẩn Đoán** | `SettingActivity` | `activity_setting_new.xml` | • **Account & VIP Pass Header**: `[16, 56, 358, 96]`<br/>• **Cloud Integration (OneDrive/Drive)**: `[16, 168, 358, 110]`<br/>• **Camera & Anti-Cheat Preferences**: `[16, 294, 358, 460]` | Tích hợp thẻ đồng hồ đếm ngược `AdFreePass`, trạng thái kết nối Microsoft OneDrive (`MSAL`), tùy chọn lưu ảnh gốc kép và công cụ kiểm tra tốc độ mạng. |

---

## 3. Cấu Trúc Thư Mục Bàn Giao Thiết Kế Lại (`Redesign/`)

Toàn bộ gói thiết kế lại UI/UX và tài nguyên giao diện đã được xuất ra thư mục `E:\Decompiler\APP\IMAGE\Timemark\Redesign\`:

1. **[Redesign/design_tokens.json](file:///E:/Decompiler/APP/IMAGE/Timemark/Redesign/design_tokens.json)**:
   - Chứa toàn bộ hệ thống biến màu sắc (Dark/Light Mode, Brand, VIP, Anti-Tamper, Earn), hệ thống Typography (bao gồm cả font đồng hồ số `Segment7`, `BebasNeue`, `RobotoMono`), khoảng cách (`spacing`), độ bo góc (`borderRadius`), bóng đổ (`elevation`) và cấu hình tọa độ khung mẫu Watermark.
2. **[Redesign/preview_screens.html](file:///E:/Decompiler/APP/IMAGE/Timemark/Redesign/preview_screens.html)**:
   - Bản **Interactive UI/UX Prototype (HTML5 + CSS3 + JS)** chạy trực tiếp trên trình duyệt mà không cần cài đặt thêm công cụ.
   - Hỗ trợ chuyển đổi trực tiếp giữa **7 màn hình tương tác cốt lõi** (Camera Live Watermark, Kho Mẫu & Editor, Teamspace SaaS & Geofence, Thư viện & Xuất PDF/Excel/KMZ, Giám định ảnh chống giả VerifyAuth, Paywall Kép Plus/Business, và Module Kiếm tiền Robot Earn `.mcap`), hỗ trợ chuyển đổi **Dark Mode / Light Mode** và xem thông số kỹ thuật từng thành phần.
3. **[Redesign/layouts_xml/](file:///E:/Decompiler/APP/IMAGE/Timemark/Redesign/layouts_xml/)**:
   - Thư mục chứa các tệp XML giao diện cốt lõi được trích xuất nguyên bản từ `decompiled/resources/res/layout/` để đội ngũ lập trình Android đối chiếu và tái sử dụng trực tiếp trong Android Studio.
