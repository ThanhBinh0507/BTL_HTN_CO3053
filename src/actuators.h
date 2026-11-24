#ifndef ACTUATORS_H
#define ACTUATORS_H

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "app_types.h"

/* Khởi tạo GPIO chấp hành và gắn ngắt ngoài EXTI cho 2 nút nhấn */
void actuators_init(QueueHandle_t event_queue);

/* Xuất tín hiệu điều khiển đèn và quạt */
void actuators_set_loads(bool light_on, bool fan_on);

/* Cập nhật LED trạng thái xanh/đỏ và còi Buzzer (gọi mỗi 250 ms để nháy 2 Hz) */
void actuators_update_indicators(alarm_fsm_state_t alarm_state);

#endif /* ACTUATORS_H */