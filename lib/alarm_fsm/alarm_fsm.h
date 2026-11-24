#ifndef ALARM_FSM_H
#define ALARM_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mã lý do cảnh báo / báo động */
typedef enum {
    REASON_NONE = 0,
    REASON_TEMP_HIGH,
    REASON_TEMP_RISE_FAST,
    REASON_CO2_CRITICAL,
    REASON_OVERCURRENT,
    REASON_CO2_WARNING,
    REASON_HUMIDITY_HIGH,
    REASON_OVER_CAPACITY,
    REASON_POWER_WASTE,
    REASON_SENSOR_FAULT
} alarm_reason_t;

typedef struct {
    alarm_fsm_state_t state;
    alarm_reason_t reason;
    bool alarm_latched;         /* Giữ ALARM cho tới khi có người ACK */

    /* Bộ đệm lịch sử 60 mẫu nhiệt độ (mỗi 1 s) để tính tốc độ tăng nhiệt/phút */
    float temp_history[60];
    uint8_t temp_hist_idx;
    uint8_t temp_hist_count;

    /* Bộ đếm thời gian duy trì ngưỡng */
    uint32_t co2_warn_ms;
    uint32_t co2_alarm_ms;
    uint32_t power_waste_ms;
    uint32_t normal_clear_ms;
} alarm_fsm_t;

void afsm_init(alarm_fsm_t *afsm);

/* Gọi mỗi 1 s để đánh giá toàn bộ điều kiện bất thường */
void afsm_step(alarm_fsm_t *afsm, const room_state_t *st, uint32_t dt_ms);

/* Người dùng nhấn nút RESET hoặc gửi lệnh ack_alarm từ Dashboard */
void afsm_ack_alarm(alarm_fsm_t *afsm);

/* Ghi đè đầu ra đèn/quạt khi đang có ALARM (tắt quạt, bật đèn 100%) */
void afsm_apply_safety_override(const alarm_fsm_t *afsm, bool *light_on, bool *fan_on);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_FSM_H */