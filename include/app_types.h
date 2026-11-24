#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Trạng thái FSM điều khiển phòng (Moore FSM) */
typedef enum {
    ROOM_ST_EMPTY = 0,
    ROOM_ST_OCCUPIED,
    ROOM_ST_GRACE
} room_fsm_state_t;

/* Trạng thái FSM cảnh báo (Ưu tiên: ALARM > FAULT > WARNING > NORMAL) */
typedef enum {
    ALARM_ST_NORMAL = 0,
    ALARM_ST_WARNING,
    ALARM_ST_FAULT,
    ALARM_ST_ALARM
} alarm_fsm_state_t;

/* Chế độ vận hành thiết bị */
typedef enum {
    CTRL_MODE_AUTO = 0,
    CTRL_MODE_MANUAL
} ctrl_mode_t;

/* Các loại sự kiện gửi vào control_task qua Queue */
typedef enum {
    EVT_NONE = 0,
    EVT_PERSON_ENTERED,
    EVT_PERSON_EXITED,
    EVT_COUNT_RESET,
    EVT_COUNT_MISMATCH,
    EVT_PRESENCE_CHANGED,
    EVT_SENSOR_UPDATED,
    EVT_BTN_MANUAL_PRESSED,
    EVT_BTN_RESET_ALARM,
    EVT_MQTT_CMD_RECEIVED,
    EVT_TICK_1S
} app_event_type_t;

/* Cấu trúc bản tin sự kiện truyền trong Queue */
typedef struct {
    app_event_type_t type;
    int32_t param1;     /* Ví dụ: trạng thái đèn/quạt từ lệnh MQTT */
    int32_t param2;
} app_event_t;

/* Cấu trúc lưu trữ toàn bộ trạng thái phòng (chia sẻ qua Mutex) */
typedef struct {
    /* Số người & hiện diện */
    int16_t people_count;
    bool presence_detected;

    /* Thông số môi trường */
    float temperature_c;
    float humidity_rh;
    uint16_t co2_ppm;
    float illuminance_lux;

    /* Thông số điện năng */
    float voltage_v;
    float current_a;
    float power_w;
    float energy_wh;

    /* Trạng thái đầu ra chấp hành */
    bool light_on;
    bool fan_on;
    ctrl_mode_t mode;

    /* Trạng thái các FSM */
    room_fsm_state_t room_state;
    alarm_fsm_state_t alarm_state;
    uint8_t fault_flags;    /* Bitmask báo cảm biến nào đang lỗi */
    uint8_t alarm_reason;   /* Mã nguyên nhân cảnh báo/báo động */
} room_state_t;

/* Bitmask lỗi cảm biến */
#define FAULT_BIT_DOOR_SENSOR   (1 << 0)
#define FAULT_BIT_DHT           (1 << 1)
#define FAULT_BIT_CO2           (1 << 2)
#define FAULT_BIT_LUX           (1 << 3)
#define FAULT_BIT_POWER         (1 << 4)

#endif /* APP_TYPES_H */