#include "people_counter.h"
#include "config.h"

/* Bảng chuyển trạng thái Mealy 7x4 theo tài liệu thiết kế:
 * input p = (A << 1) | B : 0 = 00, 1 = 01 (B), 2 = 10 (A), 3 = 11 */
static const cnt_st_t next_st[CNT_N_ST][4] = {
/*               00        01         10         11       */
/* IDLE  */ { CNT_IDLE, CNT_EX_B,  CNT_EN_A,  CNT_IDLE  },
/* EN_A  */ { CNT_IDLE, CNT_EN_B,  CNT_EN_A,  CNT_EN_AB },
/* EN_AB */ { CNT_IDLE, CNT_EN_B,  CNT_EN_A,  CNT_EN_AB },
/* EN_B  */ { CNT_IDLE, CNT_EN_B,  CNT_IDLE,  CNT_EN_AB }, /* 00 => count++ */
/* EX_B  */ { CNT_IDLE, CNT_EX_B,  CNT_EX_A,  CNT_EX_AB },
/* EX_AB */ { CNT_IDLE, CNT_EX_B,  CNT_EX_A,  CNT_EX_AB },
/* EX_A  */ { CNT_IDLE, CNT_IDLE,  CNT_EX_A,  CNT_EX_AB }  /* 00 => count-- */
};

void pc_init(people_counter_t *pc) {
    pc->state = CNT_IDLE;
    pc->count = 0;
    pc->zone_a_occ = false;
    pc->zone_b_occ = false;
    pc->non_idle_ms = 0;
    pc->pir_empty_ms = 0;
    pc->pir_occ_zero_ms = 0;
    pc->mismatch_reported = false;
}

static bool update_zone_hysteresis(bool current_occ, uint16_t dist_mm) {
    if (!current_occ && dist_mm < DOOR_OCCUPIED_DIST_MM) {
        return true;
    }
    if (current_occ && dist_mm > DOOR_CLEAR_DIST_MM) {
        return false;
    }
    return current_occ;
}

app_event_type_t pc_update_door(people_counter_t *pc, uint16_t dist_a_mm, uint16_t dist_b_mm, uint32_t dt_ms) {
    app_event_type_t evt = EVT_NONE;

    pc->zone_a_occ = update_zone_hysteresis(pc->zone_a_occ, dist_a_mm);
    pc->zone_b_occ = update_zone_hysteresis(pc->zone_b_occ, dist_b_mm);

    uint8_t p = ((pc->zone_a_occ ? 1 : 0) << 1) | (pc->zone_b_occ ? 1 : 0);
    cnt_st_t prev = pc->state;
    cnt_st_t next = next_st[prev][p];

    /* Hành động Mealy gắn trên chuyển trạng thái về IDLE */
    if (prev == CNT_EN_B && p == 0) {
        if (pc->count < (MAX_ROOM_CAPACITY + 10)) {
            pc->count++;
        }
        evt = EVT_PERSON_ENTERED;
    } else if (prev == CNT_EX_A && p == 0) {
        if (pc->count > 0) {
            pc->count--;
        }
        evt = EVT_PERSON_EXITED;
    }

    pc->state = next;

    /* Timeout 3 giây nếu người đứng kẹt giữa cửa */
    if (pc->state != CNT_IDLE) {
        pc->non_idle_ms += dt_ms;
        if (pc->non_idle_ms >= COUNT_STUCK_TIMEOUT_MS) {
            pc->state = CNT_IDLE;
            pc->non_idle_ms = 0;
        }
    } else {
        pc->non_idle_ms = 0;
    }

    return evt;
}

app_event_type_t pc_update_presence(people_counter_t *pc, bool presence, uint32_t dt_ms) {
    if (!presence) {
        pc->pir_occ_zero_ms = 0;
        pc->mismatch_reported = false;

        if (pc->count > 0) {
            pc->pir_empty_ms += dt_ms;
            if (pc->pir_empty_ms >= DRIFT_EMPTY_RESET_MS) {
                pc->count = 0;
                pc->pir_empty_ms = 0;
                return EVT_COUNT_RESET;
            }
        } else {
            pc->pir_empty_ms = 0;
        }
    } else {
        pc->pir_empty_ms = 0;

        if (pc->count == 0) {
            pc->pir_occ_zero_ms += dt_ms;
            if (pc->pir_occ_zero_ms >= DRIFT_OCCUPIED_WARN_MS && !pc->mismatch_reported) {
                pc->mismatch_reported = true;
                return EVT_COUNT_MISMATCH;
            }
        } else {
            pc->pir_occ_zero_ms = 0;
            pc->mismatch_reported = false;
        }
    }
    return EVT_NONE;
}

void pc_reset_count(people_counter_t *pc) {
    pc->count = 0;
    pc->state = CNT_IDLE;
    pc->non_idle_ms = 0;
    pc->pir_empty_ms = 0;
}