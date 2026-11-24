#ifndef PEOPLE_COUNTER_H
#define PEOPLE_COUNTER_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CNT_IDLE = 0,
    CNT_EN_A,
    CNT_EN_AB,
    CNT_EN_B,
    CNT_EX_B,
    CNT_EX_AB,
    CNT_EX_A,
    CNT_N_ST
} cnt_st_t;

typedef struct {
    cnt_st_t state;
    int16_t count;
    bool zone_a_occ;
    bool zone_b_occ;
    uint32_t non_idle_ms;       /* Thời gian kẹt ở trạng thái khác IDLE */
    uint32_t pir_empty_ms;      /* Thời gian PIR báo trống liên tục */
    uint32_t pir_occ_zero_ms;   /* Thời gian PIR báo có người nhưng count == 0 */
    bool mismatch_reported;
} people_counter_t;

void pc_init(people_counter_t *pc);

/* Gọi mỗi 25 ms với khoảng cách đo được từ 2 vùng cửa (mm) */
app_event_type_t pc_update_door(people_counter_t *pc, uint16_t dist_a_mm, uint16_t dist_b_mm, uint32_t dt_ms);

/* Gọi mỗi 1 s để đối chiếu với cảm biến hiện diện (PIR / mmWave) */
app_event_type_t pc_update_presence(people_counter_t *pc, bool presence, uint32_t dt_ms);

void pc_reset_count(people_counter_t *pc);

#ifdef __cplusplus
}
#endif

#endif /* PEOPLE_COUNTER_H */