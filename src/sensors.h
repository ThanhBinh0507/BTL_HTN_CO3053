#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

void sensors_init(void);

/* Đọc khoảng cách 2 vùng cửa (mm) - gọi mỗi 25 ms */
void sensors_read_door_mm(uint16_t *dist_a_mm, uint16_t *dist_b_mm);

/* Đọc cảm biến hiện diện PIR - gọi mỗi 1 s */
bool sensors_read_presence(void);

/* Đọc cảm biến nhanh: ánh sáng (lux) và điện năng (V, I, P, Wh) - gọi mỗi 1 s */
void sensors_read_fast(room_state_t *st, uint32_t dt_ms);

/* Đọc cảm biến chậm: nhiệt độ, độ ẩm, CO2 - gọi mỗi 5 s */
void sensors_read_slow(room_state_t *st);

#endif /* SENSORS_H */