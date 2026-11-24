#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "app_types.h"
#include "people_counter.h"
#include "room_fsm.h"
#include "alarm_fsm.h"
#include "sensors.h"
#include "actuators.h"
#include "mqtt_comm.h"

/* Đối tượng đồng bộ FreeRTOS */
static QueueHandle_t xEventQueue = NULL;
static SemaphoreHandle_t xStateMutex = NULL;

/* Trạng thái toàn cục được bảo vệ bởi xStateMutex */
static room_state_t g_room_state = {0};

/* Các thực thể thuật toán FSM */
static people_counter_t g_pc;
static room_fsm_t g_rfsm;
static alarm_fsm_t g_afsm;

/* Cờ heartbeat kiểm tra treo task */
static volatile uint8_t g_task_heartbeat = 0;
#define HB_BIT_COUNTING (1 << 0)
#define HB_BIT_CONTROL  (1 << 1)
#define HB_BIT_SENSOR   (1 << 2)
#define HB_BIT_COMM     (1 << 3)
#define HB_ALL_TASKS    (HB_BIT_COUNTING | HB_BIT_CONTROL | HB_BIT_SENSOR | HB_BIT_COMM)

/* ================= TASK 1: ĐẾM NGƯỜI (Chu kỳ 25 ms - Ưu tiên cao nhất) ================= */
void counting_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIOD_COUNTING_MS);

    for (;;) {
        uint16_t dist_a = 2000, dist_b = 2000;
        sensors_read_door_mm(&dist_a, &dist_b);

        app_event_type_t ev_type = pc_update_door(&g_pc, dist_a, dist_b, PERIOD_COUNTING_MS);

        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            g_room_state.people_count = g_pc.count;
            xSemaphoreGive(xStateMutex);
        }

        if (ev_type != EVT_NONE) {
            app_event_t ev = { .type = ev_type, .param1 = g_pc.count, .param2 = 0 };
            xQueueSend(xEventQueue, &ev, 0);
        }

        g_task_heartbeat |= HB_BIT_COUNTING;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* ================= TASK 2: ĐIỀU KHIỂN FSM & BÁO ĐỘNG (Event-driven) ================= */
void control_task(void *pvParameters) {
    app_event_t ev;
    alarm_fsm_state_t prev_alarm_st = ALARM_ST_NORMAL;
    uint32_t last_ind_ms = 0;

    for (;;) {
        /* Chờ sự kiện tối đa 250 ms để đảm bảo cập nhật nháy LED/còi 2 Hz đúng nhịp */
        if (xQueueReceive(xEventQueue, &ev, pdMS_TO_TICKS(250)) == pdTRUE) {
            switch (ev.type) {
                case EVT_PERSON_ENTERED:
                    Serial.printf("[EVENT] Nguoi vao phong -> Count = %d\n", ev.param1);
                    mqtt_comm_publish_event("COUNT", "ENTER", ev.param1);
                    break;

                case EVT_PERSON_EXITED:
                    Serial.printf("[EVENT] Nguoi ra phong -> Count = %d\n", ev.param1);
                    mqtt_comm_publish_event("COUNT", "EXIT", ev.param1);
                    break;

                case EVT_COUNT_RESET:
                    Serial.println("[EVENT] Tu dong reset count = 0 do phong trong 5 phut");
                    mqtt_comm_publish_event("COUNT", "RESET", 0);
                    break;

                case EVT_COUNT_MISMATCH:
                    Serial.println("[WARN] Phat hien nguoi nhung count == 0!");
                    mqtt_comm_publish_event("WARN", "COUNT_MISMATCH", 0);
                    break;

                case EVT_BTN_MANUAL_PRESSED:
                    rfsm_toggle_manual_button(&g_rfsm);
                    Serial.printf("[CTRL] Nut MANUAL -> Mode: %s\n", g_rfsm.mode == CTRL_MODE_AUTO ? "AUTO" : "MANUAL");
                    break;

                case EVT_BTN_RESET_ALARM:
                    afsm_ack_alarm(&g_afsm);
                    Serial.println("[ALARM] Da xac nhan RESET bao dong!");
                    mqtt_comm_publish_event("CLEAR", "ALARM_ACK", 0);
                    break;

                case EVT_MQTT_CMD_RECEIVED:
                    rfsm_apply_remote_cmd(&g_rfsm, ev.param1, ev.param2);
                    Serial.printf("[MQTT] Nhan lenh tu xa: light=%d, fan=%d\n", ev.param1, ev.param2);
                    break;

                default:
                    break;
            }
        }

        /* Chạy cập nhật Room FSM và Alarm FSM */
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            rfsm_step(&g_rfsm, g_room_state.people_count, g_room_state.presence_detected,
                      g_room_state.illuminance_lux, g_room_state.temperature_c, 250);

            g_room_state.room_state = g_rfsm.state;
            g_room_state.mode       = g_rfsm.mode;

            afsm_step(&g_afsm, &g_room_state, 250);

            bool final_light = g_rfsm.light_on;
            bool final_fan   = g_rfsm.fan_on;
            afsm_apply_safety_override(&g_afsm, &final_light, &final_fan);

            g_room_state.light_on     = final_light;
            g_room_state.fan_on       = final_fan;
            g_room_state.alarm_state  = g_afsm.state;
            g_room_state.alarm_reason = (uint8_t)g_afsm.reason;

            actuators_set_loads(final_light, final_fan);

            /* Gửi event ngay lập tức khi chuyển mức cảnh báo */
            if (g_afsm.state != prev_alarm_st) {
                if (g_afsm.state == ALARM_ST_ALARM) {
                    Serial.printf("[ALARM!] Kich hoat bao dong khan cap! Ma ly do: %d\n", g_afsm.reason);
                    mqtt_comm_publish_event("ALARM", "TRIGGERED", g_afsm.reason);
                } else if (g_afsm.state == ALARM_ST_WARNING) {
                    Serial.printf("[WARNING] Canh bao bat thuong! Ma ly do: %d\n", g_afsm.reason);
                    mqtt_comm_publish_event("WARN", "TRIGGERED", g_afsm.reason);
                } else if (g_afsm.state == ALARM_ST_FAULT) {
                    Serial.printf("[FAULT] Loi cam bien! Bitmask: 0x%02X\n", g_room_state.fault_flags);
                    mqtt_comm_publish_event("FAULT", "SENSOR_ERR", g_room_state.fault_flags);
                }
                prev_alarm_st = g_afsm.state;
            }
            xSemaphoreGive(xStateMutex);
        }

        uint32_t now = millis();
        if (now - last_ind_ms >= 250) {
            last_ind_ms = now;
            actuators_update_indicators(g_afsm.state);
        }

        g_task_heartbeat |= HB_BIT_CONTROL;
    }
}

/* ================= TASK 3: ĐỌC CẢM BIẾN MÔI TRƯỜNG & ĐIỆN NĂNG ================= */
void sensor_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(PERIOD_FAST_SENSOR_MS);
    uint8_t sec_counter = 0;

    for (;;) {
        bool pres = sensors_read_presence();
        app_event_type_t drift_ev = pc_update_presence(&g_pc, pres, PERIOD_FAST_SENSOR_MS);

        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
            g_room_state.presence_detected = pres;
            g_room_state.people_count = g_pc.count;
            sensors_read_fast(&g_room_state, PERIOD_FAST_SENSOR_MS);

            /* Mỗi 5 giây đọc nhóm cảm biến chậm (Nhiệt độ, Độ ẩm, CO2) */
            if (sec_counter == 0) {
                sensors_read_slow(&g_room_state);
            }
            xSemaphoreGive(xStateMutex);
        }

        if (drift_ev != EVT_NONE) {
            app_event_t ev = { .type = drift_ev, .param1 = 0, .param2 = 0 };
            xQueueSend(xEventQueue, &ev, 0);
        }

        sec_counter = (sec_counter + 1) % 5;
        g_task_heartbeat |= HB_BIT_SENSOR;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/* ================= TASK 4: GIAO TIẾP MQTT & ĐẨY BÙ OFFLINE BUFFER ================= */
void comm_task(void *pvParameters) {
    uint32_t last_tel_ms = 0;
    room_state_t snapshot;

    for (;;) {
        mqtt_comm_loop();

        uint32_t now = millis();
        if (now - last_tel_ms >= PERIOD_TELEMETRY_MS) {
            last_tel_ms = now;
            if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
                memcpy(&snapshot, &g_room_state, sizeof(room_state_t));
                xSemaphoreGive(xStateMutex);

                mqtt_comm_publish_telemetry(&snapshot);
                Serial.printf("[TEL] Cnt:%d | T:%.1fC RH:%.0f%% CO2:%d | Lux:%.0f | P:%.2fW | L:%d F:%d\r\n",
                              snapshot.people_count, snapshot.temperature_c, snapshot.humidity_rh,
                              snapshot.co2_ppm, snapshot.illuminance_lux, snapshot.power_w,
                              snapshot.light_on, snapshot.fan_on);
            }
        }

        g_task_heartbeat |= HB_BIT_COMM;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ================= TASK 5: HEALTH MONITOR & HARDWARE WATCHDOG ================= */
void health_task(void *pvParameters) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_task_wdt_config_t wdt_config = {
        .timeout_ms = 10000,
        .idle_core_mask = 0,
        .trigger_panic = true
    };
    esp_task_wdt_reconfigure(&wdt_config);
#else
    esp_task_wdt_init(10, true);
#endif
    esp_task_wdt_add(NULL);

    for (;;) {
        if ((g_task_heartbeat & HB_ALL_TASKS) == HB_ALL_TASKS) {
            g_task_heartbeat = 0;
            esp_task_wdt_reset(); /* Chỉ nạp lại Watchdog khi cả 4 task đều sống */
        }
        vTaskDelay(pdMS_TO_TICKS(PERIOD_HEALTH_MS));
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== KHOI DONG SMART VIRTUAL SCHOOL ROOM NODE ===");

    xEventQueue = xQueueCreate(16, sizeof(app_event_t));
    xStateMutex = xSemaphoreCreateMutex();

    pc_init(&g_pc);
    rfsm_init(&g_rfsm);
    afsm_init(&g_afsm);

    sensors_init();
    actuators_init(xEventQueue);
    mqtt_comm_init(xEventQueue);

    /* Tạo 5 task FreeRTOS trên lõi 1 với thứ tự ưu tiên từ cao xuống thấp */
    xTaskCreatePinnedToCore(counting_task, "CountingTask", 3072, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(control_task,  "ControlTask",  4096, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(sensor_task,   "SensorTask",   4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(comm_task,     "CommTask",     6144, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(health_task,   "HealthTask",   2048, NULL, 1, NULL, 1);
}

void loop() {
    /* Không dùng loop() vì toàn bộ hệ thống chạy bằng FreeRTOS tasks */
    vTaskDelay(portMAX_DELAY);
}