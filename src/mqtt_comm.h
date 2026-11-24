#ifndef MQTT_COMM_H
#define MQTT_COMM_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "app_types.h"

/* Khởi tạo Wi-Fi, MQTT Client và gắn hàng đợi nhận lệnh từ Dashboard */
void mqtt_comm_init(QueueHandle_t event_queue);

/* Duy trì kết nối mạng và đẩy bù dữ liệu từ Ring Buffer (gọi định kỳ trong comm_task) */
void mqtt_comm_loop(void);

/* Gửi bản tin telemetry định kỳ 5 s (hoặc đẩy vào Ring Buffer nếu mất mạng) */
void mqtt_comm_publish_telemetry(const room_state_t *st);

/* Gửi ngay sự kiện tức thì: COUNT, ALARM, WARNING, FAULT, CLEAR */
void mqtt_comm_publish_event(const char *ev_type, const char *detail, int32_t value);

#endif /* MQTT_COMM_H */