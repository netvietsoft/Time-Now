-- ============================================================================
-- TIMEMARK CAMERA (com.oceangalaxy.camera.new) v10.0.281 (VersionCode 100028100)
-- ROOM DATABASE: TodayCameraDB (Version 61)
-- Identity Hash: 42c2d7ad2a08151fd235f16b87d1a965
-- Complete DDL (42 Tables), Realistic Seed Data & Analytical SQL Queries
-- ============================================================================

PRAGMA foreign_keys = OFF;
BEGIN TRANSACTION;

-- ----------------------------------------------------------------------------
-- GROUP A: WATERMARK TEMPLATES, CLOUD STORE & EDIT HISTORY (8 TABLES)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `new_watermarks_table` (
    `id` TEXT NOT NULL,
    `name` TEXT,
    `base_id` TEXT,
    `category_id` TEXT,
    `type` INTEGER NOT NULL DEFAULT 0,
    `update_time` TEXT,
    `version` TEXT,
    `water_mark_content` TEXT,
    `is_pgc` INTEGER NOT NULL DEFAULT 0,
    `pgc_Order` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `cloud_water_mark_table` (
    `id` TEXT NOT NULL,
    `name` TEXT,
    `base_id` TEXT,
    `category_id` TEXT,
    `type` INTEGER NOT NULL DEFAULT 0,
    `update_time` TEXT,
    `version` TEXT,
    `cover_image_url` TEXT,
    `water_mark_content` TEXT,
    `vip_type` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `share_water_mark_table` (
    `id` TEXT NOT NULL,
    `name` TEXT,
    `base_id` TEXT,
    `category_id` TEXT,
    `type` INTEGER NOT NULL DEFAULT 0,
    `update_time` TEXT,
    `version` TEXT,
    `cover_image_url` TEXT,
    `water_mark_content` TEXT,
    `share_code` TEXT,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `cloud_category_table` (
    `id` TEXT NOT NULL,
    `name` TEXT,
    `icon_url` TEXT,
    `sort_order` INTEGER NOT NULL DEFAULT 0,
    `status` INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `use_recent_water_mark_table` (
    `id` TEXT NOT NULL,
    `base_id` TEXT,
    `update_time` INTEGER NOT NULL DEFAULT 0,
    `use_count` INTEGER NOT NULL DEFAULT 1,
    `from_type` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `recently_edited_table` (
    `id` TEXT NOT NULL,
    `base_id` TEXT,
    `watermark_id` TEXT,
    `item_id` INTEGER NOT NULL DEFAULT 0,
    `content` TEXT,
    `edit_time` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `watermark_template_edit_guide_table` (
    `id` TEXT NOT NULL,
    `shown_count` INTEGER NOT NULL DEFAULT 0,
    `last_shown_time` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `history_grouping_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `watermark_id` TEXT,
    `base_id` TEXT,
    `item_id` INTEGER NOT NULL DEFAULT 0,
    `group_name` TEXT,
    `item_content` TEXT,
    `update_time` INTEGER NOT NULL DEFAULT 0
);

-- ----------------------------------------------------------------------------
-- GROUP B: PHOTOS, ATTENDANCE, ANTI-FAKE CODES & CLOUD UPLOAD QUEUE (7 TABLES)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `stand_alone_photo_table` (
    `path` TEXT NOT NULL,
    `time` INTEGER NOT NULL,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `address` TEXT,
    `watermark_id` TEXT,
    `base_id` TEXT,
    `media_type` INTEGER NOT NULL DEFAULT 0,
    `photo_code` TEXT,
    `synced` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`path`)
);

CREATE TABLE IF NOT EXISTS `picture_up_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `local_file_path` TEXT,
    `oss_file_path` TEXT,
    `upload_status` INTEGER NOT NULL DEFAULT 0,
    `take_photo_time` INTEGER NOT NULL DEFAULT 0,
    `lat` REAL NOT NULL DEFAULT 0.0,
    `lng` REAL NOT NULL DEFAULT 0.0,
    `location_address` TEXT,
    `watermark_base_id` TEXT,
    `watermark_id` TEXT,
    `photo_info_json` TEXT,
    `retry_count` INTEGER NOT NULL DEFAULT 0,
    `media_type` INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS `album_number_table_new` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `take_date` TEXT,
    `photo_num` INTEGER NOT NULL DEFAULT 0,
    `video_num` INTEGER NOT NULL DEFAULT 0,
    `watermark_id` TEXT
);

CREATE TABLE IF NOT EXISTS `water_check_in_take_history_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `take_time` INTEGER NOT NULL,
    `date_str` TEXT,
    `month_str` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `address` TEXT,
    `photo_path` TEXT,
    `watermark_id` TEXT,
    `shift_type` INTEGER NOT NULL DEFAULT 1,
    `remark` TEXT
);

CREATE TABLE IF NOT EXISTS `photo_code_entity` (
    `photo_code` TEXT NOT NULL,
    `local_path` TEXT,
    `oss_url` TEXT,
    `capture_time` INTEGER NOT NULL,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `address` TEXT,
    `watermark_json` TEXT,
    `verify_status` INTEGER NOT NULL DEFAULT 1,
    `created_at` INTEGER NOT NULL,
    PRIMARY KEY(`photo_code`)
);

CREATE TABLE IF NOT EXISTS `anti_code_entry` (
    `code` TEXT NOT NULL,
    `status` INTEGER NOT NULL DEFAULT 0,
    `create_time` INTEGER NOT NULL,
    `use_time` INTEGER NOT NULL DEFAULT 0,
    `photo_path` TEXT,
    `extra_json` TEXT,
    PRIMARY KEY(`code`)
);
CREATE UNIQUE INDEX IF NOT EXISTS `index_anti_code_entry_code` ON `anti_code_entry` (`code`);

CREATE TABLE IF NOT EXISTS `oss_data_table` (
    `id` TEXT NOT NULL,
    `access_key_id` TEXT,
    `access_key_secret` TEXT,
    `security_token` TEXT,
    `expiration` INTEGER NOT NULL DEFAULT 0,
    `bucket_name` TEXT,
    `endpoint` TEXT,
    `region` TEXT,
    PRIMARY KEY(`id`)
);

-- ----------------------------------------------------------------------------
-- GROUP C: GPS LOCATION, POI CACHE & OFFLINE REVERSE GEOCODING (5 TABLES)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `place_list_entity` (
    `place_id` TEXT NOT NULL,
    `name` TEXT,
    `address` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `distance` REAL NOT NULL DEFAULT 0.0,
    `type_code` TEXT,
    `city` TEXT,
    `district` TEXT,
    `update_time` INTEGER NOT NULL,
    PRIMARY KEY(`place_id`)
);

CREATE TABLE IF NOT EXISTS `search_place_entity` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `keyword` TEXT,
    `place_id` TEXT,
    `name` TEXT,
    `address` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `search_time` INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS `prefer_place_entity` (
    `place_id` TEXT NOT NULL,
    `name` TEXT,
    `address` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `use_count` INTEGER NOT NULL DEFAULT 1,
    `last_use_time` INTEGER NOT NULL,
    `custom_alias` TEXT,
    PRIMARY KEY(`place_id`)
);

CREATE TABLE IF NOT EXISTS `custom_poi_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `poi_name` TEXT,
    `address` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `create_time` INTEGER NOT NULL,
    `group_id` TEXT
);

CREATE TABLE IF NOT EXISTS `lat_long_poi_entity` (
    `grid_key` TEXT NOT NULL,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `poi_list_json` TEXT,
    `weather_json` TEXT,
    `cache_time` INTEGER NOT NULL,
    PRIMARY KEY(`grid_key`)
);

-- ----------------------------------------------------------------------------
-- GROUP D: TEAMSPACE SaaS, PROJECTS, CHECKLISTS & PATROL TASKS (8 TABLES)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `team_space_entry` (
    `team_id` TEXT NOT NULL,
    `team_name` TEXT,
    `role` INTEGER NOT NULL DEFAULT 1,
    `member_count` INTEGER NOT NULL DEFAULT 1,
    `photo_count` INTEGER NOT NULL DEFAULT 0,
    `plan_type` INTEGER NOT NULL DEFAULT 0,
    `storage_used_bytes` INTEGER NOT NULL DEFAULT 0,
    `storage_limit_bytes` INTEGER NOT NULL DEFAULT 5368709120,
    `avatar_url` TEXT,
    `settings_json` TEXT,
    `updated_at` INTEGER NOT NULL,
    PRIMARY KEY(`team_id`)
);

CREATE TABLE IF NOT EXISTS `team_watermark_template` (
    `template_id` TEXT NOT NULL,
    `team_id` TEXT NOT NULL,
    `project_id` TEXT,
    `name` TEXT,
    `base_id` TEXT,
    `content_json` TEXT,
    `force_apply` INTEGER NOT NULL DEFAULT 0,
    `lock_time_location` INTEGER NOT NULL DEFAULT 1,
    `sort_order` INTEGER NOT NULL DEFAULT 0,
    `updated_at` INTEGER NOT NULL,
    PRIMARY KEY(`template_id`)
);

CREATE TABLE IF NOT EXISTS `upload_progress_task_entry` (
    `task_id` TEXT NOT NULL,
    `team_id` TEXT NOT NULL,
    `project_id` TEXT,
    `local_path` TEXT NOT NULL,
    `remote_url` TEXT,
    `media_type` INTEGER NOT NULL DEFAULT 0,
    `file_size` INTEGER NOT NULL DEFAULT 0,
    `uploaded_bytes` INTEGER NOT NULL DEFAULT 0,
    `status` INTEGER NOT NULL DEFAULT 0,
    `error_msg` TEXT,
    `meta_json` TEXT,
    `created_at` INTEGER NOT NULL,
    PRIMARY KEY(`task_id`)
);
CREATE INDEX IF NOT EXISTS `index_upload_progress_task_entry_team_id` ON `upload_progress_task_entry` (`team_id`);

CREATE TABLE IF NOT EXISTS `doc_upload_task_entry` (
    `doc_id` TEXT NOT NULL,
    `team_id` TEXT NOT NULL,
    `project_id` TEXT NOT NULL,
    `file_name` TEXT,
    `local_path` TEXT,
    `remote_url` TEXT,
    `mime_type` TEXT,
    `file_size` INTEGER NOT NULL DEFAULT 0,
    `status` INTEGER NOT NULL DEFAULT 0,
    `created_at` INTEGER NOT NULL,
    PRIMARY KEY(`doc_id`)
);

CREATE TABLE IF NOT EXISTS `checklist_template` (
    `checklist_id` TEXT NOT NULL,
    `team_id` TEXT NOT NULL,
    `project_id` TEXT,
    `title` TEXT,
    `description` TEXT,
    `items_json` TEXT,
    `require_photo` INTEGER NOT NULL DEFAULT 1,
    `status` INTEGER NOT NULL DEFAULT 1,
    `updated_at` INTEGER NOT NULL,
    PRIMARY KEY(`checklist_id`)
);

CREATE TABLE IF NOT EXISTS `checklist_submission` (
    `submission_id` TEXT NOT NULL,
    `checklist_id` TEXT NOT NULL,
    `team_id` TEXT NOT NULL,
    `project_id` TEXT,
    `user_id` TEXT,
    `answers_json` TEXT,
    `photo_urls_json` TEXT,
    `status` INTEGER NOT NULL DEFAULT 1,
    `submitted_at` INTEGER NOT NULL,
    PRIMARY KEY(`submission_id`)
);

CREATE TABLE IF NOT EXISTS `patrol_table` (
    `patrol_id` TEXT NOT NULL,
    `route_name` TEXT,
    `group_id` TEXT,
    `total_points` INTEGER NOT NULL DEFAULT 0,
    `completed_points` INTEGER NOT NULL DEFAULT 0,
    `start_time` INTEGER NOT NULL,
    `end_time` INTEGER NOT NULL DEFAULT 0,
    `status` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`patrol_id`)
);

CREATE TABLE IF NOT EXISTS `patrol_task_table` (
    `task_id` TEXT NOT NULL,
    `patrol_id` TEXT NOT NULL,
    `point_name` TEXT,
    `qr_code` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `radius_meters` INTEGER NOT NULL DEFAULT 50,
    `check_time` INTEGER NOT NULL DEFAULT 0,
    `photo_path` TEXT,
    `status` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`task_id`)
);

-- ----------------------------------------------------------------------------
-- GROUP E: WORKGROUP & PDF/EXCEL/KMZ REPORT TEMPLATES (10 TABLES)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `work_group_table` (
    `group_id` TEXT NOT NULL,
    `group_name` TEXT,
    `owner_id` TEXT,
    `role` INTEGER NOT NULL DEFAULT 1,
    `member_num` INTEGER NOT NULL DEFAULT 1,
    `avatar` TEXT,
    `invite_code` TEXT,
    `create_time` INTEGER NOT NULL,
    PRIMARY KEY(`group_id`)
);

CREATE TABLE IF NOT EXISTS `work_group_picture_table` (
    `pic_id` TEXT NOT NULL,
    `group_id` TEXT NOT NULL,
    `user_id` TEXT,
    `user_name` TEXT,
    `local_path` TEXT,
    `oss_url` TEXT,
    `thumb_url` TEXT,
    `take_time` INTEGER NOT NULL,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `address` TEXT,
    `watermark_id` TEXT,
    `sync_status` INTEGER NOT NULL DEFAULT 1,
    PRIMARY KEY(`pic_id`)
);

CREATE TABLE IF NOT EXISTS `work_group_water_mark_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `group_id` TEXT NOT NULL,
    `watermark_id` TEXT NOT NULL,
    `base_id` TEXT,
    `name` TEXT,
    `content_json` TEXT,
    `update_time` INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS `work_group_report_template_type_table` (
    `type_id` TEXT NOT NULL,
    `type_name` TEXT,
    `icon_url` TEXT,
    `sort_order` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`type_id`)
);

CREATE TABLE IF NOT EXISTS `work_group_report_one_template_table` (
    `template_id` TEXT NOT NULL,
    `type_id` TEXT,
    `title` TEXT,
    `preview_url` TEXT,
    `format_type` TEXT,
    `config_json` TEXT,
    `vip_required` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`template_id`)
);

CREATE TABLE IF NOT EXISTS `work_group_report_history_table` (
    `report_id` TEXT NOT NULL,
    `template_id` TEXT,
    `title` TEXT,
    `local_file_path` TEXT,
    `remote_share_url` TEXT,
    `photo_count` INTEGER NOT NULL DEFAULT 0,
    `format_type` TEXT,
    `create_time` INTEGER NOT NULL,
    PRIMARY KEY(`report_id`)
);

CREATE TABLE IF NOT EXISTS `work_group_report_template_id_map_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `watermark_base_id` TEXT,
    `report_template_id` TEXT,
    `priority` INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS `work_report_recommend_table` (
    `id` TEXT NOT NULL,
    `scene_tag` TEXT,
    `template_id` TEXT,
    `title` TEXT,
    `score` REAL NOT NULL DEFAULT 1.0,
    `update_time` INTEGER NOT NULL,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `project_item_table` (
    `project_id` TEXT NOT NULL,
    `team_id` TEXT,
    `project_name` TEXT,
    `address` TEXT,
    `lat` REAL NOT NULL,
    `lng` REAL NOT NULL,
    `radius` INTEGER NOT NULL DEFAULT 200,
    `photo_count` INTEGER NOT NULL DEFAULT 0,
    `status` INTEGER NOT NULL DEFAULT 1,
    `updated_at` INTEGER NOT NULL,
    PRIMARY KEY(`project_id`)
);

CREATE TABLE IF NOT EXISTS `input_tab_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `watermark_id` TEXT,
    `item_id` INTEGER NOT NULL,
    `tab_name` TEXT,
    `input_history_json` TEXT,
    `update_time` INTEGER NOT NULL
);

-- ----------------------------------------------------------------------------
-- GROUP F: NOTIFICATIONS, CAMPAIGNS & PERSONAL UTILITIES (4 TABLES)
-- ----------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS `notification_entity` (
    `msg_id` TEXT NOT NULL,
    `title` TEXT,
    `body` TEXT,
    `type` INTEGER NOT NULL DEFAULT 1,
    `deep_link` TEXT,
    `team_id` TEXT,
    `is_read` INTEGER NOT NULL DEFAULT 0,
    `received_at` INTEGER NOT NULL,
    PRIMARY KEY(`msg_id`)
);

CREATE TABLE IF NOT EXISTS `home_notice_table` (
    `notice_id` TEXT NOT NULL,
    `title` TEXT,
    `content` TEXT,
    `action_url` TEXT,
    `priority` INTEGER NOT NULL DEFAULT 0,
    `is_dismissed` INTEGER NOT NULL DEFAULT 0,
    `expire_time` INTEGER NOT NULL,
    PRIMARY KEY(`notice_id`)
);

CREATE TABLE IF NOT EXISTS `home_notice_new_table` (
    `id` TEXT NOT NULL,
    `scene` TEXT,
    `payload_json` TEXT,
    `show_count` INTEGER NOT NULL DEFAULT 0,
    `max_show_count` INTEGER NOT NULL DEFAULT 3,
    `last_show_time` INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY(`id`)
);

CREATE TABLE IF NOT EXISTS `baby_info_table` (
    `id` INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    `baby_name` TEXT,
    `birthday_timestamp` INTEGER NOT NULL,
    `gender` INTEGER NOT NULL DEFAULT 0,
    `avatar_path` TEXT,
    `watermark_id` TEXT
);

-- ============================================================================
-- REALISTIC SEED DATA (INSERT STATEMENTS)
-- ============================================================================

INSERT INTO `cloud_category_table` (`id`, `name`, `icon_url`, `sort_order`, `status`) VALUES
('cat_time_gps', 'Time & GPS Official', 'https://static.xhey.top/icons/cat_gps.png', 1, 1),
('cat_construction', 'Construction & Engineering', 'https://static.xhey.top/icons/cat_build.png', 2, 1),
('cat_attendance', 'Attendance & Shift Clock', 'https://static.xhey.top/icons/cat_clock.png', 3, 1),
('cat_patrol_security', 'Security Patrol & Property', 'https://static.xhey.top/icons/cat_patrol.png', 4, 1),
('cat_logistics', 'Delivery & Field Service', 'https://static.xhey.top/icons/cat_delivery.png', 5, 1);

INSERT INTO `new_watermarks_table` (`id`, `name`, `base_id`, `category_id`, `type`, `update_time`, `version`, `water_mark_content`, `is_pgc`, `pgc_Order`) VALUES
('wm_10011', 'Real-Time Official Anti-Tamper', '10011', 'cat_time_gps', 1, '1761108000000', '3.2', '{"base_id":"10011","theme":"#2563EB","antiFake":true,"items":["time","date","latlng","address","weather","compass","antiCode"]}', 1, 1),
('wm_20', 'Engineering Construction Log', '20', 'cat_construction', 1, '1761108000000', '2.8', '{"base_id":"20","theme":"#F59E0B","antiFake":true,"items":["project_name","construction_area","content","inspector","time","address"]}', 1, 2),
('wm_57', 'AI Steel Rebar Counter (CVModel=4)', '57', 'cat_construction', 2, '1761108000000', '4.0', '{"base_id":"57","cvModelId":4,"theme":"#EF4444","items":["steel_count","batch_no","operator","time","latlng"]}', 1, 3),
('wm_34', 'Geofence Shift Attendance', '34', 'cat_attendance', 1, '1761108000000', '3.0', '{"base_id":"34","theme":"#10B981","lockTime":true,"items":["shift_time","employee_name","geofence_name","company_logo","antiCode"]}', 1, 4),
('wm_52', 'AI License Plate Recognition (CVModel=1)', '52', 'cat_patrol_security', 2, '1761108000000', '2.1', '{"base_id":"52","cvModelId":1,"theme":"#3B82F6","items":["plate_number","vehicle_type","gate_name","time"]}', 1, 5);

INSERT INTO `team_space_entry` (`team_id`, `team_name`, `role`, `member_count`, `photo_count`, `plan_type`, `storage_used_bytes`, `storage_limit_bytes`, `avatar_url`, `settings_json`, `updated_at`) VALUES
('team_vn_001', 'Coteccons Metro Line 1 Engineering', 1, 42, 18450, 2, 68934205440, 107374182400, 'https://static.xhey.top/teams/metro1.png', '{"forceLiveCapture":true,"blockMockGps":true,"onedriveSync":true}', 1761109000000),
('team_vn_002', 'Vinhomes Grand Park Facility Patrol', 2, 28, 9320, 2, 31138512896, 107374182400, 'https://static.xhey.top/teams/vhgp.png', '{"forceLiveCapture":true,"requireQrPatrol":true}', 1761109200000),
('team_br_003', 'Sao Paulo Solar Grid Installation', 1, 15, 4120, 1, 8589934592, 53687091200, 'https://static.xhey.top/teams/solar_sp.png', '{"forceLiveCapture":true,"autoExportPdf":true}', 1761109500000);

INSERT INTO `project_item_table` (`project_id`, `team_id`, `project_name`, `address`, `lat`, `lng`, `radius`, `photo_count`, `status`, `updated_at`) VALUES
('proj_01', 'team_vn_001', 'Ga Ngầm Ba Son - Tầng B2', '2 Tôn Đức Thắng, Bến Nghé, Quận 1, TP.HCM', 10.7815, 106.7072, 250, 6420, 1, 1761109100000),
('proj_02', 'team_vn_001', 'Depot Long Bình - Khu Bảo Dưỡng', 'Long Bình, TP. Thủ Đức, TP.HCM', 10.8754, 106.8291, 500, 5180, 1, 1761109150000),
('proj_03', 'team_vn_002', 'Phân Khu The Origami - Tuần Tra Đêm', 'Nguyễn Xiển, Long Thạnh Mỹ, TP. Thủ Đức', 10.8421, 106.8365, 300, 4890, 1, 1761109300000);

INSERT INTO `stand_alone_photo_table` (`path`, `time`, `lat`, `lng`, `address`, `watermark_id`, `base_id`, `media_type`, `photo_code`, `synced`) VALUES
('/storage/emulated/0/DCIM/Timemark/TM_20261022_081512.jpg', 1761105312000, 10.78152, 106.70724, '2 Tôn Đức Thắng, Quận 1, TP.HCM', 'wm_10011', '10011', 0, 'TM9X-84K2-V71Q', 1),
('/storage/emulated/0/DCIM/Timemark/TM_20261022_084230.jpg', 1761106950000, 10.78158, 106.70731, '2 Tôn Đức Thắng, Quận 1, TP.HCM', 'wm_57', '57', 0, 'TM9X-92M4-P18Z', 1),
('/storage/emulated/0/DCIM/Timemark/TM_20261022_091005.jpg', 1761108605000, 10.87541, 106.82915, 'Depot Long Bình, TP. Thủ Đức', 'wm_20', '20', 0, 'TM9X-31L9-K55B', 1),
('/storage/emulated/0/DCIM/Timemark/TM_20261022_093540.mp4', 1761110140000, 10.84214, 106.83652, 'The Origami, Vinhomes Grand Park', 'wm_34', '34', 1, 'TM9X-77C3-R04N', 0);

INSERT INTO `photo_code_entity` (`photo_code`, `local_path`, `oss_url`, `capture_time`, `lat`, `lng`, `address`, `watermark_json`, `verify_status`, `created_at`) VALUES
('TM9X-84K2-V71Q', '/storage/emulated/0/DCIM/Timemark/TM_20261022_081512.jpg', 'https://timemark-oversea.s3.us-west-1.amazonaws.com/photos/TM9X-84K2-V71Q.jpg', 1761105312000, 10.78152, 106.70724, '2 Tôn Đức Thắng, Quận 1, TP.HCM', '{"ntpDeltaMs":42,"isMockGps":false,"aegisNonce":"9f8e7d6c5b4a","jpegPacked":true}', 1, 1761105313000),
('TM9X-92M4-P18Z', '/storage/emulated/0/DCIM/Timemark/TM_20261022_084230.jpg', 'https://timemark-oversea.s3.us-west-1.amazonaws.com/photos/TM9X-92M4-P18Z.jpg', 1761106950000, 10.78158, 106.70731, '2 Tôn Đức Thắng, Quận 1, TP.HCM', '{"ntpDeltaMs":35,"isMockGps":false,"steelCount":128,"jpegPacked":true}', 1, 1761106951000),
('TM9X-31L9-K55B', '/storage/emulated/0/DCIM/Timemark/TM_20261022_091005.jpg', 'https://timemark-oversea.s3.us-west-1.amazonaws.com/photos/TM9X-31L9-K55B.jpg', 1761108605000, 10.87541, 106.82915, 'Depot Long Bình, TP. Thủ Đức', '{"ntpDeltaMs":51,"isMockGps":false,"inspector":"KS. Lê Hoàng Nam","jpegPacked":true}', 1, 1761108606000);

INSERT INTO `water_check_in_take_history_table` (`take_time`, `date_str`, `month_str`, `lat`, `lng`, `address`, `photo_path`, `watermark_id`, `shift_type`, `remark`) VALUES
(1761102600000, '2026-10-22', '2026-10', 10.78152, 106.70724, 'Cổng 2 Công trường Ga Ba Son', '/storage/emulated/0/DCIM/Timemark/ATT_0730.jpg', 'wm_34', 1, 'Vào ca sáng đúng giờ - Trong vùng Geofence (28m)'),
(1761052200000, '2026-10-21', '2026-10', 10.78150, 106.70720, 'Cổng 2 Công trường Ga Ba Son', '/storage/emulated/0/DCIM/Timemark/ATT_1730.jpg', 'wm_34', 2, 'Tan ca chiều - Hoàn thành đổ bê tông sàn B2');

INSERT INTO `work_group_report_one_template_table` (`template_id`, `type_id`, `title`, `preview_url`, `format_type`, `config_json`, `vip_required`) VALUES
('tpl_pdf_4grid', 'type_construction', 'Báo Cáo Nghiệm Thu Công Trường (4 Ảnh / Trang PDF)', 'https://static.xhey.top/report/pdf_4grid.png', 'PDF', '{"photosPerPage":4,"showCover":true,"showAntiFakeQr":true}', 0),
('tpl_xlsx_poi', 'type_attendance', 'Bảng Kê Chi Tiết Ảnh & Tọa Độ GPS (Excel .xlsx)', 'https://static.xhey.top/report/xlsx_full.png', 'EXCEL', '{"engine":"ApachePOI","embedThumbnail":true,"columns":["No","Photo","Time","LatLng","Address","PhotoCode","Remark"]}', 1),
('tpl_kmz_earth', 'type_survey', 'Bản Đồ Khảo Sát Địa Hình Google Earth (.KMZ)', 'https://static.xhey.top/report/kmz_earth.png', 'KMZ', '{"includePlacemarks":true,"bundleOriginalJpeg":false}', 1);

COMMIT;

-- ============================================================================
-- ANALYTICAL SQL QUERIES (REQUIREMENT 21)
-- ============================================================================

-- Query 1: Thống kê dung lượng lưu trữ đám mây S3 và trung bình số ảnh trên mỗi thành viên theo từng Teamspace
-- SELECT
--     team_id,
--     team_name,
--     member_count,
--     photo_count,
--     ROUND(CAST(photo_count AS REAL) / member_count, 1) AS avg_photos_per_member,
--     ROUND(CAST(storage_used_bytes AS REAL) / (1024*1024*1024), 2) AS used_gb,
--     ROUND(CAST(storage_limit_bytes AS REAL) / (1024*1024*1024), 2) AS limit_gb,
--     ROUND(CAST(storage_used_bytes AS REAL) * 100.0 / storage_limit_bytes, 1) AS usage_percent
-- FROM team_space_entry
-- ORDER BY usage_percent DESC;

-- Query 2: Đối chiếu xác thực ảnh chụp nội bộ với bảng chứng thực chống giả mạo (Anti-Fake Photo Code Audit)
-- SELECT
--     p.photo_code,
--     w.name AS watermark_name,
--     p.address,
--     p.lat,
--     p.lng,
--     c.oss_url,
--     c.watermark_json AS cryptographic_audit_meta
-- FROM stand_alone_photo_table p
-- INNER JOIN photo_code_entity c ON p.photo_code = c.photo_code
-- LEFT JOIN new_watermarks_table w ON p.watermark_id = w.id
-- WHERE c.verify_status = 1;
