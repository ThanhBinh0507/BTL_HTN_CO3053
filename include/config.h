#ifndef CONFIG_H
#define CONFIG_H

/* ================= 0. CHẾ ĐỘ MÔ PHỎNG HAY MẠCH THẬT ================= */
/* 1 = Wokwi (dùng DHT22, biến trở CO2 & biến trở dòng điện)            */
/* 0 = Mạch thật (dùng DHT20 I2C 0x38 & INA219 I2C 0x40)               */
#define SIMULATE_ON_WOKWI       1

/* ================= 1. ĐỊNH DANH PHÒNG & LOẠI NODE ================= */
#define ROOM_TYPE_CLASSROOM     1
#define ROOM_TYPE_LAB           2
#define ROOM_TYPE_CORRIDOR      3

#define CURRENT_ROOM_TYPE       ROOM_TYPE_CLASSROOM
#define BUILDING_ID             "A1"
#define FLOOR_ID                "2"
#define ROOM_ID                 "A1-201"

/* ================= 2. MẠNG WI-FI & MQTT BROKER ================= */
#define WIFI_SSID               "Wokwi-GUEST"
#define WIFI_PASSWORD           ""

#define MQTT_BROKER_HOST        "broker.hivemq.com"
#define MQTT_BROKER_PORT        1883

#define TOPIC_TELEMETRY         "school/" BUILDING_ID "/" FLOOR_ID "/" ROOM_ID "/telemetry"
#define TOPIC_EVENT             "school/" BUILDING_ID "/" FLOOR_ID "/" ROOM_ID "/event"
#define TOPIC_CMD               "school/" BUILDING_ID "/" FLOOR_ID "/" ROOM_ID "/cmd"
#define TOPIC_STATUS            "school/" BUILDING_ID "/" FLOOR_ID "/" ROOM_ID "/status"

/* ================= 3. GÁN CHÂN GPIO ESP32 ================= */
/* Cảm biến đếm người (2 vùng cửa) & Hiện diện */
#define PIN_US_A_TRIG           14
#define PIN_US_A_ECHO           27
#define PIN_US_B_TRIG           26
#define PIN_US_B_ECHO           25
#define PIN_PIR_OUT             33

/* Cảm biến môi trường & Điện năng */
#define PIN_DHT_DATA            4       /* Dùng khi SIMULATE_ON_WOKWI == 1 */
#define PIN_LDR_ADC             34
#define PIN_CO2_ADC             35      /* Dùng khi SIMULATE_ON_WOKWI == 1 */
#define PIN_CURRENT_ADC         36      /* Dùng khi SIMULATE_ON_WOKWI == 1 */
#define PIN_I2C_SDA             21      /* Dùng cho DHT20 & INA219 khi SIMULATE_ON_WOKWI == 0 */
#define PIN_I2C_SCL             22

/* Thiết bị chấp hành & Báo hiệu */
#define PIN_ACT_LIGHT           18
#define PIN_ACT_FAN             19
#define PIN_LED_GREEN           16
#define PIN_LED_RED             17
#define PIN_BUZZER              23

/* Nút nhấn điều khiển tại chỗ */
#define PIN_BTN_MANUAL          32
#define PIN_BTN_RESET_ALARM     13

/* ================= 4. CHU KỲ & ĐỊNH THỜI RTOS (ms) ================= */
#define PERIOD_COUNTING_MS      25      /* Lấy mẫu cửa mỗi 25 ms */
#define PERIOD_FAST_SENSOR_MS   1000    /* Đọc Lux & Điện năng mỗi 1 s */
#define PERIOD_SLOW_SENSOR_MS   5000    /* Đọc T, RH, CO2 mỗi 5 s */
#define PERIOD_TELEMETRY_MS     5000    /* Gửi MQTT telemetry mỗi 5 s */
#define PERIOD_HEALTH_MS        1000    /* Kiểm tra Watchdog mỗi 1 s */

#define COUNT_STUCK_TIMEOUT_MS  3000    /* Kẹt giữa cửa > 3 s -> về IDLE */
#define GRACE_PERIOD_MS         300000  /* Trễ tắt thiết bị 5 phút (300.000 ms) */
#define DRIFT_EMPTY_RESET_MS    300000  /* PIR trống 5 phút -> reset count = 0 */
#define DRIFT_OCCUPIED_WARN_MS  60000   /* PIR có người 60 s mà count=0 -> cảnh báo */
#define DEBOUNCE_MS             50      /* Chống dội phím <= 50 ms */

/* ================= 5. NGƯỠNG VẬT LÝ & CẢNH BÁO ================= */
#define DOOR_OCCUPIED_DIST_MM   1300    /* < 1300 mm coi như có người chắn */
#define DOOR_CLEAR_DIST_MM      1400    /* > 1400 mm coi như trống */

#define LUX_TURN_ON_TH          300.0f  /* Bật đèn khi < 300 lux */
#define LUX_TURN_OFF_TH         400.0f  /* Tắt đèn khi > 400 lux */

#if CURRENT_ROOM_TYPE == ROOM_TYPE_LAB
  #define TEMP_FAN_ON_TH        26.0f
  #define TEMP_FAN_OFF_TH       25.0f
  #define MAX_ROOM_CAPACITY     30
#else
  #define TEMP_FAN_ON_TH        28.0f   /* Phòng học chuẩn: bật >= 28 C */
  #define TEMP_FAN_OFF_TH       27.0f   /* Tắt <= 27 C */
  #define MAX_ROOM_CAPACITY     40
#endif

#define CO2_WARN_PPM            1000
#define CO2_ALARM_PPM           1500
#define CO2_HOLD_TIME_MS        300000

#define HUMIDITY_WARN_TH        80.0f
#define TEMP_ALARM_ABS_TH       50.0f
#define TEMP_RISE_RATE_TH       5.0f
#define CURRENT_ALARM_TH        2.5f
#define IDLE_POWER_WASTE_W      5.0f
#define IDLE_POWER_HOLD_MS      600000

#define SENSOR_MAX_ERR_COUNT    3
#define OFFLINE_BUFFER_SIZE     120

#endif /* CONFIG_H */