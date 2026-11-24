#ifndef ROOM_FSM_H
#define ROOM_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    room_fsm_state_t state;
    ctrl_mode_t mode;
    bool light_on;
    bool fan_on;
    uint32_t grace_elapsed_ms;
} room_fsm_t;

void rfsm_init(room_fsm_t *fsm);

/* Cập nhật trạng thái phòng mỗi 1 s (hoặc khi có sự kiện mới) */
void rfsm_step(room_fsm_t *fsm, int16_t count, bool presence, float lux, float temp_c, uint32_t dt_ms);

/* Xử lý nút nhấn MANUAL tại phòng (đảo trạng thái) */
void rfsm_toggle_manual_button(room_fsm_t *fsm);

/* Xử lý lệnh từ MQTT Dashboard (-1: giữ nguyên, 0: OFF, 1: ON, 2: AUTO) */
void rfsm_apply_remote_cmd(room_fsm_t *fsm, int light_cmd, int fan_cmd);

#ifdef __cplusplus
}
#endif

#endif /* ROOM_FSM_H */