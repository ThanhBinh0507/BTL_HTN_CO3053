#include "room_fsm.h"
#include "config.h"

void rfsm_init(room_fsm_t *fsm) {
    fsm->state = ROOM_ST_EMPTY;
    fsm->mode = CTRL_MODE_AUTO;
    fsm->light_on = false;
    fsm->fan_on = false;
    fsm->grace_elapsed_ms = 0;
}

void rfsm_step(room_fsm_t *fsm, int16_t count, bool presence, float lux, float temp_c, uint32_t dt_ms) {
    bool has_people = (count > 0) || presence;

    /* 1. Chuyển trạng thái Moore FSM */
    switch (fsm->state) {
        case ROOM_ST_EMPTY:
            if (has_people) {
                fsm->state = ROOM_ST_OCCUPIED;
                fsm->grace_elapsed_ms = 0;
            }
            break;

        case ROOM_ST_OCCUPIED:
            if (!has_people) {
                fsm->state = ROOM_ST_GRACE;
                fsm->grace_elapsed_ms = 0;
            }
            break;

        case ROOM_ST_GRACE:
            if (has_people) {
                fsm->state = ROOM_ST_OCCUPIED;
                fsm->grace_elapsed_ms = 0;
            } else {
                fsm->grace_elapsed_ms += dt_ms;
                if (fsm->grace_elapsed_ms >= GRACE_PERIOD_MS) {
                    fsm->state = ROOM_ST_EMPTY;
                    fsm->grace_elapsed_ms = 0;
                    /* Tự hủy chế độ tay khi phòng trống hoàn toàn để chống lãng phí */
                    fsm->mode = CTRL_MODE_AUTO;
                }
            }
            break;
    }

    /* 2. Tính đầu ra chấp hành theo trạng thái hiện tại */
    if (fsm->state == ROOM_ST_EMPTY) {
        fsm->light_on = false;
        fsm->fan_on = false;
    } else if (fsm->mode == CTRL_MODE_AUTO) {
        /* Vùng trễ ánh sáng 300 - 400 lux */
        if (!fsm->light_on && lux < LUX_TURN_ON_TH) {
            fsm->light_on = true;
        } else if (fsm->light_on && lux > LUX_TURN_OFF_TH) {
            fsm->light_on = false;
        }

        /* Vùng trễ nhiệt độ 27 - 28 C (riêng hành lang không có quạt) */
#if CURRENT_ROOM_TYPE == ROOM_TYPE_CORRIDOR
        fsm->fan_on = false;
#else
        if (!fsm->fan_on && temp_c >= TEMP_FAN_ON_TH) {
            fsm->fan_on = true;
        } else if (fsm->fan_on && temp_c <= TEMP_FAN_OFF_TH) {
            fsm->fan_on = false;
        }
#endif
    }
}

void rfsm_toggle_manual_button(room_fsm_t *fsm) {
    if (fsm->mode == CTRL_MODE_AUTO) {
        fsm->mode = CTRL_MODE_MANUAL;
        fsm->light_on = !fsm->light_on;
        fsm->fan_on = !fsm->fan_on;
    } else {
        fsm->mode = CTRL_MODE_AUTO;
    }
}

void rfsm_apply_remote_cmd(room_fsm_t *fsm, int light_cmd, int fan_cmd) {
    if (light_cmd == 2 || fan_cmd == 2) {
        fsm->mode = CTRL_MODE_AUTO;
        return;
    }
    if (light_cmd == 0 || light_cmd == 1) {
        fsm->mode = CTRL_MODE_MANUAL;
        fsm->light_on = (light_cmd == 1);
    }
    if (fan_cmd == 0 || fan_cmd == 1) {
        fsm->mode = CTRL_MODE_MANUAL;
        fsm->fan_on = (fan_cmd == 1);
    }
}