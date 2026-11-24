#include "alarm_fsm.h"
#include "config.h"

void afsm_init(alarm_fsm_t *afsm) {
    afsm->state = ALARM_ST_NORMAL;
    afsm->reason = REASON_NONE;
    afsm->alarm_latched = false;
    afsm->temp_hist_idx = 0;
    afsm->temp_hist_count = 0;
    afsm->co2_warn_ms = 0;
    afsm->co2_alarm_ms = 0;
    afsm->power_waste_ms = 0;
    afsm->normal_clear_ms = 0;
}

static float update_and_get_temp_rise(alarm_fsm_t *afsm, float current_temp) {
    float rise = 0.0f;
    if (afsm->temp_hist_count == 60) {
        /* Mẫu cũ nhất cách đây đúng 60 giây nằm ngay tại vị trí temp_hist_idx */
        float oldest_temp = afsm->temp_history[afsm->temp_hist_idx];
        rise = current_temp - oldest_temp;
    }
    afsm->temp_history[afsm->temp_hist_idx] = current_temp;
    afsm->temp_hist_idx = (afsm->temp_hist_idx + 1) % 60;
    if (afsm->temp_hist_count < 60) {
        afsm->temp_hist_count++;
    }
    return rise;
}

void afsm_step(alarm_fsm_t *afsm, const room_state_t *st, uint32_t dt_ms) {
    float temp_rise = update_and_get_temp_rise(afsm, st->temperature_c);

    /* Cập nhật timer CO2 */
    if (st->co2_ppm > CO2_ALARM_PPM) {
        afsm->co2_alarm_ms += dt_ms;
        afsm->co2_warn_ms += dt_ms;
    } else if (st->co2_ppm >= CO2_WARN_PPM) {
        afsm->co2_alarm_ms = 0;
        afsm->co2_warn_ms += dt_ms;
    } else {
        afsm->co2_alarm_ms = 0;
        afsm->co2_warn_ms = 0;
    }

    /* Cập nhật timer lãng phí điện khi phòng EMPTY */
    if (st->room_state == ROOM_ST_EMPTY && st->power_w > IDLE_POWER_WASTE_W) {
        afsm->power_waste_ms += dt_ms;
    } else {
        afsm->power_waste_ms = 0;
    }

    /* 1. Kiểm tra điều kiện ALARM (Ưu tiên cao nhất) */
    bool cond_alarm = false;
    alarm_reason_t active_reason = REASON_NONE;

    if (st->temperature_c > TEMP_ALARM_ABS_TH) {
        cond_alarm = true;
        active_reason = REASON_TEMP_HIGH;
    } else if (temp_rise > TEMP_RISE_RATE_TH) {
        cond_alarm = true;
        active_reason = REASON_TEMP_RISE_FAST;
    } else if (afsm->co2_alarm_ms >= CO2_HOLD_TIME_MS) {
        cond_alarm = true;
        active_reason = REASON_CO2_CRITICAL;
    } else if (st->current_a > CURRENT_ALARM_TH) {
        cond_alarm = true;
        active_reason = REASON_OVERCURRENT;
    }

    if (cond_alarm) {
        afsm->alarm_latched = true;
        afsm->state = ALARM_ST_ALARM;
        afsm->reason = active_reason;
        afsm->normal_clear_ms = 0;
        return;
    }

    /* Giữ trạng thái ALARM nếu điều kiện hết nhưng chưa nhấn RESET */
    if (afsm->alarm_latched) {
        afsm->state = ALARM_ST_ALARM;
        return;
    }

    /* 2. Kiểm tra điều kiện FAULT (Ưu tiên thứ 2) */
    if (st->fault_flags != 0) {
        afsm->state = ALARM_ST_FAULT;
        afsm->reason = REASON_SENSOR_FAULT;
        afsm->normal_clear_ms = 0;
        return;
    }

    /* 3. Kiểm tra điều kiện WARNING (Ưu tiên thứ 3) */
    bool cond_warn = false;
    if (afsm->co2_warn_ms >= CO2_HOLD_TIME_MS) {
        cond_warn = true;
        active_reason = REASON_CO2_WARNING;
    } else if (st->humidity_rh > HUMIDITY_WARN_TH) {
        cond_warn = true;
        active_reason = REASON_HUMIDITY_HIGH;
    } else if (st->people_count > MAX_ROOM_CAPACITY) {
        cond_warn = true;
        active_reason = REASON_OVER_CAPACITY;
    } else if (afsm->power_waste_ms >= IDLE_POWER_HOLD_MS) {
        cond_warn = true;
        active_reason = REASON_POWER_WASTE;
    }

    if (cond_warn) {
        afsm->state = ALARM_ST_WARNING;
        afsm->reason = active_reason;
        afsm->normal_clear_ms = 0;
    } else if (afsm->state == ALARM_ST_WARNING) {
        /* Hết vi phạm liên tục 1 phút mới hạ về NORMAL */
        afsm->normal_clear_ms += dt_ms;
        if (afsm->normal_clear_ms >= 60000) {
            afsm->state = ALARM_ST_NORMAL;
            afsm->reason = REASON_NONE;
            afsm->normal_clear_ms = 0;
        }
    } else {
        afsm->state = ALARM_ST_NORMAL;
        afsm->reason = REASON_NONE;
    }
}

void afsm_ack_alarm(alarm_fsm_t *afsm) {
    afsm->alarm_latched = false;
}

void afsm_apply_safety_override(const alarm_fsm_t *afsm, bool *light_on, bool *fan_on) {
    if (afsm->state == ALARM_ST_ALARM) {
        *fan_on = false;    /* Tắt quạt để tránh thổi thêm oxy nếu có cháy */
        *light_on = true;   /* Bật đèn 100% hỗ trợ thoát hiểm */
    }
}